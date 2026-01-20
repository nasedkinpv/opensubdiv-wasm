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
        const positions = new Float32Array(posAttr.array);
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
    initFromPolygons(positions, faceIndices, faceSizes) {
        const mesh = this.ensureInitialized();
        const indices = faceIndices instanceof Int32Array
            ? faceIndices
            : new Int32Array(faceIndices);
        const sizes = faceSizes instanceof Int32Array ? faceSizes : new Int32Array(faceSizes);
        return mesh.initFromPolygons(positions, indices, sizes, this.level, this.boundaryInterpolation);
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
    setUVs(uvs, uvIndices) {
        const mesh = this.ensureInitialized();
        const indices = uvIndices instanceof Int32Array ? uvIndices : new Int32Array(uvIndices);
        return mesh.setUVs(uvs, indices);
    }
    getUVs() {
        return new Float32Array(this.ensureInitialized().getUVs());
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
            const uvs = this.getUVs();
            geometry.setAttribute('uv', new THREE.BufferAttribute(uvs, 2));
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
