FROM ubuntu:22.04 AS builder

RUN apt-get update && apt-get install -y \
    build-essential cmake git wget libssl-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build
COPY vss_service.cpp schema_vss.obx.cpp schema_vss.obx.hpp CMakeLists.txt ./

RUN cmake -DCMAKE_BUILD_TYPE=Release . && \
    cmake --build . --parallel $(nproc)

FROM ubuntu:22.04

RUN apt-get update && apt-get install -y libssl3 wget \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /build/vss_service /app/
COPY --from=builder /build/_deps/objectbox-sync-download-src/lib/libobjectbox.so /usr/local/lib/
RUN ldconfig

RUN groupadd -r appuser && useradd -r -g appuser appuser \
    && mkdir -p /app/vss-db \
    && chown -R appuser:appuser /app

# Declare volume for persistent database storage (chown must precede VOLUME)
VOLUME ["/app/vss-db"]

USER appuser

EXPOSE 8086

HEALTHCHECK --interval=10s --timeout=5s --retries=3 \
    CMD wget -q --spider http://localhost:8086/health || exit 1

CMD ["./vss_service", "/app/vss-db", "ws://sync-server:9999", "true"]
