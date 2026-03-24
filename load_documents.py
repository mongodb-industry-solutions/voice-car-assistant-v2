"""
Document Loader for Car Manual Voice Assistant
Loads text documents, chunks them, creates embeddings, and stores in ObjectBox
"""

import os
import sys
from pathlib import Path
from typing import List
import ollama
from objectbox import Entity, Store, Id, String, Int32, Float32Vector, HnswIndex, VectorDistanceType


@Entity()
class ManualChunk:
    """Entity to store car manual text chunks with embeddings"""
    id = Id()
    text = String()
    source_file = String()
    chunk_index = Int32()
    embedding = Float32Vector(index=HnswIndex(
        dimensions=512,
        distance_type=VectorDistanceType.COSINE
    ))


def chunk_text(text: str, chunk_size: int = 500, overlap: int = 50) -> List[str]:
    """
    Split text into overlapping chunks for better context preservation
    
    Args:
        text: The text to chunk
        chunk_size: Maximum characters per chunk
        overlap: Number of characters to overlap between chunks
    
    Returns:
        List of text chunks
    """
    chunks = []
    start = 0
    
    while start < len(text):
        end = start + chunk_size
        chunk = text[start:end]
        
        # Try to break at sentence or word boundary
        if end < len(text):
            last_period = chunk.rfind('.')
            last_newline = chunk.rfind('\n')
            last_space = chunk.rfind(' ')
            
            break_point = max(last_period, last_newline, last_space)
            if break_point > chunk_size * 0.7:  # Only break if we're not losing too much
                chunk = chunk[:break_point + 1]
                end = start + len(chunk)
        
        chunks.append(chunk.strip())
        start = end - overlap
    
    return chunks


def load_text_file(file_path: str) -> str:
    """Load text from a file"""
    with open(file_path, 'r', encoding='utf-8') as f:
        return f.read()


def load_documents(documents_dir: str, embedding_model: str = "nub235/voyage-4-nano"):
    """
    Load documents from a directory, chunk them, create embeddings, and store in ObjectBox
    
    Args:
        documents_dir: Path to directory containing text documents
        embedding_model: Ollama model to use for embeddings
    """
    # Initialize ObjectBox store
    db_path = "car-manual-db"
    if os.path.exists(db_path):
        print(f"Removing existing database at {db_path}")
        Store.remove_db_files(db_path)
    
    store = Store(directory=db_path)
    box = store.box(ManualChunk)
    
    # Check if documents directory exists
    if not os.path.exists(documents_dir):
        print(f"Error: Documents directory '{documents_dir}' not found!")
        print(f"Please create the directory and add your car manual text files (.txt)")
        os.makedirs(documents_dir, exist_ok=True)
        return
    
    # Find all text files
    text_files = list(Path(documents_dir).glob("*.txt"))
    
    if not text_files:
        print(f"No .txt files found in {documents_dir}")
        print("Please add your car manual documents as .txt files")
        return
    
    print(f"\nFound {len(text_files)} document(s) to process")
    print(f"Using embedding model: {embedding_model}\n")
    
    total_chunks = 0
    
    # Process each document
    for file_path in text_files:
        print(f"Processing: {file_path.name}")
        
        # Load and chunk the document
        text = load_text_file(str(file_path))
        chunks = chunk_text(text)
        
        print(f"  - Created {len(chunks)} chunks")
        
        # Create embeddings and store each chunk
        for idx, chunk in enumerate(chunks):
            try:
                # Generate embedding using Ollama
                response = ollama.embeddings(model=embedding_model, prompt=chunk)
                embedding = response["embedding"]
                
                # Store in ObjectBox
                manual_chunk = ManualChunk()
                manual_chunk.text = chunk
                manual_chunk.source_file = file_path.name
                manual_chunk.chunk_index = idx
                manual_chunk.embedding = embedding
                
                box.put(manual_chunk)
                total_chunks += 1
                
                if (idx + 1) % 10 == 0:
                    print(f"  - Embedded {idx + 1}/{len(chunks)} chunks")
            
            except Exception as e:
                print(f"  - Error processing chunk {idx}: {e}")
                continue
        
        print(f"  ✓ Completed {file_path.name}\n")
    
    print(f"\n{'='*60}")
    print(f"Successfully loaded {total_chunks} chunks from {len(text_files)} document(s)")
    print(f"Database saved to: {db_path}")
    print(f"{'='*60}\n")
    
    store.close()


if __name__ == "__main__":
    # Default documents directory
    docs_dir = "documents"
    
    # Allow custom directory via command line
    if len(sys.argv) > 1:
        docs_dir = sys.argv[1]
    
    print("\n" + "="*60)
    print("Car Manual Document Loader")
    print("="*60)
    
    load_documents(docs_dir)
    
    print("Done! You can now run the voice assistant (main.py)")
