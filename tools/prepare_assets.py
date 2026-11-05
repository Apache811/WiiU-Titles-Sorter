#!/usr/bin/env python3
"""Prepare Wii U runtime artwork from high-resolution source assets.

Folder artwork is resized offline to 256x235, the generic fallback icon to
256x256, and the small transparent lock overlay to 48x48. ZL/ZR artwork keeps its source dimensions. Original files under
resources/ are never modified.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import sys

from PIL import Image

FOLDER_SIZE = (256, 235)
FOLDER_ASSETS = (
    "folder_blue.png",
    "folder_green.png",
    "folder_yellow.png",
    "folder_orange.png",
    "folder_red.png",
    "folder_rose.png",
    "folder_purple.png",
    "folder_grey.png",
)
UI_ASSETS = ("ZL.png", "ZR.png")
FALLBACK_ASSETS = ("unknown.png",)
LOCK_ASSETS = ("lock.png",)
LOCK_SIZE = (48, 48)


def prepare_asset(source: Path, destination: Path, size: tuple[int, int] | None) -> None:
    with Image.open(source) as image:
        image.load()
        if size is not None:
            image = image.resize(size, Image.Resampling.LANCZOS)
        # Keep RGB/RGBA semantics; palette and grayscale images are normalised
        # to RGB(A) so SDL_image sees predictable pixel formats.
        if image.mode not in ("RGB", "RGBA"):
            image = image.convert("RGBA" if "transparency" in image.info else "RGB")
        destination.parent.mkdir(parents=True, exist_ok=True)
        image.save(destination, format="PNG", optimize=True, compress_level=9)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True, help="Source resources directory")
    parser.add_argument("--output", type=Path, required=True, help="Generated asset directory")
    args = parser.parse_args()

    assets = [(name, FOLDER_SIZE) for name in FOLDER_ASSETS]
    assets.extend((name, None) for name in UI_ASSETS)
    assets.extend((name, (256, 256)) for name in FALLBACK_ASSETS)
    assets.extend((name, LOCK_SIZE) for name in LOCK_ASSETS)

    total_source = 0
    total_output = 0
    for name, size in assets:
        source = args.input / name
        destination = args.output / name
        if not source.is_file():
            print(f"ERROR: required source asset not found: {source}", file=sys.stderr)
            return 1
        try:
            prepare_asset(source, destination, size)
        except (OSError, ValueError) as error:
            print(f"ERROR: unable to process {source}: {error}", file=sys.stderr)
            return 1
        source_size = source.stat().st_size
        output_size = destination.stat().st_size
        total_source += source_size
        total_output += output_size
        dimensions = f"{size[0]}x{size[1]}" if size else "source dimensions"
        print(f"{name}: {source_size:,} -> {output_size:,} bytes ({dimensions})")

    reduction = (1.0 - total_output / total_source) * 100.0 if total_source else 0.0
    print(f"Total: {total_source:,} -> {total_output:,} bytes ({reduction:.1f}% smaller)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
