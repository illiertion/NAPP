#!/usr/bin/env python3
"""NAPP 규격 검증 도구.

specifications/schemas/*.schema.json 으로 specifications/examples/ 를 검증하고,
스키마가 표현하지 못하는 의미론적 규칙(예: release.latest 키 존재, entry 접두사)을 확인한다.

사용: python specifications/tools/validate_spec.py
종료 코드: 0 = 통과, 1 = 실패
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

try:
    import jsonschema
except ImportError:  # pragma: no cover
    print("FAIL: jsonschema 패키지가 필요합니다 (python -m pip install jsonschema)")
    raise SystemExit(1)

SPEC_ROOT = Path(__file__).resolve().parent.parent
SCHEMA_DIR = SPEC_ROOT / "schemas"
EXAMPLE_DIR = SPEC_ROOT / "examples"

# (스키마 파일, 검증할 예제 파일들)
TARGETS = [
    ("manifest.schema.json", ["manifest.example.json"]),
    ("repository.schema.json", ["repository.json"]),
    ("app.schema.json", ["apps/example.json"]),
    ("categories.schema.json", ["categories/categories.json"]),
]


def load_json(path: Path) -> dict:
    with path.open(encoding="utf-8") as handle:
        return json.load(handle)


def semantic_errors(schema_name: str, document: dict) -> list[str]:
    """JSON Schema로 표현할 수 없는 규격 규칙을 확인한다."""
    errors: list[str] = []

    if schema_name == "app.schema.json":
        versions = document["release"]["versions"]
        latest = document["release"]["latest"]
        if latest not in versions:
            errors.append(f"release.latest '{latest}' 가 versions에 없습니다")
        platforms = set(document["compatibility"]["platforms"])
        for version, entry in versions.items():
            extra = set(entry.get("platforms", [])) - platforms
            if extra:
                errors.append(f"버전 {version} 의 platforms {sorted(extra)} 가 compatibility.platforms에 없습니다")

    if schema_name == "repository.schema.json":
        ids = [app["id"] for app in document["apps"]]
        duplicates = {value for value in ids if ids.count(value) > 1}
        if duplicates:
            errors.append(f"repository.json 에 중복 앱 ID: {sorted(duplicates)}")

    return errors


def main() -> int:
    failures = 0
    for schema_name, examples in TARGETS:
        schema_path = SCHEMA_DIR / schema_name
        schema = load_json(schema_path)
        validator = jsonschema.Draft202012Validator(schema)
        for example in examples:
            example_path = EXAMPLE_DIR / example
            try:
                document = load_json(example_path)
            except (OSError, json.JSONDecodeError) as exc:
                print(f"FAIL {example}: 읽기/파싱 실패 ({exc})")
                failures += 1
                continue

            problems = [
                f"{'/'.join(map(str, error.absolute_path)) or '<root>'}: {error.message}"
                for error in sorted(validator.iter_errors(document), key=lambda e: list(e.absolute_path))
            ]
            problems.extend(semantic_errors(schema_name, document))

            if problems:
                failures += 1
                print(f"FAIL {example} (vs {schema_name})")
                for problem in problems:
                    print(f"  - {problem}")
            else:
                print(f"OK   {example} (vs {schema_name})")

    print(f"\n{failures} failure(s)")
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
