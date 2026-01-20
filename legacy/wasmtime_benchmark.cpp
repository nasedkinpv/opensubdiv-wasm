#include "opensubdiv/far/topologyDescriptor.h"
#include "opensubdiv/far/primvarRefiner.h"
#include "opensubdiv/far/stencilTableFactory.h"
#include <vector>
#include <iostream>
#include <cstdio>
#include <chrono>
#include <cmath>
#include <cstring>

// High-resolution timing
using Clock = std::chrono::high_resolution_clock;
using TimePoint = std::chrono::time_point<Clock>;
using Duration = std::chrono::duration<double, std::milli>;

// Vertex container implementation
struct Vertex {
    Vertex() { }

    Vertex(Vertex const & src) {
        _position[0] = src._position[0];
        _position[1] = src._position[1];
        _position[2] = src._position[2];
    }

    void Clear( void * =0 ) {
        _position[0]=_position[1]=_position[2]=0.0f;
    }

    void AddWithWeight(Vertex const & src, float weight) {
        _position[0]+=weight*src._position[0];
        _position[1]+=weight*src._position[1];
        _position[2]+=weight*src._position[2];
    }

    void SetPosition(float x, float y, float z) {
        _position[0]=x;
        _position[1]=y;
        _position[2]=z;
    }

    const float * GetPosition() const {
        return _position;
    }

private:
    float _position[3];
};

using namespace OpenSubdiv;

// Test mesh generators
class MeshGenerator {
public:
    struct Mesh {
        std::vector<float> vertices;
        std::vector<int> faces;
        std::vector<int> faceCounts;
        int numVertices;
        int numFaces;
        std::string name;
    };
    
    // Generate simple cube (8 vertices, 6 faces)
    static Mesh generateCube() {
        Mesh mesh;
        mesh.name = "Cube";
        
        // Cube vertices
        float cubeVerts[8][3] = {
            { -0.5f, -0.5f,  0.5f }, {  0.5f, -0.5f,  0.5f },
            { -0.5f,  0.5f,  0.5f }, {  0.5f,  0.5f,  0.5f },
            { -0.5f,  0.5f, -0.5f }, {  0.5f,  0.5f, -0.5f },
            { -0.5f, -0.5f, -0.5f }, {  0.5f, -0.5f, -0.5f }
        };
        
        int cubeFaces[6][4] = {
            { 0, 1, 3, 2 }, { 2, 3, 5, 4 }, { 4, 5, 7, 6 },
            { 6, 7, 1, 0 }, { 1, 7, 5, 3 }, { 6, 0, 2, 4 }
        };
        
        for (int i = 0; i < 8; ++i) {
            for (int j = 0; j < 3; ++j) {
                mesh.vertices.push_back(cubeVerts[i][j]);
            }
        }
        
        for (int i = 0; i < 6; ++i) {
            mesh.faceCounts.push_back(4);
            for (int j = 0; j < 4; ++j) {
                mesh.faces.push_back(cubeFaces[i][j]);
            }
        }
        
        mesh.numVertices = 8;
        mesh.numFaces = 6;
        return mesh;
    }
    
    // Generate icosahedron (12 vertices, 20 faces)
    static Mesh generateIcosahedron() {
        Mesh mesh;
        mesh.name = "Icosahedron";
        
        const float phi = (1.0f + sqrt(5.0f)) * 0.5f; // golden ratio
        const float a = 1.0f;
        const float b = 1.0f / phi;
        
        float verts[12][3] = {
            {0, b, -a}, {b, a, 0}, {-b, a, 0}, {0, b, a},
            {0, -b, a}, {-b, -a, 0}, {b, -a, 0}, {0, -b, -a},
            {a, 0, -b}, {a, 0, b}, {-a, 0, b}, {-a, 0, -b}
        };
        
        int faces[20][3] = {
            {2, 1, 0}, {1, 2, 3}, {5, 4, 3}, {4, 8, 3},
            {7, 6, 0}, {6, 9, 0}, {11, 10, 4}, {10, 11, 6},
            {9, 8, 2}, {8, 9, 4}, {11, 7, 2}, {7, 11, 10},
            {5, 3, 10}, {3, 8, 10}, {5, 10, 6}, {10, 8, 6},
            {1, 9, 6}, {9, 1, 8}, {1, 3, 9}, {7, 0, 2}
        };
        
        for (int i = 0; i < 12; ++i) {
            for (int j = 0; j < 3; ++j) {
                mesh.vertices.push_back(verts[i][j]);
            }
        }
        
        for (int i = 0; i < 20; ++i) {
            mesh.faceCounts.push_back(3);
            for (int j = 0; j < 3; ++j) {
                mesh.faces.push_back(faces[i][j]);
            }
        }
        
        mesh.numVertices = 12;
        mesh.numFaces = 20;
        return mesh;
    }
    
    // Generate subdivided plane (creates larger mesh)
    static Mesh generatePlane(int subdivisions) {
        Mesh mesh;
        mesh.name = "Plane_" + std::to_string(subdivisions) + "x" + std::to_string(subdivisions);
        
        int vertsPerSide = subdivisions + 1;
        mesh.numVertices = vertsPerSide * vertsPerSide;
        mesh.numFaces = subdivisions * subdivisions;
        
        // Generate vertices
        for (int y = 0; y < vertsPerSide; ++y) {
            for (int x = 0; x < vertsPerSide; ++x) {
                float u = (float)x / (float)subdivisions;
                float v = (float)y / (float)subdivisions;
                mesh.vertices.push_back(u * 2.0f - 1.0f);  // x: -1 to 1
                mesh.vertices.push_back(0.0f);             // y: 0 (flat plane)
                mesh.vertices.push_back(v * 2.0f - 1.0f);  // z: -1 to 1
            }
        }
        
        // Generate faces (quads)
        for (int y = 0; y < subdivisions; ++y) {
            for (int x = 0; x < subdivisions; ++x) {
                int bottomLeft = y * vertsPerSide + x;
                int bottomRight = bottomLeft + 1;
                int topLeft = (y + 1) * vertsPerSide + x;
                int topRight = topLeft + 1;
                
                mesh.faceCounts.push_back(4);
                mesh.faces.push_back(bottomLeft);
                mesh.faces.push_back(bottomRight);
                mesh.faces.push_back(topRight);
                mesh.faces.push_back(topLeft);
            }
        }
        
        return mesh;
    }
};

// Benchmark configuration
struct BenchmarkConfig {
    int iterations;
    int maxLevel;
    bool verbose;
    bool warmup;
    
    BenchmarkConfig() : iterations(100), maxLevel(2), verbose(false), warmup(true) {}
};

// Benchmark result structure
struct BenchmarkResult {
    std::string meshName;
    std::string buildType;
    int inputVertices;
    int inputFaces;
    int outputVertices;
    int outputFaces;
    double avgTime;        // milliseconds
    double minTime;
    double maxTime;
    double stdDev;
    double verticesPerMs;
    double subdivPerSec;
    int maxLevel;
    int iterations;
};

// Main benchmark function
BenchmarkResult benchmarkSubdivision(const MeshGenerator::Mesh& testMesh, 
                                    const BenchmarkConfig& config,
                                    const std::string& buildType) {
    printf("Benchmarking %s subdivision on %s mesh...\n", 
           buildType.c_str(), testMesh.name.c_str());
    
    BenchmarkResult result;
    result.meshName = testMesh.name;
    result.buildType = buildType;
    result.inputVertices = testMesh.numVertices;
    result.inputFaces = testMesh.numFaces;
    result.maxLevel = config.maxLevel;
    result.iterations = config.iterations;
    
    // Create topology descriptor
    typedef Far::TopologyDescriptor Descriptor;
    
    Sdc::SchemeType type = (testMesh.name.find("Icosahedron") != std::string::npos) ? 
                          Sdc::SCHEME_LOOP : Sdc::SCHEME_CATMARK;
    Sdc::Options options;
    options.SetVtxBoundaryInterpolation(Sdc::Options::VTX_BOUNDARY_EDGE_ONLY);

    Descriptor desc;
    desc.numVertices = testMesh.numVertices;
    desc.numFaces = testMesh.numFaces;
    desc.numVertsPerFace = const_cast<int*>(&testMesh.faceCounts[0]);
    desc.vertIndicesPerFace = const_cast<int*>(&testMesh.faces[0]);
    
    // Create topology refiner
    Far::TopologyRefiner * refiner = Far::TopologyRefinerFactory<Descriptor>::Create(desc,
                                            Far::TopologyRefinerFactory<Descriptor>::Options(type, options));
    
    if (!refiner) {
        printf("ERROR: Failed to create topology refiner for %s\n", testMesh.name.c_str());
        result.avgTime = -1.0;
        return result;
    }
    
    // Refine topology
    refiner->RefineUniform(Far::TopologyRefiner::UniformOptions(config.maxLevel));
    
    result.outputVertices = refiner->GetNumVerticesTotal();
    result.outputFaces = refiner->GetNumFacesTotal();
    
    // Warmup run
    if (config.warmup) {
        std::vector<Vertex> warmupVertices(refiner->GetNumVerticesTotal());
        Vertex * verts = &warmupVertices[0];
        
        // Initialize coarse vertices
        for (int i = 0; i < testMesh.numVertices; ++i) {
            verts[i].SetPosition(testMesh.vertices[i*3], 
                               testMesh.vertices[i*3+1], 
                               testMesh.vertices[i*3+2]);
        }
        
        Far::PrimvarRefiner primvarRefiner(*refiner);
        Vertex * src = verts;
        for (int level = 1; level <= config.maxLevel; ++level) {
            Vertex * dst = src + refiner->GetLevel(level-1).GetNumVertices();
            primvarRefiner.Interpolate(level, src, dst);
            src = dst;
        }
    }
    
    // Benchmark runs
    std::vector<double> times;
    times.reserve(config.iterations);
    
    for (int iter = 0; iter < config.iterations; ++iter) {
        std::vector<Vertex> vertices(refiner->GetNumVerticesTotal());
        Vertex * verts = &vertices[0];
        
        // Initialize coarse vertices
        for (int i = 0; i < testMesh.numVertices; ++i) {
            verts[i].SetPosition(testMesh.vertices[i*3], 
                               testMesh.vertices[i*3+1], 
                               testMesh.vertices[i*3+2]);
        }
        
        // Time the subdivision
        TimePoint startTime = Clock::now();
        
        Far::PrimvarRefiner primvarRefiner(*refiner);
        Vertex * src = verts;
        for (int level = 1; level <= config.maxLevel; ++level) {
            Vertex * dst = src + refiner->GetLevel(level-1).GetNumVertices();
            primvarRefiner.Interpolate(level, src, dst);
            src = dst;
        }
        
        TimePoint endTime = Clock::now();
        Duration duration = endTime - startTime;
        times.push_back(duration.count());
    }
    
    // Calculate statistics
    double sum = 0.0;
    result.minTime = times[0];
    result.maxTime = times[0];
    
    for (double time : times) {
        sum += time;
        if (time < result.minTime) result.minTime = time;
        if (time > result.maxTime) result.maxTime = time;
    }
    
    result.avgTime = sum / times.size();
    
    // Calculate standard deviation
    double varianceSum = 0.0;
    for (double time : times) {
        double diff = time - result.avgTime;
        varianceSum += diff * diff;
    }
    result.stdDev = sqrt(varianceSum / times.size());
    
    // Calculate derived metrics
    result.verticesPerMs = result.outputVertices / result.avgTime;
    result.subdivPerSec = 1000.0 / result.avgTime;
    
    if (config.verbose) {
        printf("  Input:  %d vertices, %d faces\n", result.inputVertices, result.inputFaces);
        printf("  Output: %d vertices, %d faces\n", result.outputVertices, result.outputFaces);
        printf("  Time: %.3f ± %.3f ms (min: %.3f, max: %.3f)\n", 
               result.avgTime, result.stdDev, result.minTime, result.maxTime);
        printf("  Rate: %.0f vertices/ms, %.0f subdivisions/sec\n", 
               result.verticesPerMs, result.subdivPerSec);
    }
    
    delete refiner;
    return result;
}

// Print benchmark results table
void printResults(const std::vector<BenchmarkResult>& results) {
    printf("\n");
    printf("========================================================================\n");
    printf("                    WASMTIME BENCHMARK RESULTS\n");
    printf("========================================================================\n");
    printf("%-12s %-8s %8s %8s %8s %10s %12s %10s\n",
           "Mesh", "Build", "In_Verts", "Out_Verts", "Time_ms", "Verts/ms", "Subdivs/sec", "Speedup");
    printf("------------------------------------------------------------------------\n");
    
    // Find baseline performance for speedup calculation
    double baselineTime = 0.0;
    for (const auto& result : results) {
        if (result.buildType == "CPU-Basic" && result.meshName == "Cube") {
            baselineTime = result.avgTime;
            break;
        }
    }
    
    for (const auto& result : results) {
        if (result.avgTime < 0) continue; // Skip failed benchmarks
        
        double speedup = (baselineTime > 0) ? baselineTime / result.avgTime : 1.0;
        
        printf("%-12s %-8s %8d %8d %8.3f %10.0f %12.0f %9.2fx\n",
               result.meshName.c_str(),
               result.buildType.c_str(),
               result.inputVertices,
               result.outputVertices,
               result.avgTime,
               result.verticesPerMs,
               result.subdivPerSec,
               speedup);
    }
    
    printf("------------------------------------------------------------------------\n");
    printf("All tests: %d iterations, %d refinement levels, Wasmtime v36.0.2\n",
           results.empty() ? 0 : results[0].iterations,
           results.empty() ? 0 : results[0].maxLevel);
    printf("========================================================================\n");
}

// Export results to CSV
void exportResultsCSV(const std::vector<BenchmarkResult>& results, const std::string& filename) {
    FILE* file = fopen(filename.c_str(), "w");
    if (!file) {
        printf("ERROR: Could not open %s for writing\n", filename.c_str());
        return;
    }
    
    fprintf(file, "Mesh,Build,InputVertices,InputFaces,OutputVertices,OutputFaces,");
    fprintf(file, "AvgTime_ms,MinTime_ms,MaxTime_ms,StdDev_ms,VerticesPerMs,SubdivisionsPerSec,");
    fprintf(file, "MaxLevel,Iterations\n");
    
    for (const auto& result : results) {
        fprintf(file, "%s,%s,%d,%d,%d,%d,%.6f,%.6f,%.6f,%.6f,%.2f,%.2f,%d,%d\n",
                result.meshName.c_str(),
                result.buildType.c_str(),
                result.inputVertices,
                result.inputFaces,
                result.outputVertices,
                result.outputFaces,
                result.avgTime,
                result.minTime,
                result.maxTime,
                result.stdDev,
                result.verticesPerMs,
                result.subdivPerSec,
                result.maxLevel,
                result.iterations);
    }
    
    fclose(file);
    printf("Results exported to %s\n", filename.c_str());
}

// Main benchmark driver
int main(int argc, char* argv[]) {
    printf("OpenSubdiv Wasmtime Benchmark Suite\n");
    printf("====================================\n");
    
    BenchmarkConfig config;
    std::string buildType = "CPU-Wasmtime";
    
    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--iterations") == 0 && i + 1 < argc) {
            config.iterations = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--levels") == 0 && i + 1 < argc) {
            config.maxLevel = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--verbose") == 0) {
            config.verbose = true;
        } else if (strcmp(argv[i], "--build") == 0 && i + 1 < argc) {
            buildType = argv[++i];
        }
    }
    
    printf("Configuration:\n");
    printf("  Build Type: %s\n", buildType.c_str());
    printf("  Iterations: %d\n", config.iterations);
    printf("  Max Levels: %d\n", config.maxLevel);
    printf("  Verbose: %s\n", config.verbose ? "Yes" : "No");
    printf("\n");
    
    // Generate test meshes
    std::vector<MeshGenerator::Mesh> testMeshes = {
        MeshGenerator::generateCube(),
        MeshGenerator::generateIcosahedron(),
        MeshGenerator::generatePlane(4),   // 25 vertices, 16 faces
        MeshGenerator::generatePlane(8),   // 81 vertices, 64 faces
        MeshGenerator::generatePlane(16)   // 289 vertices, 256 faces
    };
    
    // Run benchmarks
    std::vector<BenchmarkResult> results;
    
    for (const auto& mesh : testMeshes) {
        BenchmarkResult result = benchmarkSubdivision(mesh, config, buildType);
        if (result.avgTime >= 0) {
            results.push_back(result);
        }
    }
    
    // Print and export results
    printResults(results);
    exportResultsCSV(results, "wasmtime_benchmark_" + buildType + ".csv");
    
    return 0;
}