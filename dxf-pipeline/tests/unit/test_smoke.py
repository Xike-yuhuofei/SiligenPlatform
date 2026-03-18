import py_compile
from pathlib import Path
import unittest


class SmokeTest(unittest.TestCase):
    def test_key_files_are_syntax_valid(self) -> None:
        project_root = Path(__file__).resolve().parents[2]
        candidates = [
            project_root / 'src' / 'dxf_pipeline' / 'cli' / 'dxf_to_pb.py',
            project_root / 'src' / 'dxf_pipeline' / 'cli' / 'export_simulation_input.py',
            project_root / 'src' / 'dxf_pipeline' / 'cli' / 'path_to_trajectory.py',
            project_root / 'src' / 'dxf_pipeline' / 'contracts' / 'simulation_input.py',
            project_root / 'src' / 'proto' / 'dxf_primitives_pb2.py',
        ]
        for candidate in candidates:
            self.assertTrue(candidate.exists(), f'missing file: {candidate}')
            py_compile.compile(str(candidate), doraise=True)


if __name__ == '__main__':
    unittest.main()
