# NAPP 사용법

이 문서는 (1) 런처를 빌드·실행하는 방법, (2) 앱을 설치·실행하는 방법,
(3) 앱 패키지(.ipkg)와 저장소를 직접 만드는 방법, (4) QML UI를 수정하는 방법을 다룬다.
규격 자체는 `specifications/IPKG_SPECIFICATION.md`를 따른다.

---

## 1. 요구 사항

| 항목 | 버전 |
| --- | --- |
| Windows | 10 1809 이상 (개발·검증 환경: Windows 10) |
| Qt | 6.5 이상 (검증: 6.12.0 msvc2022_64) — `Core Gui Widgets Network Concurrent Quick QuickWidgets` |
| 컴파일러 | MSVC 2022 이상 (C++20) |
| CMake | 3.24 이상 |
| Python | 3.9 이상 (테스트 픽스처·스펙 검증 스크립트용, 선택) |

Linux는 코드상 지원하지만 아직 빌드·검증하지 않았다.

---

## 2. 빌드

```bat
REM 저장소 루트에서
scripts\build.bat Debug          REM Qt만 사용하는 빌드 (CEF 없이 UI·설치 검증)
scripts\build.bat Debug --test   REM + 유닛 테스트 실행
scripts\build-cef.bat            REM Release + CEF 런타임 호스트
```

- CEF가 없으면 먼저 `scripts\fetch-cef.bat`을 실행한다(약 180MB 다운로드, `third_party\cef`에 압축 해제).
- 산출물: `build\dev\NAPP.exe`(Debug), `build\cef\NAPP.exe`와 `build\cef\runtime-stage\`(Release).
- 두 스크립트 모두 빌드 후 `windeployqt --qmldir launcher/qml`로 Qt DLL과 QML 모듈을 복사한다.
  이 단계가 빠지면 런처 창이 비어 있고 로그에 `module "QtQuick.Controls" is not installed`가 남는다.
- 빌드 디렉터리를 지우고 다시 하려면 `build\dev` 또는 `build\cef` 폴더를 삭제한다.

---

## 3. 런처 실행

```bat
build\cef\NAPP.exe
```

- 설정·레지스트리: `%APPDATA%\ILLIERTION\NAPP\config\`
- 앱·데이터·캐시·런타임: `%LOCALAPPDATA%\ILLIERTION\NAPP\`
- 로그: `%APPDATA%\ILLIERTION\NAPP\config\napp.log` (문제 발생 시 가장 먼저 볼 파일)
- 런처는 한 번에 하나만 실행된다(단일 인스턴스). 두 번째 실행은 기존 창을 앞으로 가져온다.

화면은 6개다. 창 너비가 1040px보다 좁아지면 왼쪽 레일이 아이콘만 남는 축소 모드로 바뀐다.

| 화면 | 하는 일 |
| --- | --- |
| 홈 | 업데이트 가능한 앱 목록, 설치된 앱 실행, 파일 설치, 저장소 새로 고침 |
| 앱 스토어 | 검색·카테고리 필터·버전 선택·설치 |
| 내 앱 | 실행·데이터 폴더 열기·권한 재설정·제거 |
| 런타임 | 런타임 등록/활성화/제거, 런타임 폴더 열기 |
| 설정 | 테마(시스템/밝게/어둡게), 강조 색, 언어(en/ko), 시작 시 저장소 확인, 저장소 관리 |
| 정보 | 버전·빌드 정보·서드파티 라이선스, ILLIERTION 크레딧 |

---

## 4. 런타임 준비

앱을 실행하려면 **NAPP Web Runtime**이 등록되어 있어야 한다.

```bat
scripts\build-cef.bat
build\cef\NAPP.exe --register-runtime build\cef\runtime-stage
```

- 등록 결과: `%LOCALAPPDATA%\ILLIERTION\NAPP\runtime\1.0.0\` (약 400MB), 활성 포인터 `config\runtime-current`
- 런처의 **런타임** 화면에서 `런타임 폴더 등록`으로 같은 일을 할 수 있다(폴더에 `runtime.json` 필요).
- 런타임 폴더에는 `runtime.json`, `napp-runtime-host.exe`, CEF 리소스(`libcef.dll`, `*.pak`, `icudtl.dat`, `locales\`), Qt DLL이 함께 있어야 한다.
- 앱 1개당 호스트 프로세스 1개가 뜨고, 프로필은 `data\<appId>\web\`에 만들어진다. 앱을 종료하면 프로세스가 사라진다.

로그 확인:

```
%APPDATA%\ILLIERTION\NAPP\config\napp.log        런처
%LOCALAPPDATA%\ILLIERTION\NAPP\data\<appId>\logs\cef.log   해당 앱 호스트
```

---

## 5. 앱 설치·실행

### 스토어에서 설치

1. **앱 스토어** → 앱 카드 클릭 → 버전 선택 → `설치`
2. 다운로드 진행률은 창 아래 상태 표시줄에 나온다.
3. 다운로드 → SHA-256 검증 → 패키지 검증 → `apps\<appId>\<version>\`로 원자적 설치 → `current` 포인터 갱신 순서로 진행된다.
4. 실패하면 기존 설치본은 그대로 남는다.

### 파일로 설치

**내 앱** → `패키지 파일 설치 (.ipkg)` → `.ipkg` 선택.

저장소 없이 바로 확인하려면 저장소에 들어 있는 테스트 앱을 설치한다.

```bat
python scripts\make-demo-ipkg.py
REM → examples\napp-demo-lab.ipkg (com.illiertion.demo.lab v1.0.0)
```

이 패키지는 런타임 정보·localStorage·권한 목록·이벤트 로그를 보여 주는 페이지(`NAPP Demo Lab`)를 담고 있어,
설치→실행→데이터 저장까지 한 번에 확인할 수 있다. 소스는 `examples/demo-app/`.

### 실행

**내 앱** → `실행`. 처음 실행할 때 매니페스트가 요청한 권한을 보여 주고 허용 여부를 묻는다.
허용 결과는 앱별로 저장되며, **내 앱 → 권한**에서 다시 바꿀 수 있다.

### 아이콘과 바로가기

**내 앱** 타일 오른쪽 위 `⋯` 메뉴에서 다음을 할 수 있다.

- `아이콘 변경`: PNG/JPG/BMP/WEBP/ICO 파일을 고른다. 512px로 줄여 `config\icons\<appId>.png`에 저장되고,
  홈·내 앱·상세 화면 타일에 바로 반영된다. 앱을 업데이트해도 아이콘은 유지된다.
- `아이콘 초기화`: 기본 그라데이션 커버로 되돌린다.
- `바탕 화면 바로가기`: 바탕 화면에 `.lnk`를 만든다. 대상은 `NAPP.exe --run-app <appId>`이고,
  아이콘을 바꾼 앱은 그 아이콘(`config\icons\<appId>.ico`)을 쓴다. 바로가기로 실행하면 앱 창을 닫을 때
  프로세스가 끝나며, 한 번도 권한을 승인하지 않은 앱은 런처가 열려 승인을 요청한다.
- `제거`: 앱 제거(6번 항목과 같다).

### 제거

**내 앱** → `⋯` → `제거` → 대화상자에서 앱 데이터 삭제 여부를 선택한다.
프로그램 파일은 항상 지워지고, `data\<appId>`는 "예"를 골랐을 때만 지워진다.

---

## 6. 앱 패키지(.ipkg) 만들기

`.ipkg`는 **ZIP 컨테이너**이고, `manifest.json`이 압축 파일의 **루트**에 있어야 한다.
최상위 폴더로 감싸면(`MyApp/manifest.json`) 설치가 거부된다.

### 6.1 폴더 구조

```
Hello.ipkg
├── manifest.json
├── metadata/
│   ├── icon.png        (실존 파일이어야 함)
│   └── preview.png     (실존 파일이어야 함)
└── app/
    └── index.html      (entry)
```

### 6.2 manifest.json

```json
{
  "format": "IPKG",
  "format_version": 1,
  "app": {
    "id": "com.example.hello",
    "name": "Hello NAPP",
    "version": "1.0.0",
    "description": "첫 번째 NAPP 앱",
    "author": "Your Name",
    "license": "MIT"
  },
  "runtime": { "type": "chromium", "minimum_version": "1.0.0" },
  "sdk": { "minimum_version": "1.0.0" },
  "entry": "app/index.html",
  "platforms": ["windows-x64"],
  "permissions": [],
  "metadata": {
    "icon": "metadata/icon.png",
    "preview": "metadata/preview.png"
  }
}
```

주의할 점:

- `app.id`는 역DNS 소문자(2개 이상 세그먼트), `app.version`은 semver다.
- `entry`는 반드시 `app/` 하위 경로다.
- `permissions`는 `storage`, `window`, `notifications`, `dialog`, `filesystem`, `clipboard`, `external-links`, `system-info` 중에서만 쓸 수 있고, 모르는 값이 있으면 설치가 거부된다.
- `metadata.icon`/`metadata.preview`는 매니페스트에 적은 경로에 실제 파일이 있어야 한다(없으면 `E1019`).

### 6.3 압축하기

Python으로 만드는 예:

```python
import zipfile, pathlib

src = pathlib.Path("HelloApp")          # manifest.json이 있는 폴더
out = pathlib.Path("Hello.ipkg")

with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
    for path in sorted(src.rglob("*")):
        if path.is_file():
            z.write(path, path.relative_to(src).as_posix())
```

7-Zip을 쓴다면 `stored` 또는 `deflate`만 사용한다(bzip2/LZMA는 `E1002`로 거부).
ZIP 암호화, 심볼릭 링크, 중복 경로는 모두 거부된다.

### 6.4 설치해서 확인

```bat
build\cef\NAPP.exe
```
**내 앱 → 패키지 파일 설치 (.ipkg)** 로 방금 만든 파일을 선택한다.
검증에 실패하면 오류 코드와 메시지가 대화상자와 로그에 남는다.

---

## 7. 저장소 만들기

저장소는 HTTPS로 접근 가능한 정적 JSON 트리다. GitHub 저장소의 `main` 브랜치 + Raw URL 조합을 권장한다.

```
NAPP-Repository/
├── repository.json
├── categories/
│   └── categories.json
├── apps/
│   └── hello.json
└── icons/
    └── hello.png
```

### repository.json

```json
{
  "format": "NAPP-REPOSITORY",
  "schema_version": 1,
  "repository": {
    "id": "com.example.repo",
    "name": "Example Repository",
    "owner": "example"
  },
  "categories": "categories/categories.json",
  "apps": [
    { "id": "com.example.hello", "manifest": "apps/hello.json", "category": "utilities" }
  ]
}
```

### apps/hello.json

```json
{
  "format": "NAPP-APP",
  "schema_version": 1,
  "app": {
    "id": "com.example.hello",
    "name": "Hello NAPP",
    "description": "첫 번째 NAPP 앱",
    "author": "Your Name",
    "category": "utilities",
    "icon": "https://raw.githubusercontent.com/example/NAPP-Repository/main/icons/hello.png",
    "license": "MIT"
  },
  "compatibility": {
    "runtime": "chromium",
    "minimum_runtime_version": "1.0.0",
    "platforms": ["windows-x64"]
  },
  "release": {
    "latest": "1.0.0",
    "versions": {
      "1.0.0": {
        "release_date": "2026-11-20",
        "changelog": "첫 배포",
        "platforms": ["windows-x64"],
        "package": {
          "type": "web",
          "url": "https://github.com/example/NAPP-Repository/releases/download/hello-1.0.0/Hello.ipkg",
          "sha256": "<.ipkg 파일의 SHA-256, 소문자 64자>",
          "size": 123456
        }
      }
    }
  }
}
```

SHA-256 계산:

```bash
sha256sum Hello.ipkg          # Linux/macOS/Git Bash
certutil -hashfile Hello.ipkg SHA256   # Windows
```

### 런처에 등록

**설정 → 저장소 추가**에 표시 이름과 base URL을 넣는다. 예:

```
https://raw.githubusercontent.com/<owner>/<repo>/main/
```

- HTTPS만 허용하고, 리다이렉트는 최대 5회까지 각 hop의 호스트를 검사한다.
- 문서 TTL은 6시간이다. 갱신 직후 GitHub CDN 캐시 때문에 잠시 이전 내용이 보일 수 있다.
- 등록한 저장소 호스트는 자동으로 접근 allowlist에 들어간다.
- `repository.json`이 없으면(404) 스토어 상단에 오류 메시지가 표시되고, 마지막 정상 캐시가 있으면 그 내용을 계속 보여 준다.

---

## 8. UI 수정하기 (QML 구조)

런처 창은 Qt Widgets(`QMainWindow` + `QQuickWidget`)가 띄우고, **모든 화면은 QML**이다.
Widgets는 창·네이티브 대화상자(파일 선택, 권한 확인, 오류 메시지)만 담당한다.

```
launcher/
├── qml/                     화면 (리소스 qrc:/qml/... 로 임베드)
│   ├── Main.qml             내비게이션 레일 + StackLayout + 상태 표시줄 + 앱 상세 오버레이
│   ├── HomePage.qml
│   ├── StorePage.qml
│   ├── LibraryPage.qml
│   ├── RuntimePage.qml
│   ├── SettingsPage.qml
│   └── AboutPage.qml         버전·빌드 정보·서드파티 라이선스
│   └── components/           스타일 컨트롤 + Theme/I18n 싱글턴
├── assets/logo-256.png      레일 로고·창 아이콘(리소스 :/branding/)
└── src/ui/
└── src/ui/
    ├── MainWindow.cpp       서비스 생성, QQuickWidget 연결, 테마 적용
    ├── LauncherBackend.cpp  QML에 노출되는 모델·속성·액션 전부
    └── MapListModel.*       QVariantMap 행을 QML 역할(role)로 노출하는 범용 모델
```

QML에서는 컨텍스트 속성 `backend`로 C++에 접근한다.

```qml
Button {
    text: "새로 고침"
    onClicked: backend.refreshStore()
}
```

### 노출되는 모델 (ListView 모델)

| 속성 | 역할 |
| --- | --- |
| `backend.storeModel` | `appId, name, description, author, category, latest, installedVersion, updateAvailable, supported, iconText` |
| `backend.installedModel` | `appId, name, version, permissionsLabel, permissionList, running, repositoryId, installedDate, updateAvailable` (라이브러리 검색/필터/정렬이 적용된 목록) |
| `backend.updatesModel` | `appId, name, fromVersion, toVersion` |
| `backend.runtimeModel` | `version, chromiumVersion, active, valid, problem, directory, stateLabel` |
| `backend.repositoryModel` | `id, name, baseUrl, repoEnabled, official` |
| `backend.licensesModel` | `name, version, license, url, note` (정보 탭) |

### 노출되는 상태 속성

`currentPage`, `darkMode`, `themeMode`, `accent`, `language`, `checkUpdates`, `statusText`, `busy`,
`progress`, `storeStatusText`, `runtimeStatusText`, `detail`(선택한 앱 정보 맵), `detailVisible`,
`libraryQuery`, `libraryFilter`, `librarySort`,
`appVersion`, `qtVersion`, `buildCompiler`, `buildDate`, `cefVersion`, `platformName`(정보 탭)

### 호출 가능한 액션

`refreshStore`, `setStoreQuery`, `setStoreCategory`, `categories`, `showAppDetails`,
`installApp(appId, version)`, `installFromFile`, `launchApp`, `removeApp`, `openDataFolder`,
`editPermissions`, `activateRuntime`, `removeRuntime`, `registerRuntimeFolder`, `openRuntimeFolder`,
`addRepository(name, url)`(오류 문자열 반환), `removeRepository(id)`, `toggleRepository(id)`,
`stopApp(appId)`, `setTheme("system"|"light"|"dark")`, `setAccent("blue"|"violet"|"mint"|"amber")`,
`setLanguage("en"|"ko")`, `setLibraryQuery`, `setLibraryFilter("all"|"running"|"updates")`,
`setLibrarySort(0|1|2)`, `openExternal(url)`, `setCheckUpdates(bool)`, `setStatus(text)`,
`chooseAppIcon(appId)`, `resetAppIcon(appId)`, `createShortcut(appId)`

### 화면 추가 절차

1. `launcher/qml/MyPage.qml`에 `Rectangle { property var theme ... }` 루트로 작성한다.
2. `Main.qml`의 `StackLayout`에 추가하고, 내비게이션 목록(`Repeater` model)과 `backend.currentPage` 범위를 맞춘다.
3. `launcher/CMakeLists.txt`의 `qt_add_resources(... FILES qml/MyPage.qml)` 목록에 파일을 추가한다.
4. C++ 모델이나 액션이 필요하면 `LauncherBackend`에 `Q_PROPERTY`/`Q_INVOKABLE`로 추가한다. QML에서 직접 서비스를 호출하지 않는다.
5. 디자인 토큰은 `components/Theme.qml` 싱글턴(`Theme.bg/surface/border/text/muted/accent/...`)을 사용한다. 색을 하드코딩하지 않는다.
6. 화면에 보이는 문자열은 `I18n.t("key")` / `I18n.f("key", arg1, arg2)`로 쓴다. 키는 `components/I18n.qml`의 `en`·`ko` 표에 함께 추가한다(영어가 기본).
7. 화면 인덱스는 `0 홈 · 1 스토어 · 2 내 앱 · 3 런타임 · 4 설정 · 5 정보`이며 `LauncherBackend::setCurrentPage`가 0–5만 받는다.

빌드 후 QML 오류는 `napp.log`에 `qml load:` / `qml runtime:`으로 기록된다.

---

## 9. 테스트와 검증 명령

```bat
scripts\build.bat Debug --test                 REM 유닛 테스트 (46개)
python specifications\tools\validate_spec.py   REM 스펙 예제 JSON Schema 검증
bash scripts\smoke-runtime.sh                  REM 두 앱 동시 실행 + 프로필 격리 확인
```

픽스처를 다시 만들려면:

```bash
python tests/fixtures/make_fixtures.py
```

테스트는 임시 디렉터리에서 실행되며 실제 사용자 데이터(`%LOCALAPPDATA%`)를 건드리지 않는다.
`scripts/smoke-runtime.sh`는 `build/cef/runtime-stage`와 `build/smoke`를 사용한다.

---

## 10. 문제 해결

| 증상 | 확인할 것 |
| --- | --- |
| 창이 비어 있음 | `napp.log`의 `module "QtQuick..." is not installed` → `windeployqt --qmldir launcher/qml` 실행 여부(다시 빌드) |
| 스토어가 비어 있음 | `repository.json` 404 여부, base URL 끝의 `/`, 네트워크·프록시, `napp.log`의 `repository ...` 경고 |
| "런타임이 없습니다" | **런타임** 화면에서 등록/활성화 상태 확인, `runtime\1.0.0\libcef.dll` 존재 여부 |
| 앱 창이 뜨지 않음 | `data\<appId>\logs\cef.log`, `runtime.json`의 `platform`이 현재 플랫폼과 같은지 |
| 설치가 `E1006` | 저장소 JSON의 `sha256`이 실제 파일과 다름 |
| 설치가 `E1010` / `E1011` | 패키지에 `..` 경로·절대 경로·심볼릭 링크가 들어 있음 |
| 설치가 `E1013`–`E1016` | 압축 폭탄 방지 상한 초과(엔트리 20,000개, 전체 512MiB, 압축비 200:1 등) |
| 런타임 등록 실패 | `runtime.json`의 `format`/`platform`/`host` 값, 폴더 안에 호스트 실행 파일이 있는지 |

---

## 11. 앱에 노출되는 런타임 환경 (`window.napp`)

앱의 모든 HTML 문서에는 `</head>` 앞에 환경 스크립트가 주입된다. 앱은 사용자에게 언어/테마를 다시 묻지 말고 이 값을 쓴다.

```js
const { language, theme, accent, accentColor, repositoryUrl, permissions } = window.napp;
```

| 필드 | 값 |
| --- | --- |
| `language` | `en` · `ko` (런처 설정) |
| `theme` | `dark` · `light` (시스템 추종이면 현재 OS 값) |
| `accent` / `accentColor` | `blue`/`violet`/`mint`/`amber` 와 `#rrggbb` |
| `repositoryUrl` | 저장소 base URL (문서/콘텐츠를 필요할 때 내려받는 용도) |
| `permissions` | 앱 매니페스트에 선언된 권한 목록 |
| `runtimeVersion`, `apiVersion`, `platform`, `appId` | 런타임/앱 식별자 |

`<html>` 에는 `data-napp-theme`, `data-napp-language`, `color-scheme` 이 함께 설정되고
`--napp-accent`, `--napp-bg`, `--napp-surface`, `--napp-border`, `--napp-text`, `--napp-muted` CSS 변수가 제공된다.
값은 앱 프로세스를 시작할 때 복사되므로, 실행 중에 바꾼 설정은 앱을 다시 열어야 반영된다.

### 웹 권한과 알림

- 카메라·마이크·위치·클립보드 등 웹 권한 요청은 **Chromium 권한 창 없이 호스트가 자동 승인**한다.
- `Notification` API는 런타임 1.1.0부터 **Windows 시스템 알림**으로 연결된다. 매니페스트에 `notifications` 권한을 선언하고 사용자가 승인한 앱만 보낼 수 있고(그 외에는 `Notification.permission === 'denied'`), 10초에 5개로 제한된다. `window.napp.notify(title, body)` 로 바로 보낼 수도 있다. Chromium 자체 알림/권한 창은 뜨지 않는다.
- 팝업(`window.open`, `target=_blank`), 우클릭 메뉴, `alert/confirm/prompt`, 인쇄 미리보기, 다운로드 저장 창 등 **Chromium이 띄우는 UI는 모두 차단**된다. `dialog` 권한이 있는 앱의 `alert/confirm/prompt`는 Chromium 대신 OS 대화상자로, 다운로드는 앱 데이터의 `downloads` 폴더로 조용히 저장된다. `external-links` 권한이 있으면 http(s) 링크는 시스템 브라우저로 열린다.
- `.ipkg` 파일을 더블클릭하면 런처가 열려 설치 확인과 권한 승인을 시스템 대화상자로 묻는다.
- 앱 페이지에는 DevTools를 붙일 수 없다(`remote_debugging_port=0`, `disable-dev-tools`).
- 파일 저장은 `POST /__host/export/<name>` 를 쓴다(아래 12절). 매니페스트의 `window` 블록으로 창 제목·시작 크기·트레이 아이콘을 지정할 수 있다(런타임 1.3.0+, `window` 권한 필요).
- `POST /__host/pick-folder` 는 OS 폴더 선택 대화상자를 열고, 사용자가 고른 폴더만 `POST /__host/export/<name>?folder=<경로>` 의 저장 대상으로 허용한다. 페이지가 임의 경로에 쓰는 것은 403으로 거부된다.
- `window.tray: true` 인 앱은 창을 닫아도 트레이에 남고, 트레이 메뉴의 **열기/종료**로 조작한다.

## 12. 배포: 설치와 포터블

```bat
scripts\build-cef.bat --dist
powershell -ExecutionPolicy Bypass -File scripts\make-release.ps1
```

- `scripts\build-cef.bat --dist` (또는 `scripts\make-dist.ps1`) → 빌드 한 번으로 배포 파일까지 만든다.
- `scripts\make-dist.ps1` 결과
  - `dist\NAPP-<버전>-Setup.exe` — **단일 파일 설치 프로그램**. 실행하면 설치/포터블을 고르고, 앱 아이콘·바로 가기·제거 항목을 만든다. 관리자 권한 불필요.
    - `/S` 없이도 무인 설치 가능: `NAPP-Setup.exe --install "D:\Apps\NAPP" --desktop --quiet`,
      `NAPP-Setup.exe --portable "D:\NAPP" --quiet`
  - `dist\NAPP-<버전>-portable.zip` — 압축을 풀면 `payload\NAPP.exe` 로 바로 실행
  - `dist\NAPP-<버전>\` — payload + PowerShell 설치 스크립트(수동 배포·디버깅용)
  - payload에는 `NAPP.exe`, Qt DLL/QML 모듈, `runtime\<버전>\`(CEF 런타임, 현재 1.3.0), `docs\`, `Uninstall.ps1` 이 들어간다.
  - 런처는 시작할 때 `NAPP.exe` 옆의 `runtime\<버전>\` 을 자동 등록하므로, 새 PC에서 런타임을 수동 등록할 필요가 없다.
- 설치 프로그램과 `dist\NAPP-<버전>\NAPP-Setup.cmd` 모두 **설치** 또는 **포터블**을 고른다.
  - 설치: `%LOCALAPPDATA%\Programs\NAPP`, 시작 메뉴 바로 가기, 선택 시 바탕 화면 바로 가기, 앱 및 기능 제거 항목, `.ipkg` 연결 (관리자 권한 불필요)
  - 포터블: 폴더 복사만. 레지스트리·바로 가기 없음. `PORTABLE.txt` 표시
  - 제거: 설치 폴더의 `Uninstall.ps1` (앱 데이터 삭제 여부를 따로 묻는다)
- `scripts\make-release.ps1` → `release\repository\`(GitHub raw 업로드용 트리, `docs/` 포함),
  `release\apps\`(빌드된 `.ipkg`). 설치 프로그램과 포터블 ZIP은 `dist\` 에서 GitHub 릴리스에 첨부한다.

## 13. 현재 제한 사항

- **CEF 샌드박스 비활성**: 프로세스는 앱별로 분리되지만 Chromium 샌드박스는 아직 켜져 있지 않다. 신뢰할 수 없는 앱을 배포하기 전에 해결해야 한다.
- Ed25519 서명 검증 없음(SHA-256 무결성만 확인).
- 자동 업데이트 없음(알림 + 수동 설치).
- `window.napp` 은 읽기 전용이다. 창 제목/크기는 매니페스트 `window` 블록으로만 정할 수 있고 페이지에서 실행 중에 바꾸는 웹 API는 없다. 네이티브 대화상자는 폴더 선택만 제공한다.
- 스토어 아이콘은 저장소 이미지를 내려받지 않고 글자 타일로 표시한다.
- Linux 빌드·실행 미검증.
- 공식 저장소(`ILLIERTION/NAPP-Repository`)가 아직 없어 기본 상태에서는 404가 표시된다. 정상 동작이다.

---

## 14. 화면 구성 요소 (launcher/qml/components)

모든 컨트롤은 기본 스타일을 그대로 쓰지 않고 `background`/`contentItem`을 직접 지정한다.
(플랫폼 스타일이 빠지면 빈 회색 사각형만 남는 문제가 있었기 때문이다.)

| 파일 | 역할 |
| --- | --- |
| `Theme.qml` (싱글턴, `qmldir` 등록) | 팔레트 전체. `Theme.setDark()`/`Theme.setAccent()`로 전 화면이 전환된다 |
| `I18n.qml` (싱글턴, `qmldir` 등록) | 영어/한국어 문자열 표. `I18n.setLanguage()` 즉시 반영(재시작 불필요) |
| `NappButton.qml` | 버튼 — `variant: primary / secondary / ghost / danger`, `small` |
| `NappTextField.qml` | 입력창 (포커스 시 강조 테두리) |
| `NappComboBox.qml` | 드롭다운 (팝업 목록·항목 강조 포함) |
| `NappCheckBox.qml`, `NappProgress.qml` | 체크박스, 얇은 진행 막대 |
| `AppCover.qml` | 아이콘 대신 쓰는 그라데이션 커버(앱 이름 첫 글자) |
| `SectionHeader.qml`, `EmptyState.qml` | 구역 제목, 빈 화면 안내 |

`LauncherBackend`는 목록 모델 외에 개수 속성도 제공한다:
`storeCount`, `installedCount`, `updatesCount`, `runtimeCount`, `repositoryCount`.
`QAbstractItemModel`에는 QML `count` 속성이 없으므로 QML에서 `model.count`를 쓰면 `undefined`가 된다.

테마는 `main.cpp`에서 `QT_QUICK_CONTROLS_STYLE=Basic`으로 고정하고, `Main.qml`이
`backend.darkMode`를 `Theme.setDark()`로 전달한다.

---

## 15. UI 스냅샷 (레이아웃 확인용)

네이티브 창은 화면 캡처 도구로 잡히지 않으므로, 렌더 결과를 PNG로 저장하는 디버그 옵션이 있다.

```bat
build\dev\NAPP.exe --ui-snapshot build\ui-snap
```

`build\ui-snap\page-0.png` ~ `page-5.png` (홈/스토어/내 앱/런타임/설정/정보)가 생성되고 자동 종료된다.
현재 저장된 예시: `docs/screenshots/`.

페이지 전환은 런타임 스캔 등으로 잠시 멈출 수 있으므로 캡처 간격을 900ms로 두었다.
간격이 짧으면 이벤트 루프가 막혀 여러 페이지가 같은 프레임으로 찍힌다.

---

## 16. 언어·테마·정보 탭

### 언어

- 기본값은 **영어(en)** 이고, 설정 → `Language` 에서 **한국어(ko)** 로 바꿀 수 있다.
- 선택값은 `config/settings.json`의 `language`에 저장되고, 전환은 즉시 반영된다(재시작 불필요).
- QML 문자열은 `components/I18n.qml`, C++ 네이티브 대화상자·모델 라벨은 `LauncherBackend`의
  `pick(language, english, korean)`을 쓴다. 새 문자열은 **양쪽 표에 모두** 추가해야 한다.

### 테마

| 선택 | 동작 |
| --- | --- |
| 시스템 설정 따르기 | Windows 밝게/어둡게 변경을 `QStyleHints::colorSchemeChanged`로 받아 즉시 반영 |
| 밝게 / 어둡게 | 런처에만 고정. Qt의 앱 색상 스킴을 고정하므로 OS 변경을 덮어쓴다 |

- `system` 모드에서는 `applyTheme()`이 `styleHints()->unsetColorScheme()`으로 OS 추종을 되돌린다.
  (고정 상태로 두면 이후 OS 테마 변경이 전달되지 않는 버그가 있었다.)
- 강조 색은 파랑/보라/민트/주황 4가지이며 버튼·강조선·네이티브 대화상자에 함께 적용된다.

### 정보 탭

- 실행 중인 바이너리 기준 버전(`0.1.0`), Qt 버전, CEF/Chromium 버전, 컴파일러, 플랫폼, 빌드 날짜를 보여 준다.
- 라이선스 목록: NAPP Launcher, Qt(LGPL-3.0/상용), CEF(BSD-3-Clause), Chromium(BSD-3-Clause + third-party),
  miniz(MIT). 링크가 있는 항목은 `Open licence`로 기본 브라우저에서 연다.
- 이 저장소에는 `LICENSE` 파일이 없으므로 NAPP 자체는 **독점(© 2026 ILLIERTION)** 으로 표기한다.
  배포 정책을 정했다면 `LICENSE`를 추가하고 `LauncherBackend::buildLicenses()`의 첫 항목을 고친다.

### 창 아이콘

`logo.PNG`(1254×1254)를 256/64px로 줄여 `launcher/assets/logo-256.png`에 두고,
`qt_add_resources(... PREFIX "/branding")`로 바이너리에 넣는다. 레일 로고·정보 탭·창 아이콘
(`:/branding/logo-256.png`)이 같은 파일을 쓴다. 로고를 바꾸려면 원본을 교체한 뒤 다시 줄여서 넣는다.
