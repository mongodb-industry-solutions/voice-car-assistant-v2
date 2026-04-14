#!/usr/bin/env python3
"""
Test script for the hybrid Docker architecture
Verifies all components are working correctly
"""

import requests
import sys
import json

def test_search_service():
    """Test the search service is running and responsive"""
    print("\n" + "="*70)
    print("Testing Search Service")
    print("="*70)
    
    # Test health endpoint
    print("\n1. Testing /health endpoint...")
    try:
        response = requests.get("http://localhost:8080/health", timeout=5)
        response.raise_for_status()
        health_data = response.json()
        
        print(f"   ✅ Status: {health_data.get('status')}")
        print(f"   ✅ Chunk count: {health_data.get('chunk_count', 0):,}")
        
        if health_data.get('chunk_count', 0) == 0:
            print("   ⚠️  Warning: No chunks in database!")
            print("      The sync server may still be importing data.")
            return False
            
    except requests.exceptions.ConnectionError:
        print("   ❌ Cannot connect to search service")
        print("      Run: cd sync-server-setup && docker compose up -d")
        return False
    except Exception as e:
        print(f"   ❌ Error: {e}")
        return False
    
    # Test search endpoint with dummy embedding
    print("\n2. Testing /search endpoint...")
    try:
        # Create a dummy 1024-dim embedding
        dummy_embedding = [0.1] * 1024
        
        response = requests.post(
            "http://localhost:8080/search",
            json={"embedding": dummy_embedding, "limit": 3},
            timeout=10
        )
        response.raise_for_status()
        search_data = response.json()
        
        result_count = len(search_data.get('results', []))
        print(f"   ✅ Search returned {result_count} results")
        
        if result_count > 0:
            first_result = search_data['results'][0]
            print(f"   📄 Sample result:")
            print(f"      ID: {first_result.get('id')}")
            print(f"      Text: {first_result.get('text', '')[:100]}...")
            print(f"      Source: {first_result.get('source_file')}")
        
    except Exception as e:
        print(f"   ❌ Search test failed: {e}")
        return False
    
    print("\n   ✅ Search service is working correctly!")
    return True


def test_ollama():
    """Test Ollama is running"""
    print("\n" + "="*70)
    print("Testing Ollama")
    print("="*70)
    
    try:
        import ollama
        
        print("\n1. Checking embedding model...")
        try:
            response = ollama.embeddings(
                model="nub235/voyage-4-nano",
                prompt="test"
            )
            print(f"   ✅ Embedding model working (dim: {len(response['embedding'])})")
        except Exception as e:
            print(f"   ❌ Embedding model not available: {e}")
            print("      Run: ollama pull nub235/voyage-4-nano")
            return False
        
        print("\n2. Checking LLM model...")
        try:
            response = ollama.generate(
                model="llama3.2",
                prompt="Say 'test' and nothing else"
            )
            print(f"   ✅ LLM model working")
        except Exception as e:
            print(f"   ❌ LLM model not available: {e}")
            print("      Run: ollama pull llama3.2")
            return False
            
    except ImportError:
        print("   ❌ Ollama Python package not installed")
        print("      Run: pip install ollama")
        return False
    
    print("\n   ✅ Ollama is configured correctly!")
    return True


def test_voice_assistant():
    """Test voice assistant can import"""
    print("\n" + "="*70)
    print("Testing Voice Assistant")
    print("="*70)
    
    print("\n1. Checking Python dependencies...")
    try:
        import speech_recognition
        import pyttsx3
        import ollama
        import requests
        print("   ✅ All dependencies installed")
    except ImportError as e:
        print(f"   ❌ Missing dependency: {e}")
        print("      Run: pip install -r requirements.txt")
        return False
    
    print("\n2. Checking config...")
    try:
        import config
        print(f"   ✅ Search service URL: {config.SEARCH_SERVICE_URL}")
        print(f"   ✅ Embedding model: {config.EMBEDDING_MODEL}")
        print(f"   ✅ LLM model: {config.LLM_MODEL}")
    except ImportError as e:
        print(f"   ❌ Cannot load config: {e}")
        return False
    
    print("\n   ✅ Voice assistant is ready!")
    return True


def main():
    """Run all tests"""
    print("\n" + "="*70)
    print("HYBRID DOCKER ARCHITECTURE - SYSTEM TEST")
    print("="*70)
    
    all_passed = True
    
    # Test search service
    if not test_search_service():
        all_passed = False
    
    # Test Ollama
    if not test_ollama():
        all_passed = False
    
    # Test voice assistant
    if not test_voice_assistant():
        all_passed = False
    
    # Summary
    print("\n" + "="*70)
    if all_passed:
        print("✅ ALL TESTS PASSED!")
        print("="*70)
        print("\nYou can now run the voice assistant:")
        print("  python main.py --text-only")
        print("  python main.py              (voice mode)")
        return 0
    else:
        print("❌ SOME TESTS FAILED")
        print("="*70)
        print("\nPlease fix the issues above before running the voice assistant.")
        return 1


if __name__ == "__main__":
    sys.exit(main())
