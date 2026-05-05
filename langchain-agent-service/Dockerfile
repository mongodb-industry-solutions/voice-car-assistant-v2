FROM python:3.11-slim
ENV PYTHONUNBUFFERED=1

RUN apt-get update && apt-get install -y --no-install-recommends wget && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY requirements.txt .

# CPU-only torch first to avoid pulling the large CUDA wheels
RUN pip install --no-cache-dir torch --index-url https://download.pytorch.org/whl/cpu
RUN pip install --no-cache-dir -r requirements.txt

# Pre-bake the embedding model so the first request is fast
RUN python -c "from sentence_transformers import SentenceTransformer; SentenceTransformer('voyageai/voyage-4-nano')"

COPY . .

EXPOSE 5002

CMD ["python", "agent_service.py"]
