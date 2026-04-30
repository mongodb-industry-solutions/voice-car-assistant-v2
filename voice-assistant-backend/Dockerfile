FROM python:3.11-slim

# System dependencies:
#   portaudio19-dev + gcc  → build PyAudio
#   espeak-ng              → pyttsx3 TTS engine on Linux
#   ffmpeg                 → Whisper audio processing
RUN apt-get update && apt-get install -y --no-install-recommends \
    portaudio19-dev \
    espeak-ng \
    libsndfile1 \
    ffmpeg \
    gcc \
    g++ \
    wget \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY requirements.txt .

# Install CPU-only PyTorch before the rest so openai-whisper doesn't pull
# the CUDA-enabled wheels (saves ~1.5 GB on the image).
RUN pip install --no-cache-dir torch torchaudio \
    --index-url https://download.pytorch.org/whl/cpu

RUN pip install --no-cache-dir -r requirements.txt

# Pre-bake the Whisper "small" model so the first container start is fast.
RUN python -c "import whisper; whisper.load_model('small')"

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

EXPOSE 5000

# NOTE: Voice input (microphone) and TTS audio output require the host's
# audio devices to be passed through to the container, which is not
# supported on Windows Docker Desktop. The web UI, map navigation, and
# telemetry dashboard all work without audio.
CMD ["python", "web_server.py"]
