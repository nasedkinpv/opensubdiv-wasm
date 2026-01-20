/**
 * OpenSubdiv WASM Module Type Declarations
 * @author nasedkinpv
 * @license Apache-2.0
 */

export interface WasmSubdivisionMesh {
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

declare const factory: OpenSubdivFactory;
export default factory;
