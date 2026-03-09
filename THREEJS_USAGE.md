# OpenSubdiv WASM - Three.js Integration Guide

## Installation

```bash
npm install @nasedkinpv/opensubdiv-wasm
```

## Quick Start

### Browser (ES Modules)

```html
<script type="importmap">
{
  "imports": {
    "three": "https://cdn.jsdelivr.net/npm/three@0.183.2/build/three.module.js"
  }
}
</script>
<script type="module">
import * as THREE from 'three';
import OpenSubdiv from './dist/opensubdiv.mjs';

const module = await OpenSubdiv();

// Define a quad mesh (cube)
const positions = new Float32Array([
  -0.5, -0.5,  0.5,   // 0
   0.5, -0.5,  0.5,   // 1
  -0.5,  0.5,  0.5,   // 2
   0.5,  0.5,  0.5,   // 3
  -0.5,  0.5, -0.5,   // 4
   0.5,  0.5, -0.5,   // 5
  -0.5, -0.5, -0.5,   // 6
   0.5, -0.5, -0.5    // 7
]);

const quadIndices = new Int32Array([
  0, 1, 3, 2,  // front
  2, 3, 5, 4,  // top
  4, 5, 7, 6,  // back
  6, 7, 1, 0,  // bottom
  1, 7, 5, 3,  // right
  6, 0, 2, 4   // left
]);

// Create subdivision mesh
const mesh = new module.SubdivisionMesh();
mesh.initFromQuads(
  positions, 
  quadIndices, 
  6,  // numQuads
  2,  // subdivisionLevel
  module.BOUNDARY_EDGE_ONLY
);

// Get subdivided geometry
const geometry = new THREE.BufferGeometry();
geometry.setAttribute('position', new THREE.BufferAttribute(new Float32Array(mesh.getPositions()), 3));
geometry.setAttribute('normal', new THREE.BufferAttribute(new Float32Array(mesh.getNormals()), 3));
geometry.setIndex(new THREE.BufferAttribute(new Uint32Array(mesh.getIndices()), 1));

const material = new THREE.MeshStandardMaterial({ color: 0x4a90d9 });
const subdividedMesh = new THREE.Mesh(geometry, material);
scene.add(subdividedMesh);

// Clean up when done
mesh.delete();
</script>
```

### Node.js

```javascript
const OpenSubdiv = require('@nasedkinpv/opensubdiv-wasm');

async function main() {
  const module = await OpenSubdiv();
  
  const positions = new Float32Array([...]);
  const indices = new Int32Array([...]);
  
  const mesh = new module.SubdivisionMesh();
  mesh.initFromQuads(positions, indices, numQuads, level, module.BOUNDARY_EDGE_ONLY);
  
  console.log('Vertices:', mesh.getVertexCount());
  console.log('Triangles:', mesh.getTriangleCount());
  
  mesh.delete();
}

main();
```

### Custom WASM Location

When your bundler serves `opensubdiv.wasm` from a non-default URL, pass Emscripten module options into the factory:

```typescript
import OpenSubdiv from '@nasedkinpv/opensubdiv-wasm';

const module = await OpenSubdiv({
  locateFile: (path, prefix) => new URL(path, prefix).toString(),
});
```

## TypeScript Wrapper

For a higher-level TypeScript API:

```typescript
import * as THREE from 'three';
import { SubdivisionSurface, BOUNDARY_EDGE_ONLY } from '@nasedkinpv/opensubdiv-wasm/threejs';

const surface = new SubdivisionSurface({ level: 3 });
await surface.init();

// Initialize from raw data
surface.initFromQuads(positions, quadIndices);

// Or from Three.js geometry (must be indexed quad mesh)
surface.initFromQuadGeometry(myQuadGeometry);

// Interleaved position attributes are supported as well
surface.initFromQuadGeometry(interleavedQuadGeometry);

// Get Three.js BufferGeometry (pass THREE module)
const subdividedGeometry = surface.toBufferGeometry(THREE);

// Animation: update control points
function animate() {
  updatePositions(positions);  // modify control vertices
  surface.updatePositions(positions);
  surface.updateBufferGeometry(subdividedGeometry);
}

// Clean up
surface.dispose();
```

### Helper Function

For one-shot subdivision without managing lifecycle:

```typescript
import * as THREE from 'three';
import { subdivideQuadGeometry } from '@nasedkinpv/opensubdiv-wasm/threejs';

// Subdivide and return new BufferGeometry
const subdividedGeometry = await subdivideQuadGeometry(THREE, myQuadGeometry, { level: 2 });
```

## API Reference

### SubdivisionMesh (WASM)

```typescript
class SubdivisionMesh {
  // Initialize from pure quad mesh
  initFromQuads(
    positions: Float32Array,      // [x0,y0,z0, x1,y1,z1, ...]
    quadIndices: Int32Array,      // [v0,v1,v2,v3, ...] CCW winding
    numQuads: number,
    subdivisionLevel: number,     // 1-5 recommended
    boundaryInterpolation: number // BOUNDARY_NONE | BOUNDARY_EDGE_ONLY | BOUNDARY_EDGE_AND_CORNER
  ): boolean;

  // Initialize from mixed polygon mesh
  initFromPolygons(
    positions: Float32Array,
    faceIndices: Int32Array,      // all face vertices concatenated
    faceSizes: Int32Array,        // [4, 4, 3, 4, ...] vertices per face
    subdivisionLevel: number,
    boundaryInterpolation: number
  ): boolean;

  // Update positions for animation (fast path)
  updatePositions(positions: Float32Array): void;

  // Get results (zero-copy views into WASM memory)
  getPositions(): Float32Array;  // subdivided vertex positions
  getNormals(): Float32Array;    // computed vertex normals
  getIndices(): Uint32Array;     // triangle indices

  // Initialize with UV support (face-varying interpolation)
  initFromQuadsWithUVs(
    positions: Float32Array,
    quadIndices: Int32Array,
    numQuads: number,
    subdivisionLevel: number,
    boundaryInterpolation: number,
    uvs: Float32Array,           // [u0,v0, u1,v1, ...] unique UV coordinates
    uvIndices: Int32Array,       // UV index per face corner (same length as quadIndices)
    numUVs: number               // number of unique UV coordinates
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

  // UV accessors (only valid after initFrom*WithUVs)
  getUVs(): Float32Array;        // subdivided UV values
  getUVIndices(): Uint32Array;   // UV index per triangle vertex
  getUVCount(): number;          // number of subdivided UVs
  hasUVData(): boolean;

  // Info
  getVertexCount(): number;
  getTriangleCount(): number;
  getInputVertexCount(): number;

  // Clean up
  delete(): void;
}
```

### Constants

```typescript
BOUNDARY_NONE = 0;           // No boundary interpolation
BOUNDARY_EDGE_ONLY = 1;      // Interpolate boundary edges
BOUNDARY_EDGE_AND_CORNER = 2; // Interpolate edges and corners
```

## Animation Performance

The WASM module uses pre-computed StencilTables for efficient animation:

```javascript
// Initial setup (expensive)
mesh.initFromQuads(positions, indices, numQuads, level, boundary);

// Per-frame update (fast ~10-100x faster than re-subdivision)
function animate() {
  // Modify control vertices
  for (let i = 0; i < numVerts; i++) {
    positions[i * 3 + 1] = Math.sin(time + i) * 0.1;
  }
  
  // Fast stencil-based update
  mesh.updatePositions(positions);
  
  // Update Three.js geometry
  const posAttr = geometry.getAttribute('position');
  posAttr.array.set(mesh.getPositions());
  posAttr.needsUpdate = true;
  
  const normAttr = geometry.getAttribute('normal');
  normAttr.array.set(mesh.getNormals());
  normAttr.needsUpdate = true;
}
```

## Subdivision Levels

| Level | Quads per face | Memory | Performance |
|-------|---------------|--------|-------------|
| 1     | 4             | Low    | Fast        |
| 2     | 16            | Medium | Fast        |
| 3     | 64            | Medium | Good        |
| 4     | 256           | High   | Slow        |
| 5     | 1024          | Very High | Very Slow |

For most use cases, level 2-3 provides good visual quality.

### Benchmarks (984 input vertices)

| Level | Output Verts | Triangles | Init Time | Throughput |
|-------|--------------|-----------|-----------|------------|
| 1     | ~4K          | ~8K       | ~1.5ms    | ~2,600 v/ms |
| 2     | ~16K         | ~31K      | ~5ms      | ~3,000 v/ms |
| 3     | ~63K         | ~126K     | ~25ms     | ~2,500 v/ms |
| 4     | ~251K        | ~503K     | ~120ms    | ~2,100 v/ms |

- **WASM module init**: ~17ms (one-time)
- **Animation updates** (`updatePositions`): ~10-100x faster than initial subdivision

## Memory Management

The WASM module manages its own memory. Always call `delete()` when done:

```javascript
const mesh = new module.SubdivisionMesh();
try {
  mesh.initFromQuads(...);
  // use mesh...
} finally {
  mesh.delete();  // Free WASM memory
}
```

Views returned by `getPositions()`, `getNormals()`, `getIndices()` become invalid after:
- Calling `delete()`
- WASM heap growth (large allocations)

Always copy data to your own arrays if you need to persist it:

```javascript
const positions = new Float32Array(mesh.getPositions());  // Safe copy
```
