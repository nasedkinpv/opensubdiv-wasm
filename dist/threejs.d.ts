/**
 * OpenSubdiv WASM - Three.js Integration
 * Copyright 2024-2025 nasedkinpv
 *
 * SPDX-License-Identifier: TOST-1.0
 * See LICENSE.txt for details
 */
import type { BufferGeometry } from 'three';
export declare const BOUNDARY_NONE = 0;
export declare const BOUNDARY_EDGE_ONLY = 1;
export declare const BOUNDARY_EDGE_AND_CORNER = 2;
export type BoundaryInterpolation = typeof BOUNDARY_NONE | typeof BOUNDARY_EDGE_ONLY | typeof BOUNDARY_EDGE_AND_CORNER;
interface WasmSubdivisionMesh {
    initFromQuads(positions: Float32Array, indices: Int32Array, numQuads: number, subdivisionLevel: number, boundaryInterpolation: number): boolean;
    initFromPolygons(positions: Float32Array, faceIndices: Int32Array, faceSizes: Int32Array, subdivisionLevel: number, boundaryInterpolation: number): boolean;
    updatePositions(positions: Float32Array): void;
    getPositions(): Float32Array;
    getNormals(): Float32Array;
    getIndices(): Uint32Array;
    getVertexCount(): number;
    getTriangleCount(): number;
    getInputVertexCount(): number;
    setUVs(uvs: Float32Array, uvIndices: Int32Array): boolean;
    getUVs(): Float32Array;
    hasUVData(): boolean;
    delete(): void;
}
export interface OpenSubdivModule {
    SubdivisionMesh: new () => WasmSubdivisionMesh;
    BOUNDARY_NONE: number;
    BOUNDARY_EDGE_ONLY: number;
    BOUNDARY_EDGE_AND_CORNER: number;
}
export type OpenSubdivFactory = () => Promise<OpenSubdivModule>;
export declare function initOpenSubdiv(wasmFactory?: OpenSubdivFactory): Promise<OpenSubdivModule>;
export declare function resetModule(): void;
export interface SubdivisionOptions {
    level?: number;
    boundaryInterpolation?: BoundaryInterpolation;
}
export declare class SubdivisionSurface {
    private mesh;
    private readonly level;
    private readonly boundaryInterpolation;
    constructor(options?: SubdivisionOptions);
    init(wasmFactory?: OpenSubdivFactory): Promise<this>;
    get isInitialized(): boolean;
    private ensureInitialized;
    initFromQuadGeometry(geometry: BufferGeometry): boolean;
    initFromQuads(positions: Float32Array, quadIndices: Int32Array | Uint32Array): boolean;
    initFromPolygons(positions: Float32Array, faceIndices: Int32Array | Uint32Array, faceSizes: Int32Array | Uint32Array): boolean;
    updatePositions(positions: Float32Array): void;
    getPositions(): Float32Array;
    getNormals(): Float32Array;
    getIndices(): Uint32Array;
    getVertexCount(): number;
    getTriangleCount(): number;
    getInputVertexCount(): number;
    setUVs(uvs: Float32Array, uvIndices: Int32Array | Uint32Array): boolean;
    getUVs(): Float32Array;
    hasUVData(): boolean;
    toBufferGeometry(THREE: typeof import('three'), existingGeometry?: BufferGeometry): BufferGeometry;
    updateBufferGeometry(geometry: BufferGeometry): void;
    dispose(): void;
}
export declare function subdivideQuadGeometry(THREE: typeof import('three'), geometry: BufferGeometry, options?: SubdivisionOptions): Promise<BufferGeometry>;
export {};
