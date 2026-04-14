#!/usr/bin/env python3
"""
Simple launcher for Car Manual Voice Assistant Demo
Starts web server and opens browser automatically
"""

import webbrowser
import time
import threading
from web_server import main, socketio, app

def open_browser():
    """Open browser after short delay"""
    time.sleep(2)  # Wait for server to start
    print("\n🌐 Opening browser to http://localhost:5000")
    webbrowser.open('http://localhost:5000')

if __name__ == '__main__':
    # Start browser opener in background
    threading.Thread(target=open_browser, daemon=True).start()
    
    # Start server
    main()
