# Quick Reference Guide

## Installation & Setup (5 minutes)

```bash
# 1. Install Ollama from https://ollama.com/download

# 2. Pull models
ollama pull nub235/voyage-4-nano
ollama pull llama3.2

# 3. Install Python packages (includes Whisper for offline speech recognition)
pip install -r requirements.txt

# 4. Check setup
python setup_check.py
```

**Note**: First voice input will download Whisper small model (244MB). This only happens once and provides excellent accuracy for technical terms.

## Loading Documents (2-10 minutes depending on size)

```bash
# Add your .txt files to documents/ folder
# Then run:
python load_documents.py
```

## Running the Assistant

### Voice Mode (Full Experience)
```bash
python main.py
```
- Press Enter and speak your question
- Wait for the assistant to respond
- Type 'quit' to exit

### Text Mode (No Microphone/Speakers)
```bash
python main.py --text-only
```
- Type your questions
- See text responses
- Good for testing or low-resource systems

## Example Questions to Ask

- "How do I change the oil?"
- "What does the check engine light mean?"
- "How often should I rotate my tires?"
- "What type of fuel should I use?"
- "How do I jump start the car?"
- "What is the tire pressure supposed to be?"
- "How do I check the brake fluid?"

## Common Issues & Solutions

### "Database not found"
**Solution:** Run `python load_documents.py` first

### "No speech detected"
**Solution:** 
- Check microphone is working
- Speak clearly after "Listening..." appears
- Try text mode: `python main.py --text-only`

### PyAudio installation fails
**Windows:** `pip install pipwin && pipwin install pyaudio`
**Linux:** `sudo apt-get install portaudio19-dev python3-pyaudio`

### Slow responses
**Solution:**
- Use smaller Whisper model: Edit main.py to use `model="tiny"`
- Use smaller LLM: `python main.py --llm-model llama3.2:1b`
- Reduce context: `python main.py --num-results 2`

### First run is slow / downloading files
**Solution:** 
- Normal! Whisper downloads model on first use (~74MB for base)
- Subsequent runs will be much faster
- Use `model="tiny"` for faster download (39MB)

## File Structure

```
car-manual-voice-assistant/
├── main.py                 # Run this for the assistant
├── load_documents.py       # Run this first to load docs
├── requirements.txt        # Install dependencies
├── setup_check.py         # Check if everything is installed
├── README.md              # Full documentation
├── QUICKSTART.md          # This file
├── documents/             # Put your .txt files here
│   └── sample_car_manual.txt
└── car-manual-db/         # Created automatically
```

## Customization

### Use different LLM models
```bash
# Faster, smaller model
python main.py --llm-model llama3.2:1b

# Better quality, slower
python main.py --llm-model llama3:8b
```

### Retrieve more context
```bash
# Get 5 relevant sections instead of 3
python main.py --num-results 5
```

### Custom database location
```bash
python main.py --db /path/to/custom-db
```

## Speech Recognition

The project uses **Whisper small** for high-accuracy offline speech recognition. It provides excellent accuracy for technical terms like "red oil light", "brake fluid", "tire pressure", etc.

## Performance Expectations

### Loading Documents:
- Small manual (10-20 pages): ~30 seconds
- Medium manual (50-100 pages): ~2-3 minutes
- Large manual (200+ pages): ~5-10 minutes

### Query Response Time:
- Speech-to-text (Whisper): 0.5-2s (base model)
- Vector search: < 100ms
### Query Response Time:
- Speech-to-text (Whisper small): 2-3s
- Vector search: < 100ms
- LLM generation: 2-10 seconds (depending on model)
- Text-to-speech: < 1s
- **Total response: 5-15 seconds**

### Resource Usage:
- RAM: 4-6GB (with Whisper small + LLM)
- Storage: 3-5GB (includes Whisper small + Ollama models)
- CPU: Modern multi-core processor recommended

---

**Need help?** See [README.md](README.md) for full documentation.
