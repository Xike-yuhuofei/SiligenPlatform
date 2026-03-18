#!/usr/bin/env python3
"""Siligen HMI Application Entry Point."""
import sys
from pathlib import Path

# Add hmi directory to path for imports
sys.path.insert(0, str(Path(__file__).parent))

from ui.main_window import main

if __name__ == "__main__":
    main()
