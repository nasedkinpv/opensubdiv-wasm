/**
 * OpenSubdiv WASM - Three.js Integration
 * Copyright 2024-2025 nasedkinpv
 * 
 * SPDX-License-Identifier: TOST-1.0
 * See LICENSE.txt for details
 */

import type {
  BufferGeometry,
  BufferAttribute,
  InterleavedBufferAttribute,
} from 'three';

export const BOUNDARY_NONE = 0;
export const BOUNDARY_EDGE_ONLY = 1;
export const BOUNDARY_EDGE_AND_CORNER = 2;

export type BoundaryInterpolation =
  | typeof BOUNDARY_NONE
  | typeof BOUNDARY_EDGE_ONLY
  | typeof BOUNDARY_EDGE_AND_CORNER;

interface WasmSubdivisionMesh {
  initFromQuads(
    positions: Float32Array,
    indices: Int32Array,
    numQuads: number,
    subdivisionLevel: number,
    boundaryInterpolation: number
  ): boolean;
  initFromQuadsWithUVs(
    positions: Float32Array,
    indices: Int32Array,
    numQuads: number,
    subdivisionLevel: number,
    boundaryInterpolation: number,
    uvs: Float32Array,
    uvIndices: Int32Array,
    numUVs: number
  ): boolean;
  initFromPolygons(
    positions: Float32Array,
    faceIndices: Int32Array,
    faceSizes: Int32Array,
    subdivisionLevel: number,
    boundaryInterpolation: number
  ): boolean;
  initFromPolygonsWithUVs(
    positions: Float32Array,
    faceIndices: Int32Array,
    faceSizes: Int32Array,
    subdivisionLevel: number,
    boundaryInterpolation: number,
    uvs: Float32Array,
    uvIndices: Int32Array,
    numUVs: number
  ): boolean;
  updatePositions(positions: Float32Array): void;
  getPositions(): Float32Array;
  getNormals(): Float32Array;
  getIndices(): Uint32Array;
  getVertexCount(): number;
  getTriangleCount(): number;
  getInputVertexCount(): number;
  getUVs(): Float32Array;
  getUVIndices(): Uint32Array;
  getUVCount(): number;
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

let modulePromise: Promise<OpenSubdivModule> | null = null;
let cachedModule: OpenSubdivModule | null = null;

export async function initOpenSubdiv(
  wasmFactory?: OpenSubdivFactory
): Promise<OpenSubdivModule> {
  if (cachedModule) return cachedModule;

  if (!modulePromise) {
    if (wasmFactory) {
      modulePromise = wasmFactory();
    } else {
      const factory = (await import('./opensubdiv.mjs'))
        .default as OpenSubdivFactory;
      modulePromise = factory();
    }
  }

  cachedModule = await modulePromise;
  return cachedModule;
}

export function resetModule(): void {
  cachedModule = null;
  modulePromise = null;
}

export interface SubdivisionOptions {
  level?: number;
  boundaryInterpolation?: BoundaryInterpolation;
}

export class SubdivisionSurface {
  private mesh: WasmSubdivisionMesh | null = null;
  private readonly level: number;
  private readonly boundaryInterpolation: BoundaryInterpolation;

  constructor(options: SubdivisionOptions = {}) {
    this.level = options.level ?? 2;
    this.boundaryInterpolation =
      options.boundaryInterpolation ?? BOUNDARY_EDGE_ONLY;
  }

  async init(wasmFactory?: OpenSubdivFactory): Promise<this> {
    const module = await initOpenSubdiv(wasmFactory);
    this.mesh = new module.SubdivisionMesh();
    return this;
  }

  get isInitialized(): boolean {
    return this.mesh !== null;
  }

  private ensureInitialized(): WasmSubdivisionMesh {
    if (!this.mesh) {
      throw new Error('SubdivisionSurface not initialized. Call init() first.');
    }
    return this.mesh;
  }

  initFromQuadGeometry(geometry: BufferGeometry): boolean {
    const mesh = this.ensureInitialized();

    const posAttr = geometry.getAttribute('position') as
      | BufferAttribute
      | InterleavedBufferAttribute
      | undefined;

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
      throw new Error(
        `Indices length (${indices.length}) must be a multiple of 4 for quad geometry`
      );
    }

    const numQuads = indices.length / 4;
    return mesh.initFromQuads(
      positions,
      indices,
      numQuads,
      this.level,
      this.boundaryInterpolation
    );
  }

  initFromQuads(
    positions: Float32Array,
    quadIndices: Int32Array | Uint32Array
  ): boolean {
    const mesh = this.ensureInitialized();

    const indices =
      quadIndices instanceof Int32Array
        ? quadIndices
        : new Int32Array(quadIndices);

    if (indices.length % 4 !== 0) {
      throw new Error(
        `Indices length (${indices.length}) must be a multiple of 4 for quad geometry`
      );
    }

    const numQuads = indices.length / 4;
    return mesh.initFromQuads(
      positions,
      indices,
      numQuads,
      this.level,
      this.boundaryInterpolation
    );
  }

  initFromQuadsWithUVs(
    positions: Float32Array,
    quadIndices: Int32Array | Uint32Array,
    uvs: Float32Array,
    uvIndices: Int32Array | Uint32Array
  ): boolean {
    const mesh = this.ensureInitialized();

    const indices =
      quadIndices instanceof Int32Array
        ? quadIndices
        : new Int32Array(quadIndices);

    const uvIdxs =
      uvIndices instanceof Int32Array ? uvIndices : new Int32Array(uvIndices);

    if (indices.length % 4 !== 0) {
      throw new Error(
        `Indices length (${indices.length}) must be a multiple of 4 for quad geometry`
      );
    }

    if (uvIdxs.length !== indices.length) {
      throw new Error(
        `UV indices length (${uvIdxs.length}) must match quad indices length (${indices.length})`
      );
    }

    const numQuads = indices.length / 4;
    const numUVs = uvs.length / 2;

    return mesh.initFromQuadsWithUVs(
      positions,
      indices,
      numQuads,
      this.level,
      this.boundaryInterpolation,
      uvs,
      uvIdxs,
      numUVs
    );
  }

  initFromPolygons(
    positions: Float32Array,
    faceIndices: Int32Array | Uint32Array,
    faceSizes: Int32Array | Uint32Array
  ): boolean {
    const mesh = this.ensureInitialized();

    const indices =
      faceIndices instanceof Int32Array
        ? faceIndices
        : new Int32Array(faceIndices);
    const sizes =
      faceSizes instanceof Int32Array ? faceSizes : new Int32Array(faceSizes);

    return mesh.initFromPolygons(
      positions,
      indices,
      sizes,
      this.level,
      this.boundaryInterpolation
    );
  }

  initFromPolygonsWithUVs(
    positions: Float32Array,
    faceIndices: Int32Array | Uint32Array,
    faceSizes: Int32Array | Uint32Array,
    uvs: Float32Array,
    uvIndices: Int32Array | Uint32Array
  ): boolean {
    const mesh = this.ensureInitialized();

    const indices =
      faceIndices instanceof Int32Array
        ? faceIndices
        : new Int32Array(faceIndices);
    const sizes =
      faceSizes instanceof Int32Array ? faceSizes : new Int32Array(faceSizes);
    const uvIdxs =
      uvIndices instanceof Int32Array ? uvIndices : new Int32Array(uvIndices);

    if (uvIdxs.length !== indices.length) {
      throw new Error(
        `UV indices length (${uvIdxs.length}) must match face indices length (${indices.length})`
      );
    }

    const numUVs = uvs.length / 2;

    return mesh.initFromPolygonsWithUVs(
      positions,
      indices,
      sizes,
      this.level,
      this.boundaryInterpolation,
      uvs,
      uvIdxs,
      numUVs
    );
  }

  updatePositions(positions: Float32Array): void {
    this.ensureInitialized().updatePositions(positions);
  }

  getPositions(): Float32Array {
    return new Float32Array(this.ensureInitialized().getPositions());
  }

  getNormals(): Float32Array {
    return new Float32Array(this.ensureInitialized().getNormals());
  }

  getIndices(): Uint32Array {
    return new Uint32Array(this.ensureInitialized().getIndices());
  }

  getVertexCount(): number {
    return this.ensureInitialized().getVertexCount();
  }

  getTriangleCount(): number {
    return this.ensureInitialized().getTriangleCount();
  }

  getInputVertexCount(): number {
    return this.ensureInitialized().getInputVertexCount();
  }

  getUVs(): Float32Array {
    return new Float32Array(this.ensureInitialized().getUVs());
  }

  getUVIndices(): Uint32Array {
    return new Uint32Array(this.ensureInitialized().getUVIndices());
  }

  getUVCount(): number {
    return this.ensureInitialized().getUVCount();
  }

  hasUVData(): boolean {
    return this.ensureInitialized().hasUVData();
  }

  toBufferGeometry(
    THREE: typeof import('three'),
    existingGeometry?: BufferGeometry
  ): BufferGeometry {
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
        } else if (vertexUVAssigned[vertexIdx] !== uvIdx) {
          hasSeams = true;
        }
      }

      if (hasSeams) {
        console.warn(
          'OpenSubdiv: UV seams detected. Indexed geometry cannot represent ' +
            'face-varying UVs perfectly. Call geometry.toNonIndexed() for correct seams.'
        );
      }

      geometry.setAttribute('uv', new THREE.BufferAttribute(expandedUVs, 2));
    }

    return geometry;
  }

  updateBufferGeometry(geometry: BufferGeometry): void {
    const mesh = this.ensureInitialized();

    const positions = mesh.getPositions();
    const normals = mesh.getNormals();

    const posAttr = geometry.getAttribute('position') as
      | BufferAttribute
      | undefined;
    const normAttr = geometry.getAttribute('normal') as
      | BufferAttribute
      | undefined;

    if (posAttr && posAttr.array.length === positions.length) {
      (posAttr.array as Float32Array).set(positions);
      posAttr.needsUpdate = true;
    }

    if (normAttr && normAttr.array.length === normals.length) {
      (normAttr.array as Float32Array).set(normals);
      normAttr.needsUpdate = true;
    }
  }

  dispose(): void {
    if (this.mesh) {
      this.mesh.delete();
      this.mesh = null;
    }
  }
}

export async function subdivideQuadGeometry(
  THREE: typeof import('three'),
  geometry: BufferGeometry,
  options: SubdivisionOptions = {}
): Promise<BufferGeometry> {
  const surface = new SubdivisionSurface(options);
  await surface.init();

  try {
    surface.initFromQuadGeometry(geometry);
    return surface.toBufferGeometry(THREE);
  } finally {
    surface.dispose();
  }
}
