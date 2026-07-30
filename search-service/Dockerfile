# C++ ObjectBox Search Service with Sync
FROM ubuntu:22.04 AS builder

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    wget \
    libssl-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build

# Copy source files including schema
COPY search_service.cpp schema.obx.cpp schema.obx.hpp CMakeLists.txt ./

# Build
RUN cmake -DCMAKE_BUILD_TYPE=Release . && \
    cmake --build . --parallel $(nproc)

# Runtime stage
FROM ubuntu:22.04

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libssl3 \
    wget \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy binary and library from builder
COPY --from=builder /build/search_service /app/
COPY --from=builder /build/_deps/objectbox-sync-download-src/lib/libobjectbox.so /usr/local/lib/

# Update library cache
RUN ldconfig

RUN groupadd -r appuser && useradd -r -g appuser appuser \
    && mkdir -p /app/search-service-db \
    && chown -R appuser:appuser /app

# Declare volume for persistent database storage (chown must precede VOLUME)
VOLUME ["/app/search-service-db"]

USER appuser

# Expose port
EXPOSE 8080

# Healthcheck
HEALTHCHECK --interval=10s --timeout=5s --retries=3 \
    CMD wget -q --spider http://localhost:8080/health || exit 1

# Run the service
CMD ["./search_service", "/app/search-service-db", "ws://sync-server:9999", "true"]
