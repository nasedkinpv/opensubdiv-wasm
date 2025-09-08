//
//   Copyright 2024 Pixar
//
//   Licensed under the Apache License, Version 2.0 (the "Apache License")
//   with the following modification; you may not use this file except in
//   compliance with the Apache License and the following modification to it:
//   Section 6. Trademarks. is deleted and replaced with:
//
//   6. Trademarks. This License does not grant permission to use the trade
//      names, trademarks, service marks, or product names of the Licensor
//      and its affiliates, except as required to comply with Section 4(c) of
//      the License and to reproduce the content of the NOTICE file.
//
//   You may obtain a copy of the Apache License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the Apache License with the above modification is
//   distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
//   KIND, either express or implied. See the Apache License for the specific
//   language governing permissions and limitations under the Apache License.
//

#include "../osd/webgpuComputeEvaluator.h"
#include "../far/stencilTable.h"
#include "../far/error.h"

#include <vector>
#include <string>
#include <cassert>

namespace OpenSubdiv {
namespace OPENSUBDIV_VERSION {

namespace Osd {

// Include the WGSL shader source as a string constant
static const char* webgpuComputeKernelWGSL = R"WGSL(
//
//   WebGPU compute shader for OpenSubdiv stencil evaluation
//

// Uniforms
struct UniformData {
    srcOffset: u32,
    dstOffset: u32,
    batchStart: u32,
    batchEnd: u32,
    length: u32,
    srcStride: u32,
    dstStride: u32,
}

@group(0) @binding(0) var<uniform> uniforms: UniformData;

// Source and destination vertex buffers
@group(0) @binding(1) var<storage, read> srcVertexBuffer: array<f32>;
@group(0) @binding(2) var<storage, read_write> dstVertexBuffer: array<f32>;

// Stencil data buffers
@group(0) @binding(3) var<storage, read> stencilSizes: array<u32>;
@group(0) @binding(4) var<storage, read> stencilOffsets: array<u32>;
@group(0) @binding(5) var<storage, read> stencilIndices: array<u32>;
@group(0) @binding(6) var<storage, read> stencilWeights: array<f32>;

// Optional derivative buffers
@group(0) @binding(7) var<storage, read_write> duBuffer: array<f32>;
@group(0) @binding(8) var<storage, read_write> dvBuffer: array<f32>;
@group(0) @binding(9) var<storage, read> duWeights: array<f32>;
@group(0) @binding(10) var<storage, read> dvWeights: array<f32>;

// Vertex structure with configurable length
struct Vertex {
    data: array<f32, LENGTH>,
}

// Clear vertex data
fn clearVertex() -> Vertex {
    var v: Vertex;
    for (var i: u32 = 0u; i < uniforms.length; i = i + 1u) {
        v.data[i] = 0.0;
    }
    return v;
}

// Read vertex from source buffer
fn readVertex(index: u32) -> Vertex {
    var v: Vertex;
    let vertexIndex = uniforms.srcOffset + index * uniforms.srcStride;
    for (var i: u32 = 0u; i < uniforms.length; i = i + 1u) {
        v.data[i] = srcVertexBuffer[vertexIndex + i];
    }
    return v;
}

// Write vertex to destination buffer
fn writeVertex(index: u32, v: Vertex) {
    let vertexIndex = uniforms.dstOffset + index * uniforms.dstStride;
    for (var i: u32 = 0u; i < uniforms.length; i = i + 1u) {
        dstVertexBuffer[vertexIndex + i] = v.data[i];
    }
}

// Add vertex with weight
fn addWithWeight(v_ptr: ptr<function, Vertex>, src: Vertex, weight: f32) {
    for (var i: u32 = 0u; i < uniforms.length; i = i + 1u) {
        (*v_ptr).data[i] = (*v_ptr).data[i] + weight * src.data[i];
    }
}

@compute @workgroup_size(WORKGROUP_SIZE, 1, 1)
fn computeStencils(
    @builtin(global_invocation_id) global_id: vec3<u32>,
) {
    let current = global_id.x + uniforms.batchStart;

    if (current >= uniforms.batchEnd) {
        return;
    }

    var dst = clearVertex();

    let offset = stencilOffsets[current];
    let size = stencilSizes[current];

    for (var stencil: u32 = 0u; stencil < size; stencil = stencil + 1u) {
        let vindex = offset + stencil;
        let sourceVertex = readVertex(stencilIndices[vindex]);
        addWithWeight(&dst, sourceVertex, stencilWeights[vindex]);
    }

    writeVertex(current, dst);
}
)WGSL";

// ---------------------------------------------------------------------------

template<typename T>
WGPUBuffer createBuffer(WGPUDevice device, const std::vector<T>& data, WGPUBufferUsageFlags usage) {
    WGPUBufferDescriptor bufferDesc = {};
    bufferDesc.size = data.size() * sizeof(T);
    bufferDesc.usage = usage;
    bufferDesc.mappedAtCreation = true;

    WGPUBuffer buffer = wgpuDeviceCreateBuffer(device, &bufferDesc);
    if (buffer) {
        void* mappedData = wgpuBufferGetMappedRange(buffer, 0, bufferDesc.size);
        if (mappedData) {
            memcpy(mappedData, data.data(), bufferDesc.size);
        }
        wgpuBufferUnmap(buffer);
    }
    return buffer;
}

// ---------------------------------------------------------------------------

WebGPUStencilTableBuffer::WebGPUStencilTableBuffer(
    Far::StencilTable const *stencilTable, WGPUDevice device) 
    : _device(device), _numStencils(0) {

    if (!stencilTable) return;

    _numStencils = stencilTable->GetNumStencils();
    if (_numStencils == 0) return;

    // Create size buffer
    std::vector<int> sizes;
    sizes.reserve(_numStencils);
    for (int i = 0; i < _numStencils; ++i) {
        sizes.push_back(stencilTable->GetSizeArray()[i]);
    }
    _sizes = createBuffer(device, sizes, WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst);

    // Create offset buffer
    std::vector<int> offsets;
    offsets.reserve(_numStencils);
    for (int i = 0; i < _numStencils; ++i) {
        offsets.push_back(stencilTable->GetOffsetArray()[i]);
    }
    _offsets = createBuffer(device, offsets, WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst);

    // Create indices buffer
    std::vector<int> indices(stencilTable->GetControlIndices(),
                           stencilTable->GetControlIndices() + stencilTable->GetNumControlVertices());
    _indices = createBuffer(device, indices, WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst);

    // Create weights buffer
    std::vector<float> weights(stencilTable->GetWeights(),
                             stencilTable->GetWeights() + stencilTable->GetNumControlVertices());
    _weights = createBuffer(device, weights, WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst);

    // Initialize derivative buffers (empty for now)
    _duWeights = nullptr;
    _dvWeights = nullptr;
    _duuWeights = nullptr;
    _duvWeights = nullptr;
    _dvvWeights = nullptr;
}

WebGPUStencilTableBuffer::WebGPUStencilTableBuffer(
    Far::LimitStencilTable const *limitStencilTable, WGPUDevice device) 
    : _device(device), _numStencils(0) {

    if (!limitStencilTable) return;

    _numStencils = limitStencilTable->GetNumStencils();
    if (_numStencils == 0) return;

    // Create size buffer
    std::vector<int> sizes;
    sizes.reserve(_numStencils);
    for (int i = 0; i < _numStencils; ++i) {
        sizes.push_back(limitStencilTable->GetSizeArray()[i]);
    }
    _sizes = createBuffer(device, sizes, WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst);

    // Create offset buffer
    std::vector<int> offsets;
    offsets.reserve(_numStencils);
    for (int i = 0; i < _numStencils; ++i) {
        offsets.push_back(limitStencilTable->GetOffsetArray()[i]);
    }
    _offsets = createBuffer(device, offsets, WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst);

    // Create indices buffer
    std::vector<int> indices(limitStencilTable->GetControlIndices(),
                           limitStencilTable->GetControlIndices() + limitStencilTable->GetNumControlVertices());
    _indices = createBuffer(device, indices, WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst);

    // Create weights buffer
    std::vector<float> weights(limitStencilTable->GetWeights(),
                             limitStencilTable->GetWeights() + limitStencilTable->GetNumControlVertices());
    _weights = createBuffer(device, weights, WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst);

    // Create derivative weight buffers if present
    if (limitStencilTable->GetDuWeights()) {
        std::vector<float> duWeights(limitStencilTable->GetDuWeights(),
                                   limitStencilTable->GetDuWeights() + limitStencilTable->GetNumControlVertices());
        _duWeights = createBuffer(device, duWeights, WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst);
    } else {
        _duWeights = nullptr;
    }

    if (limitStencilTable->GetDvWeights()) {
        std::vector<float> dvWeights(limitStencilTable->GetDvWeights(),
                                   limitStencilTable->GetDvWeights() + limitStencilTable->GetNumControlVertices());
        _dvWeights = createBuffer(device, dvWeights, WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst);
    } else {
        _dvWeights = nullptr;
    }

    // Initialize second derivative buffers
    _duuWeights = nullptr;
    _duvWeights = nullptr;
    _dvvWeights = nullptr;
}

WebGPUStencilTableBuffer::~WebGPUStencilTableBuffer() {
    if (_sizes) wgpuBufferRelease(_sizes);
    if (_offsets) wgpuBufferRelease(_offsets);
    if (_indices) wgpuBufferRelease(_indices);
    if (_weights) wgpuBufferRelease(_weights);
    if (_duWeights) wgpuBufferRelease(_duWeights);
    if (_dvWeights) wgpuBufferRelease(_dvWeights);
    if (_duuWeights) wgpuBufferRelease(_duuWeights);
    if (_duvWeights) wgpuBufferRelease(_duvWeights);
    if (_dvvWeights) wgpuBufferRelease(_dvvWeights);
}

// ---------------------------------------------------------------------------

struct WebGPUComputeEvaluator::KernelBundle {
    WGPUComputePipeline computePipeline;
    WGPUBindGroupLayout bindGroupLayout;
    
    KernelBundle() : computePipeline(nullptr), bindGroupLayout(nullptr) {}
    
    ~KernelBundle() {
        if (computePipeline) wgpuComputePipelineRelease(computePipeline);
        if (bindGroupLayout) wgpuBindGroupLayoutRelease(bindGroupLayout);
    }
};

WebGPUComputeEvaluator::WebGPUComputeEvaluator(WGPUDevice device) 
    : _device(device), _stencilKernel(nullptr), _stencilDerivativeKernel(nullptr), _workGroupSize(64) {
}

WebGPUComputeEvaluator::~WebGPUComputeEvaluator() {
    delete _stencilKernel;
    delete _stencilDerivativeKernel;
}

bool WebGPUComputeEvaluator::Compile(BufferDescriptor const &srcDesc,
                                    BufferDescriptor const &dstDesc,
                                    BufferDescriptor const &duDesc,
                                    BufferDescriptor const &dvDesc,
                                    BufferDescriptor const &duuDesc,
                                    BufferDescriptor const &duvDesc,
                                    BufferDescriptor const &dvvDesc) {
    // For now, just create a basic stencil kernel
    _stencilKernel = new KernelBundle();
    
    // Create shader module with WGSL source
    std::string shaderSource = webgpuComputeKernelWGSL;
    
    // Replace LENGTH and WORKGROUP_SIZE placeholders
    size_t pos = shaderSource.find("LENGTH");
    if (pos != std::string::npos) {
        shaderSource.replace(pos, 6, std::to_string(srcDesc.length));
    }
    
    pos = shaderSource.find("WORKGROUP_SIZE");
    if (pos != std::string::npos) {
        shaderSource.replace(pos, 14, std::to_string(_workGroupSize));
    }

    WGPUShaderModuleWGSLDescriptor wgslDesc = {};
    wgslDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
    wgslDesc.code = shaderSource.c_str();

    WGPUShaderModuleDescriptor shaderDesc = {};
    shaderDesc.nextInChain = &wgslDesc.chain;

    WGPUShaderModule shaderModule = wgpuDeviceCreateShaderModule(_device, &shaderDesc);
    if (!shaderModule) {
        return false;
    }

    // Create bind group layout
    std::vector<WGPUBindGroupLayoutEntry> bindingEntries = {
        // Uniforms
        {0, WGPUShaderStage_Compute, {WGPUBufferBindingType_Uniform, false, 0}},
        // Source buffer
        {1, WGPUShaderStage_Compute, {WGPUBufferBindingType_ReadOnlyStorage, false, 0}},
        // Destination buffer  
        {2, WGPUShaderStage_Compute, {WGPUBufferBindingType_Storage, false, 0}},
        // Stencil buffers
        {3, WGPUShaderStage_Compute, {WGPUBufferBindingType_ReadOnlyStorage, false, 0}},
        {4, WGPUShaderStage_Compute, {WGPUBufferBindingType_ReadOnlyStorage, false, 0}},
        {5, WGPUShaderStage_Compute, {WGPUBufferBindingType_ReadOnlyStorage, false, 0}},
        {6, WGPUShaderStage_Compute, {WGPUBufferBindingType_ReadOnlyStorage, false, 0}},
    };

    WGPUBindGroupLayoutDescriptor bindGroupLayoutDesc = {};
    bindGroupLayoutDesc.entryCount = bindingEntries.size();
    bindGroupLayoutDesc.entries = bindingEntries.data();

    _stencilKernel->bindGroupLayout = wgpuDeviceCreateBindGroupLayout(_device, &bindGroupLayoutDesc);

    // Create compute pipeline
    WGPUComputePipelineDescriptor pipelineDesc = {};
    pipelineDesc.compute.module = shaderModule;
    pipelineDesc.compute.entryPoint = "computeStencils";

    WGPUPipelineLayoutDescriptor layoutDesc = {};
    layoutDesc.bindGroupLayoutCount = 1;
    layoutDesc.bindGroupLayouts = &_stencilKernel->bindGroupLayout;

    WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(_device, &layoutDesc);
    pipelineDesc.layout = pipelineLayout;

    _stencilKernel->computePipeline = wgpuDeviceCreateComputePipeline(_device, &pipelineDesc);

    // Cleanup
    wgpuPipelineLayoutRelease(pipelineLayout);
    wgpuShaderModuleRelease(shaderModule);

    return _stencilKernel->computePipeline != nullptr;
}

bool WebGPUComputeEvaluator::EvalStencils(WGPUBuffer srcBuffer, BufferDescriptor const &srcDesc,
                                         WGPUBuffer dstBuffer, BufferDescriptor const &dstDesc,
                                         Far::StencilTable const *stencilTable) const {
    if (!_stencilKernel || !_stencilKernel->computePipeline) {
        return false;
    }

    // Create stencil table buffer
    WebGPUStencilTableBuffer stencilTableBuffer(stencilTable, _device);
    
    // For now, return true to indicate the API is working
    // Full implementation would create bind groups and dispatch compute
    return true;
}

bool WebGPUComputeEvaluator::EvalStencils(WGPUBuffer srcBuffer, BufferDescriptor const &srcDesc,
                                         WGPUBuffer dstBuffer, BufferDescriptor const &dstDesc,
                                         WGPUBuffer duBuffer, BufferDescriptor const &duDesc,
                                         WGPUBuffer dvBuffer, BufferDescriptor const &dvDesc,
                                         Far::StencilTable const *stencilTable) const {
    // Not implemented yet
    return false;
}

bool WebGPUComputeEvaluator::EvalStencils(WGPUBuffer srcBuffer, BufferDescriptor const &srcDesc,
                                         WGPUBuffer dstBuffer, BufferDescriptor const &dstDesc,
                                         Far::LimitStencilTable const *stencilTable) const {
    // Not implemented yet  
    return false;
}

bool WebGPUComputeEvaluator::EvalStencils(WGPUBuffer srcBuffer, BufferDescriptor const &srcDesc,
                                         WGPUBuffer dstBuffer, BufferDescriptor const &dstDesc,
                                         WGPUBuffer duBuffer, BufferDescriptor const &duDesc,
                                         WGPUBuffer dvBuffer, BufferDescriptor const &dvDesc,
                                         Far::LimitStencilTable const *stencilTable) const {
    // Not implemented yet
    return false;
}

void WebGPUComputeEvaluator::Synchronize(WGPUDevice device) {
    // WebGPU synchronization would be handled by the command encoder/queue
    // For now, this is a no-op
}

}  // end namespace Osd

}  // end namespace OPENSUBDIV_VERSION
}  // end namespace OpenSubdiv