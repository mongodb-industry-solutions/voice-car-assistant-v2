#!/usr/bin/env python3
"""
Quick setup and test script for Car Manual Voice Assistant
"""

import sys
import subprocess
import os


def check_command(cmd):
    """Check if a command exists"""
    try:
        subprocess.run([cmd, "--version"], capture_output=True, check=True)
        return True
    except (subprocess.CalledProcessError, FileNotFoundError):
        return False


def main():
    print("="*70)
    print("🚗 Car Manual Voice Assistant - Setup Check")
    print("="*70)
    print()
    
    all_good = True
    
    # Check Python version
    print("1. Checking Python version...")
    if sys.version_info >= (3, 8):
        print(f"   ✓ Python {sys.version_info.major}.{sys.version_info.minor} (OK)")
    else:
        print(f"   ✗ Python {sys.version_info.major}.{sys.version_info.minor} (Need 3.8+)")
        all_good = False
    print()
    
    # Check Ollama
    print("2. Checking Ollama installation...")
    if check_command("ollama"):
        print("   ✓ Ollama is installed")
        
        # Check for required models
        try:
            result = subprocess.run(["ollama", "list"], capture_output=True, text=True)
            models_output = result.stdout
            
            print("\n   Checking required models:")
            
            if "nub235/voyage-4-nano" in models_output or "voyage-4-nano" in models_output:
                print("   ✓ nub235/voyage-4-nano (embedding model)")
            else:
                print("   ✗ nub235/voyage-4-nano (missing)")
                print("     Run: ollama pull nub235/voyage-4-nano")
                all_good = False
            
            has_llm = False
            for model in ["llama3.2", "llama3", "llama2"]:
                if model in models_output:
                    print(f"   ✓ {model} (LLM model)")
                    has_llm = True
                    break
            
            if not has_llm:
                print("   ✗ No LLM model found")
                print("     Run: ollama pull llama3.2")
                all_good = False
        
        except Exception as e:
            print(f"   ⚠ Could not check models: {e}")
    else:
        print("   ✗ Ollama is not installed")
        print("     Install from: https://ollama.com/download")
        all_good = False
    print()
    
    # Check documents
    print("3. Checking documents folder...")
    docs_dir = "documents"
    if os.path.exists(docs_dir):
        txt_files = [f for f in os.listdir(docs_dir) if f.endswith('.txt')]
        if txt_files:
            print(f"   ✓ Found {len(txt_files)} .txt file(s)")
            for f in txt_files[:5]:  # Show first 5
                print(f"     - {f}")
            if len(txt_files) > 5:
                print(f"     ... and {len(txt_files) - 5} more")
        else:
            print("   ⚠ No .txt files found in documents/")
            print("     A sample file is provided. Add your own manuals!")
    else:
        print("   ✗ Documents folder not found")
        all_good = False
    print()
    
    # Check database
    print("4. Checking database...")
    if os.path.exists("car-manual-db"):
        print("   ✓ Database exists (documents already loaded)")
    else:
        print("   ⚠ Database not found (need to run load_documents.py)")
    print()
    
    # Check Python packages
    print("5. Checking Python packages...")
    required_packages = [
        ("objectbox", "objectbox"),
        ("ollama", "ollama"),
        ("speech_recognition", "SpeechRecognition"),
        ("pyttsx3", "pyttsx3"),
    ]
    
    missing_packages = []
    for package_import, package_name in required_packages:
        try:
            __import__(package_import)
            print(f"   ✓ {package_name}")
        except ImportError:
            print(f"   ✗ {package_name} (missing)")
            missing_packages.append(package_name)
    
    if missing_packages:
        print(f"\n   Install missing packages with:")
        print(f"   pip install {' '.join(missing_packages)}")
        all_good = False
    print()
    
    # Summary and next steps
    print("="*70)
    if all_good:
        print("✅ Setup complete! Ready to use.")
        print()
        print("Next steps:")
        if not os.path.exists("car-manual-db"):
            print("  1. Run: python load_documents.py")
            print("  2. Run: python main.py")
        else:
            print("  1. Run: python main.py")
            print("     (or 'python main.py --text-only' for text mode)")
    else:
        print("⚠️  Setup incomplete. Please address the issues above.")
        print()
        print("Quick fix commands:")
        print("  pip install -r requirements.txt")
        print("  ollama pull nub235/voyage-4-nano")
        print("  ollama pull llama3.2")
    print("="*70)


if __name__ == "__main__":
    main()
