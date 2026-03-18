from dxf_pipeline._bootstrap import ensure_engineering_data_on_path

ensure_engineering_data_on_path()

from engineering_data.cli.generate_preview import *  # noqa: F401,F403,E402


if __name__ == "__main__":
    raise SystemExit(main())
