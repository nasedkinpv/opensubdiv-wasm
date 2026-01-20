//
// OpenSubdiv WASM Bindings for Three.js Integration
// Copyright 2024-2025 nasedkinpv
// 
// Licensed under the Tomorrow Open Source Technology License 1.0 (TOST-1.0)
// See LICENSE.txt for details
//
// Based on OpenSubdiv by Pixar Animation Studios
// See NOTICE.txt for full attribution
//

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <opensubdiv/far/topologyDescriptor.h>
#include <opensubdiv/far/topologyRefinerFactory.h>
#include <opensubdiv/far/primvarRefiner.h>
#include <opensubdiv/far/stencilTableFactory.h>
#include <opensubdiv/osd/cpuEvaluator.h>
#include <opensubdiv/osd/bufferDescriptor.h>
#include <opensubdiv/sdc/types.h>
#include <opensubdiv/sdc/options.h>

#include <vector>
#include <cstring>
#include <cmath>

using namespace OpenSubdiv;
using namespace emscripten;

struct Vertex3f {
    float x, y, z;
    
    Vertex3f() : x(0), y(0), z(0) {}
    Vertex3f(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
    
    void Clear() { x = y = z = 0.0f; }
    
    void AddWithWeight(Vertex3f const& src, float weight) {
        x += weight * src.x;
        y += weight * src.y;
        z += weight * src.z;
    }
};

struct Vertex2f {
    float u, v;
    
    Vertex2f() : u(0), v(0) {}
    Vertex2f(float _u, float _v) : u(_u), v(_v) {}
    
    void Clear() { u = v = 0.0f; }
    
    void AddWithWeight(Vertex2f const& src, float weight) {
        u += weight * src.u;
        v += weight * src.v;
    }
};

class SubdivisionMesh {
public:
    SubdivisionMesh() : refiner_(nullptr), stencilTable_(nullptr) {}
    
    ~SubdivisionMesh() {
        cleanup();
    }
    
    bool initFromQuads(
        val positionsJS,
        val quadIndicesJS,
        int numQuads,
        int subdivisionLevel,
        int boundaryInterpolation = 1
    ) {
        return initInternal(positionsJS, quadIndicesJS, numQuads, subdivisionLevel, 
                           boundaryInterpolation, val::null(), val::null(), 0);
    }
    
    bool initFromQuadsWithUVs(
        val positionsJS,
        val quadIndicesJS,
        int numQuads,
        int subdivisionLevel,
        int boundaryInterpolation,
        val uvsJS,
        val uvIndicesJS,
        int numUVs
    ) {
        return initInternal(positionsJS, quadIndicesJS, numQuads, subdivisionLevel,
                           boundaryInterpolation, uvsJS, uvIndicesJS, numUVs);
    }
    
    bool initFromPolygons(
        val positionsJS,
        val faceIndicesJS,
        val faceSizesJS,
        int subdivisionLevel,
        int boundaryInterpolation = 1
    ) {
        return initPolygonsInternal(positionsJS, faceIndicesJS, faceSizesJS, 
                                    subdivisionLevel, boundaryInterpolation,
                                    val::null(), val::null(), 0);
    }
    
    bool initFromPolygonsWithUVs(
        val positionsJS,
        val faceIndicesJS,
        val faceSizesJS,
        int subdivisionLevel,
        int boundaryInterpolation,
        val uvsJS,
        val uvIndicesJS,
        int numUVs
    ) {
        return initPolygonsInternal(positionsJS, faceIndicesJS, faceSizesJS,
                                    subdivisionLevel, boundaryInterpolation,
                                    uvsJS, uvIndicesJS, numUVs);
    }
    
    void updatePositions(val positionsJS) {
        std::vector<float> positions = floatVecFromJS(positionsJS);
        
        int numVertices = positions.size() / 3;
        if (numVertices != static_cast<int>(inputVertices_.size())) {
            return;
        }
        
        for (int i = 0; i < numVertices; ++i) {
            inputVertices_[i].x = positions[i * 3 + 0];
            inputVertices_[i].y = positions[i * 3 + 1];
            inputVertices_[i].z = positions[i * 3 + 2];
        }
        
        subdivide();
    }
    
    val getPositions() const {
        return val(typed_memory_view(outputPositions_.size(), outputPositions_.data()));
    }
    
    val getNormals() const {
        return val(typed_memory_view(outputNormals_.size(), outputNormals_.data()));
    }
    
    val getIndices() const {
        return val(typed_memory_view(outputIndices_.size(), outputIndices_.data()));
    }
    
    val getUVs() const {
        return val(typed_memory_view(outputUVs_.size(), outputUVs_.data()));
    }
    
    val getUVIndices() const {
        return val(typed_memory_view(outputUVIndices_.size(), outputUVIndices_.data()));
    }
    
    int getVertexCount() const {
        return outputPositions_.size() / 3;
    }
    
    int getTriangleCount() const {
        return outputIndices_.size() / 3;
    }
    
    int getInputVertexCount() const {
        return inputVertices_.size();
    }
    
    int getUVCount() const {
        return outputUVs_.size() / 2;
    }
    
    bool hasUVData() const {
        return hasUVs_;
    }
    
private:
    std::vector<float> floatVecFromJS(const val& v) {
        return convertJSArrayToNumberVector<float>(v);
    }
    
    std::vector<int> intVecFromJS(const val& v) {
        return convertJSArrayToNumberVector<int>(v);
    }
    
    void cleanup() {
        delete refiner_;
        refiner_ = nullptr;
        delete stencilTable_;
        stencilTable_ = nullptr;
        inputVertices_.clear();
        inputUVs_.clear();
        outputPositions_.clear();
        outputNormals_.clear();
        outputIndices_.clear();
        outputUVs_.clear();
        outputUVIndices_.clear();
        hasUVs_ = false;
    }
    
    bool initInternal(
        val positionsJS,
        val quadIndicesJS,
        int numQuads,
        int subdivisionLevel,
        int boundaryInterpolation,
        val uvsJS,
        val uvIndicesJS,
        int numUVs
    ) {
        cleanup();
        
        std::vector<float> positions = floatVecFromJS(positionsJS);
        std::vector<int> quadIndices = intVecFromJS(quadIndicesJS);
        
        std::vector<float> uvs;
        std::vector<int> uvIndices;
        bool hasUVs = !uvsJS.isNull() && !uvIndicesJS.isNull() && numUVs > 0;
        
        if (hasUVs) {
            uvs = floatVecFromJS(uvsJS);
            uvIndices = intVecFromJS(uvIndicesJS);
        }
        
        int numVertices = positions.size() / 3;
        std::vector<int> faceSizes(numQuads, 4);
        
        Far::TopologyDescriptor desc;
        desc.numVertices = numVertices;
        desc.numFaces = numQuads;
        desc.numVertsPerFace = faceSizes.data();
        desc.vertIndicesPerFace = quadIndices.data();
        
        Far::TopologyDescriptor::FVarChannel uvChannel;
        if (hasUVs) {
            uvChannel.numValues = numUVs;
            uvChannel.valueIndices = uvIndices.data();
            desc.numFVarChannels = 1;
            desc.fvarChannels = &uvChannel;
        }
        
        Sdc::SchemeType type = Sdc::SCHEME_CATMARK;
        Sdc::Options options;
        options.SetFVarLinearInterpolation(Sdc::Options::FVAR_LINEAR_CORNERS_ONLY);
        
        switch (boundaryInterpolation) {
            case 0: options.SetVtxBoundaryInterpolation(Sdc::Options::VTX_BOUNDARY_NONE); break;
            case 1: options.SetVtxBoundaryInterpolation(Sdc::Options::VTX_BOUNDARY_EDGE_ONLY); break;
            case 2: options.SetVtxBoundaryInterpolation(Sdc::Options::VTX_BOUNDARY_EDGE_AND_CORNER); break;
        }
        
        refiner_ = Far::TopologyRefinerFactory<Far::TopologyDescriptor>::Create(
            desc, Far::TopologyRefinerFactory<Far::TopologyDescriptor>::Options(type, options));
        
        if (!refiner_) {
            return false;
        }
        
        subdivisionLevel_ = subdivisionLevel;
        hasUVs_ = hasUVs;
        
        Far::TopologyRefiner::UniformOptions refineOptions(subdivisionLevel);
        refineOptions.fullTopologyInLastLevel = true;
        refiner_->RefineUniform(refineOptions);
        
        inputVertices_.resize(numVertices);
        for (int i = 0; i < numVertices; ++i) {
            inputVertices_[i].x = positions[i * 3 + 0];
            inputVertices_[i].y = positions[i * 3 + 1];
            inputVertices_[i].z = positions[i * 3 + 2];
        }
        
        if (hasUVs) {
            inputUVs_.resize(numUVs);
            for (int i = 0; i < numUVs; ++i) {
                inputUVs_[i].u = uvs[i * 2 + 0];
                inputUVs_[i].v = uvs[i * 2 + 1];
            }
        }
        
        Far::StencilTableFactory::Options stencilOptions;
        stencilOptions.generateOffsets = true;
        stencilOptions.generateControlVerts = false;
        stencilOptions.generateIntermediateLevels = false;
        
        stencilTable_ = Far::StencilTableFactory::Create(*refiner_, stencilOptions);
        
        subdivide();
        if (hasUVs_) {
            subdivideUVs();
        }
        generateTriangleIndices();
        
        return true;
    }
    
    bool initPolygonsInternal(
        val positionsJS,
        val faceIndicesJS,
        val faceSizesJS,
        int subdivisionLevel,
        int boundaryInterpolation,
        val uvsJS,
        val uvIndicesJS,
        int numUVs
    ) {
        cleanup();
        
        std::vector<float> positions = floatVecFromJS(positionsJS);
        std::vector<int> faceIndices = intVecFromJS(faceIndicesJS);
        std::vector<int> faceSizes = intVecFromJS(faceSizesJS);
        
        std::vector<float> uvs;
        std::vector<int> uvIndices;
        bool hasUVs = !uvsJS.isNull() && !uvIndicesJS.isNull() && numUVs > 0;
        
        if (hasUVs) {
            uvs = floatVecFromJS(uvsJS);
            uvIndices = intVecFromJS(uvIndicesJS);
        }
        
        int numVertices = positions.size() / 3;
        int numFaces = faceSizes.size();
        
        Far::TopologyDescriptor desc;
        desc.numVertices = numVertices;
        desc.numFaces = numFaces;
        desc.numVertsPerFace = faceSizes.data();
        desc.vertIndicesPerFace = faceIndices.data();
        
        Far::TopologyDescriptor::FVarChannel uvChannel;
        if (hasUVs) {
            uvChannel.numValues = numUVs;
            uvChannel.valueIndices = uvIndices.data();
            desc.numFVarChannels = 1;
            desc.fvarChannels = &uvChannel;
        }
        
        Sdc::SchemeType type = Sdc::SCHEME_CATMARK;
        Sdc::Options options;
        options.SetFVarLinearInterpolation(Sdc::Options::FVAR_LINEAR_CORNERS_ONLY);
        
        switch (boundaryInterpolation) {
            case 0: options.SetVtxBoundaryInterpolation(Sdc::Options::VTX_BOUNDARY_NONE); break;
            case 1: options.SetVtxBoundaryInterpolation(Sdc::Options::VTX_BOUNDARY_EDGE_ONLY); break;
            case 2: options.SetVtxBoundaryInterpolation(Sdc::Options::VTX_BOUNDARY_EDGE_AND_CORNER); break;
        }
        
        refiner_ = Far::TopologyRefinerFactory<Far::TopologyDescriptor>::Create(
            desc, Far::TopologyRefinerFactory<Far::TopologyDescriptor>::Options(type, options));
        
        if (!refiner_) return false;
        
        subdivisionLevel_ = subdivisionLevel;
        hasUVs_ = hasUVs;
        
        Far::TopologyRefiner::UniformOptions refineOptions(subdivisionLevel);
        refineOptions.fullTopologyInLastLevel = true;
        refiner_->RefineUniform(refineOptions);
        
        inputVertices_.resize(numVertices);
        for (int i = 0; i < numVertices; ++i) {
            inputVertices_[i].x = positions[i * 3 + 0];
            inputVertices_[i].y = positions[i * 3 + 1];
            inputVertices_[i].z = positions[i * 3 + 2];
        }
        
        if (hasUVs) {
            inputUVs_.resize(numUVs);
            for (int i = 0; i < numUVs; ++i) {
                inputUVs_[i].u = uvs[i * 2 + 0];
                inputUVs_[i].v = uvs[i * 2 + 1];
            }
        }
        
        Far::StencilTableFactory::Options stencilOptions;
        stencilOptions.generateOffsets = true;
        stencilOptions.generateControlVerts = false;
        stencilOptions.generateIntermediateLevels = false;
        
        stencilTable_ = Far::StencilTableFactory::Create(*refiner_, stencilOptions);
        
        subdivide();
        if (hasUVs_) {
            subdivideUVs();
        }
        generateTriangleIndices();
        
        return true;
    }
    
    void subdivide() {
        if (!refiner_ || !stencilTable_) return;
        
        int numOutputVerts = stencilTable_->GetNumStencils();
        int numControlVerts = inputVertices_.size();
        int totalVerts = numControlVerts + numOutputVerts;
        
        std::vector<float> vertexBuffer(totalVerts * 3);
        
        for (int i = 0; i < numControlVerts; ++i) {
            vertexBuffer[i * 3 + 0] = inputVertices_[i].x;
            vertexBuffer[i * 3 + 1] = inputVertices_[i].y;
            vertexBuffer[i * 3 + 2] = inputVertices_[i].z;
        }
        
        Osd::BufferDescriptor srcDesc(0, 3, 3);
        Osd::BufferDescriptor dstDesc(numControlVerts * 3, 3, 3);
        
        Osd::CpuEvaluator::EvalStencils(
            vertexBuffer.data(), srcDesc,
            vertexBuffer.data(), dstDesc,
            &stencilTable_->GetSizes()[0],
            &stencilTable_->GetOffsets()[0],
            &stencilTable_->GetControlIndices()[0],
            &stencilTable_->GetWeights()[0],
            0,
            stencilTable_->GetNumStencils()
        );
        
        Far::TopologyLevel const& lastLevel = refiner_->GetLevel(subdivisionLevel_);
        int numFinalVerts = lastLevel.GetNumVertices();
        
        outputPositions_.resize(numFinalVerts * 3);
        for (int i = 0; i < numFinalVerts; ++i) {
            int srcIdx = (numControlVerts + i) * 3;
            outputPositions_[i * 3 + 0] = vertexBuffer[srcIdx + 0];
            outputPositions_[i * 3 + 1] = vertexBuffer[srcIdx + 1];
            outputPositions_[i * 3 + 2] = vertexBuffer[srcIdx + 2];
        }
        
        computeNormals();
    }
    
    void computeNormals() {
        Far::TopologyLevel const& lastLevel = refiner_->GetLevel(subdivisionLevel_);
        int numVerts = lastLevel.GetNumVertices();
        int numFaces = lastLevel.GetNumFaces();
        
        outputNormals_.resize(numVerts * 3, 0.0f);
        
        for (int face = 0; face < numFaces; ++face) {
            Far::ConstIndexArray fverts = lastLevel.GetFaceVertices(face);
            
            if (fverts.size() >= 3) {
                int i0 = fverts[0], i1 = fverts[1], i2 = fverts[2];
                
                float ax = outputPositions_[i1 * 3 + 0] - outputPositions_[i0 * 3 + 0];
                float ay = outputPositions_[i1 * 3 + 1] - outputPositions_[i0 * 3 + 1];
                float az = outputPositions_[i1 * 3 + 2] - outputPositions_[i0 * 3 + 2];
                
                float bx = outputPositions_[i2 * 3 + 0] - outputPositions_[i0 * 3 + 0];
                float by = outputPositions_[i2 * 3 + 1] - outputPositions_[i0 * 3 + 1];
                float bz = outputPositions_[i2 * 3 + 2] - outputPositions_[i0 * 3 + 2];
                
                float nx = ay * bz - az * by;
                float ny = az * bx - ax * bz;
                float nz = ax * by - ay * bx;
                
                for (int v = 0; v < fverts.size(); ++v) {
                    int idx = fverts[v];
                    outputNormals_[idx * 3 + 0] += nx;
                    outputNormals_[idx * 3 + 1] += ny;
                    outputNormals_[idx * 3 + 2] += nz;
                }
            }
        }
        
        for (int i = 0; i < numVerts; ++i) {
            float nx = outputNormals_[i * 3 + 0];
            float ny = outputNormals_[i * 3 + 1];
            float nz = outputNormals_[i * 3 + 2];
            
            float len = std::sqrt(nx * nx + ny * ny + nz * nz);
            if (len > 0.0001f) {
                outputNormals_[i * 3 + 0] = nx / len;
                outputNormals_[i * 3 + 1] = ny / len;
                outputNormals_[i * 3 + 2] = nz / len;
            }
        }
    }
    
    void generateTriangleIndices() {
        Far::TopologyLevel const& lastLevel = refiner_->GetLevel(subdivisionLevel_);
        int numFaces = lastLevel.GetNumFaces();
        
        outputIndices_.clear();
        outputIndices_.reserve(numFaces * 6);
        
        if (hasUVs_) {
            outputUVIndices_.clear();
            outputUVIndices_.reserve(numFaces * 6);
        }
        
        for (int face = 0; face < numFaces; ++face) {
            Far::ConstIndexArray fverts = lastLevel.GetFaceVertices(face);
            Far::ConstIndexArray fuvs;
            if (hasUVs_) {
                fuvs = lastLevel.GetFaceFVarValues(face, 0);
            }
            
            if (fverts.size() == 4) {
                outputIndices_.push_back(fverts[0]);
                outputIndices_.push_back(fverts[1]);
                outputIndices_.push_back(fverts[2]);
                outputIndices_.push_back(fverts[0]);
                outputIndices_.push_back(fverts[2]);
                outputIndices_.push_back(fverts[3]);
                
                if (hasUVs_ && fuvs.size() == 4) {
                    outputUVIndices_.push_back(fuvs[0]);
                    outputUVIndices_.push_back(fuvs[1]);
                    outputUVIndices_.push_back(fuvs[2]);
                    outputUVIndices_.push_back(fuvs[0]);
                    outputUVIndices_.push_back(fuvs[2]);
                    outputUVIndices_.push_back(fuvs[3]);
                }
            } else if (fverts.size() == 3) {
                outputIndices_.push_back(fverts[0]);
                outputIndices_.push_back(fverts[1]);
                outputIndices_.push_back(fverts[2]);
                
                if (hasUVs_ && fuvs.size() == 3) {
                    outputUVIndices_.push_back(fuvs[0]);
                    outputUVIndices_.push_back(fuvs[1]);
                    outputUVIndices_.push_back(fuvs[2]);
                }
            } else {
                for (int i = 1; i < fverts.size() - 1; ++i) {
                    outputIndices_.push_back(fverts[0]);
                    outputIndices_.push_back(fverts[i]);
                    outputIndices_.push_back(fverts[i + 1]);
                    
                    if (hasUVs_ && fuvs.size() > i + 1) {
                        outputUVIndices_.push_back(fuvs[0]);
                        outputUVIndices_.push_back(fuvs[i]);
                        outputUVIndices_.push_back(fuvs[i + 1]);
                    }
                }
            }
        }
    }
    
    void subdivideUVs() {
        if (!refiner_ || !hasUVs_) return;
        
        Far::PrimvarRefiner primvarRefiner(*refiner_);
        
        std::vector<Vertex2f> srcUVs = inputUVs_;
        std::vector<Vertex2f> dstUVs;
        
        for (int level = 1; level <= subdivisionLevel_; ++level) {
            Far::TopologyLevel const& lvl = refiner_->GetLevel(level);
            dstUVs.resize(lvl.GetNumFVarValues(0));
            primvarRefiner.InterpolateFaceVarying(level, srcUVs, dstUVs, 0);
            std::swap(srcUVs, dstUVs);
        }
        
        Far::TopologyLevel const& lastLevel = refiner_->GetLevel(subdivisionLevel_);
        int numFinalUVs = lastLevel.GetNumFVarValues(0);
        
        outputUVs_.resize(numFinalUVs * 2);
        for (int i = 0; i < numFinalUVs; ++i) {
            outputUVs_[i * 2 + 0] = srcUVs[i].u;
            outputUVs_[i * 2 + 1] = srcUVs[i].v;
        }
    }
    
    Far::TopologyRefiner* refiner_;
    Far::StencilTable const* stencilTable_;
    int subdivisionLevel_;
    bool hasUVs_ = false;
    
    std::vector<Vertex3f> inputVertices_;
    std::vector<Vertex2f> inputUVs_;
    std::vector<float> outputPositions_;
    std::vector<float> outputNormals_;
    std::vector<float> outputUVs_;
    std::vector<uint32_t> outputIndices_;
    std::vector<uint32_t> outputUVIndices_;
};

EMSCRIPTEN_BINDINGS(opensubdiv_module) {
    
    class_<SubdivisionMesh>("SubdivisionMesh")
        .constructor<>()
        .function("initFromQuads", &SubdivisionMesh::initFromQuads)
        .function("initFromQuadsWithUVs", &SubdivisionMesh::initFromQuadsWithUVs)
        .function("initFromPolygons", &SubdivisionMesh::initFromPolygons)
        .function("initFromPolygonsWithUVs", &SubdivisionMesh::initFromPolygonsWithUVs)
        .function("updatePositions", &SubdivisionMesh::updatePositions)
        .function("getPositions", &SubdivisionMesh::getPositions)
        .function("getNormals", &SubdivisionMesh::getNormals)
        .function("getIndices", &SubdivisionMesh::getIndices)
        .function("getUVs", &SubdivisionMesh::getUVs)
        .function("getUVIndices", &SubdivisionMesh::getUVIndices)
        .function("getVertexCount", &SubdivisionMesh::getVertexCount)
        .function("getTriangleCount", &SubdivisionMesh::getTriangleCount)
        .function("getInputVertexCount", &SubdivisionMesh::getInputVertexCount)
        .function("getUVCount", &SubdivisionMesh::getUVCount)
        .function("hasUVData", &SubdivisionMesh::hasUVData);
    
    constant("BOUNDARY_NONE", 0);
    constant("BOUNDARY_EDGE_ONLY", 1);
    constant("BOUNDARY_EDGE_AND_CORNER", 2);
}
