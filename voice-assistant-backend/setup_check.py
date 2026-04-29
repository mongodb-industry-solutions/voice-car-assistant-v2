"""
Quick setup check for Voice Assistant
Verifies all prerequisites are met before running
"""

import sys
import os
from pathlib import Path


def check_python_version():
    """Check Python version"""
    print("🐍 Checking Python version...")
    version = sys.version_info
    if version.major >= 3 and version.minor >= 8:
        print(f"   ✅ Python {version.major}.{version.minor}.{version.micro}")
        return True
    else:
        print(f"   ❌ Python {version.major}.{version.minor}.{version.micro} (need 3.8+)")
        return False


def check_sync_server_db():
    """Check if sync server database exists"""
    print("\n📂 Checking ObjectBox database...")
    
    sync_dir = Path(__file__).parent.parent / "sync-server-setup"
    db_path = sync_dir / "objectbox"
    model_path = sync_dir / "objectbox-model.json"
    
    if not db_path.exists():
        print(f"   ❌ Database not found at: {db_path}")
        print("   → Start sync server: cd sync-server-setup && docker compose up -d")
        return False
    
    data_file = db_path / "data.mdb"
    if not data_file.exists():
        print(f"   ❌ Database files not initialized")
        print("   → Sync server may still be starting up")
        return False
    
    if not model_path.exists():
        print(f"   ❌ Model file not found at: {model_path}")
        return False
    
    print(f"   ✅ Database found: {db_path}")
    print(f"   ✅ Model found: {model_path}")
    
    # Check file sizes
    size_mb = data_file.stat().st_size / (1024 * 1024)
    print(f"   📊 Database size: {size_mb:.2f} MB")
    
    return True


def check_ollama():
    """Check if Ollama is available"""
    print("\n🤖 Checking Ollama...")
    
    try:
        import ollama
        print("   ✅ Ollama Python package installed")
        
        # Try to list models
        try:
            models = ollama.list()
            model_names = [m['name'] for m in models.get('models', [])]
            
            required_models = {
                'nub235/voyage-4-nano': 'Embedding model',
                'llama3.1:8b': 'LLM model'
            }
            
            all_found = True
            for model, desc in required_models.items():
                # Check if model name starts with the required name
                found = any(m.startswith(model) for m in model_names)
                if found:
                    print(f"   ✅ {desc}: {model}")
                else:
                    print(f"   ❌ {desc}: {model} not found")
                    print(f"      → Install: ollama pull {model}")
                    all_found = False
            
            return all_found
            
        except Exception as e:
            print(f"   ⚠️  Could not list Ollama models: {e}")
            print("   → Make sure Ollama is running")
            return False
            
    except ImportError:
        print("   ❌ Ollama package not installed")
        print("   → Install: pip install ollama")
        return False


def check_dependencies():
    """Check required Python packages"""
    print("\n📦 Checking dependencies...")
    
    required_packages = {
        'objectbox': 'ObjectBox',
        'speech_recognition': 'Speech Recognition',
        'pyttsx3': 'Text-to-Speech',
        'whisper': 'OpenAI Whisper',
    }
    
    all_installed = True
    for package, name in required_packages.items():
        try:
            __import__(package)
            print(f"   ✅ {name}")
        except ImportError:
            print(f"   ❌ {name} not installed")
            all_installed = False
    
    if not all_installed:
        print("\n   → Install all: pip install -r requirements.txt")
    
    return all_installed


def check_audio():
    """Check audio capabilities"""
    print("\n🎤 Checking audio support...")
    
    try:
        import pyaudio
        print("   ✅ PyAudio installed (microphone support)")
        
        # Try to list audio devices
        try:
            p = pyaudio.PyAudio()
            device_count = p.get_device_count()
            print(f"   📊 Found {device_count} audio devices")
            p.terminate()
        except Exception as e:
            print(f"   ⚠️  Could not enumerate audio devices: {e}")
        
        return True
        
    except ImportError:
        print("   ⚠️  PyAudio not installed (voice mode won't work)")
        print("   → You can still use --text-only mode")
        print("   → To install PyAudio on Windows:")
        print("      https://www.lfd.uci.edu/~gohlke/pythonlibs/#pyaudio")
        return False


def main():
    """Run all checks"""
    print("=" * 70)
    print("🔍 Voice Assistant - Setup Check")
    print("=" * 70)
    
    checks = [
        ("Python version", check_python_version()),
        ("ObjectBox database", check_sync_server_db()),
        ("Ollama", check_ollama()),
        ("Dependencies", check_dependencies()),
        ("Audio support", check_audio()),
    ]
    
    print("\n" + "=" * 70)
    print("📋 Summary")
    print("=" * 70)
    
    all_passed = True
    for name, passed in checks:
        status = "✅" if passed else "❌"
        print(f"{status} {name}")
        if not passed:
            all_passed = False
    
    print("\n" + "=" * 70)
    
    if all_passed:
        print("🎉 All checks passed! You're ready to run the voice assistant.")
        print("\nTo start:")
        print("  Voice mode:     python main.py")
        print("  Text-only mode: python main.py --text-only")
    else:
        print("⚠️  Some checks failed. Please resolve the issues above.")
        print("\nYou may still be able to run in text-only mode:")
        print("  python main.py --text-only")
    
    print("=" * 70)


if __name__ == "__main__":
    main()
