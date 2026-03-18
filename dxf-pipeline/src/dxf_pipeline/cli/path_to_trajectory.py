from dxf_pipeline._bootstrap import ensure_engineering_data_on_path

ensure_engineering_data_on_path()

from engineering_data.trajectory.offline_path_to_trajectory import *  # noqa: F401,F403,E402


if __name__ == "__main__":
    import sys

    raise SystemExit(main(sys.argv[1:]))
