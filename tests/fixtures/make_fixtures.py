#!/usr/bin/env python3
"""IPKG 테스트 픽스처 생성기.

C++ 테스트가 읽는 .ipkg 파일을 만든다. 악성/손상 케이스는 C++ ZIP 라이터로는
표현하기 어려운 ZIP 플래그(external attributes, 암호화 비트, 중복 엔트리)를
직접 설정해야 하므로 여기서 생성한다.

사용: python tests/fixtures/make_fixtures.py
"""

from __future__ import annotations

import json
import zipfile
from pathlib import Path

OUT = Path(__file__).resolve().parent
ICON = bytes.fromhex(
    "89504e470d0a1a0a0000000d49484452000000010000000108060000001f15c489"
    "0000000a49444154789c6300010000050001"
)

MANIFEST = {
    "format": "IPKG",
    "format_version": 1,
    "app": {
        "id": "com.illiertion.fixture",
        "name": "Fixture App",
        "version": "1.0.0",
        "description": "Test fixture",
        "author": "ILLIERTION",
        "license": "MIT",
    },
    "runtime": {"type": "chromium", "minimum_version": "1.0.0"},
    "sdk": {"minimum_version": "1.0.0"},
    "entry": "app/index.html",
    "platforms": ["windows-x64", "linux-x64", "windows-arm64", "linux-arm64", "macos-arm64", "macos-x64"],
    "permissions": ["storage"],
    "metadata": {"icon": "metadata/icon.png", "preview": "metadata/preview.png"},
}

BASIC_FILES = {
    "app/index.html": b"<!doctype html><title>Fixture</title><h1>Fixture</h1>",
    "app/scripts/main.js": b"console.log('fixture');",
    "metadata/icon.png": ICON,
    "metadata/preview.png": ICON,
}


def mark_encrypted(path: Path, target: str) -> None:
    """CPython's zipfile resets ZipInfo.flag_bits, so set the encryption bit in
    the raw headers instead (local + central directory copies)."""
    data = bytearray(path.read_bytes())

    def patch(signature: bytes, flags_offset: int, name_len_offset: int, name_offset: int) -> None:
        position = 0
        while True:
            position = data.find(signature, position)
            if position < 0:
                return
            name_length = int.from_bytes(
                data[position + name_len_offset:position + name_len_offset + 2], "little"
            )
            entry_name = data[position + name_offset:position + name_offset + name_length].decode()
            if entry_name == target:
                flags = int.from_bytes(data[position + flags_offset:position + flags_offset + 2], "little") | 0x1
                data[position + flags_offset:position + flags_offset + 2] = flags.to_bytes(2, "little")
            position += 4

    patch(b"PK\x03\x04", 6, 26, 30)   # local file header
    patch(b"PK\x01\x02", 8, 28, 46)   # central directory header
    path.write_bytes(data)


def write(name: str, entries: list[tuple[zipfile.ZipInfo, bytes]]) -> None:
    path = OUT / name
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as archive:
        for info, payload in entries:
            archive.writestr(info, payload)
    print(f"wrote {path.name} ({path.stat().st_size} bytes)")


def info(name: str, *, external_attr: int = 0o644 << 16, encrypted: bool = False) -> zipfile.ZipInfo:
    entry = zipfile.ZipInfo(name, date_time=(2026, 11, 20, 0, 0, 0))
    entry.external_attr = external_attr
    entry.compress_type = zipfile.ZIP_DEFLATED
    if encrypted:
        entry.flag_bits |= 0x1
    return entry


def manifest_bytes(**overrides) -> bytes:
    document = json.loads(json.dumps(MANIFEST))
    document.update(overrides)
    return json.dumps(document, indent=2).encode()


def basic_entries(manifest: bytes | None = None) -> list[tuple[zipfile.ZipInfo, bytes]]:
    entries = [(info("manifest.json"), manifest if manifest is not None else manifest_bytes())]
    entries += [(info(name), payload) for name, payload in BASIC_FILES.items()]
    return entries


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)

    write("valid.ipkg", basic_entries())

    # Path traversal: entry escapes the install root.
    write("traversal.ipkg", basic_entries() + [(info("../escape.txt"), b"pwned")])

    # Absolute path entry.
    write("absolute.ipkg", basic_entries() + [(info("/etc/passwd"), b"pwned")])

    # Duplicate path differing only by case (Windows/Linux divergence).
    write("duplicate.ipkg", basic_entries() + [(info("app/INDEX.html"), b"duplicate")])

    # Symlink entry (UNIX mode 0xA1FF).
    write("symlink.ipkg", basic_entries() + [(info("app/link", external_attr=0o120777 << 16), b"/etc/passwd")])

    # Encrypted entry bit set (patched into the raw headers afterwards).
    write("encrypted.ipkg", basic_entries() + [(info("app/secret.txt"), b"secret")])
    mark_encrypted(OUT / "encrypted.ipkg", "app/secret.txt")

    # Compression ratio attack: 4 MiB of zeros inside a few KiB.
    write("bomb.ipkg", basic_entries() + [(info("app/zeros.bin"), b"\0" * (4 * 1024 * 1024))])

    # No manifest at all.
    write("nomanifest.ipkg", [(info(name), payload) for name, payload in BASIC_FILES.items()])

    # Broken JSON.
    write("badmanifest.ipkg", basic_entries(manifest=b"{ not json"))

    # Schema violation: uppercase app id.
    documented = json.loads(MANIFEST and json.dumps(MANIFEST))
    documented["app"]["id"] = "Com.Illiertion.Fixture"
    write("badid.ipkg", basic_entries(manifest=json.dumps(documented).encode()))

    # Unknown permission must be rejected, not silently ignored.
    documented = json.loads(json.dumps(MANIFEST))
    documented["permissions"] = ["storage", "root-access"]
    write("badpermission.ipkg", basic_entries(manifest=json.dumps(documented).encode()))

    # Entry file missing from the archive.
    entries = [(info("manifest.json"), manifest_bytes())]
    entries += [(info(name), payload) for name, payload in BASIC_FILES.items() if name != "app/index.html"]
    write("noentry.ipkg", entries)

    # Windows-only app: cannot be installed on Linux.
    entries = basic_entries(manifest=manifest_bytes(platforms=["windows-x64"]))
    write("windows-only.ipkg", entries)

    # Second app for multi-app scenarios (distinct id).
    other = json.loads(json.dumps(MANIFEST))
    other["app"]["id"] = "com.illiertion.other"
    other["app"]["name"] = "Other App"
    write("other.ipkg", basic_entries(manifest=json.dumps(other).encode()))

    # Same app, next version (update path).
    newer = json.loads(json.dumps(MANIFEST))
    newer["app"]["version"] = "1.1.0"
    entries = basic_entries(manifest=json.dumps(newer).encode())
    entries.append((info("app/new-feature.txt"), b"1.1.0"))
    write("valid-1.1.0.ipkg", entries)

    # Full window block, including a custom window icon.
    windowed = json.loads(json.dumps(MANIFEST))
    windowed["permissions"] = ["storage", "window"]
    windowed["window"] = {
        "title": "Fixture Window",
        "width": 1024,
        "height": 720,
        "icon": "metadata/window-icon.png",
        "tray": True,
    }
    entries = basic_entries(manifest=json.dumps(windowed).encode())
    entries.append((info("metadata/window-icon.png"), ICON))
    write("window.ipkg", entries)

    # window.icon declared but absent from the archive -> E1019.
    dangling = json.loads(json.dumps(MANIFEST))
    dangling["permissions"] = ["storage", "window"]
    dangling["window"] = {"icon": "metadata/window-icon.png"}
    entries = basic_entries(manifest=json.dumps(dangling).encode())
    write("window-missing-icon.ipkg", entries)


if __name__ == "__main__":
    main()
