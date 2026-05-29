FROM python:3.12-slim-bookworm
ENV PYTHONUNBUFFERED=1

RUN apt-get update && apt-get upgrade -y && apt-get install -y --no-install-recommends wget && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY requirements.txt .

# CPU-only torch first to avoid pulling the large CUDA wheels
RUN pip install --no-cache-dir torch --index-url https://download.pytorch.org/whl/cpu
RUN pip install --no-cache-dir -r requirements.txt

# Pre-bake the embedding model so the first request is fast
RUN python -c "from sentence_transformers import SentenceTransformer; SentenceTransformer('voyageai/voyage-4-nano')"

COPY . .

EXPOSE 5002

# --timeout 150: kill a worker that hasn't responded in 150 s (covers worst-case LLM latency)
# --workers 2:   allow two concurrent requests (each blocks a thread during the Ollama call)
CMD ["gunicorn", "--bind", "0.0.0.0:5002", "--workers", "1", "--worker-class", "gthread", "--threads", "4", "--preload", "agent_service:app"]
