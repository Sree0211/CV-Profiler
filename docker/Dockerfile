# CV Pipeline Profiler — Multi-stage Dockerfile
#
# Stage 1 (builder): Ubuntu 22.04 + OpenCV + CMake + build tools
# Stage 2 (runtime): minimal image with only runtime libs + binaries
#
# Build:
#   docker build -t cvprofiler:latest .
#
# Run Mode 1 (synthetic, no camera):
#   docker run --rm cvprofiler:latest ./cvprofiler_demo1 --synthetic
#
# Run Mode 2 (probe injection demo):
#   docker run --rm cvprofiler:latest ./cvprofiler_demo2
#
# Run with a video file (mount host directory):
#   docker run --rm -v /path/to/videos:/data cvprofiler:latest \
#       ./cvprofiler_demo1 /data/myvideo.mp4
#
# Run with a webcam (Linux only):
#   docker run --rm --device=/dev/video0 cvprofiler:latest \
#       ./cvprofiler_demo1
#
# Access logs:
#   docker run --rm -v $(pwd)/logs:/app/logs cvprofiler:latest \
#       ./cvprofiler_demo1 --synthetic


# Stage 1: Builder
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=UTC

# System deps
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    git \
    pkg-config \
    # OpenCV runtime deps
    libopencv-dev \
    # Needed by OpenCV video I/O
    libv4l-dev \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    # Nice to have for debugging inside container
    gdb \
    && rm -rf /var/lib/apt/lists/*

# Copy source
WORKDIR /src
COPY . .

# Build
RUN cmake -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/install \
    && cmake --build build --parallel $(nproc) \
    && cmake --install build

# Stage 2: Runtime
# Smaller image — only runtime libraries, not build tools
FROM ubuntu:22.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=UTC

# Only runtime OpenCV libs
RUN apt-get update && apt-get install -y --no-install-recommends \
    libopencv-core4.5d \
    libopencv-imgproc4.5d \
    libopencv-videoio4.5d \
    libopencv-highgui4.5d \
    && rm -rf /var/lib/apt/lists/*

# Copy installed binaries from builder
COPY --from=builder /install/bin /app
WORKDIR /app

# Create log directory (can be mounted as a volume)
RUN mkdir -p logs

# Default: run the synthetic demo (no camera required)
CMD ["./cvprofiler_demo1", "--synthetic"]
