"""
Web Server for Car Manual Voice Assistant Demo
Provides WebSocket interface for real-time UI updates
"""

import threading
import uuid
import json
import requests as http_requests
from typing import Optional
from flask import Flask, render_template
from flask_socketio import SocketIO, emit
from flask_cors import CORS

# Import the voice assistant
from main import VoiceAssistant
import config

# Create Flask app
app = Flask(__name__)
app.config['SECRET_KEY'] = 'car-manual-demo-secret'
CORS(app)
socketio = SocketIO(app, cors_allowed_origins="*")

# Conversation service URL
CONVERSATION_SERVICE_URL = "http://localhost:8081"

# Global assistant instance and state
assistant = None
assistant_thread = None
should_stop = False


@app.route('/')
def index():
    """Serve the main UI"""
    return render_template('index.html')


@socketio.on('connect')
def handle_connect():
    """Client connected - send stats only, don't initialize voice"""
    print("🔌 Client connected to web UI")
    emit('status', {'state': 'ready', 'message': 'Click microphone to start (say "thank you" to exit)'})
    
    # Send initial stats
    try:
        import requests
        health = requests.get(f"{config.SEARCH_SERVICE_URL}/health", timeout=2).json()
        emit('stats', {
            'search_service': 'online',
            'chunk_count': health.get('chunk_count', 0)
        })
    except:
        emit('stats', {
            'search_service': 'offline',
            'chunk_count': 0
        })


@socketio.on('disconnect')
def handle_disconnect():
    """Client disconnected"""
    print("🔌 Client disconnected")


@socketio.on('start_listening')
def handle_start_listening():
    """Start voice assistant listening - lazy initialization"""
    global assistant, assistant_thread, should_stop
    
    print("🎤 User clicked start - initializing voice assistant...")
    should_stop = False
    
    # Create assistant only when user clicks start (lazy initialization)
    if assistant is None:
        print("⚙️ Creating voice assistant instance...")
        assistant = VoiceAssistantWithEvents(socketio)
    
    # Reset the stop flag on the assistant
    assistant.should_stop = False
    
    # Start assistant thread if not running
    if assistant_thread is None or not assistant_thread.is_alive():
        print("🚀 Starting voice assistant thread...")
        assistant_thread = threading.Thread(target=run_assistant_loop, daemon=True)
        assistant_thread.start()
    
    emit('status', {'state': 'ready', 'message': 'Ready to listen'})
    print("✅ Voice assistant ready")


@socketio.on('stop_listening')
def handle_stop_listening():
    """Stop voice assistant listening"""
    global should_stop, assistant
    
    print("🛑 User clicked stop - ending voice session...")
    should_stop = True
    if assistant:
        assistant.should_stop = True
    emit('status', {'state': 'ready', 'message': 'Click microphone to start (say "thank you" to exit)'})
    emit('session_complete')
    print("✅ Voice session stopped")


def run_assistant_loop():
    """Run voice assistant in loop (emits events via SocketIO)"""
    global assistant, should_stop
    
    # Exit phrases that end the conversation
    exit_phrases = [
        'thank you', 'thanks', 'thank', 'bye', 'goodbye', 
        'that\'s all', 'thats all', 'stop', 'exit', 'quit'
    ]
    
    while not should_stop:
        try:
            # Emit listening status
            socketio.emit('status', {'state': 'listening', 'message': '🎤 Listening... (speak your question or say "thank you" to exit)'})
            
            # Listen for voice input
            question = assistant.listen()
            
            # Check if stopped while listening
            if should_stop:
                break
            
            if not question:
                socketio.sleep(0.5)
                continue
            
            # Check if user wants to exit
            question_lower = question.lower().strip()
            if any(phrase in question_lower for phrase in exit_phrases):
                # User said goodbye
                socketio.emit('question', {'text': question})
                
                # Save user's goodbye message
                assistant.save_message("user", question)
                
                goodbye_message = "You're welcome! Have a great day! 🚗"
                socketio.emit('answer', {'text': goodbye_message})
                socketio.emit('status', {'state': 'speaking', 'message': '🔊 Saying goodbye...'})
                assistant.speak(goodbye_message)
                
                # Save assistant's goodbye message
                assistant.save_message("assistant", goodbye_message)
                
                socketio.sleep(1)
                break
            
            # Emit question
            socketio.emit('question', {'text': question})
            socketio.emit('status', {'state': 'processing', 'message': '🔍 Processing your question...'})
            
            # Save user question
            assistant.save_message("user", question)
            
            # Check if stopped
            if should_stop:
                break
            
            # Process question
            answer = assistant.process_question(question)
            
            # Get the last search results to save as sources
            sources = []
            if hasattr(assistant, 'last_search_results') and assistant.last_search_results:
                sources = [
                    {
                        'source_file': r.get('source_file', 'Unknown'),
                        'chunk_index': r.get('chunk_index', 0),
                        'score': r.get('score', 0)
                    }
                    for r in assistant.last_search_results[:3]  # Top 3 sources
                ]
            
            # Check if stopped
            if should_stop:
                break
            
            # Emit answer
            socketio.emit('answer', {'text': answer})
            socketio.emit('status', {'state': 'speaking', 'message': '🔊 Speaking answer...'})
            
            # Save assistant answer with sources
            assistant.save_message("assistant", answer, sources)
            
            # Speak answer
            assistant.speak(answer)
            
            # Back to ready
            socketio.sleep(1)
            socketio.emit('status', {'state': 'ready', 'message': 'Ready for next question'})
            
        except KeyboardInterrupt:
            break
        except Exception as e:
            socketio.emit('error', {'message': str(e)})
            socketio.sleep(2)
    
    # Session ended
    socketio.emit('session_complete')
    socketio.emit('status', {'state': 'ready', 'message': 'Click microphone to start (say "thank you" to exit)'})


class VoiceAssistantWithEvents(VoiceAssistant):
    """Extended voice assistant that emits WebSocket events"""
    
    def __init__(self, socketio_instance):
        super().__init__()
        self.socketio = socketio_instance
        self.should_stop = False
        
        # Generate unique IDs for this session
        self.user_id = str(uuid.uuid4())
        self.conversation_id = str(uuid.uuid4())
        print(f"🆔 New session: user={self.user_id[:8]}..., conversation={self.conversation_id[:8]}...")
    
    def save_message(self, role: str, message: str, sources: list = None):
        """Save a message to the conversation service"""
        try:
            payload = {
                "conversation_id": self.conversation_id,
                "user_id": self.user_id,
                "role": role,
                "message": message,
                "sources": json.dumps(sources) if sources else ""
            }
            
            response = http_requests.post(
                f"{CONVERSATION_SERVICE_URL}/conversations/message",
                json=payload,
                timeout=2
            )
            
            if response.ok:
                print(f"💾 Saved {role} message to conversation service")
            else:
                print(f"⚠️ Failed to save message: {response.status_code}")
        except Exception as e:
            print(f"⚠️ Error saving message: {e}")
    
    def listen(self) -> Optional[str]:
        """Override listen to support interruption"""
        import speech_recognition as sr
        
        with sr.Microphone() as source:
            try:
                # Adjust for ambient noise quickly
                self.recognizer.adjust_for_ambient_noise(source, duration=0.3)
                
                # Listen with shorter timeout to allow interruption checks
                # Use a loop to allow checking stop flag
                max_wait = 10  # Maximum 10 seconds total
                elapsed = 0
                
                while elapsed < max_wait and not self.should_stop:
                    try:
                        # Listen with 2-second timeout chunks
                        audio = self.recognizer.listen(source, timeout=2, phrase_time_limit=10)
                        
                        if self.should_stop:
                            return None
                        
                        # Convert speech to text using Whisper
                        text = self.recognizer.recognize_whisper(
                            audio,
                            model=config.WHISPER_MODEL,
                            language=config.WHISPER_LANGUAGE
                        )
                        return text
                        
                    except sr.WaitTimeoutError:
                        elapsed += 2
                        continue
                
                return None
                
            except sr.UnknownValueError:
                return None
            except Exception as e:
                if not self.should_stop:
                    print(f"❌ Error: {e}")
                return None
    
    def search_manual(self, query: str):
        """Override to emit search events"""
        if self.should_stop:
            return []
        
        self.socketio.emit('status', {'state': 'searching', 'message': f'🔍 Searching: {query[:50]}...'})
        
        # Call parent search
        results = super().search_manual(query)
        
        # Save results for later reference (for conversation sources)
        self.last_search_results = results
        
        # Emit results
        if results:
            chunks_data = [
                {
                    'id': r.get('id'),
                    'text': r.get('text', '')[:200] + '...',
                    'source': r.get('source_file', 'Unknown'),
                    'score': r.get('score', 0)
                }
                for r in results
            ]
            self.socketio.emit('search_results', {
                'count': len(results),
                'chunks': chunks_data
            })
        
        return results
    
    def generate_answer(self, question: str, context_chunks):
        """Override to emit answer generation events"""
        if self.should_stop:
            return "Stopped"
        
        self.socketio.emit('status', {'state': 'generating', 'message': '🤖 Generating answer...'})
        return super().generate_answer(question, context_chunks)


def main():
    """Start the web server (BACKEND ONLY - no voice functionality)"""
    print("="*70)
    print("🚗 Car Manual Voice Assistant - Demo UI")
    print("="*70)
    print(f"\n🌐 Starting web server on http://localhost:5000")
    print("📱 UI will open in your browser automatically")
    print("\n💡 How it works:")
    print("  1. Backend server starts (NO voice functionality yet)")
    print("  2. Click microphone button in UI to initialize voice assistant")
    print("  3. Speak your questions or say 'thank you' to exit")
    print("  4. Press Ctrl+C to stop the server")
    print("\nℹ️  Voice assistant will ONLY start when you click the button")
    print("\n" + "="*70 + "\n")
    
    # Start server (just the web server, no voice functionality)
    socketio.run(app, host='0.0.0.0', port=5000, debug=False, allow_unsafe_werkzeug=True)


if __name__ == '__main__':
    main()
