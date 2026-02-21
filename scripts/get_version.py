#!/usr/bin/env python3
"""Extract semantic version from src/XpressFormula/Version.h."""

from __future__ import annotations

import argparse
import pathlib
import re
import sys


def read_version_header(path: pathlib.Path) -> tuple[int, int, int]:
    text = path.read_text(encoding="utf-8")
    def get_macro(name: str) -> int:
        match = re.search(rf"#define\s+{name}\s+(\d+)", text)
        if not match:
            raise ValueError(f"Missing macro: {name}")
        return int(match.group(1))

    major = get_macro("XF_VERSION_MAJOR")
    minor = get_macro("XF_VERSION_MINOR")
    patch = get_macro("XF_VERSION_PATCH")
    return major, minor, patch


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--header",
        default="src/XpressFormula/Version.h",
        help="Path to Version.h",
    )
    parser.add_argument(
        "--format",
        choices=("plain", "github"),
        default="plain",
        help="Output format",
    )
    args = parser.parse_args()

    version_path = pathlib.Path(args.header)
    major, minor, patch = read_version_header(version_path)
    version = f"{major}.{minor}.{patch}"

    if args.format == "github":
        print(f"APP_VERSION={version}")
        print(f"APP_VERSION_TAG=v{version}")
    else:
        print(version)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
