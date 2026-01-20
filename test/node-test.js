const OpenSubdiv = require('../dist/opensubdiv.js');

async function runTests() {
  console.log('=== OpenSubdiv WASM Test Suite ===\n');
  
  const module = await OpenSubdiv();
  let passed = 0;
  let failed = 0;
  
  function test(name, fn) {
    try {
      fn();
      console.log(`✓ ${name}`);
      passed++;
    } catch (e) {
      console.log(`✗ ${name}: ${e.message}`);
      failed++;
    }
  }
  
  function assert(condition, message) {
    if (!condition) throw new Error(message || 'Assertion failed');
  }
  
  test('Module loads correctly', () => {
    assert(typeof module.SubdivisionMesh === 'function');
    assert(module.BOUNDARY_NONE === 0);
    assert(module.BOUNDARY_EDGE_ONLY === 1);
    assert(module.BOUNDARY_EDGE_AND_CORNER === 2);
  });
  
  test('Create SubdivisionMesh', () => {
    const mesh = new module.SubdivisionMesh();
    assert(mesh);
    mesh.delete();
  });
  
  test('initFromQuads - simple plane', () => {
    const positions = new Float32Array([
      -1, 0, -1,
       1, 0, -1,
       1, 0,  1,
      -1, 0,  1
    ]);
    const indices = new Int32Array([0, 1, 2, 3]);
    
    const mesh = new module.SubdivisionMesh();
    const success = mesh.initFromQuads(positions, indices, 1, 2, module.BOUNDARY_EDGE_ONLY);
    
    assert(success, 'initFromQuads should return true');
    assert(mesh.getInputVertexCount() === 4);
    assert(mesh.getVertexCount() === 25);
    assert(mesh.getTriangleCount() === 32);
    
    mesh.delete();
  });
  
  test('initFromQuads - cube (6 faces)', () => {
    const positions = new Float32Array([
      -0.5, -0.5,  0.5,
       0.5, -0.5,  0.5,
      -0.5,  0.5,  0.5,
       0.5,  0.5,  0.5,
      -0.5,  0.5, -0.5,
       0.5,  0.5, -0.5,
      -0.5, -0.5, -0.5,
       0.5, -0.5, -0.5
    ]);
    const indices = new Int32Array([
      0, 1, 3, 2,
      2, 3, 5, 4,
      4, 5, 7, 6,
      6, 7, 1, 0,
      1, 7, 5, 3,
      6, 0, 2, 4
    ]);
    
    const mesh = new module.SubdivisionMesh();
    const success = mesh.initFromQuads(positions, indices, 6, 2, module.BOUNDARY_EDGE_ONLY);
    
    assert(success);
    assert(mesh.getInputVertexCount() === 8);
    assert(mesh.getVertexCount() > 8);
    
    mesh.delete();
  });
  
  test('getPositions returns valid data', () => {
    const positions = new Float32Array([-1, 0, -1, 1, 0, -1, 1, 0, 1, -1, 0, 1]);
    const indices = new Int32Array([0, 1, 2, 3]);
    
    const mesh = new module.SubdivisionMesh();
    mesh.initFromQuads(positions, indices, 1, 1, module.BOUNDARY_EDGE_ONLY);
    
    const outPos = mesh.getPositions();
    assert(outPos.length === mesh.getVertexCount() * 3);
    assert(outPos.length > 0);
    
    mesh.delete();
  });
  
  test('getNormals returns valid data', () => {
    const positions = new Float32Array([-1, 0, -1, 1, 0, -1, 1, 0, 1, -1, 0, 1]);
    const indices = new Int32Array([0, 1, 2, 3]);
    
    const mesh = new module.SubdivisionMesh();
    mesh.initFromQuads(positions, indices, 1, 1, module.BOUNDARY_EDGE_ONLY);
    
    const normals = mesh.getNormals();
    assert(normals.length === mesh.getVertexCount() * 3);
    
    const n = [normals[0], normals[1], normals[2]];
    const len = Math.sqrt(n[0]*n[0] + n[1]*n[1] + n[2]*n[2]);
    assert(Math.abs(len - 1.0) < 0.01, 'Normals should be unit length');
    
    mesh.delete();
  });
  
  test('getIndices returns valid triangles', () => {
    const positions = new Float32Array([-1, 0, -1, 1, 0, -1, 1, 0, 1, -1, 0, 1]);
    const indices = new Int32Array([0, 1, 2, 3]);
    
    const mesh = new module.SubdivisionMesh();
    mesh.initFromQuads(positions, indices, 1, 1, module.BOUNDARY_EDGE_ONLY);
    
    const outIndices = mesh.getIndices();
    assert(outIndices.length === mesh.getTriangleCount() * 3);
    assert(outIndices.length % 3 === 0);
    
    mesh.delete();
  });
  
  test('updatePositions for animation', () => {
    const positions = new Float32Array([-1, 0, -1, 1, 0, -1, 1, 0, 1, -1, 0, 1]);
    const indices = new Int32Array([0, 1, 2, 3]);
    
    const mesh = new module.SubdivisionMesh();
    mesh.initFromQuads(positions, indices, 1, 1, module.BOUNDARY_EDGE_ONLY);
    
    const before = Array.from(mesh.getPositions().slice(0, 3));
    
    positions[1] = 0.5;
    mesh.updatePositions(positions);
    
    const after = Array.from(mesh.getPositions().slice(0, 3));
    assert(before[1] !== after[1], 'Positions should change after update');
    
    mesh.delete();
  });
  
  test('Different subdivision levels', () => {
    const positions = new Float32Array([-1, 0, -1, 1, 0, -1, 1, 0, 1, -1, 0, 1]);
    const indices = new Int32Array([0, 1, 2, 3]);
    
    const counts = [];
    for (let level = 1; level <= 4; level++) {
      const mesh = new module.SubdivisionMesh();
      mesh.initFromQuads(positions, indices, 1, level, module.BOUNDARY_EDGE_ONLY);
      counts.push(mesh.getVertexCount());
      mesh.delete();
    }
    
    assert(counts[0] < counts[1], 'Level 2 should have more verts than level 1');
    assert(counts[1] < counts[2], 'Level 3 should have more verts than level 2');
    assert(counts[2] < counts[3], 'Level 4 should have more verts than level 3');
  });
  
  test('hasUVData returns false when no UVs set', () => {
    const positions = new Float32Array([-1, 0, -1, 1, 0, -1, 1, 0, 1, -1, 0, 1]);
    const indices = new Int32Array([0, 1, 2, 3]);
    
    const mesh = new module.SubdivisionMesh();
    mesh.initFromQuads(positions, indices, 1, 1, module.BOUNDARY_EDGE_ONLY);
    
    assert(mesh.hasUVData() === false);
    
    mesh.delete();
  });
  
  // ============ UV Support Tests ============
  
  test('initFromQuadsWithUVs - basic UV subdivision', () => {
    // Single quad with standard UV layout
    const positions = new Float32Array([
      -1, 0, -1,  // vertex 0 - bottom-left
       1, 0, -1,  // vertex 1 - bottom-right
       1, 0,  1,  // vertex 2 - top-right
      -1, 0,  1   // vertex 3 - top-left
    ]);
    const indices = new Int32Array([0, 1, 2, 3]);
    
    // UV coordinates matching the vertices
    const uvs = new Float32Array([
      0, 0,  // uv 0 - bottom-left
      1, 0,  // uv 1 - bottom-right  
      1, 1,  // uv 2 - top-right
      0, 1   // uv 3 - top-left
    ]);
    // UV indices per face corner (same as vertex indices for simple case)
    const uvIndices = new Int32Array([0, 1, 2, 3]);
    
    const mesh = new module.SubdivisionMesh();
    const success = mesh.initFromQuadsWithUVs(
      positions, indices, 1,  // 1 quad
      2,                       // subdivision level 2
      module.BOUNDARY_EDGE_ONLY,
      uvs, uvIndices, 4        // 4 unique UV coordinates
    );
    
    assert(success, 'initFromQuadsWithUVs should return true');
    assert(mesh.hasUVData() === true, 'hasUVData should return true');
    
    mesh.delete();
  });
  
  test('initFromQuadsWithUVs - UV counts are valid', () => {
    const positions = new Float32Array([-1, 0, -1, 1, 0, -1, 1, 0, 1, -1, 0, 1]);
    const indices = new Int32Array([0, 1, 2, 3]);
    const uvs = new Float32Array([0, 0, 1, 0, 1, 1, 0, 1]);
    const uvIndices = new Int32Array([0, 1, 2, 3]);
    
    const mesh = new module.SubdivisionMesh();
    mesh.initFromQuadsWithUVs(positions, indices, 1, 2, module.BOUNDARY_EDGE_ONLY, uvs, uvIndices, 4);
    
    const outputUVs = mesh.getUVs();
    const uvCount = mesh.getUVCount();
    
    assert(uvCount > 0, 'UV count should be > 0');
    assert(outputUVs.length === uvCount * 2, 'getUVs length should match getUVCount * 2');
    assert(outputUVs.length > 8, 'Subdivided UVs should have more values than input');
    
    mesh.delete();
  });
  
  test('initFromQuadsWithUVs - UV indices match triangle count', () => {
    const positions = new Float32Array([-1, 0, -1, 1, 0, -1, 1, 0, 1, -1, 0, 1]);
    const indices = new Int32Array([0, 1, 2, 3]);
    const uvs = new Float32Array([0, 0, 1, 0, 1, 1, 0, 1]);
    const uvIndices = new Int32Array([0, 1, 2, 3]);
    
    const mesh = new module.SubdivisionMesh();
    mesh.initFromQuadsWithUVs(positions, indices, 1, 2, module.BOUNDARY_EDGE_ONLY, uvs, uvIndices, 4);
    
    const triIndices = mesh.getIndices();
    const uvIdxs = mesh.getUVIndices();
    
    assert(uvIdxs.length === triIndices.length, 
      `UV indices (${uvIdxs.length}) should match triangle indices (${triIndices.length})`);
    
    mesh.delete();
  });
  
  test('initFromQuadsWithUVs - UV values are in valid range', () => {
    const positions = new Float32Array([-1, 0, -1, 1, 0, -1, 1, 0, 1, -1, 0, 1]);
    const indices = new Int32Array([0, 1, 2, 3]);
    const uvs = new Float32Array([0, 0, 1, 0, 1, 1, 0, 1]);
    const uvIndices = new Int32Array([0, 1, 2, 3]);
    
    const mesh = new module.SubdivisionMesh();
    mesh.initFromQuadsWithUVs(positions, indices, 1, 2, module.BOUNDARY_EDGE_ONLY, uvs, uvIndices, 4);
    
    const outputUVs = mesh.getUVs();
    
    // Check all UV values are within [0, 1] range (for standard UV mapping)
    let allValid = true;
    for (let i = 0; i < outputUVs.length; i++) {
      if (outputUVs[i] < -0.001 || outputUVs[i] > 1.001) {
        allValid = false;
        break;
      }
    }
    assert(allValid, 'All UV values should be in valid [0,1] range');
    
    mesh.delete();
  });
  
  test('initFromQuadsWithUVs - cube with per-face UVs', () => {
    // A cube needs separate UVs per face (24 UVs, not 8)
    const positions = new Float32Array([
      -0.5, -0.5,  0.5,  // 0
       0.5, -0.5,  0.5,  // 1
      -0.5,  0.5,  0.5,  // 2
       0.5,  0.5,  0.5,  // 3
      -0.5,  0.5, -0.5,  // 4
       0.5,  0.5, -0.5,  // 5
      -0.5, -0.5, -0.5,  // 6
       0.5, -0.5, -0.5   // 7
    ]);
    const indices = new Int32Array([
      0, 1, 3, 2,  // front
      2, 3, 5, 4,  // top
      4, 5, 7, 6,  // back
      6, 7, 1, 0,  // bottom
      1, 7, 5, 3,  // right
      6, 0, 2, 4   // left
    ]);
    
    // 24 UV coordinates (4 per face, 6 faces)
    const uvs = new Float32Array([
      // Each face gets standard 0-1 UV layout
      0, 0, 1, 0, 1, 1, 0, 1,  // face 0
      0, 0, 1, 0, 1, 1, 0, 1,  // face 1
      0, 0, 1, 0, 1, 1, 0, 1,  // face 2
      0, 0, 1, 0, 1, 1, 0, 1,  // face 3
      0, 0, 1, 0, 1, 1, 0, 1,  // face 4
      0, 0, 1, 0, 1, 1, 0, 1   // face 5
    ]);
    // UV indices: each face corner maps to its own UV
    const uvIndices = new Int32Array([
      0, 1, 2, 3,      // face 0
      4, 5, 6, 7,      // face 1
      8, 9, 10, 11,    // face 2
      12, 13, 14, 15,  // face 3
      16, 17, 18, 19,  // face 4
      20, 21, 22, 23   // face 5
    ]);
    
    const mesh = new module.SubdivisionMesh();
    const success = mesh.initFromQuadsWithUVs(
      positions, indices, 6,
      2, module.BOUNDARY_EDGE_ONLY,
      uvs, uvIndices, 24
    );
    
    assert(success, 'Cube with per-face UVs should initialize successfully');
    assert(mesh.hasUVData() === true);
    assert(mesh.getUVCount() > 24, 'Subdivided UV count should be greater than input');
    
    mesh.delete();
  });
  
  console.log(`\n=== Results: ${passed} passed, ${failed} failed ===`);
  
  if (failed > 0) {
    process.exit(1);
  }
}

runTests().catch(err => {
  console.error('Test error:', err);
  process.exit(1);
});
