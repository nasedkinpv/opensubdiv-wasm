/**
 * OpenSubdiv WASM - Three.js Integration
 * Copyright 2024-2025 nasedkinpv
 *
 * SPDX-License-Identifier: TOST-1.0
 * See LICENSE.txt for details
 */
export const BOUNDARY_NONE = 0;
export const BOUNDARY_EDGE_ONLY = 1;
export const BOUNDARY_EDGE_AND_CORNER = 2;
let modulePromise = null;
let cachedModule = null;
export async function initOpenSubdiv(wasmFactory) {
    if (cachedModule)
        return cachedModule;
    if (!modulePromise) {
        if (wasmFactory) {
            modulePromise = wasmFactory();
        }
        else {
            const factory = (await import('./opensubdiv.mjs'))
                .default;
            modulePromise = factory();
        }
    }
    cachedModule = await modulePromise;
    return cachedModule;
}
export function resetModule() {
    cachedModule = null;
    modulePromise = null;
}
function extractPositions(attribute) {
    const isInterleaved = 'isInterleavedBufferAttribute' in attribute &&
        attribute.isInterleavedBufferAttribute;
    if (attribute.itemSize < 3) {
        throw new Error(`Position attribute itemSize (${attribute.itemSize}) must be at least 3`);
    }
    if (!isInterleaved &&
        attribute.itemSize === 3 &&
        attribute.array instanceof Float32Array) {
        return new Float32Array(attribute.array);
    }
    const positions = new Float32Array(attribute.count * 3);
    for (let i = 0; i < attribute.count; i++) {
        positions[i * 3 + 0] = attribute.getX(i);
        positions[i * 3 + 1] = attribute.getY(i);
        positions[i * 3 + 2] = attribute.getZ(i);
    }
    return positions;
}
export class SubdivisionSurface {
    constructor(options = {}) {
        this.mesh = null;
        this.level = options.level ?? 2;
        this.boundaryInterpolation =
            options.boundaryInterpolation ?? BOUNDARY_EDGE_ONLY;
    }
    async init(wasmFactory) {
        const module = await initOpenSubdiv(wasmFactory);
        this.mesh = new module.SubdivisionMesh();
        return this;
    }
    get isInitialized() {
        return this.mesh !== null;
    }
    ensureInitialized() {
        if (!this.mesh) {
            throw new Error('SubdivisionSurface not initialized. Call init() first.');
        }
        return this.mesh;
    }
    initFromQuadGeometry(geometry) {
        const mesh = this.ensureInitialized();
        const posAttr = geometry.getAttribute('position');
        if (!posAttr) {
            throw new Error('Geometry must have position attribute');
        }
        const indexAttr = geometry.getIndex();
        if (!indexAttr) {
            throw new Error('Geometry must be indexed');
        }
        const positions = extractPositions(posAttr);
        const indices = new Int32Array(indexAttr.array);
        if (indices.length % 4 !== 0) {
            throw new Error(`Indices length (${indices.length}) must be a multiple of 4 for quad geometry`);
        }
        const numQuads = indices.length / 4;
        return mesh.initFromQuads(positions, indices, numQuads, this.level, this.boundaryInterpolation);
    }
    initFromQuads(positions, quadIndices) {
        const mesh = this.ensureInitialized();
        const indices = quadIndices instanceof Int32Array
            ? quadIndices
            : new Int32Array(quadIndices);
        if (indices.length % 4 !== 0) {
            throw new Error(`Indices length (${indices.length}) must be a multiple of 4 for quad geometry`);
        }
        const numQuads = indices.length / 4;
        return mesh.initFromQuads(positions, indices, numQuads, this.level, this.boundaryInterpolation);
    }
    initFromQuadsWithUVs(positions, quadIndices, uvs, uvIndices) {
        const mesh = this.ensureInitialized();
        const indices = quadIndices instanceof Int32Array
            ? quadIndices
            : new Int32Array(quadIndices);
        const uvIdxs = uvIndices instanceof Int32Array ? uvIndices : new Int32Array(uvIndices);
        if (indices.length % 4 !== 0) {
            throw new Error(`Indices length (${indices.length}) must be a multiple of 4 for quad geometry`);
        }
        if (uvIdxs.length !== indices.length) {
            throw new Error(`UV indices length (${uvIdxs.length}) must match quad indices length (${indices.length})`);
        }
        const numQuads = indices.length / 4;
        const numUVs = uvs.length / 2;
        return mesh.initFromQuadsWithUVs(positions, indices, numQuads, this.level, this.boundaryInterpolation, uvs, uvIdxs, numUVs);
    }
    initFromPolygons(positions, faceIndices, faceSizes) {
        const mesh = this.ensureInitialized();
        const indices = faceIndices instanceof Int32Array
            ? faceIndices
            : new Int32Array(faceIndices);
        const sizes = faceSizes instanceof Int32Array ? faceSizes : new Int32Array(faceSizes);
        return mesh.initFromPolygons(positions, indices, sizes, this.level, this.boundaryInterpolation);
    }
    initFromPolygonsWithUVs(positions, faceIndices, faceSizes, uvs, uvIndices) {
        const mesh = this.ensureInitialized();
        const indices = faceIndices instanceof Int32Array
            ? faceIndices
            : new Int32Array(faceIndices);
        const sizes = faceSizes instanceof Int32Array ? faceSizes : new Int32Array(faceSizes);
        const uvIdxs = uvIndices instanceof Int32Array ? uvIndices : new Int32Array(uvIndices);
        if (uvIdxs.length !== indices.length) {
            throw new Error(`UV indices length (${uvIdxs.length}) must match face indices length (${indices.length})`);
        }
        const numUVs = uvs.length / 2;
        return mesh.initFromPolygonsWithUVs(positions, indices, sizes, this.level, this.boundaryInterpolation, uvs, uvIdxs, numUVs);
    }
    updatePositions(positions) {
        this.ensureInitialized().updatePositions(positions);
    }
    getPositions() {
        return new Float32Array(this.ensureInitialized().getPositions());
    }
    getNormals() {
        return new Float32Array(this.ensureInitialized().getNormals());
    }
    getIndices() {
        return new Uint32Array(this.ensureInitialized().getIndices());
    }
    getVertexCount() {
        return this.ensureInitialized().getVertexCount();
    }
    getTriangleCount() {
        return this.ensureInitialized().getTriangleCount();
    }
    getInputVertexCount() {
        return this.ensureInitialized().getInputVertexCount();
    }
    getUVs() {
        return new Float32Array(this.ensureInitialized().getUVs());
    }
    getUVIndices() {
        return new Uint32Array(this.ensureInitialized().getUVIndices());
    }
    getUVCount() {
        return this.ensureInitialized().getUVCount();
    }
    hasUVData() {
        return this.ensureInitialized().hasUVData();
    }
    toBufferGeometry(THREE, existingGeometry) {
        const mesh = this.ensureInitialized();
        const geometry = existingGeometry ?? new THREE.BufferGeometry();
        const positions = this.getPositions();
        const normals = this.getNormals();
        const indices = this.getIndices();
        geometry.setAttribute('position', new THREE.BufferAttribute(positions, 3));
        geometry.setAttribute('normal', new THREE.BufferAttribute(normals, 3));
        geometry.setIndex(new THREE.BufferAttribute(indices, 1));
        if (mesh.hasUVData()) {
            const uvData = mesh.getUVs();
            const uvIndices = mesh.getUVIndices();
            const numVerts = positions.length / 3;
            const expandedUVs = new Float32Array(numVerts * 2);
            const vertexUVAssigned = new Int32Array(numVerts).fill(-1);
            let hasSeams = false;
            for (let i = 0; i < indices.length; i++) {
                const vertexIdx = indices[i];
                const uvIdx = uvIndices[i];
                if (vertexUVAssigned[vertexIdx] === -1) {
                    expandedUVs[vertexIdx * 2 + 0] = uvData[uvIdx * 2 + 0];
                    expandedUVs[vertexIdx * 2 + 1] = uvData[uvIdx * 2 + 1];
                    vertexUVAssigned[vertexIdx] = uvIdx;
                }
                else if (vertexUVAssigned[vertexIdx] !== uvIdx) {
                    hasSeams = true;
                }
            }
            if (hasSeams) {
                console.warn('OpenSubdiv: UV seams detected. Indexed geometry cannot represent ' +
                    'face-varying UVs perfectly. Call geometry.toNonIndexed() for correct seams.');
            }
            geometry.setAttribute('uv', new THREE.BufferAttribute(expandedUVs, 2));
        }
        return geometry;
    }
    updateBufferGeometry(geometry) {
        const mesh = this.ensureInitialized();
        const positions = mesh.getPositions();
        const normals = mesh.getNormals();
        const posAttr = geometry.getAttribute('position');
        const normAttr = geometry.getAttribute('normal');
        if (posAttr && posAttr.array.length === positions.length) {
            posAttr.array.set(positions);
            posAttr.needsUpdate = true;
        }
        if (normAttr && normAttr.array.length === normals.length) {
            normAttr.array.set(normals);
            normAttr.needsUpdate = true;
        }
    }
    dispose() {
        if (this.mesh) {
            this.mesh.delete();
            this.mesh = null;
        }
    }
}
export async function subdivideQuadGeometry(THREE, geometry, options = {}) {
    const surface = new SubdivisionSurface(options);
    await surface.init();
    try {
        surface.initFromQuadGeometry(geometry);
        return surface.toBufferGeometry(THREE);
    }
    finally {
        surface.dispose();
    }
}
