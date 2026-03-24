# Whisper Model Configuration

The project now uses **OpenAI Whisper** for offline speech-to-text recognition.

## Current Configuration

**Model**: `base` (74MB)
- Good balance between speed and accuracy
- Works well on most systems
- No internet connection required

## Available Models

You can change the model in `main.py` line 117:

```python
text = self.recognizer.recognize_whisper(audio, model="base", language="english")
```

| Model | Size | Speed | Accuracy | Best For |
|-------|------|-------|----------|----------|
| `tiny` | 39MB | ⚡⚡⚡⚡⚡ | ⭐⭐⭐ | Raspberry Pi, embedded systems |
| `base` | 74MB | ⚡⚡⚡⚡ | ⭐⭐⭐⭐ | **Default - good balance** |
| `small` | 244MB | ⚡⚡⚡ | ⭐⭐⭐⭐⭐ | Desktop systems, better accuracy |
| `medium` | 769MB | ⚡⚡ | ⭐⭐⭐⭐⭐ | High accuracy needed |
| `large` | 1.5GB | ⚡ | ⭐⭐⭐⭐⭐ | Maximum accuracy |

## First Run

The first time you use Whisper, it will automatically download the model:
- **tiny**: ~39MB download
- **base**: ~74MB download  
- **small**: ~244MB download

Subsequent runs will use the cached model (no download).

## Language Support

Whisper supports 99 languages. To change language:

```python
# English (default)
text = self.recognizer.recognize_whisper(audio, model="base", language="english")

# Spanish
text = self.recognizer.recognize_whisper(audio, model="base", language="spanish")

# French
text = self.recognizer.recognize_whisper(audio, model="base", language="french")

# Or let Whisper auto-detect
text = self.recognizer.recognize_whisper(audio, model="base")
```

## Performance Tips

### For Embedded Systems (Raspberry Pi, etc.)
```python
text = self.recognizer.recognize_whisper(audio, model="tiny")
```

### For Better Accuracy
```python
text = self.recognizer.recognize_whisper(audio, model="small")
```

### For Noisy Environments
Use `small` or `medium` models for better noise handling.

## Installation

```bash
# Update dependencies
pip install -r requirements.txt

# Or install Whisper directly
pip install openai-whisper
```

## System Requirements

| Model | RAM | CPU | Response Time |
|-------|-----|-----|---------------|
| tiny | 1GB | Any | 0.3-1s |
| base | 1GB | Any | 0.5-2s |
| small | 2GB | Modern | 1-3s |
| medium | 5GB | Fast | 3-6s |
| large | 10GB | Fast | 6-12s |

## Benefits Over Google API

✅ **Offline**: No internet connection needed  
✅ **No Rate Limits**: Use as much as you want  
✅ **Privacy**: All processing happens locally  
✅ **Reliable**: No network errors or timeouts  
✅ **Accurate**: State-of-the-art accuracy  
✅ **Multi-language**: 99 languages supported  

## Troubleshooting

### Slow First Run
First run downloads the model. This is normal and only happens once.

### Out of Memory
Use a smaller model: `tiny` or `base`

### Slow Transcription
- Use `tiny` model for faster processing
- Upgrade hardware (more RAM/CPU)
- Reduce `phrase_time_limit` in listen() method

### Installation Issues
```bash
# On Windows, you may need
pip install setuptools-rust

# On Linux, you may need
sudo apt-get install ffmpeg
```

## Switching Back to Google API

If you prefer online Google API, change line 117 in `main.py`:

```python
# Replace:
text = self.recognizer.recognize_whisper(audio, model="base", language="english")

# With:
text = self.recognizer.recognize_google(audio)
```
