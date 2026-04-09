"""
ObjectBox Search Service with Sync
Python implementation matching automotive demo architecture
"""

from objectbox import Entity, Id, String, Int32, Int64, Float32Vector, HnswIndex, Store, SyncCredentials, VectorDistanceType
from flask import Flask, request, jsonify
import time
import os

# Entity definitions matching sync-server schema
@Entity()
class ManualChunk:
    id = Id()
    text = String()
    source_file = String()
    chunk_index = Int32()
    embedding = Float32Vector(index=HnswIndex(
        dimensions=1024,
        distance_type=VectorDistanceType.COSINE
    ))
    sync_clock = Int64()


@Entity()
class Manual:
    id = Id()
    filename = String()
    make = String()
    model = String()
    total_chunks = Int32()
    status = String()
    sync_clock = Int64()


class SearchService:
    def __init__(self, db_path: str, sync_url: str = None, enable_sync: bool = True):
        print("=== ObjectBox Search Service (Python) ===")
        print(f"Database: {db_path}")
        
        # Create ObjectBox store
        self.store = Store(directory=db_path)
        self.chunk_box = self.store.box(ManualChunk)
        
        print(f"✓ Store opened ({self.chunk_box.count()} chunks)")
        
        # Initialize sync if enabled
        if enable_sync and sync_url:
            print(f"Connecting to sync server: {sync_url}")
            try:
                self.sync_client = self.store.sync_client(
                    server_url=sync_url,
                    credentials=SyncCredentials.none()
                )
                self.sync_client.start()
                print("✓ Sync client started")
                
                # Wait for initial sync
                print("Waiting for initial sync (5 seconds)...")
                time.sleep(5)
                print(f"✓ Initial sync complete ({self.chunk_box.count()} chunks)")
            except Exception as e:
                print(f"Warning: Sync failed - {e}")
                self.sync_client = None
        else:
            self.sync_client = None
            print("Sync disabled")
        
        print("=== Service Ready ===")
    
    def search(self, embedding: list, limit: int = 3):
        """
        Perform vector search for nearest neighbors
        """
        if len(embedding) != 1024:
            raise ValueError(f"Embedding must be 1024 dimensions, got {len(embedding)}")
        
        print(f"Searching with limit={limit}")
        
        # Build query with vector search
        query = self.chunk_box.query(
            ManualChunk.embedding.nearest_neighbor(embedding, limit)
        ).build()
        
        # Execute search and get results with scores
        results_with_scores = query.find_with_scores()
        
        print(f"Found {len(results_with_scores)} results")
        
        # Convert to JSON-serializable format
        results = []
        for chunk, score in results_with_scores:
            results.append({
                "id": chunk.id,
                "text": chunk.text,
                "source_file": chunk.source_file,
                "chunk_index": chunk.chunk_index,
                "score": float(score)
            })
        
        return results
    
    def get_chunk_count(self):
        return self.chunk_box.count()
    
    def close(self):
        if self.sync_client:
            self.sync_client.stop()
        self.store.close()


# Flask app
app = Flask(__name__)
service = None


@app.route('/health', methods=['GET'])
def health():
    return jsonify({
        "status": "healthy",
        "chunk_count": service.get_chunk_count()
    })


@app.route('/search', methods=['POST'])
def search():
    try:
        data = request.get_json()
        
        if 'embedding' not in data:
            return jsonify({"error": "Missing 'embedding' field"}), 400
        
        embedding = data['embedding']
        limit = data.get('limit', 3)
        
        if not isinstance(embedding, list):
            return jsonify({"error": "'embedding' must be a list"}), 400
        
        if len(embedding) != 1024:
            return jsonify({"error": "Embedding must be 1024 dimensions"}), 400
        
        results = service.search(embedding, limit)
        
        return jsonify({
            "count": len(results),
            "results": results
        })
    
    except Exception as e:
        print(f"Search error: {e}")
        return jsonify({"error": str(e)}), 500


if __name__ == "__main__":
    import sys
    
    # Get configuration from args
    db_path = sys.argv[1] if len(sys.argv) > 1 else "search-service-db"
    sync_url = sys.argv[2] if len(sys.argv) > 2 else "ws://sync-server:9999"
    enable_sync = sys.argv[3].lower() == "true" if len(sys.argv) > 3 else True
    
    print(f"\nConfig:")
    print(f"  Database: {db_path}")
    print(f"  Sync URL: {sync_url if enable_sync else 'disabled'}")
    print()
    
    # Initialize service
    service = SearchService(db_path, sync_url, enable_sync)
    
    # Start Flask server
    print("Starting HTTP server on 0.0.0.0:8080...")
    app.run(host="0.0.0.0", port=8080, debug=False)
