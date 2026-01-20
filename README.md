# @nasedkinpv/opensubdiv-wasm

WebAssembly port of [Pixar's OpenSubdiv](https://github.com/PixarAnimationStudios/OpenSubdiv) for browser and Node.js.

High-performance Catmull-Clark subdivision surfaces with Three.js integration.

**[Live Demo](https://nasedkinpv.github.io/opensubdiv-wasm/)** — WebGPU + HDR environment

## Features

- Catmull-Clark subdivision (levels 1-5)
- Real-time animation via StencilTable caching
- Zero-copy typed array access to WASM memory
- Three.js BufferGeometry integration
- Face-varying UV interpolation
- Mixed quad/triangle mesh support

## Bundle Size

| File | Size | Gzipped |
|------|------|---------|
| `opensubdiv.wasm` | 156 KB | **57 KB** |
| `opensubdiv.mjs` | 46 KB | 12 KB |
| Total | 202 KB | **69 KB** |

## Installation

```bash
npm install @nasedkinpv/opensubdiv-wasm
```

## Quick Start

### Low-level WASM API

```javascript
import OpenSubdiv from '@nasedkinpv/opensubdiv-wasm';

const module = await OpenSubdiv();

const positions = new Float32Array([
  -1, -1, 1,  1, -1, 1,  1, 1, 1,  -1, 1, 1,  // front
  -1, -1, -1, 1, -1, -1, 1, 1, -1, -1, 1, -1  // back
]);

const quadIndices = new Int32Array([
  0, 1, 2, 3,  // front
  5, 4, 7, 6,  // back
  4, 0, 3, 7,  // left
  1, 5, 6, 2,  // right
  3, 2, 6, 7,  // top
  4, 5, 1, 0   // bottom
]);

const mesh = new module.SubdivisionMesh();
mesh.initFromQuads(positions, quadIndices, 6, 2, module.BOUNDARY_EDGE_ONLY);

console.log('Vertices:', mesh.getVertexCount());
console.log('Triangles:', mesh.getTriangleCount());

const subdivided = mesh.getPositions();  // Float32Array
const normals = mesh.getNormals();       // Float32Array  
const indices = mesh.getIndices();       // Uint32Array

mesh.delete();  // Free WASM memory
```

### Three.js Integration

```typescript
import * as THREE from 'three';
import { SubdivisionSurface } from '@nasedkinpv/opensubdiv-wasm/threejs';

const surface = new SubdivisionSurface({ level: 2 });
await surface.init();

surface.initFromQuads(positions, quadIndices);
const geometry = surface.toBufferGeometry(THREE);

const mesh = new THREE.Mesh(geometry, material);
scene.add(mesh);

// Animation (fast path using cached stencils)
function animate() {
  modifyControlVertices(positions);
  surface.updatePositions(positions);
  surface.updateBufferGeometry(geometry);
}

surface.dispose();
```

## API Reference

See [THREEJS_USAGE.md](./THREEJS_USAGE.md) for complete API documentation.

## Performance

Benchmarked with 984 input vertices (quad mesh):

| Level | Output Verts | Triangles | Time | Throughput |
|-------|--------------|-----------|------|------------|
| 1 | ~4K | ~8K | ~1.5ms | ~2,600 v/ms |
| 2 | ~16K | ~31K | ~5ms | ~3,000 v/ms |
| 3 | ~63K | ~126K | ~25ms | ~2,500 v/ms |
| 4 | ~251K | ~503K | ~120ms | ~2,100 v/ms |

- **WASM init**: ~17ms (one-time)
- **Level 2**: Sweet spot for real-time (5ms for 16K verts)
- **Animation updates**: Use pre-computed StencilTables, ~10-100x faster than re-subdivision

## Building from Source

### Requirements

- [Emscripten](https://emscripten.org/docs/getting_started/downloads.html) (for WASM compilation)
- [Binaryen](https://github.com/WebAssembly/binaryen) (for `wasm-opt` optimization)

```bash
# Install Emscripten (one-time)
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk && ./emsdk install latest && ./emsdk activate latest

# Install Binaryen (macOS)
brew install binaryen

# Or via npm (cross-platform)
npm install -g binaryen
```

### Build Commands

```bash
# Activate Emscripten (every terminal session)
source /path/to/emsdk/emsdk_env.sh

# Full build (WASM + wasm-opt + TypeScript wrapper)
npm run build

# Quick build without wasm-opt (faster iteration)
npm run build:quick

# Run tests
npm test
```

### Build Scripts

| Script | Description |
|--------|-------------|
| `build` | Full optimized build (WASM + wasm-opt + threejs) |
| `build:quick` | Skip wasm-opt for faster iteration |
| `build:wasm` | Build CommonJS WASM module |
| `build:wasm:esm` | Build ES Module WASM module |
| `build:wasm:optimize` | Run wasm-opt -Oz on WASM binary |
| `build:threejs` | Compile TypeScript wrapper |

## License

[TOST-1.0](./LICENSE.txt) (Tomorrow Open Source Technology License - modified Apache 2.0)

Based on [OpenSubdiv](https://github.com/PixarAnimationStudios/OpenSubdiv) by Pixar Animation Studios.

## Trademark Notice

This project is **not affiliated with, endorsed by, or sponsored by Pixar Animation Studios, DreamWorks Animation, or any of their affiliates**.

"OpenSubdiv", "Pixar", and "DreamWorks" are trademarks of their respective owners. Use of these names is solely for attribution purposes as required by the license.

## Credits

- **OpenSubdiv**: Pixar Animation Studios
- **WASM Port**: [nasedkinpv](https://github.com/nasedkinpv)
- **Original Fork**: [discere-os/OpenSubdiv.wasm](https://github.com/discere-os/OpenSubdiv.wasm)

See [NOTICE.txt](./NOTICE.txt) for full attribution.
