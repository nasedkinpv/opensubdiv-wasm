#include "opensubdiv/far/topologyDescriptor.h"
#include "opensubdiv/far/primvarRefiner.h"
#include <cstdio>
#include <chrono>
#include <vector>

// Simple vertex implementation
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

// Cube mesh data
static float g_verts[8][3] = {{ -0.5f, -0.5f,  0.5f },
                              {  0.5f, -0.5f,  0.5f },
                              { -0.5f,  0.5f,  0.5f },
                              {  0.5f,  0.5f,  0.5f },
                              { -0.5f,  0.5f, -0.5f },
                              {  0.5f,  0.5f, -0.5f },
                              { -0.5f, -0.5f, -0.5f },
                              {  0.5f, -0.5f, -0.5f }};

static int g_nverts = 8, g_nfaces = 6;
static int g_vertsperface[6] = { 4, 4, 4, 4, 4, 4 };
static int g_vertIndices[24] = { 0, 1, 3, 2,
                                 2, 3, 5, 4,
                                 4, 5, 7, 6,
                                 6, 7, 1, 0,
                                 1, 7, 5, 3,
                                 6, 0, 2, 4  };

using namespace OpenSubdiv;

int main() {
    printf("OpenSubdiv Wasmtime Benchmark\n");
    printf("==============================\n\n");
    
    // Detect build type based on compilation flags
    #ifdef __wasm_simd128__
    printf("Build Type: CPU-SIMD (WebAssembly SIMD128)\n");
    #else
    printf("Build Type: CPU-Basic\n");
    #endif
    
    printf("\nRunning subdivision benchmark...\n");
    
    // Create topology descriptor
    typedef Far::TopologyDescriptor Descriptor;
    
    Sdc::SchemeType type = Sdc::SCHEME_CATMARK;
    Sdc::Options options;
    options.SetVtxBoundaryInterpolation(Sdc::Options::VTX_BOUNDARY_EDGE_ONLY);

    Descriptor desc;
    desc.numVertices = g_nverts;
    desc.numFaces = g_nfaces;
    desc.numVertsPerFace = g_vertsperface;
    desc.vertIndicesPerFace = g_vertIndices;
    
    // Run multiple iterations for timing
    const int iterations = 1000;
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int iter = 0; iter < iterations; ++iter) {
        // Create topology refiner
        Far::TopologyRefiner * refiner = Far::TopologyRefinerFactory<Descriptor>::Create(desc,
                                                Far::TopologyRefinerFactory<Descriptor>::Options(type, options));
        
        // Refine the topology
        int maxLevel = 3;
        refiner->RefineUniform(Far::TopologyRefiner::UniformOptions(maxLevel));
        
        // Allocate vertex buffer
        std::vector<Vertex> vertices(refiner->GetNumVerticesTotal());
        
        // Initialize coarse vertices
        Vertex * verts = &vertices[0];
        for (int i=0; i<g_nverts; ++i) {
            verts[i].SetPosition(g_verts[i][0], g_verts[i][1], g_verts[i][2]);
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
    
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;
    
    printf("\nResults:\n");
    printf("--------\n");
    printf("Iterations: %d\n", iterations);
    printf("Total time: %.2f ms\n", elapsed.count());
    printf("Average time per iteration: %.4f ms\n", elapsed.count() / iterations);
    printf("Subdivisions per second: %.0f\n", (iterations * 1000.0) / elapsed.count());
    
    // Final mesh stats from last iteration
    printf("\nFinal Mesh Stats (Level 3):\n");
    printf("Base mesh: 8 vertices, 6 faces\n");
    printf("Refined mesh: ~386 vertices, ~384 faces\n");
    
    return 0;
}