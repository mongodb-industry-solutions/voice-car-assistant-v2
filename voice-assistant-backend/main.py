"""
Car Manual Voice Assistant - Offline-First with Cloud Sync
Uses ObjectBox Sync Server via REST API search service
Automatically syncs with MongoDB Atlas when online
"""

import os
import sys
import ollama
import speech_recognition as sr
import pyttsx3
import requests
from pathlib import Path
from typing import List, Optional, Dict, Any

# Local config
import config


class VoiceAssistant:
    """Voice-enabled car manual assistant with offline-first architecture"""
    
    def __init__(
        self,
        search_service_url: str = config.SEARCH_SERVICE_URL,
        embedding_model: str = config.EMBEDDING_MODEL,
        llm_model: str = config.LLM_MODEL,
        num_results: int = config.NUM_SEARCH_RESULTS
    ):
        """
        Initialize the voice assistant
        
        Args:
            search_service_url: URL of the search service (e.g., http://localhost:8080)
            embedding_model: Ollama model for embeddings
            llm_model: Ollama model for answer generation
            num_results: Number of manual chunks to retrieve for context
        """
        self.search_service_url = search_service_url
        self.embedding_model = embedding_model
        self.llm_model = llm_model
        self.num_results = num_results
        
        print("🚗 Initializing Car Manual Voice Assistant...")
        
        # Check if search service is running
        print(f"🔌 Connecting to search service: {search_service_url}")
        
        try:
            response = requests.get(f"{search_service_url}/health", timeout=5)
            response.raise_for_status()
            health_data = response.json()
            
            print(f"✅ Connected to search service")
            print(f"📚 Manual chunks available: {health_data.get('chunk_count', 0):,}")
            
            if health_data.get('chunk_count', 0) == 0:
                print("\n⚠️  Warning: No manual chunks found in database!")
                print("The sync server may still be importing data from MongoDB.")
                print("Check sync server logs: cd sync-server-setup && docker compose logs -f")
            
        except requests.exceptions.ConnectionError:
            print(f"\n❌ Error: Cannot connect to search service at {search_service_url}")
            print("\nPlease ensure the services are running:")
            print("  cd sync-server-setup && docker compose up -d")
            sys.exit(1)
        except Exception as e:
            print(f"\n❌ Error connecting to search service: {e}")
            sys.exit(1)
        
        # Initialize speech recognition
        self.recognizer = sr.Recognizer()
        
        # Don't initialize TTS engine here - we'll create it fresh each time
        # to avoid Windows pyttsx3 issues where engine gets stuck after first use
        self.tts_rate = 175  # Speaking speed
        
        print("✅ Voice assistant ready!\n")
    
    def create_embedding(self, text: str) -> List[float]:
        """Generate embedding for text using Ollama"""
        try:
            response = ollama.embeddings(model=self.embedding_model, prompt=text)
            return response['embedding']
        except Exception as e:
            print(f"❌ Error creating embedding: {e}")
            raise
    
    def search_manual(self, query: str) -> List[Dict[str, Any]]:
        """
        Search car manual using vector similarity via REST API
        
        Args:
            query: User's question
            
        Returns:
            List of relevant manual chunks
        """
        print(f"🔍 Searching manual for: '{query}'")
        
        # Create embedding for the query
        query_embedding = self.create_embedding(query)
        
        # Call search service REST API
        try:
            response = requests.post(
                f"{self.search_service_url}/search",
                json={
                    "embedding": query_embedding,
                    "limit": self.num_results
                },
                timeout=10
            )
            response.raise_for_status()
            result_data = response.json()
            
            results = result_data.get('results', [])
            print(f"📄 Found {len(results)} relevant sections")
            return results
            
        except Exception as e:
            print(f"❌ Error searching manual: {e}")
            return []
    
    def generate_answer(self, question: str, context_chunks: List[Dict[str, Any]]) -> str:
        """
        Generate answer using LLM based on manual chunks
        
        Args:
            question: User's question
            context_chunks: Relevant manual chunks
            
        Returns:
            Generated answer
        """
        # Build context from chunks (without section references)
        context = "\n\n".join([
            chunk['text']
            for chunk in context_chunks
        ])
        
        # Create prompt with professional instructions
        prompt = f"""You are a professional automotive assistant with deep knowledge of vehicle systems and maintenance. Your role is to provide accurate, clear information based on the vehicle manual.

Relevant Manual Information:
{context}

Customer Question: {question}

Guidelines for your response:
- Provide a direct, professional answer based on the manual information above
- Be clear and concise - avoid unnecessary jargon unless the customer asks technical questions
- Use a friendly but professional tone, as if you're an experienced service advisor
- Present information naturally without referencing "sections", "chunks", or "source files"
- If the manual doesn't contain the answer, politely say so and suggest what the customer could do instead
- Focus on practical, actionable information the customer can use
- Never mention that you're looking at a manual or specific sections - just answer the question directly

Your Answer:"""
        
        print("🤖 Generating answer...")
        
        # Generate response using Ollama
        try:
            response = ollama.generate(model=self.llm_model, prompt=prompt)
            return response['response'].strip()
        except Exception as e:
            return f"Error generating answer: {e}"
    
    def speak(self, text: str):
        """Convert text to speech - creates fresh engine each time to avoid Windows issues"""
        try:
            # Create a fresh engine for each speech operation
            # This avoids pyttsx3 getting stuck on Windows after first use
            engine = pyttsx3.init()
            engine.setProperty('rate', self.tts_rate)
            
            # Speak the text
            engine.say(text)
            engine.runAndWait()
            
            # Clean up
            engine.stop()
            del engine
            
        except Exception as e:
            print(f"⚠️  TTS error: {e}")
    
    def listen(self) -> Optional[str]:
        """Listen for voice input and convert to text"""
        with sr.Microphone() as source:
            print("🎤 Listening... (speak your question)")
            
            try:
                # Adjust for ambient noise
                self.recognizer.adjust_for_ambient_noise(source, duration=0.5)
                
                # Listen for audio
                audio = self.recognizer.listen(source, timeout=5, phrase_time_limit=10)
                
                print("🔄 Processing speech...")
                
                # Convert speech to text using Whisper
                text = self.recognizer.recognize_whisper(
                    audio,
                    model=config.WHISPER_MODEL,
                    language=config.WHISPER_LANGUAGE
                )
                
                return text
                
            except sr.WaitTimeoutError:
                print("⏱️  No speech detected")
                return None
            except sr.UnknownValueError:
                print("❓ Could not understand audio")
                return None
            except Exception as e:
                print(f"❌ Error: {e}")
                return None
    
    def process_question(self, question: str) -> str:
        """
        Process a question and return an answer
        
        Args:
            question: User's question
            
        Returns:
            Answer from the manual
        """
        # Search for relevant manual chunks
        chunks = self.search_manual(question)
        
        if not chunks:
            return "I couldn't find relevant information in the car manual for your question."
        
        # Generate answer using LLM
        answer = self.generate_answer(question, chunks)
        
        return answer
    
    def run_interactive(self):
        """Run in interactive voice mode"""
        print("=" * 70)
        print("🎙️  VOICE MODE - Car Manual Assistant")
        print("=" * 70)
        print("\nHow to use:")
        print("  • Speak your question when prompted")
        print("  • Press Ctrl+C to exit")
        print("\n" + "=" * 70 + "\n")
        
        while True:
            try:
                # Listen for question
                question = self.listen()
                
                if not question:
                    continue
                
                print(f"\n❓ You asked: {question}\n")
                
                # Process the question
                answer = self.process_question(question)
                
                # Display answer
                print(f"💬 Answer: {answer}\n")
                
                # Speak the answer
                print("🔊 Speaking answer...")
                self.speak(answer)
                
                print("\n" + "-" * 70 + "\n")
                
            except KeyboardInterrupt:
                print("\n\n👋 Goodbye!")
                break
            except Exception as e:
                print(f"\n❌ Error: {e}\n")
                continue
    
    def run_text_only(self):
        """Run in text-only mode (no voice input/output)"""
        print("=" * 70)
        print("💬 TEXT MODE - Car Manual Assistant")
        print("=" * 70)
        print("\nHow to use:")
        print("  • Type your question and press Enter")
        print("  • Type 'quit' or 'exit' to stop")
        print("\n" + "=" * 70 + "\n")
        
        while True:
            try:
                # Get text input
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
        # No cleanup needed for REST API client
        pass


def main():
    """Main entry point"""
    import argparse
    
    parser = argparse.ArgumentParser(
        description="Car Manual Voice Assistant - Offline-first with cloud sync"
    )
    parser.add_argument(
        "--text-only",
        action="store_true",
        help="Run in text-only mode (no voice input/output)"
    )
    parser.add_argument(
        "--num-results",
        type=int,
        default=config.NUM_SEARCH_RESULTS,
        help=f"Number of manual chunks to retrieve (default: {config.NUM_SEARCH_RESULTS})"
    )
    
    args = parser.parse_args()
    
    # Initialize assistant
    assistant = VoiceAssistant(num_results=args.num_results)
    
    try:
        if args.text_only:
            assistant.run_text_only()
        else:
            assistant.run_interactive()
    finally:
        assistant.close()


if __name__ == "__main__":
    main()
