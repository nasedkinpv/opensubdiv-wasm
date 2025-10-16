//
//   Copyright 2015 Pixar
//
//   Licensed under the terms set forth in the LICENSE.txt file available at
//   https://opensubdiv.org/license.
//

#ifndef OPENSUBDIV3_OSD_GL_LEGACY_GREGORY_PATCH_TABLE_H
#define OPENSUBDIV3_OSD_GL_LEGACY_GREGORY_PATCH_TABLE_H

#include "../version.h"

#include "../far/patchTable.h"
#include "../osd/nonCopyable.h"
#include "../osd/opengl.h"

namespace OpenSubdiv {
namespace OPENSUBDIV_VERSION {

namespace Osd {

class GLLegacyGregoryPatchTable
    : private NonCopyable<GLLegacyGregoryPatchTable> {
public:
    ~GLLegacyGregoryPatchTable();

    static GLLegacyGregoryPatchTable *Create(Far::PatchTable const *patchTable);

    void UpdateVertexBuffer(GLuint vbo);

    GLuint GetVertexTextureBuffer() const {
        return _vertexTextureBuffer;
    }

    GLuint GetVertexValenceTextureBuffer() const {
        return _vertexValenceTextureBuffer;
    }

    GLuint GetQuadOffsetsTextureBuffer() const {
        return _quadOffsetsTextureBuffer;
    }

    GLuint GetQuadOffsetsBase(Far::PatchDescriptor::Type type) {
        if (type == Far::PatchDescriptor::GREGORY_BOUNDARY) {
            return _quadOffsetsBase[1];
        }
        return _quadOffsetsBase[0];
    }

protected:
    GLLegacyGregoryPatchTable();

private:
    GLuint _vertexTextureBuffer;
    GLuint _vertexValenceTextureBuffer;
    GLuint _quadOffsetsTextureBuffer;
    GLuint _quadOffsetsBase[2];       // gregory, boundaryGregory
};



}  // end namespace Osd

}  // end namespace OPENSUBDIV_VERSION
using namespace OPENSUBDIV_VERSION;

}  // end namespace OpenSubdiv

#endif  // OPENSUBDIV3_OSD_GL_LEGACY_GREGORY_PATCH_TABLE_H
