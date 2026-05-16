"""Small bootstrap for running legacy test scripts directly."""
from __future__ import annotations

import os
import sys


def ensure_repo_root_on_path() -> str:
    tests_dir = os.path.dirname(os.path.abspath(__file__))
    repo_root = os.path.dirname(tests_dir)
    if repo_root not in sys.path:
        sys.path.insert(0, repo_root)
    return repo_root
