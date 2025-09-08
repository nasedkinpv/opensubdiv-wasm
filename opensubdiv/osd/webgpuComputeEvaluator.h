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

#ifndef OPENSUBDIV3_OSD_WEBGPU_COMPUTE_EVALUATOR_H
#define OPENSUBDIV3_OSD_WEBGPU_COMPUTE_EVALUATOR_H

#include "../version.h"
#include "../osd/types.h"
#include "../osd/bufferDescriptor.h"

#include <emscripten/html5_webgpu.h>

namespace OpenSubdiv {
namespace OPENSUBDIV_VERSION {

namespace Far {
    class StencilTable;
    class LimitStencilTable;
}

namespace Osd {

/// \brief WebGPU stencil table (Storage buffer)
///
/// This class is a WebGPU buffer representation of Far::StencilTable.
///
/// WebGPUComputeKernel consumes this table to apply stencils
///
class WebGPUStencilTableBuffer {
public:
    static WebGPUStencilTableBuffer *Create(Far::StencilTable const *stencilTable,
                                          WGPUDevice device) {
        return new WebGPUStencilTableBuffer(stencilTable, device);
    }
    static WebGPUStencilTableBuffer *Create(
        Far::LimitStencilTable const *limitStencilTable,
        WGPUDevice device) {
        return new WebGPUStencilTableBuffer(limitStencilTable, device);
    }

    explicit WebGPUStencilTableBuffer(Far::StencilTable const *stencilTable, WGPUDevice device);
    explicit WebGPUStencilTableBuffer(Far::LimitStencilTable const *limitStencilTable, WGPUDevice device);
    ~WebGPUStencilTableBuffer();

    // interfaces needed for WebGPUComputeKernel
    WGPUBuffer GetSizesBuffer() const { return _sizes; }
    WGPUBuffer GetOffsetsBuffer() const { return _offsets; }
    WGPUBuffer GetIndicesBuffer() const { return _indices; }
    WGPUBuffer GetWeightsBuffer() const { return _weights; }
    WGPUBuffer GetDuWeightsBuffer() const { return _duWeights; }
    WGPUBuffer GetDvWeightsBuffer() const { return _dvWeights; }
    WGPUBuffer GetDuuWeightsBuffer() const { return _duuWeights; }
    WGPUBuffer GetDuvWeightsBuffer() const { return _duvWeights; }
    WGPUBuffer GetDvvWeightsBuffer() const { return _dvvWeights; }
    int GetNumStencils() const { return _numStencils; }

private:
    WGPUDevice _device;
    WGPUBuffer _sizes;
    WGPUBuffer _offsets;
    WGPUBuffer _indices;
    WGPUBuffer _weights;
    WGPUBuffer _duWeights;
    WGPUBuffer _dvWeights;
    WGPUBuffer _duuWeights;
    WGPUBuffer _duvWeights;
    WGPUBuffer _dvvWeights;
    int _numStencils;
};

// ---------------------------------------------------------------------------

class WebGPUComputeEvaluator {
public:
    typedef bool Instantiatable;
    
    static WebGPUComputeEvaluator * Create(BufferDescriptor const &srcDesc,
                                          BufferDescriptor const &dstDesc,
                                          WGPUDevice device) {
        return Create(srcDesc, dstDesc,
                      BufferDescriptor(),
                      BufferDescriptor(),
                      device);
    }

    static WebGPUComputeEvaluator * Create(BufferDescriptor const &srcDesc,
                                          BufferDescriptor const &dstDesc,
                                          BufferDescriptor const &duDesc,
                                          BufferDescriptor const &dvDesc,
                                          WGPUDevice device) {
        return Create(srcDesc, dstDesc, duDesc, dvDesc,
                      BufferDescriptor(),
                      BufferDescriptor(),
                      BufferDescriptor(),
                      device);
    }

    static WebGPUComputeEvaluator * Create(BufferDescriptor const &srcDesc,
                                          BufferDescriptor const &dstDesc,
                                          BufferDescriptor const &duDesc,
                                          BufferDescriptor const &dvDesc,
                                          BufferDescriptor const &duuDesc,
                                          BufferDescriptor const &duvDesc,
                                          BufferDescriptor const &dvvDesc,
                                          WGPUDevice device) {
        WebGPUComputeEvaluator *instance = new WebGPUComputeEvaluator(device);
        if (instance->Compile(srcDesc, dstDesc, duDesc, dvDesc,
                              duuDesc, duvDesc, dvvDesc))
            return instance;
        delete instance;
        return NULL;
    }

    /// Constructor.
    explicit WebGPUComputeEvaluator(WGPUDevice device);

    /// Destructor.
    ~WebGPUComputeEvaluator();

    /// ----------------------------------------------------------------------
    ///
    ///   Stencil evaluations with StencilTable
    ///
    /// ----------------------------------------------------------------------

    /// \brief Generic static stencil function. This function has a same
    ///        signature as other device kernels have so that it can be called
    ///        transparently from OsdMesh template interface.
    ///
    /// @param srcBuffer      Input primvar buffer.
    ///                       must have BindBuffer() method returning a WebGPU
    ///                       buffer object of source data
    ///
    /// @param srcDesc        vertex buffer descriptor for the input buffer
    ///
    /// @param dstBuffer      Output primvar buffer
    ///                       must have BindBuffer() method returning a WebGPU
    ///                       buffer object of destination data
    ///
    /// @param dstDesc        vertex buffer descriptor for the output buffer
    ///
    /// @param stencilTable   Far::StencilTable or Far::LimitStencilTable
    ///
    /// @param evaluator      cached compiled WebGPUComputeEvaluator
    ///
    template <typename SRC_BUFFER, typename DST_BUFFER, typename STENCIL_TABLE>
    static bool EvalStencils(
        SRC_BUFFER *srcBuffer, BufferDescriptor const &srcDesc,
        DST_BUFFER *dstBuffer, BufferDescriptor const &dstDesc,
        STENCIL_TABLE const *stencilTable,
        WebGPUComputeEvaluator const *evaluator) {

        if (evaluator) {
            return evaluator->EvalStencils(srcBuffer->BindBuffer(), srcDesc,
                                          dstBuffer->BindBuffer(), dstDesc,
                                          stencilTable);
        } else {
            // Create a temporary evaluator
            BufferDescriptor duDesc, dvDesc, duuDesc, duvDesc, dvvDesc;
            WebGPUComputeEvaluator *e = Create(srcDesc, dstDesc, duDesc, dvDesc,
                                              duuDesc, duvDesc, dvvDesc,
                                              evaluator ? evaluator->_device : nullptr);
            if (e == NULL) return false;
            bool r = e->EvalStencils(srcBuffer->BindBuffer(), srcDesc,
                                    dstBuffer->BindBuffer(), dstDesc,
                                    stencilTable);
            delete e;
            return r;
        }
    }

    /// \brief Generic static stencil function with derivative evaluation.
    ///
    template <typename SRC_BUFFER, typename DST_BUFFER, typename STENCIL_TABLE>
    static bool EvalStencils(
        SRC_BUFFER *srcBuffer, BufferDescriptor const &srcDesc,
        DST_BUFFER *dstBuffer, BufferDescriptor const &dstDesc,
        DST_BUFFER *duBuffer,  BufferDescriptor const &duDesc,
        DST_BUFFER *dvBuffer,  BufferDescriptor const &dvDesc,
        STENCIL_TABLE const *stencilTable,
        WebGPUComputeEvaluator const *evaluator) {

        if (evaluator) {
            return evaluator->EvalStencils(srcBuffer->BindBuffer(), srcDesc,
                                          dstBuffer->BindBuffer(), dstDesc,
                                          duBuffer->BindBuffer(), duDesc,
                                          dvBuffer->BindBuffer(), dvDesc,
                                          stencilTable);
        } else {
            BufferDescriptor duuDesc, duvDesc, dvvDesc;
            WebGPUComputeEvaluator *e = Create(srcDesc, dstDesc, duDesc, dvDesc,
                                              duuDesc, duvDesc, dvvDesc,
                                              evaluator ? evaluator->_device : nullptr);
            if (e == NULL) return false;
            bool r = e->EvalStencils(srcBuffer->BindBuffer(), srcDesc,
                                    dstBuffer->BindBuffer(), dstDesc,
                                    duBuffer->BindBuffer(), duDesc,
                                    dvBuffer->BindBuffer(), dvDesc,
                                    stencilTable);
            delete e;
            return r;
        }
    }

    /// Stencil evaluation function.
    bool EvalStencils(WGPUBuffer srcBuffer, BufferDescriptor const &srcDesc,
                      WGPUBuffer dstBuffer, BufferDescriptor const &dstDesc,
                      Far::StencilTable const *stencilTable) const;

    bool EvalStencils(WGPUBuffer srcBuffer, BufferDescriptor const &srcDesc,
                      WGPUBuffer dstBuffer, BufferDescriptor const &dstDesc,
                      WGPUBuffer duBuffer,  BufferDescriptor const &duDesc,
                      WGPUBuffer dvBuffer,  BufferDescriptor const &dvDesc,
                      Far::StencilTable const *stencilTable) const;

    bool EvalStencils(WGPUBuffer srcBuffer, BufferDescriptor const &srcDesc,
                      WGPUBuffer dstBuffer, BufferDescriptor const &dstDesc,
                      Far::LimitStencilTable const *stencilTable) const;

    bool EvalStencils(WGPUBuffer srcBuffer, BufferDescriptor const &srcDesc,
                      WGPUBuffer dstBuffer, BufferDescriptor const &dstDesc,
                      WGPUBuffer duBuffer,  BufferDescriptor const &duDesc,
                      WGPUBuffer dvBuffer,  BufferDescriptor const &dvDesc,
                      Far::LimitStencilTable const *stencilTable) const;

    /// Wait the dispatched kernel finish.
    static void Synchronize(WGPUDevice device);

    bool Compile(BufferDescriptor const &srcDesc,
                 BufferDescriptor const &dstDesc,
                 BufferDescriptor const &duDesc,
                 BufferDescriptor const &dvDesc,
                 BufferDescriptor const &duuDesc,
                 BufferDescriptor const &duvDesc,
                 BufferDescriptor const &dvvDesc);

protected:
    struct KernelBundle;

    /// Dispatches the compute kernel asynchronously.
    /// returns false if the kernel hasn't been compiled yet.
    bool DispatchCompute(KernelBundle const * kernel,
                         WGPUBuffer srcBuffer,
                         WGPUBuffer dstBuffer,
                         int numStencils) const;

    bool DispatchCompute(KernelBundle const * kernel,
                         WGPUBuffer srcBuffer,
                         WGPUBuffer dstBuffer,
                         WGPUBuffer duBuffer,
                         WGPUBuffer dvBuffer,
                         int numStencils) const;

private:
    WGPUDevice _device;
    struct KernelBundle * _stencilKernel;
    struct KernelBundle * _stencilDerivativeKernel;

    int _workGroupSize;
};

}  // end namespace Osd

}  // end namespace OPENSUBDIV_VERSION
}  // end namespace OpenSubdiv

#endif  // OPENSUBDIV3_OSD_WEBGPU_COMPUTE_EVALUATOR_H