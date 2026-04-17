from __future__ import annotations

import unittest
from pathlib import Path
from typing import cast


WORKSPACE_ROOT = Path(__file__).resolve().parents[2]
MATRIX_PATH = (
    WORKSPACE_ROOT
    / "docs"
    / "architecture"
    / "governance"
    / "migration"
    / "migration-alignment-clearance-matrix.md"
)
REQUIRED_COLUMNS = (
    "module_id",
    "canonical_root_ref",
    "asset_family",
    "target_owner_surface",
    "current_live_surface",
    "legacy_paths_to_clear",
    "required_gate_entries",
    "required_reports",
    "blocking_condition",
    "status",
)
REQUIRED_ROW_IDS = {
    "M0",
    "M1",
    "M2",
    "M3",
    "M4",
    "M5",
    "M6",
    "M7",
    "M8",
    "M9",
    "M10",
    "M11",
    "ROOT-ENTRY",
    "APPS",
    "SHARED",
    "TESTS",
    "SCRIPTS",
    "ASSET-ROOTS",
}


def _table_rows(text: str) -> list[list[str]]:
    rows: list[list[str]] = []
    for raw_line in text.splitlines():
        line = raw_line.strip()
        if not line.startswith("|"):
            continue
        cells = [cell.strip() for cell in line.strip("|").split("|")]
        if cells and set(cells[0]) == {"-"}:
            continue
        rows.append(cells)
    return rows


class MigrationAlignmentMatrixContractTest(unittest.TestCase):
    def test_matrix_document_exists(self) -> None:
        self.assertTrue(MATRIX_PATH.exists(), msg=f"matrix document missing: {MATRIX_PATH}")

    def test_matrix_contains_required_columns(self) -> None:
        rows = _table_rows(MATRIX_PATH.read_text(encoding="utf-8"))
        header = next((row for row in rows if row and row[0] == "module_id"), None)
        self.assertIsNotNone(header, msg="matrix header row missing")
        self.assertEqual(tuple(cast(list[str], header)), REQUIRED_COLUMNS)

    def test_matrix_covers_all_modules_and_cross_root_assets(self) -> None:
        rows = _table_rows(MATRIX_PATH.read_text(encoding="utf-8"))
        data_rows = [row for row in rows if row and row[0] != "module_id"]
        row_ids = {row[0] for row in data_rows}
        self.assertTrue(REQUIRED_ROW_IDS.issubset(row_ids), msg=f"matrix rows missing: {sorted(REQUIRED_ROW_IDS - row_ids)}")

    def test_all_matrix_rows_are_verified(self) -> None:
        rows = _table_rows(MATRIX_PATH.read_text(encoding="utf-8"))
        data_rows = [row for row in rows if row and row[0] != "module_id"]
        for row in data_rows:
            self.assertEqual(len(row), len(REQUIRED_COLUMNS), msg=f"unexpected matrix row width: {row}")
            self.assertEqual(row[-1], "verified", msg=f"matrix row not verified: {row[0]}")


if __name__ == "__main__":
    unittest.main()
