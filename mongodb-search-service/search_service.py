"""
MongoDB Atlas Vector Search Service
Provides POST /search and GET /health endpoints compatible with the ObjectBox search-service API.
Uses $vectorSearch aggregation on the manual_chunks collection with voyage-4-nano embeddings.
"""

import os
from flask import Flask, jsonify, request
from flask_cors import CORS
from pymongo import MongoClient
from pymongo.errors import PyMongoError

app = Flask(__name__)
CORS(app)

MONGODB_URI    = os.getenv("MONGODB_URI", "")
DATABASE_NAME  = os.getenv("DATABASE_NAME", "")
COLLECTION     = "manual_chunks"
INDEX_NAME     = os.getenv("VECTOR_INDEX_NAME", "manual_chunks_vector_index")
EMBEDDING_FIELD = "embedding"

_client: MongoClient = None
_collection = None


def _get_collection():
    global _client, _collection
    if _collection is None:
        _client = MongoClient(MONGODB_URI)
        _collection = _client[DATABASE_NAME][COLLECTION]
    return _collection


@app.route("/health")
def health():
    try:
        col = _get_collection()
        count = col.estimated_document_count()
        return jsonify({
            "status": "healthy",
            "service": "mongodb-search-service",
            "chunk_count": count,
            "database": DATABASE_NAME,
            "collection": COLLECTION,
        })
    except Exception as e:
        return jsonify({
            "status": "unhealthy",
            "service": "mongodb-search-service",
            "error": str(e),
            "chunk_count": 0,
        }), 503


@app.route("/search", methods=["POST"])
def search():
    data = request.json or {}
    embedding = data.get("embedding")
    limit = int(data.get("limit", 3))

    if not embedding:
        return jsonify({"error": "embedding is required"}), 400

    try:
        col = _get_collection()
        pipeline = [
            {
                "$vectorSearch": {
                    "index": INDEX_NAME,
                    "path": EMBEDDING_FIELD,
                    "queryVector": embedding,
                    "numCandidates": limit * 10,
                    "limit": limit,
                }
            },
            {
                "$project": {
                    "_id": 0,
                    "text": 1,
                    "source_file": 1,
                    "score": {"$meta": "vectorSearchScore"},
                }
            },
        ]
        docs = list(col.aggregate(pipeline))
        results = [{"text": d.get("text", ""), "score": d.get("score", 0.0)} for d in docs]
        return jsonify({"results": results})
    except PyMongoError as e:
        return jsonify({"error": f"MongoDB error: {e}"}), 503
    except Exception as e:
        return jsonify({"error": str(e)}), 500


if __name__ == "__main__":
    port = int(os.getenv("PORT", 8085))
    print(f"MongoDB Atlas Search Service starting on port {port}")
    print(f"  Database:   {DATABASE_NAME}")
    print(f"  Collection: {COLLECTION}")
    print(f"  Index:      {INDEX_NAME}")
    app.run(host="0.0.0.0", port=port, debug=False)
