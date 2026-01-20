#include "opensubdiv/far/topologyDescriptor.h"
#include "opensubdiv/far/primvarRefiner.h"
#include "opensubdiv/far/stencilTableFactory.h"
#include <cstdio>
#include <chrono>
#include <vector>
#include <cstring>
#include <cmath>

// Vertex implementation
struct Vertex {
    Vertex() { }
    Vertex(Vertex const & src) {
        std::memcpy(_position, src._position, sizeof(_position));
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

// Generate icosahedron mesh
void createIcosahedronMesh(std::vector<float>& verts, 
                          std::vector<int>& nverts,
                          std::vector<int>& faceverts,
                          int& numVerts, int& numFaces) {
    const float phi = (1.0f + sqrtf(5.0f)) * 0.5f;
    const float a = 1.0f;
    const float b = 1.0f / phi;
    
    // 12 vertices of icosahedron
    float icoverts[][3] = {
        {0, b, -a}, {b, a, 0}, {-b, a, 0}, {0, b, a},
        {0, -b, a}, {-a, 0, b}, {0, -b, -a}, {a, 0, -b},
        {a, 0, b}, {-a, 0, -b}, {b, -a, 0}, {-b, -a, 0}
    };
    
    verts.clear();
    for (int i = 0; i < 12; ++i) {
        verts.push_back(icoverts[i][0]);
        verts.push_back(icoverts[i][1]);
        verts.push_back(icoverts[i][2]);
    }
    numVerts = 12;
    
    // 20 triangular faces
    int faces[][3] = {
        {0,1,2}, {3,2,1}, {3,4,5}, {3,5,2}, {3,1,8},
        {3,8,4}, {0,6,7}, {0,7,1}, {11,4,10}, {11,10,6},
        {11,6,0}, {11,0,2}, {5,11,2}, {10,4,8}, {7,10,8},
        {7,8,1}, {6,10,7}, {9,0,6}, {9,2,0}, {9,5,2}
    };
    
    nverts.clear();
    faceverts.clear();
    for (int i = 0; i < 20; ++i) {
        nverts.push_back(3);
        faceverts.push_back(faces[i][0]);
        faceverts.push_back(faces[i][1]);
        faceverts.push_back(faces[i][2]);
    }
    numFaces = 20;
}

// Test function with different refinement levels
void runBenchmark(const char* buildType, int refinementLevel, int iterations) {
    printf("\n%s - Level %d Refinement (%d iterations)\n", buildType, refinementLevel, iterations);
    printf("----------------------------------------\n");
    
    // Create icosahedron mesh
    std::vector<float> verts;
    std::vector<int> nverts;
    std::vector<int> faceverts;
    int numVerts, numFaces;
    createIcosahedronMesh(verts, nverts, faceverts, numVerts, numFaces);
    
    // Create topology descriptor
    typedef Far::TopologyDescriptor Descriptor;
    Descriptor desc;
    desc.numVertices = numVerts;
    desc.numFaces = numFaces;
    desc.numVertsPerFace = nverts.data();
    desc.vertIndicesPerFace = faceverts.data();
    
    auto start = std::chrono::high_resolution_clock::now();
    int totalVertices = 0;
    
    for (int iter = 0; iter < iterations; ++iter) {
        // Create refiner
        Far::TopologyRefiner * refiner = 
            Far::TopologyRefinerFactory<Descriptor>::Create(desc,
                Far::TopologyRefinerFactory<Descriptor>::Options(
                    Sdc::SCHEME_CATMARK, Sdc::Options()));
        
        // Refine
        refiner->RefineUniform(Far::TopologyRefiner::UniformOptions(refinementLevel));
        
        // Allocate vertices
        std::vector<Vertex> refinedVerts(refiner->GetNumVerticesTotal());
        
        // Initialize coarse vertices
        for (int i = 0; i < numVerts; ++i) {
            refinedVerts[i].SetPosition(verts[i*3], verts[i*3+1], verts[i*3+2]);
        }
        
        // Interpolate
        Far::PrimvarRefiner primvarRefiner(*refiner);
        Vertex * src = &refinedVerts[0];
        for (int level = 1; level <= refinementLevel; ++level) {
            Vertex * dst = src + refiner->GetLevel(level-1).GetNumVertices();
            primvarRefiner.Interpolate(level, src, dst);
            src = dst;
        }
        
        totalVertices = refiner->GetNumVerticesTotal();
        delete refiner;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;
    
    printf("Input: %d vertices, %d faces\n", numVerts, numFaces);
    printf("Output: %d vertices\n", totalVertices);
    printf("Total time: %.2f ms\n", elapsed.count());
    printf("Average: %.4f ms/iteration\n", elapsed.count() / iterations);
    printf("Rate: %.0f subdivisions/sec\n", (iterations * 1000.0) / elapsed.count());
}

int main() {
    printf("OpenSubdiv WASM Comprehensive Benchmark\n");
    printf("========================================\n");
    
    #ifdef __wasm_simd128__
    printf("Build: CPU-SIMD (WebAssembly SIMD128 enabled)\n");
    #else
    printf("Build: CPU-Basic (no SIMD)\n");
    #endif
    
    // Test different refinement levels
    runBenchmark("Level 1", 1, 1000);
    runBenchmark("Level 2", 2, 500);
    runBenchmark("Level 3", 3, 200);
    runBenchmark("Level 4", 4, 50);
    
    printf("\n✓ Benchmark completed\n");
    
    return 0;
}