from __future__ import annotations

import os
import sys
import unittest
from pathlib import Path


TEST_ROOT = Path(__file__).resolve().parent


def main() -> int:
    sys.path.insert(0, str(TEST_ROOT))
    os.chdir(TEST_ROOT)
    loader = unittest.TestLoader()
    suite = unittest.TestSuite()
    for subdir in ("unit", "contract"):
        suite.addTests(
            loader.discover(
                start_dir=subdir,
                pattern="test_*.py",
                top_level_dir=str(TEST_ROOT),
            )
        )

    result = unittest.TextTestRunner(verbosity=2).run(suite)
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    raise SystemExit(main())
