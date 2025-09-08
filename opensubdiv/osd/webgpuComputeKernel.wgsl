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

//------------------------------------------------------------------------------
// WebGPU compute shader for OpenSubdiv stencil evaluation
//------------------------------------------------------------------------------

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

// Second derivative buffers (optional)
@group(0) @binding(11) var<storage, read_write> duuBuffer: array<f32>;
@group(0) @binding(12) var<storage, read_write> duvBuffer: array<f32>;
@group(0) @binding(13) var<storage, read_write> dvvBuffer: array<f32>;
@group(0) @binding(14) var<storage, read> duuWeights: array<f32>;
@group(0) @binding(15) var<storage, read> duvWeights: array<f32>;
@group(0) @binding(16) var<storage, read> dvvWeights: array<f32>;

// Vertex structure
struct Vertex {
    data: array<f32, 16>, // Maximum vertex data length
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

// Write first derivatives
fn writeDu(index: u32, du: Vertex) {
    let duIndex = index * uniforms.length;  // Simplified addressing
    for (var i: u32 = 0u; i < uniforms.length; i = i + 1u) {
        duBuffer[duIndex + i] = du.data[i];
    }
}

fn writeDv(index: u32, dv: Vertex) {
    let dvIndex = index * uniforms.length;  // Simplified addressing  
    for (var i: u32 = 0u; i < uniforms.length; i = i + 1u) {
        dvBuffer[dvIndex + i] = dv.data[i];
    }
}

// Write second derivatives
fn writeDuu(index: u32, duu: Vertex) {
    let duuIndex = index * uniforms.length;
    for (var i: u32 = 0u; i < uniforms.length; i = i + 1u) {
        duuBuffer[duuIndex + i] = duu.data[i];
    }
}

fn writeDuv(index: u32, duv: Vertex) {
    let duvIndex = index * uniforms.length;
    for (var i: u32 = 0u; i < uniforms.length; i = i + 1u) {
        duvBuffer[duvIndex + i] = duv.data[i];
    }
}

fn writeDvv(index: u32, dvv: Vertex) {
    let dvvIndex = index * uniforms.length;
    for (var i: u32 = 0u; i < uniforms.length; i = i + 1u) {
        dvvBuffer[dvvIndex + i] = dvv.data[i];
    }
}

//------------------------------------------------------------------------------
// Main compute function for stencil evaluation
//------------------------------------------------------------------------------

@compute @workgroup_size(64, 1, 1)
fn computeStencils(
    @builtin(global_invocation_id) global_id: vec3<u32>,
) {
    let current = global_id.x + uniforms.batchStart;

    if (current >= uniforms.batchEnd) {
        return;
    }

    // Initialize destination vertex
    var dst = clearVertex();

    // Get stencil info for this vertex
    let offset = stencilOffsets[current];
    let size = stencilSizes[current];

    // Apply stencil weights
    for (var stencil: u32 = 0u; stencil < size; stencil = stencil + 1u) {
        let vindex = offset + stencil;
        let sourceVertex = readVertex(stencilIndices[vindex]);
        addWithWeight(&dst, sourceVertex, stencilWeights[vindex]);
    }

    // Write result
    writeVertex(current, dst);

    // First derivatives (if enabled)
    #ifdef DERIVATIVES
    var du = clearVertex();
    var dv = clearVertex();
    
    for (var i: u32 = 0u; i < size; i = i + 1u) {
        let sourceVertex = readVertex(stencilIndices[offset + i]);
        addWithWeight(&du, sourceVertex, duWeights[offset + i]);
        addWithWeight(&dv, sourceVertex, dvWeights[offset + i]);
    }
    
    writeDu(current, du);
    writeDv(current, dv);
    #endif

    // Second derivatives (if enabled)
    #ifdef SECOND_DERIVATIVES
    var duu = clearVertex();
    var duv = clearVertex(); 
    var dvv = clearVertex();
    
    for (var i: u32 = 0u; i < size; i = i + 1u) {
        let sourceVertex = readVertex(stencilIndices[offset + i]);
        addWithWeight(&duu, sourceVertex, duuWeights[offset + i]);
        addWithWeight(&duv, sourceVertex, duvWeights[offset + i]);
        addWithWeight(&dvv, sourceVertex, dvvWeights[offset + i]);
    }
    
    writeDuu(current, duu);
    writeDuv(current, duv); 
    writeDvv(current, dvv);
    #endif
}