# Car Manual Voice Assistant 🚗🎤

A complete voice-enabled car manual assistant using ObjectBox vector search, Ollama for embeddings/LLM, and local speech processing.

## Features

✅ **Full Voice Pipeline**: Speak questions → Get spoken answers  
✅ **Offline Speech Recognition**: Uses OpenAI Whisper (no internet needed)  
✅ **Vector Search**: Fast semantic search using ObjectBox HNSW  
✅ **Local Processing**: All processing happens on-device  
✅ **Lightweight**: Designed to run on embedded systems  
✅ **Privacy-First**: No data sent to external servers

## Architecture

```
Voice Input → Speech-to-Text → Query Embedding → Vector Search → LLM Answer → Text-to-Speech
     ↓              ↓                   ↓              ↓              ↓              ↓
 Microphone   Whisper small          Ollama       ObjectBox       Ollama       pyttsx3
              (offline)            (offline)     (offline)      (offline)    (offline)
```

## Prerequisites

### 1. Install Ollama

Download and install Ollama from [https://ollama.com/download](https://ollama.com/download)

### 2. Pull Required Models

```bash
# Embedding model (required)
ollama pull nub235/voyage-4-nano

# LLM model for answer generation (required)
ollama pull llama3.2

# Optional: Larger models for better quality
# ollama pull llama3.2:3b
# ollama pull llama3
```

### 3. Python Dependencies

```bash
# Navigate to this directory
cd objectbox-python/example/car-manual-voice-assistant

# Install dependencies
pip install -r requirements.txt
```

**Note**: First run will download the Whisper small model (244MB). This only happens once and provides excellent accuracy for technical terms and car manual vocabulary.

**Note for Windows Users**: PyAudio installation might require additional steps. If you encounter errors:
```bash
pip install pipwin
pipwin install pyaudio
```

**Note for Linux Users**: You may need to install PortAudio:
```bash
# Ubuntu/Debian
sudo apt-get install portaudio19-dev python3-pyaudio

# macOS
brew install portaudio
```

## Quick Start

### Step 1: Prepare Your Documents

Create a `documents` folder and add your car manual text files:

```bash
mkdir documents
# Add your .txt files to the documents folder
```

**Example document structure**:
```
documents/
  ├── engine_maintenance.txt
  ├── dashboard_controls.txt
  └── troubleshooting.txt
```

**Tips for document preparation**:
- Convert PDFs to text using tools like `pdftotext` or online converters
- Plain text (.txt) format works best
- Break large manuals into logical sections (chapters)
- Clean formatting for better results

### Step 2: Load Documents into ObjectBox

```bash
python load_documents.py
```

This will:
1. Read all `.txt` files from the `documents` folder
2. Split them into chunks with overlap
3. Generate embeddings using Ollama
4. Store everything in ObjectBox with HNSW index

**Expected output**:
```
============================================================
Car Manual Document Loader
============================================================

Found 3 document(s) to process
Using embedding model: nub235/voyage-4-nano

Processing: engine_maintenance.txt
  - Created 45 chunks
  - Embedded 10/45 chunks
  - Embedded 20/45 chunks
  ...
  ✓ Completed engine_maintenance.txt

============================================================
Successfully loaded 127 chunks from 3 document(s)
Database saved to: car-manual-db
============================================================
```

### Step 3: Run the Voice Assistant

**Interactive Voice Mode**:
```bash
python main.py
```

**Text-Only Mode** (no microphone/speakers needed):
```bash
python main.py --text-only
```

## Usage Examples

### Voice Mode

```
🚗 CAR MANUAL VOICE ASSISTANT
======================================================================

Commands:
  - Press Enter and speak your question
  - Type 'text' to switch to text input mode
  - Type 'quit' or 'exit' to stop

Press Enter to speak (or type command): [Press Enter]
🎤 Listening... (speak now)
🔄 Processing speech...
📝 You asked: 'How do I change the oil?'

🔍 Searching manual for: 'How do I change the oil?'
  ✓ Found 3 relevant sections
🤖 Generating answer...
🔊 Speaking answer...

💬 Answer: According to the manual, to change the oil you should...
```

### Text Mode

```
❓ Your question: What does the check engine light mean?

🔍 Searching manual for: 'What does the check engine light mean?'
  ✓ Found 3 relevant sections
🤖 Generating answer...

💬 Answer: The check engine light indicates that the vehicle's...
```

## Advanced Configuration

### Custom Database Path

```bash
python main.py --db my-custom-db
```

### Use Different Models

```bash
# Use a different embedding model
python main.py --embedding-model nomic-embed-text

# Use a different LLM
python main.py --llm-model llama3:8b

# Retrieve more context chunks
python main.py --num-results 5
```

### Load Documents from Different Directory

```bash
python load_documents.py /path/to/my/manuals
```

## Speech Recognition

The project uses **Whisper small** (244MB) for speech-to-text, providing excellent accuracy for technical terms and car manual vocabulary. The model is downloaded automatically on first use and provides:

- ✅ High accuracy for technical terms ("red oil light", "brake fluid", etc.)
- ✅ Robust noise handling
- ✅ 100% offline operation
- ✅ Natural language understanding

**Performance**: ~2-3 seconds processing time on modern desktop systems.

## System Requirements

### Minimum (Text Mode):
- RAM: 2GB
- Storage: 2GB
- CPU: Any modern processor

### Recommended (Voice Mode with Whisper small):
- RAM: 4GB (6GB recommended)
- Storage: 5GB (includes models)
- CPU: Multi-core processor (i5/Ryzen 5 or better)
- Microphone and speakers

### For Best Performance:
- RAM: 8GB+
- Storage: 10GB
- CPU: Modern multi-core processor
- SSD for faster model loading

## Project Structure

```
car-manual-voice-assistant/
├── main.py              # Main voice assistant application
├── load_documents.py    # Document loader and embedding creator
├── requirements.txt     # Python dependencies
├── README.md           # This file
├── documents/          # Your car manual text files (create this)
└── car-manual-db/      # ObjectBox database (auto-created)
```

## Troubleshooting

### "No module named 'pyaudio'"
- **Windows**: `pip install pipwin && pipwin install pyaudio`
- **Linux**: `sudo apt-get install portaudio19-dev python3-pyaudio`
- **macOS**: `brew install portaudio && pip install pyaudio`

### "Database not found"
Make sure you run `python load_documents.py` before running the assistant.

### "No speech detected"
- Check your microphone is connected and working
- Adjust the `timeout` parameter in `listen()` method
- Try text-only mode: `python main.py --text-only`

### "Ollama connection error"
- Make sure Ollama is running: `ollama serve`
- Verify models are installed: `ollama list`
- Pull missing models: `ollama pull nub235/voyage-4-nano`

### Slow performance
- Use smaller LLM models: `ollama pull llama3.2:1b`
- Reduce `--num-results`: `python main.py --num-results 2`
- Use text-only mode to skip voice processing

## Customization

### Adjust Chunk Size

In `load_documents.py`, modify:
```python
chunks = chunk_text(text, chunk_size=500, overlap=50)
# Increase chunk_size for more context per chunk
# Increase overlap to preserve context across boundaries
```

### Change Voice Speed

In `main.py`, modify:
```python
self.tts_engine.setProperty('rate', 150)  # Adjust 150 to your preference
```

### Add More Context to Answers

```bash
python main.py --num-results 5  # Retrieve 5 chunks instead of 3
```

## Performance Tips

1. **Pre-load models**: Run Ollama models once before first use
2. **SSD Storage**: Use SSD for ObjectBox database for faster queries
3. **Batch Loading**: Load all documents at once for consistent embeddings
4. **Clean Text**: Remove formatting artifacts from documents before loading

## Privacy & Security

- ✅ All data stays local (documents and database)
- ✅ Ollama runs locally (no API calls)
- ✅ Whisper STT is completely offline
- ✅ TTS (pyttsx3) is completely offline
- ✅ **100% offline operation** - no internet required after initial setup

## Future Enhancements

- [ ] Add support for PDF direct processing
- [ ] Multi-language support
- [ ] Web UI interface
- [ ] Mobile app integration
- [ ] Streaming responses for longer answers
- [ ] Voice activity detection (VAD)
- [ ] Custom wake word ("Hey Car")

## License

This example is part of ObjectBox Python and follows the same Apache 2.0 license.

## Support

- ObjectBox Docs: [https://docs.objectbox.io](https://docs.objectbox.io)
- Ollama Docs: [https://ollama.com](https://ollama.com)
- Issues: [Report on GitHub](https://github.com/objectbox/objectbox-python/issues)

## Acknowledgments

- ObjectBox for vector search and storage
- Ollama for local LLM and embeddings
- SpeechRecognition for voice input
- pyttsx3 for voice output

---

**Ready to start?** Run `python load_documents.py` to begin! 🚀
