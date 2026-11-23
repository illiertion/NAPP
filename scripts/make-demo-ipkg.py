#!/usr/bin/env python3
"""examples/demo-app 를 설치 가능한 .ipkg 로 묶는다.

사용: python scripts/make-demo-ipkg.py [출력경로]

기본 출력: examples/napp-demo-lab.ipkg
같은 입력이면 같은 zip 항목 순서/타임스탬프를 쓰므로 해시가 재현된다.
"""

from __future__ import annotations

import hashlib
import json
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "examples" / "demo-app"
DEFAULT_OUTPUT = ROOT / "examples" / "napp-demo-lab.ipkg"

# 아이콘은 런처와 같은 원본을 쓴다(런처 리소스용 256px 사본이 있으면 그것부터).
ICON_CANDIDATES = [
    ROOT / "launcher" / "assets" / "logo-256.png",
    ROOT / "logo.PNG",
]


def collect() -> list[tuple[str, bytes]]:
    entries: list[tuple[str, bytes]] = []
    manifest = SOURCE / "manifest.json"
    if not manifest.is_file():
        raise SystemExit(f"manifest.json 이 없습니다: {manifest}")
    entries.append(("manifest.json", manifest.read_bytes()))

    for path in sorted(SOURCE.rglob("*")):
        if not path.is_file() or path == manifest:
            continue
        entries.append((path.relative_to(SOURCE).as_posix(), path.read_bytes()))

    icon = ICON_CANDIDATES[0] if ICON_CANDIDATES[0].is_file() else None
    if icon:
        payload = icon.read_bytes()
        existing = {name for name, _ in entries}
        if "metadata/icon.png" not in existing:
            entries.append(("metadata/icon.png", payload))
        if "metadata/preview.png" not in existing:
            entries.append(("metadata/preview.png", payload))
    return entries


def build(output: Path) -> None:
    entries = collect()
    document = json.loads(dict(entries)["manifest.json"])
    app = document["app"]

    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, "w", zipfile.ZIP_DEFLATED) as archive:
        for name, payload in entries:
            info = zipfile.ZipInfo(name, date_time=(2026, 11, 20, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o644 << 16
            archive.writestr(info, payload)

    data = output.read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    print(f"wrote {output.relative_to(ROOT)}")
    print(f"  app     {app['id']} v{app['version']}")
    print(f"  entries {len(entries)}")
    print(f"  size    {len(data)} bytes")
    print(f"  sha256  {digest}")
    print("\n런처에서 설치: 홈 → '패키지 설치' → 이 파일 선택")


def main() -> int:
    output = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else DEFAULT_OUTPUT
    build(output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
