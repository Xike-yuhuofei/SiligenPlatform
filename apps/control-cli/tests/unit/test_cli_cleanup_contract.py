import re
import unittest
from pathlib import Path


class ControlCliCleanupContractTest(unittest.TestCase):
    REPO_ROOT = Path(__file__).resolve().parents[4]
    CLI_ROOT = REPO_ROOT / "apps" / "control-cli"
    PARSER_FILES = (
        CLI_ROOT / "CommandLineParser.h",
        CLI_ROOT / "CommandLineParser.cpp",
    )
    CLI_SOURCE_GLOB = ("*.h", "*.cpp")

    REMOVED_PUBLIC_FLAGS = (
        "--cmp-channel",
        "--verbose",
        "--disable-safety-checks",
        "--no-auto-enable",
        "--show-status",
        "--dispensing-time",
        "--pulse-width",
        "--segments",
    )

    REMOVED_INTERNAL_FIELDS = (
        "mode_string",
        "command_string",
        "path_points",
        "log_to_file",
    )

    def test_removed_public_flags_do_not_reappear_in_parser(self):
        parser_text = "\n".join(path.read_text(encoding="utf-8") for path in self.PARSER_FILES)

        for flag in self.REMOVED_PUBLIC_FLAGS:
            with self.subTest(flag=flag):
                self.assertNotIn(flag, parser_text)

    def test_removed_internal_fields_do_not_reappear_in_cli_sources(self):
        source_text = []
        for pattern in self.CLI_SOURCE_GLOB:
            for path in sorted(self.CLI_ROOT.glob(pattern)):
                source_text.append(path.read_text(encoding="utf-8"))
        merged = "\n".join(source_text)

        for field in self.REMOVED_INTERNAL_FIELDS:
            with self.subTest(field=field):
                self.assertIsNone(re.search(rf"\b{re.escape(field)}\b", merged))

    def test_cli_readme_freezes_cmp_authoritative_source(self):
        readme = (self.CLI_ROOT / "README.md").read_text(encoding="utf-8")

        self.assertIn("[ValveDispenser]", readme)
        self.assertIn("CLI 不再暴露 `--cmp-channel`", readme)
        self.assertIn("死参数", readme)


if __name__ == "__main__":
    unittest.main()
