"""
Car Manual Voice Assistant
Full pipeline: Voice → Text → Vector Search → Answer Generation → Voice Output
"""

import os
import sys
import time
from typing import List, Tuple
import numpy as np

# Speech Recognition (STT)
import speech_recognition as sr

# ObjectBox
from objectbox import Store, Int32, String, Float32Vector, HnswIndex, VectorDistanceType, Entity, Id

# Ollama for embeddings and LLM
import ollama

# Text-to-Speech (TTS)
import pyttsx3


# Define the same entity as in load_documents.py
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


class VoiceAssistant:
    """Car Manual Voice Assistant with full STT → Search → LLM → TTS pipeline"""
    
    def __init__(self, 
                 db_path: str = "car-manual-db",
                 embedding_model: str = "nub235/voyage-4-nano",
                 llm_model: str = "llama3.2",
                 num_results: int = 3):
        """
        Initialize the voice assistant
        
        Args:
            db_path: Path to ObjectBox database
            embedding_model: Ollama model for embeddings
            llm_model: Ollama model for answer generation
            num_results: Number of similar chunks to retrieve
        """
        self.db_path = db_path
        self.embedding_model = embedding_model
        self.llm_model = llm_model
        self.num_results = num_results
        
        # Initialize components
        print("Initializing Voice Assistant...")
        
        # Check if database exists
        if not os.path.exists(db_path):
            print(f"\n❌ Error: Database not found at '{db_path}'")
            print("Please run 'python load_documents.py' first to load your car manuals.")
            sys.exit(1)
        
        # Initialize ObjectBox
        self.store = Store(directory=db_path)
        self.box = self.store.box(ManualChunk)
        
        # Check if database has data
        count = self.box.count()
        if count == 0:
            print(f"\n❌ Error: Database is empty!")
            print("Please run 'python load_documents.py' to load documents first.")
            sys.exit(1)
        
        print(f"  ✓ Loaded database with {count} manual chunks")
        
        # Initialize Speech Recognition with Whisper small (best offline accuracy)
        self.recognizer = sr.Recognizer()
        print("  ✓ Speech recognition: Whisper small (offline, high accuracy)")
        print("    First use will download 244MB model. Please wait...")
        
        # Initialize Text-to-Speech
        self.tts_engine = pyttsx3.init()
        self.tts_engine.setProperty('rate', 150)  # Speed of speech
        print("  ✓ Text-to-speech initialized")
        
        print(f"  ✓ Using embedding model: {embedding_model}")
        print(f"  ✓ Using LLM model: {llm_model}")
        print("\n✅ Voice Assistant ready!\n")
    
    def listen(self, timeout: int = 5) -> str:
        """
        Listen to microphone and convert speech to text using Whisper small
        
        Args:
            timeout: Maximum seconds to wait for speech
            
        Returns:
            Transcribed text or None if failed
        """
        with sr.Microphone() as source:
            print("🎤 Listening... (speak now)")
            
            # Adjust for ambient noise
            self.recognizer.adjust_for_ambient_noise(source, duration=0.5)
            
            try:
                audio = self.recognizer.listen(source, timeout=timeout, phrase_time_limit=10)
                print("🔄 Processing speech with Whisper small...")
                
                # Use Whisper small for high-accuracy offline speech recognition
                text = self.recognizer.recognize_whisper(
                    audio, 
                    model="small", 
                    language="english"
                )
                
                return text
                
            except sr.WaitTimeoutError:
                print("⏱️  No speech detected")
                return None
            except sr.UnknownValueError:
                print("❓ Could not understand audio")
                return None
            except sr.RequestError as e:
                print(f"❌ Speech recognition error: {e}")
                return None
            except Exception as e:
                print(f"❌ Whisper error: {e}")
                print("💡 First run downloads Whisper small model (244MB). This is normal.")
                return None
    
    def search_manual(self, query: str) -> List[Tuple[ManualChunk, float]]:
        """
        Search the car manual using vector similarity
        
        Args:
            query: The user's question
            
        Returns:
            List of (chunk, score) tuples
        """
        print(f"🔍 Searching manual for: '{query}'")
        
        # Generate embedding for the query
        response = ollama.embeddings(model=self.embedding_model, prompt=query)
        query_embedding = response["embedding"]
        
        # Search using vector similarity
        query_obj = self.box.query(
            ManualChunk.embedding.nearest_neighbor(query_embedding, self.num_results)
        ).build()
        
        results = query_obj.find_with_scores()
        
        print(f"  ✓ Found {len(results)} relevant sections")
        return results
    
    def generate_answer(self, question: str, context_chunks: List[Tuple[ManualChunk, float]]) -> str:
        """
        Generate an answer using the LLM and retrieved context
        
        Args:
            question: The user's question
            context_chunks: Retrieved manual chunks with scores
            
        Returns:
            Generated answer
        """
        print("🤖 Generating answer...")
        
        # Combine relevant chunks into context
        context = "\n\n".join([
            f"[From {chunk.source_file}, section {chunk.chunk_index}]:\n{chunk.text}"
            for chunk, score in context_chunks
        ])
        
        # Create prompt for the LLM
        prompt = f"""You are a helpful car manual assistant. Answer the user's question based on the following information from the car manual. Be concise and clear.

Car Manual Information:
{context}

User Question: {question}

Answer (be specific and cite the manual when relevant):"""
        
        # Generate response using Ollama
        response = ollama.generate(
            model=self.llm_model,
            prompt=prompt
        )
        
        answer = response['response'].strip()
        return answer
    
    def speak(self, text: str):
        """
        Convert text to speech and play it
        
        Args:
            text: Text to speak
        """
        print("🔊 Speaking answer...")
        print(f"\n💬 Answer: {text}\n")
        
        self.tts_engine.say(text)
        self.tts_engine.runAndWait()
    
    def process_question(self, question: str) -> str:
        """
        Full pipeline: search → generate → return answer
        
        Args:
            question: The user's question
            
        Returns:
            The generated answer
        """
        # Search the manual
        results = self.search_manual(question)
        
        if not results:
            return "I couldn't find any relevant information in the manual."
        
        # Generate answer
        answer = self.generate_answer(question, results)
        
        return answer
    
    def run_interactive(self):
        """Run the assistant in interactive voice mode"""
        print("="*70)
        print("🚗 CAR MANUAL VOICE ASSISTANT")
        print("="*70)
        print("\nCommands:")
        print("  - Press Enter and speak your question")
        print("  - Type 'text' to switch to text input mode")
        print("  - Type 'quit' or 'exit' to stop\n")
        
        while True:
            try:
                command = input("Press Enter to speak (or type command): ").strip().lower()
                
                if command in ['quit', 'exit', 'q']:
                    print("\n👋 Goodbye!")
                    break
                
                if command == 'text':
                    # Text input mode
                    question = input("\n📝 Type your question: ").strip()
                    if not question:
                        continue
                elif command == '':
                    # Voice input mode
                    question = self.listen()
                    if not question:
                        continue
                    print(f"📝 You asked: '{question}'")
                else:
                    # Treat as direct text input
                    question = command
                
                print()
                
                # Process the question
                answer = self.process_question(question)
                
                # Speak the answer
                self.speak(answer)
                
                print("-" * 70 + "\n")
                
            except KeyboardInterrupt:
                print("\n\n👋 Goodbye!")
                break
            except Exception as e:
                print(f"\n❌ Error: {e}\n")
                continue
    
    def run_text_only(self):
        """Run the assistant in text-only mode (no voice)"""
        print("="*70)
        print("🚗 CAR MANUAL ASSISTANT (Text Mode)")
        print("="*70)
        print("\nType your questions. Type 'quit' or 'exit' to stop.\n")
        
        while True:
            try:
                question = input("❓ Your question: ").strip()
                
                if question.lower() in ['quit', 'exit', 'q']:
                    print("\n👋 Goodbye!")
                    break
                
                if not question:
                    continue
                
                print()
                
                # Process the question
                answer = self.process_question(question)
                
                # Just print the answer (no voice)
                print(f"\n💬 Answer: {answer}\n")
                print("-" * 70 + "\n")
                
            except KeyboardInterrupt:
                print("\n\n👋 Goodbye!")
                break
            except Exception as e:
                print(f"\n❌ Error: {e}\n")
                continue
    
    def close(self):
        """Clean up resources"""
        self.store.close()


def main():
    """Main entry point"""
    import argparse
    
    parser = argparse.ArgumentParser(description="Car Manual Voice Assistant")
    parser.add_argument("--text-only", action="store_true", 
                       help="Run in text-only mode (no voice input/output)")
    parser.add_argument("--db", default="car-manual-db",
                       help="Path to ObjectBox database (default: car-manual-db)")
    parser.add_argument("--embedding-model", default="nub235/voyage-4-nano",
                       help="Ollama embedding model (default: nub235/voyage-4-nano)")
    parser.add_argument("--llm-model", default="llama3.2",
                       help="Ollama LLM model (default: llama3.2)")
    parser.add_argument("--num-results", type=int, default=3,
                       help="Number of manual chunks to retrieve (default: 3)")
    
    args = parser.parse_args()
    
    # Initialize assistant
    assistant = VoiceAssistant(
        db_path=args.db,
        embedding_model=args.embedding_model,
        llm_model=args.llm_model,
        num_results=args.num_results
    )
    
    try:
        if args.text_only:
            assistant.run_text_only()
        else:
            assistant.run_interactive()
    finally:
        assistant.close()


if __name__ == "__main__":
    main()
