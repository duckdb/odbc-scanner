"""Stage standalone C API binaries for the shared build/deploy workflows."""

import argparse
from pathlib import Path
import shutil


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("operation", choices=("stage", "restore"))
    parser.add_argument("--build-directory", required=True, type=Path)
    parser.add_argument("--filename", required=True)
    parser.add_argument("--duckdb-version", required=True)
    parser.add_argument("--platform", required=True)
    parser.add_argument("--repository", type=Path)
    args = parser.parse_args()

    binary = args.build_directory / args.filename
    if args.operation == "stage":
        build_root = args.build_directory
        if args.platform.startswith("wasm_"):
            build_root = build_root.parent
        source = binary
        destination = build_root / "repository" / args.duckdb_version / args.platform / args.filename
    else:
        if args.repository is None:
            parser.error("--repository is required when restoring a binary")
        source = args.repository / args.duckdb_version / args.platform / args.filename
        destination = binary

    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, destination)
    print(f"{args.operation}: {source} -> {destination}")


if __name__ == "__main__":
    main()
