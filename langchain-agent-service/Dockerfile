FROM python:3.12-slim-bookworm
ENV PYTHONUNBUFFERED=1 \
    HF_HOME=/app/.cache/huggingface

RUN apt-get update && apt-get upgrade -y && apt-get install -y --no-install-recommends wget && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY requirements.txt .

# CPU-only torch first to avoid pulling the large CUDA wheels
RUN pip install --no-cache-dir torch --index-url https://download.pytorch.org/whl/cpu
RUN pip install --no-cache-dir -r requirements.txt

# Pre-bake the embedding model into /app/.cache/huggingface (owned by appuser below)
RUN python -c "from sentence_transformers import SentenceTransformer; SentenceTransformer('voyageai/voyage-4-nano')"

COPY . .

RUN groupadd -r appuser && useradd -r -g appuser -d /app appuser \
    && chown -R appuser:appuser /app
USER appuser

EXPOSE 5002

CMD ["gunicorn", "--bind", "0.0.0.0:5002", "--workers", "1", "--worker-class", "gthread", "--threads", "4", "--preload", "agent_service:app"]
