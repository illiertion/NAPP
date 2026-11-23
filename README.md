# NAPP Launcher

**NAPP** — ILLIERTION cross-platform web application platform.

웹 앱을 `.ipkg` 패키지로 묶어 배포하고, 각 OS 에서 네이티브 앱처럼 설치·실행하는
플랫폼입니다. 런처(Qt 6)와 웹 런타임 호스트(Chromium Embedded Framework)로
구성되어 있으며, 앱은 샌드박스 안에서 실행되고 권한은 설치 때 사용자가 승인합니다.

| | |
| --- | --- |
| **런처** | 스토어 · 내 앱 · 런타임 관리 · 설정 (Qt 6 / QML) |
| **런타임** | 앱마다 독립 프로세스 + 독립 프로필 (CEF 158) |
| **패키지** | `.ipkg` — 매니페스트 + 앱 파일 + 아이콘 (재현 가능한 ZIP) |
| **저장소** | 정적 호스팅 JSON 인덱스 (`raw.githubusercontent.com`) |
| **SDK** | `window.napp` 주입 + `napp://` 호스트 라우트 |

현재 빌드·검증 대상은 **Windows 10/11 x64**입니다. macOS/Linux 는 코드상 지원하며
패키지 레이아웃까지 생성됩니다.

---

## 1. 빌드

### 요구 사항

| 항목 | 버전 |
| --- | --- |
| Windows | 10 1809 이상 |
| Qt | 6.5 이상 (검증: 6.12.0 `msvc2022_64`) |
| 컴파일러 | MSVC 2022 이상 (C++20) |
| CMake | 3.24 이상 (Ninja 권장) |
| Python | 3.9 이상 (저장소·패키징 도구) |
| CEF | `third_party/cef` (binary distribution, `scripts/fetch-cef.bat`) |

### 절차

```bat
:: CEF 바이너리가 없으면 먼저 받아 둔다
scripts\fetch-cef.bat

:: 런타임 호스트 없이 런처만 (개발용)
scripts\build.bat Release --test

:: Chromium 런타임 포함 (배포용)
scripts\build-cef.bat --test --dist
```

산출물:

| 경로 | 내용 |
| --- | --- |
| `build/cef/NAPP.exe` | 런처 |
| `build/cef/runtime-stage/` | 배포 가능한 웹 런타임 디렉터리 |
| `dist/NAPP-<version>-Setup.exe` | 단일 파일 설치 프로그램 (설치 / 포터블) |
| `dist/NAPP-<version>-portable.zip` | 포터블 압축 |

---

## 2. 배포

```powershell
# 설치 프로그램 + 포터블 zip
scripts\make-dist.ps1

# GitHub 에 올릴 저장소 트리 + .ipkg 모음
scripts\make-release.ps1
```

`make-release.ps1` 결과:

- `release/repository/` — `NAPP-Repository` 저장소에 그대로 올릴 트리
- `release/apps/<appId>/*.ipkg` — 릴리스에 첨부할 패키지

설치 프로그램은 사용자 범위로 설치하므로 관리자 권한이 필요 없습니다.
`NAPP-Setup.exe --install "D:\Apps\NAPP" --quiet` 로 무인 설치할 수 있습니다.

---

## 3. IPKG 저장소

저장소는 파일 몇 개로 이루어진 **정적 호스팅**입니다. 서버가 없습니다.

```
repository/
├── repository.json        # 인덱스 (build_repository.py 가 생성)
├── repository-meta.json   # 저장소 이름·소유자·홈페이지
├── apps-src/<appId>/      # 앱 원본 (manifest.json, app/, metadata/, publish.json)
├── apps/<appId>.json      # NAPP-APP 문서 (다운로드 URL + SHA-256)
├── packages/<appId>/<version>.ipkg
├── icons/<appId>.png
├── categories/categories.json
└── scripts/build_repository.py
```

앱을 추가하거나 갱신하려면 `apps-src/` 를 고치고 인덱스를 다시 만듭니다.

```bash
python repository/scripts/build_repository.py
python repository/scripts/build_repository.py --base-url https://raw.githubusercontent.com/<owner>/<repo>/main/
```

런처는 첫 시작 시 `NAPP_OFFICIAL_REPOSITORY_URL` 에 지정된 저장소를 등록합니다.
다른 저장소를 쓰려면 빌드 때 바꾸거나, 런처 설정에서 직접 추가하세요.

```bat
cmake -DNAPP_OFFICIAL_REPOSITORY_URL=https://raw.githubusercontent.com/<owner>/<repo>/main/ ...
```

---

## 4. 앱 패키지 (.ipkg)

규격은 [`specifications/IPKG_SPECIFICATION.md`](specifications/IPKG_SPECIFICATION.md),
JSON 스키마는 [`specifications/schemas/`](specifications/schemas/) 를 따릅니다.

매니페스트에서 창을 꾸밀 수 있습니다. 모두 선택 사항이며 `window` 권한이 필요합니다.

```json
"permissions": ["window"],
"window": {
  "title": "내 앱",
  "width": 1100,
  "height": 760,
  "icon": "metadata/window-icon.png",
  "tray": true
}
```

- `icon` — 창, 작업 표시줄 단추, Alt+Tab, 트레이 아이콘에 쓰이는 이미지입니다.
  PNG 또는 WebP, 256px 이상 권장. 없으면 NAPP 기본 아이콘을 씁니다.
  런타임 1.4.0 이상이 필요합니다.
- `metadata.icon` — 스토어 타일·바로가기용 아이콘입니다. `window.icon` 과 다를 수
  있습니다.

앱을 만들 때는 **IPKG Studio**(`com.illiertion.ipkgstudio`)를 쓰면 됩니다. 앱 아이콘,
창 아이콘, 창 제목·크기·트레이를 고르고 파일을 넣으면 `.ipkg` 를 만들어 줍니다.

---

## 5. 플랫폼 전용 앱으로 패키징

`.ipkg` 는 웹 앱이지만, 배포물은 각 OS 가 기대하는 모습으로 만들 수 있습니다.

```bash
python scripts/pack_app.py path/to/app.ipkg                    # 이 OS 용
python scripts/pack_app.py app.ipkg --platform windows-x64 --installer
python scripts/pack_app.py app.ipkg --platform all
```

| 플랫폼 | 결과물 |
| --- | --- |
| `windows-x64` | 앱 아이콘을 단 `<Name>.exe` 네이티브 스텁, `.ipkg`, `install.ps1` / `uninstall.ps1`, `README.txt`. `--installer` 를 주면 시작 메뉴·바탕 화면 바로가기와 제거 항목까지 만드는 단일 파일 `Setup.exe` |
| `macos-arm64` | `<Name>.app` 번들 (`Info.plist`, `AppRun`, `.icns`) |
| `linux-x64` | `AppDir` (`AppRun`, `.desktop`, 아이콘) + `install.sh` / `uninstall.sh`. `appimagetool` 로 `.AppImage` 를 만들 수 있게 되어 있습니다 |

결과물은 `dist-apps/` 에 플랫폼별 폴더와 `.zip` 로 나옵니다.

스텁은 NAPP 을 찾아(`NAPP_HOME` → 설치 등록 정보 → 옆 폴더) 동봉한 `.ipkg` 를
설치하고 `NAPP --run-app <id>` 로 넘깁니다. **브라우저를 동봉하지 않습니다.**
대상 기계에 NAPP 이 있으면 됩니다. 그래서 다운로드가 작고, 앱은 NAPP 런타임이 있는
어디서든 그대로 동작합니다.

---

## 6. 저장소

| | |
| --- | --- |
| 소스 | https://github.com/ILLIERTION/NAPP |
| 앱 저장소 | https://github.com/ILLIERTION/NAPP-Repository |
| 규격 | [`specifications/IPKG_SPECIFICATION.md`](specifications/IPKG_SPECIFICATION.md) |
| 아키텍처 | [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) |
| 사용법 | [`docs/USAGE.md`](docs/USAGE.md) |
| 변경 이력 | [`CHANGELOG.md`](CHANGELOG.md) |

---

## 7. 라이선스

MIT. 서드파티: Qt (LGPL/commercial), Chromium Embedded Framework (BSD-3-Clause),
miniz (MIT). 자세한 내용은 각 `third_party/` 폴더를 보세요.