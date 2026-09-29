# CV Pipeline Profiler

A production-grade observability framework for real-time computer vision pipelines.
Profile, benchmark, and debug your CV system stage by stage — without guessing
where the bottleneck is.

---

## What it does

Wraps any CV pipeline and automatically measures:

- Per-stage processing latency (p50 / p95 / p99)
- Queue wait time between stages
- End-to-end frame latency
- Frame drop rate
- Bottleneck identification (which stage is slowest)

Outputs a live terminal dashboard **and** structured JSON logs for offline analysis.

---

## Two integration modes

### Mode 1 - Pipeline engine owns execution

Implement `IStage` and register your stages. The engine runs your loop.

```cpp
engine.add_stage(std::make_shared<CaptureStage>("video.mp4"));
engine.add_stage(std::make_shared<MyPreprocessStage>());
engine.add_stage(std::make_shared<MyInferenceStage>("model.onnx"));
engine.run(); // metrics collected automatically
```

Use when: building a new pipeline, or refactoring an existing one.

### Mode 2 — Probe injection (zero restructuring)

Add `profiler.begin()` / `profiler.end()` around any section of your existing code.
Your main loop is unchanged.

```cpp
// Your existing loop — add only these 8 lines:
profiler.begin("preprocess");
your_existing_preprocess(frame);
profiler.end("preprocess");

profiler.begin("inference");
your_existing_inference(frame);
profiler.end("inference");
```

Use when: you have an existing codebase and want immediate profiling without refactoring.

---

## Quick start — local build

**Prerequisites:** CMake ≥ 3.18, OpenCV 4.x, C++17 compiler

```bash
# macOS
brew install cmake opencv

# Ubuntu / Debian
sudo apt install cmake libopencv-dev

# Build
./scripts/build.sh

# Run (no camera needed)
./build/cvprofiler_demo1 --synthetic   # Mode 1
./build/cvprofiler_demo2               # Mode 2

# Run with a video file
./build/cvprofiler_demo1 /path/to/video.mp4
```

---

## Quick start — Docker (any OS)

```bash
# Build image
docker build -t cvprofiler:latest -f docker/Dockerfile .

# Run synthetic demo (no camera needed)
docker run --rm cvprofiler:latest

# Run with a video file and capture logs
docker run --rm \
    -v $(pwd)/logs:/app/logs \
    -v $(pwd)/data:/data:ro \
    cvprofiler:latest ./cvprofiler_demo1 /data/video.mp4

# Docker Compose
docker compose -f docker/docker-compose.yml up demo1
```

---

## Run tests

```bash
./scripts/build.sh test
# or
cd build && ctest --output-on-failure
```

---

## Output example

```
  ------------------------------------------------------------------------------------------
  CV PIPELINE PROFILER   frame=   150   fps=28.4   e2e=56.3ms (mean)
  ------------------------------------------------------------------------------------------
    stage              mean(ms)   p50(ms)   p95(ms)   p99(ms)   q_wait   drops
  ------------------------------------------------------------------------------------------
    capture               1.24      1.18      2.10      3.40      0.00       0
    preprocess            4.87      4.71      6.20      7.10      0.00       0
  ▶ inference            22.14     20.88     38.10     95.40      0.00       0  ← BOTTLENECK
    postprocess           0.83      0.79      1.20      1.80      0.00       0
  ------------------------------------------------------------------------------------------
  Latency share (p95): captur[##                            ]  4%   prepro[######                        ] 12%   infere[####################          ] 72%
```

---

## Project structure

```
cv-pipeline-profiler/
├── core/
│   ├── frame.hpp            # Frame struct with timing fields
│   ├── stage.hpp            # IStage interface
│   └── pipeline_engine.hpp  # Single-threaded engine (Phase 1)
├── stages/
│   ├── capture_stage.hpp    # OpenCV VideoCapture wrapper
│   ├── preprocess_stage.hpp # Resize + normalize
│   ├── synthetic_inference_stage.hpp  # Configurable load sim
│   └── postprocess_stage.hpp
├── metrics/
│   └── metrics_collector.hpp  # Latency buffers + percentiles
├── reporting/
│   ├── console_reporter.hpp   # Live terminal table
│   └── structured_logger.hpp  # JSON lines output
├── app/
│   ├── demo_mode1.cpp         # Mode 1 demo
│   └── demo_mode2.cpp         # Mode 2 demo
├── tests/
│   ├── test_frame.cpp
│   ├── test_metrics.cpp
│   └── test_pipeline.cpp
├── docker/
│   ├── Dockerfile
│   └── docker-compose.yml
├── scripts/
│   └── build.sh
└── CMakeLists.txt
```

---

## Roadmap

| Phase | Status | Description |
|-------|--------|-------------|
| 1 | ✅ | Single-threaded engine, basic timing, console reporter, JSON logs |
| 2 | planned | YAML config, stage factory, modular build |
| 3 | planned | Multi-threaded stages, bounded queues, backpressure |
| 4 | planned | Live TUI dashboard, experiment runner |
| 5 | planned | GPU preprocessing, CUDA streams, TensorRT |
| 6 | planned | WebSocket metrics server, React dashboard, Docker packaging |

---

## Note

LLM tools were used to assist with code readability and comment/documentation improvements.


## License

MIT
