# Use specific version tag for reproducibility and security patches
FROM python:3.11.9-slim-bookworm

# Create non-root user for security
RUN groupadd -r simulator && useradd -r -g simulator simulator

WORKDIR /app

# Install dependencies as root
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt && \
    # Remove pip cache to reduce image size and attack surface
    rm -rf /root/.cache/pip

# Copy application files and set ownership to non-root user
COPY --chown=simulator:simulator simulator.py telemetry_generator.py thresholds.py ./

# Switch to non-root user
USER simulator

# Expose port
EXPOSE 8082

# Add security labels
LABEL org.opencontainers.image.description="Telemetry Simulator Service"
LABEL org.opencontainers.image.vendor="ObjectBox"
LABEL security.non-root="true"

# Health check with proper timeout
HEALTHCHECK --interval=30s --timeout=5s --start-period=10s --retries=3 \
    CMD python -c "import requests; requests.get('http://localhost:8082/health', timeout=3)" || exit 1

# Run the simulator as non-root user
CMD ["python", "-u", "simulator.py"]
