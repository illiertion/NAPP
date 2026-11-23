#!/usr/bin/env python3
"""launcher/assets/napp-icon-*.png 를 Windows .ico 하나로 묶는다.

사용: python scripts/pack-ico.py [출력경로]
각 PNG 를 그대로 ICO 엔트리로 넣는 방식(Vista+ 지원)이라 화질 손실이 없다.
"""

from __future__ import annotations

import struct
import sys
from pathlib import Path

ASSETS = Path(__file__).resolve().parent.parent / "launcher" / "assets"
SIZES = (16, 32, 64, 128, 256)
DEFAULT_OUTPUT = ASSETS / "napp.ico"


def build(output: Path) -> None:
    images: list[tuple[int, bytes]] = []
    for size in SIZES:
        path = ASSETS / f"napp-icon-{size}.png"
        if not path.is_file():
            raise SystemExit(f"아이콘이 없습니다: {path} (scripts/make-icons.ps1 먼저 실행)")
        images.append((size, path.read_bytes()))

    count = len(images)
    header = struct.pack("<HHH", 0, 1, count)  # reserved, type=icon, count
    offset = len(header) + 16 * count
    directory = bytearray()
    payload = bytearray()
    for size, data in images:
        directory += struct.pack(
            "<BBBBHHII",
            0 if size >= 256 else size,  # width (0 = 256)
            0 if size >= 256 else size,  # height
            0,                           # palette colours
            0,                           # reserved
            1,                           # colour planes
            32,                          # bits per pixel
            len(data),
            offset,
        )
        payload += data
        offset += len(data)

    output.write_bytes(bytes(header) + bytes(directory) + bytes(payload))
    print(f"wrote {output} ({output.stat().st_size} bytes, {count} sizes)")


def main() -> int:
    output = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else DEFAULT_OUTPUT
    build(output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
