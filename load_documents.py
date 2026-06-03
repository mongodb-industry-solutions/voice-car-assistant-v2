"""
Document loader — MongoDB Leafy 1.0 car manual.

Chunks the manual on heading boundaries (####, ###, ##, #), embeds each chunk
with sentence-transformers (voyage-4-nano, 1024-dim — must match the model the
agent uses for queries, or search results are garbage), and POSTs the chunks to
the running search-service over HTTP.

Why HTTP instead of writing ObjectBox directly:
    The search-service owns a SYNC_ENABLED ObjectBox store and an active Sync
    client, so the chunks it writes propagate to the Sync Server and on to
    MongoDB Atlas — exactly like the telemetry simulator POSTs snapshots to the
    C++ telemetry service. The Python ObjectBox SDK has no Sync support at all,
    so writing the store directly (the old approach) only ever produced
    local-only rows that never reached the Sync Server.

Run ONCE, AFTER the stack is up and search-service is healthy:
    pip install -r requirements.txt
    docker compose -f sync-server-setup/docker-compose.yml up   # in another shell
    python load_documents.py
"""

import os
import re
import sys
import time
from pathlib import Path
from typing import List

import requests
from sentence_transformers import SentenceTransformer

MANUAL_FILE        = "documents/mongodb_leafy_car_manual.txt"
SEARCH_SERVICE_URL = os.getenv("SEARCH_SERVICE_URL", "http://localhost:8080")
EMBEDDING_MODEL    = os.getenv("EMBEDDING_MODEL", "voyageai/voyage-4-nano")
EMBED_DIM          = 1024
MAX_CHARS          = 900   # upper bound per chunk before character fallback
OVERLAP            = 100   # character overlap on fallback splits
BATCH_SIZE         = 32    # chunks per POST /chunks request


# ── Chunking ──────────────────────────────────────────────────────────────────

def _char_split(text: str, max_chars: int, overlap: int) -> List[str]:
    """Fall-back character split with boundary-aware break points."""
    pieces = []
    start = 0
    while start < len(text):
        end = start + max_chars
        piece = text[start:end]
        if end < len(text):
            for sep in ("\n\n", "\n", ". ", " "):
                pos = piece.rfind(sep)
                if pos > max_chars * 0.6:
                    piece = piece[: pos + len(sep)]
                    break
        piece = piece.strip()
        if piece:
            pieces.append(piece)
        advance = max(len(piece) - overlap, 50)  # always move forward
        start += advance
    return pieces


def chunk_manual(text: str) -> List[str]:
    """
    Split on heading boundaries (####, ###, ##, #) so that each
    'Problem / What to do' block stays in a single chunk.
    Sections that exceed MAX_CHARS fall back to character splitting.
    """
    sections = re.split(r"(?m)(?=^#{1,4} )", text)

    chunks: List[str] = []
    for sec in sections:
        sec = sec.strip()
        if not sec:
            continue
        if len(sec) <= MAX_CHARS:
            chunks.append(sec)
        else:
            chunks.extend(_char_split(sec, MAX_CHARS, OVERLAP))

    return [c for c in chunks if len(c.strip()) > 30]


# ── search-service HTTP helpers ─────────────────────────────────────────────────

def wait_for_service(url: str, attempts: int = 20, delay: float = 3.0) -> None:
    """Block until the search-service /health endpoint responds."""
    for i in range(attempts):
        try:
            resp = requests.get(f"{url}/health", timeout=3)
            if resp.ok:
                print(f"search-service is up: {resp.json()}")
                return
        except requests.RequestException:
            pass
        print(f"  waiting for search-service at {url} … ({i + 1}/{attempts})")
        time.sleep(delay)
    print(f"ERROR: search-service not reachable at {url}. Is `docker compose up` running?")
    sys.exit(1)


def post_batch(url: str, batch: List[dict]) -> dict:
    resp = requests.post(f"{url}/chunks", json={"chunks": batch}, timeout=60)
    resp.raise_for_status()
    return resp.json()


# ── Main ──────────────────────────────────────────────────────────────────────

def main() -> None:
    manual_path = Path(MANUAL_FILE)
    if not manual_path.exists():
        print(f"ERROR: manual not found at {manual_path}")
        sys.exit(1)

    wait_for_service(SEARCH_SERVICE_URL)

    # voyage-4-nano ships custom model code (Qwen3 bidirectional), so it MUST be
    # loaded with trust_remote_code=True — without it, transformers falls back to
    # a stock Qwen3Model and emits a wrong 2048-d vector. It is a Matryoshka
    # model; truncate_dim=1024 selects its native 1024-d head, identical to the
    # Voyage API's output_dimension=1024. Requires transformers==4.57.1 (see
    # requirements.txt). agent_service.py MUST embed queries identically.
    print(f"Loading embedding model: {EMBEDDING_MODEL} @ {EMBED_DIM} dims …")
    model = SentenceTransformer(EMBEDDING_MODEL, trust_remote_code=True, truncate_dim=EMBED_DIM)
    print("Embedding model ready.\n")

    print(f"Reading {manual_path} …")
    text = manual_path.read_text(encoding="utf-8")
    chunks = chunk_manual(text)
    print(f"Created {len(chunks)} chunks from {manual_path.name}\n")

    # NOTE: the search-service has no clear/delete endpoint by design. To reload
    # from scratch, wipe the on-disk store first (see README "Resetting the search
    # index"); otherwise new chunks are appended to whatever already exists.

    written = 0
    batch: List[dict] = []

    def flush() -> None:
        nonlocal written, batch
        if not batch:
            return
        result = post_batch(SEARCH_SERVICE_URL, batch)
        written = result.get("chunk_count", written)
        batch = []

    for idx, chunk in enumerate(chunks):
        try:
            # Stored chunks use the model's "document" prompt; queries use the
            # "query" prompt (defined in the model's config). They must stay split.
            embedding = model.encode(
                chunk, prompt_name="document", normalize_embeddings=True
            ).tolist()
            if len(embedding) != EMBED_DIM:
                print(f"ERROR: got {len(embedding)} dims, expected {EMBED_DIM}. "
                      f"truncate_dim was not honoured by this model — aborting.")
                sys.exit(1)
            batch.append({
                "text":        chunk,
                "source_file": manual_path.name,
                "chunk_index": idx,
                "embedding":   embedding,
            })
            if len(batch) >= BATCH_SIZE:
                flush()
                print(f"  posted {idx + 1}/{len(chunks)} chunks …")
        except Exception as exc:
            print(f"  ERROR on chunk {idx}: {exc}")
    flush()

    print(f"\n{'=' * 60}")
    print(f"Chunks generated : {len(chunks)}")
    print(f"Records in DB    : {written}")
    if written != len(chunks):
        print("WARNING: counts differ — some POSTs may have failed")
    print(f"Search service   : {SEARCH_SERVICE_URL}")
    print(f"{'=' * 60}\n")
    print("Chunks are written via the sync-enabled search-service, so they will "
          "replicate to the Sync Server and MongoDB Atlas.")


if __name__ == "__main__":
    print("\n" + "=" * 60)
    print("MongoDB Leafy 1.0 — document loader (HTTP → sync-enabled search-service)")
    print("=" * 60 + "\n")
    main()
