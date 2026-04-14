# C++ ObjectBox Conversation Service with Sync
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
COPY conversation_service.cpp schema.obx.cpp schema.obx.hpp CMakeLists.txt objectbox-model.json ./

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

# Copy binary and library from builder (sync library includes everything)
COPY --from=builder /build/conversation_service /app/
COPY --from=builder /build/_deps/objectbox-sync-download-src/lib/libobjectbox.so /usr/local/lib/
COPY --from=builder /build/objectbox-model.json /app/

# Update library cache
RUN ldconfig

# Declare volume for persistent database storage
VOLUME ["/app/conversation-db"]

# Expose port
EXPOSE 8081

# Healthcheck
HEALTHCHECK --interval=10s --timeout=5s --retries=3 \
    CMD wget -q --spider http://localhost:8081/health || exit 1

# Run the service
CMD ["./conversation_service", "/app/conversation-db", "ws://sync-server:9999", "true"]
