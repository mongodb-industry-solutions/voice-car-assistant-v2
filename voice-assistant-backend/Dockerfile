FROM python:3.11-slim-bookworm

# System dependencies:
#   ffmpeg     → Whisper audio decoding (accepts browser webm/ogg)
#   libsndfile → soundfile
#   wget       → fetch the Piper binary + voice model at build time
RUN apt-get update && apt-get upgrade -y && apt-get install -y --no-install-recommends \
    libsndfile1 \
    ffmpeg \
    wget \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY requirements.txt .

# Install CPU-only PyTorch before the rest so openai-whisper doesn't pull
# the CUDA-enabled wheels (saves ~1.5 GB on the image).
RUN pip install --no-cache-dir torch torchaudio \
    --index-url https://download.pytorch.org/whl/cpu

RUN pip install --no-cache-dir -r requirements.txt

# Pre-bake the Whisper "small" model into /app/.cache/whisper so the
# non-root user can read it at runtime (HOME=/app at both build and run time).
RUN HOME=/app python -c "import whisper; whisper.load_model('small')"

# Piper TTS — self-contained binary (bundles its own ONNX runtime).
RUN mkdir -p /opt/piper \
 && wget -q -O /tmp/piper.tar.gz \
    https://github.com/rhasspy/piper/releases/download/2023.11.14-2/piper_linux_x86_64.tar.gz \
 && tar -xzf /tmp/piper.tar.gz -C /opt/piper --strip-components=1 \
 && rm /tmp/piper.tar.gz

# Piper voice model (en_US lessac — natural-sounding, 22 kHz).
RUN wget -q -O /app/en_US-lessac-medium.onnx \
    https://huggingface.co/rhasspy/piper-voices/resolve/main/en/en_US/lessac/medium/en_US-lessac-medium.onnx \
 && wget -q -O /app/en_US-lessac-medium.onnx.json \
    https://huggingface.co/rhasspy/piper-voices/resolve/main/en/en_US/lessac/medium/en_US-lessac-medium.onnx.json

COPY . .

RUN groupadd -r appuser && useradd -r -g appuser -d /app appuser \
    && chown -R appuser:appuser /app /opt/piper
ENV HOME=/app
USER appuser

EXPOSE 8080

# STT (Whisper) transcribes browser-uploaded audio; TTS (Piper) synthesizes
# answers server-side. No host audio devices needed — the mic lives in the browser.
CMD ["python", "app.py"]
