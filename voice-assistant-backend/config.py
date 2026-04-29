"""
Configuration for Voice Assistant
"""
import os
from pathlib import Path

# Paths
BASE_DIR = Path(__file__).parent
SYNC_SERVER_DIR = BASE_DIR.parent / "sync-server-setup"
OBJECTBOX_DB_PATH = str(SYNC_SERVER_DIR / "objectbox")

# Search Service
SEARCH_SERVICE_URL = os.getenv("SEARCH_SERVICE_URL", "http://localhost:8080")

# AI Models
EMBEDDING_MODEL = "nub235/voyage-4-nano"  # 1024-dimensional embeddings
LLM_MODEL = "llama3.1:8b"

# Speech Recognition
WHISPER_MODEL = "small"  # Options: tiny, base, small, medium, large
WHISPER_LANGUAGE = "en"

# Voice Assistant Settings
NUM_SEARCH_RESULTS = 3  # Number of manual chunks to retrieve for context
CHUNK_OVERLAP_DISPLAY = 50  # Characters to show overlap between chunks

# Database Info
print(f"📁 Using ObjectBox database at: {OBJECTBOX_DB_PATH}")
