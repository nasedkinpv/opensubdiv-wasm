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
  
  console.log(`\n=== Results: ${passed} passed, ${failed} failed ===`);
  
  if (failed > 0) {
    process.exit(1);
  }
}

runTests().catch(err => {
  console.error('Test error:', err);
  process.exit(1);
});
