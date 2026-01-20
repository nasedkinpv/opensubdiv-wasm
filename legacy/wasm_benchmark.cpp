#include "opensubdiv/far/topologyDescriptor.h"
#include "opensubdiv/far/primvarRefiner.h"
#include <vector>
#include <iostream>
#include <cstdio>
#include <chrono>
#include <cstring>

// Vertex container implementation
struct Vertex {
    Vertex() { }

    Vertex(Vertex const & src) {
        _position[0] = src._position[0];
        _position[1] = src._position[1];
        _position[2] = src._position[2];
    }

    void Clear( void * =0 ) {
        _position[0]=_position[1]=_position[2]=0.0f;
    }

    void AddWithWeight(Vertex const & src, float weight) {
        _position[0]+=weight*src._position[0];
        _position[1]+=weight*src._position[1];
        _position[2]+=weight*src._position[2];
    }

    void SetPosition(float x, float y, float z) {
        _position[0]=x;
        _position[1]=y;
        _position[2]=z;
    }

    const float * GetPosition() const {
        return _position;
    }

private:
    float _position[3];
};

using namespace OpenSubdiv;

// Generate a more complex test mesh (subdivided icosahedron)
struct TestMesh {
    std::vector<float> vertices;
    std::vector<int> faces;
    std::vector<int> faceCounts;
    
    int numVertices;
    int numFaces;
    
    TestMesh() {
        createIcosahedron();
    }
    
private:
    void createIcosahedron() {
        const float phi = (1.0f + sqrt(5.0f)) * 0.5f; // golden ratio
        const float a = 1.0f;
        const float b = 1.0f / phi;
        
        // 12 vertices of icosahedron
        float verts[12][3] = {
            {0, b, -a}, {b, a, 0}, {-b, a, 0}, {0, b, a},
            {0, -b, a}, {-b, -a, 0}, {b, -a, 0}, {0, -b, -a},
            {a, 0, -b}, {a, 0, b}, {-a, 0, b}, {-a, 0, -b}
        };
        
        // 20 triangular faces
        int faceIndices[20][3] = {
            {2, 1, 0}, {1, 2, 3}, {5, 4, 3}, {4, 8, 3},
            {7, 6, 0}, {6, 9, 0}, {11, 10, 4}, {10, 11, 6},
            {9, 8, 2}, {8, 9, 4}, {11, 7, 2}, {7, 11, 10},
            {5, 3, 10}, {3, 8, 10}, {5, 10, 6}, {10, 8, 6},
            {1, 9, 6}, {9, 1, 8}, {1, 3, 9}, {7, 0, 2}
        };
        
        // Copy vertices
        for (int i = 0; i < 12; ++i) {
            vertices.push_back(verts[i][0]);
            vertices.push_back(verts[i][1]);
            vertices.push_back(verts[i][2]);
        }
        
        // Copy faces
        for (int i = 0; i < 20; ++i) {
            faceCounts.push_back(3);
            faces.push_back(faceIndices[i][0]);
            faces.push_back(faceIndices[i][1]);
            faces.push_back(faceIndices[i][2]);
        }
        
        numVertices = 12;
        numFaces = 20;
    }
};

double benchmarkSubdivision(const TestMesh& mesh, int maxLevel, int iterations = 100) {
    typedef Far::TopologyDescriptor Descriptor;
    
    Sdc::SchemeType type = Sdc::SCHEME_LOOP; // Use Loop for triangular mesh
    Sdc::Options options;
    options.SetVtxBoundaryInterpolation(Sdc::Options::VTX_BOUNDARY_EDGE_ONLY);

    Descriptor desc;
    desc.numVertices = mesh.numVertices;
    desc.numFaces = mesh.numFaces;
    desc.numVertsPerFace = const_cast<int*>(&mesh.faceCounts[0]);
    desc.vertIndicesPerFace = const_cast<int*>(&mesh.faces[0]);
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    for (int iter = 0; iter < iterations; ++iter) {
        // Create topology refiner
        Far::TopologyRefiner * refiner = Far::TopologyRefinerFactory<Descriptor>::Create(desc,
                                                Far::TopologyRefinerFactory<Descriptor>::Options(type, options));
        
        if (!refiner) {
            printf("ERROR: Failed to create topology refiner\n");
            return -1;
        }
        
        // Refine topology
        refiner->RefineUniform(Far::TopologyRefiner::UniformOptions(maxLevel));
        
        // Allocate vertex buffer
        std::vector<Vertex> vbuffer(refiner->GetNumVerticesTotal());
        Vertex * verts = &vbuffer[0];
        
        // Initialize coarse vertices
        for (int i = 0; i < mesh.numVertices; ++i) {
            verts[i].SetPosition(mesh.vertices[i*3], 
                               mesh.vertices[i*3+1], 
                               mesh.vertices[i*3+2]);
        }
        
        // Perform subdivision
        Far::PrimvarRefiner primvarRefiner(*refiner);
        
        Vertex * src = verts;
        for (int level = 1; level <= maxLevel; ++level) {
            Vertex * dst = src + refiner->GetLevel(level-1).GetNumVertices();
            primvarRefiner.Interpolate(level, src, dst);
            src = dst;
        }
        
        delete refiner;
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    
    return duration.count() / 1000.0 / iterations; // Return average time in milliseconds
}

int main() {
    printf("OpenSubdiv WASM Performance Benchmark\n");
    printf("=====================================\n");
    
    TestMesh mesh;
    printf("Test mesh: %d vertices, %d faces (icosahedron)\n", mesh.numVertices, mesh.numFaces);
    
    const int maxLevel = 3;
    const int iterations = 50;
    
    printf("Benchmarking %d iterations of %d-level subdivision...\n", iterations, maxLevel);
    
    // Run benchmark
    double avgTime = benchmarkSubdivision(mesh, maxLevel, iterations);
    
    if (avgTime < 0) {
        printf("Benchmark failed\n");
        return -1;
    }
    
    printf("\nResults:\n");
    printf("Average subdivision time: %.3f ms\n", avgTime);
    printf("Subdivisions per second: %.1f\n", 1000.0 / avgTime);
    
    // Calculate theoretical performance
    TestMesh testMesh;
    typedef Far::TopologyDescriptor Descriptor;
    Sdc::SchemeType type = Sdc::SCHEME_LOOP;
    Sdc::Options options;
    
    Descriptor desc;
    desc.numVertices = testMesh.numVertices;
    desc.numFaces = testMesh.numFaces;
    desc.numVertsPerFace = const_cast<int*>(&testMesh.faceCounts[0]);
    desc.vertIndicesPerFace = const_cast<int*>(&testMesh.faces[0]);
    
    Far::TopologyRefiner * refiner = Far::TopologyRefinerFactory<Descriptor>::Create(desc,
                                            Far::TopologyRefinerFactory<Descriptor>::Options(type, options));
    refiner->RefineUniform(Far::TopologyRefiner::UniformOptions(maxLevel));
    
    int totalVertices = refiner->GetNumVerticesTotal();
    int totalFaces = refiner->GetNumFacesTotal();
    
    printf("Final mesh: %d vertices, %d faces\n", totalVertices, totalFaces);
    printf("Vertex processing rate: %.0f vertices/ms\n", totalVertices / avgTime);
    
    delete refiner;
    
    printf("\nBenchmark completed successfully!\n");
    
    return 0;
}