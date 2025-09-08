#include "opensubdiv/far/topologyDescriptor.h"
#include "opensubdiv/far/primvarRefiner.h"
#include <vector>
#include <iostream>
#include <cstdio>

// Vertex container implementation - required for PrimvarRefiner
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

// Simple cube topology data  
static float g_verts[8][3] = {{ -0.5f, -0.5f,  0.5f },
                              {  0.5f, -0.5f,  0.5f },
                              { -0.5f,  0.5f,  0.5f },
                              {  0.5f,  0.5f,  0.5f },
                              { -0.5f,  0.5f, -0.5f },
                              {  0.5f,  0.5f, -0.5f },
                              { -0.5f, -0.5f, -0.5f },
                              {  0.5f, -0.5f, -0.5f }};

static int g_nverts = 8,
           g_nfaces = 6;

static int g_vertsperface[6] = { 4, 4, 4, 4, 4, 4 };

static int g_vertIndices[24] = { 0, 1, 3, 2,
                                 2, 3, 5, 4,
                                 4, 5, 7, 6,
                                 6, 7, 1, 0,
                                 1, 7, 5, 3,
                                 6, 0, 2, 4  };

using namespace OpenSubdiv;

int main() {
    printf("OpenSubdiv WASM Test - CPU Only Build\n");
    printf("Creating a simple cube and performing subdivision...\n");

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
    
    // Create topology refiner
    Far::TopologyRefiner * refiner = Far::TopologyRefinerFactory<Descriptor>::Create(desc,
                                            Far::TopologyRefinerFactory<Descriptor>::Options(type, options));
    
    if (!refiner) {
        printf("ERROR: Failed to create topology refiner\n");
        return -1;
    }
    
    printf("Successfully created topology refiner\n");
    printf("Base level: %d vertices, %d faces\n", 
           refiner->GetLevel(0).GetNumVertices(), refiner->GetLevel(0).GetNumFaces());
    
    // Refine the topology
    int maxLevel = 2;
    refiner->RefineUniform(Far::TopologyRefiner::UniformOptions(maxLevel));
    
    printf("After %d levels of refinement: %d total vertices, %d total faces\n", 
           maxLevel, refiner->GetNumVerticesTotal(), refiner->GetNumFacesTotal());
    
    // Allocate buffer for vertex primvar data
    std::vector<Vertex> vbuffer(refiner->GetNumVerticesTotal());
    Vertex * verts = &vbuffer[0];
    
    // Initialize coarse mesh positions
    int nCoarseVerts = g_nverts;
    for (int i=0; i<nCoarseVerts; ++i) {
        verts[i].SetPosition(g_verts[i][0], g_verts[i][1], g_verts[i][2]);
    }
    
    // Interpolate vertex primvar data using PrimvarRefiner
    Far::PrimvarRefiner primvarRefiner(*refiner);
    
    Vertex * src = verts;
    for (int level = 1; level <= maxLevel; ++level) {
        Vertex * dst = src + refiner->GetLevel(level-1).GetNumVertices();
        primvarRefiner.Interpolate(level, src, dst);
        src = dst;
    }
    
    printf("Successfully subdivided mesh!\n");
    
    // Print some sample refined vertex positions
    printf("Sample coarse vertices:\n");
    for (int i = 0; i < std::min(4, nCoarseVerts); ++i) {
        const float * pos = verts[i].GetPosition();
        printf("  Vertex %d: (%.3f, %.3f, %.3f)\n", i, pos[0], pos[1], pos[2]);
    }
    
    printf("Sample refined vertices from level %d:\n", maxLevel);
    int level2Start = refiner->GetLevel(0).GetNumVertices() + refiner->GetLevel(1).GetNumVertices();
    int level2Count = refiner->GetLevel(2).GetNumVertices();
    for (int i = level2Start; i < std::min(level2Start + 4, level2Start + level2Count); ++i) {
        const float * pos = verts[i].GetPosition();
        printf("  Vertex %d: (%.3f, %.3f, %.3f)\n", i, pos[0], pos[1], pos[2]);
    }
    
    printf("\nWASM build test completed successfully!\n");
    printf("OpenSubdiv CPU-only build is working correctly.\n");
    
    // Cleanup
    delete refiner;
    
    return 0;
}