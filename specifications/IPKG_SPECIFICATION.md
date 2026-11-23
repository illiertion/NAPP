# IPKG 규격 v1 (Draft)

- 제품: NAPP (ILLIERTION)
- 문서 상태: **Draft 1 — 승인 대기**
- 작성일: 2026-11-20
- 대상 독자: NAPP Launcher / IPKG Studio / NAPP Web Runtime 개발자, 앱 제작자
- 관련 파일
  - `schemas/manifest.schema.json` (IPKG 내부 매니페스트)
  - `schemas/repository.schema.json` (저장소 인덱스)
  - `schemas/app.schema.json` (저장소 개별 앱 매니페스트)
  - `schemas/categories.schema.json` (카테고리)
  - `examples/` (스키마를 통과하는 예제)

이 문서는 **구현 전에 확정해야 하는 계약**이다. 런처, IPKG Studio, SDK는 모두 이 문서를 단일 기준으로 삼는다.
스키마와 문서가 충돌하면 스키마를 우선하고, 스키마가 표현하지 못하는 의미론적 규칙은 이 문서의 해당 절을 따른다.

---

## 1. 범위

이 규격은 다음을 정의한다.

1. `.ipkg` 컨테이너의 물리적 형식과 제약
2. 패키지 내부 디렉터리 구조와 `manifest.json`
3. 패키지 검증 절차와 오류 코드
4. 설치 후 디스크 레이아웃과 앱 데이터 분리 규칙
5. GitHub Raw/Releases 기반 저장소의 JSON 형식과 버전 관리 규칙
6. 패키지 무결성(SHA-256)과 향후 전자서명(Ed25519) 적용 지점

이 규격이 정의하지 **않는** 것: 런처 UI, 런타임(CEF) 임베딩 방식, SDK API 표면(별도 문서).

---

## 2. 용어

| 용어 | 의미 |
| --- | --- |
| IPKG | NAPP 애플리케이션 패키지 파일 (`.ipkg`) |
| 매니페스트 | IPKG 내부의 `manifest.json` |
| 저장소 | `repository.json`을 진입점으로 하는 HTTPS 정적 JSON 트리 |
| 앱 ID | 역DNS 소문자 식별자 (`com.illiertion.example`) |
| 런타임 | NAPP Web Runtime (Chromium/CEF 기반) |
| 앱 데이터 | 앱 프로그램 파일과 분리된 사용자 데이터 영역 |

---

## 3. 컨테이너 형식

### 3.1 기본

- `.ipkg`는 **ZIP 컨테이너**이며, 확장자만 `.ipkg`이다.
- 압축 방식: `stored(0)` 또는 `deflate(8)`만 허용. 그 외(예: bzip2, LZMA)는 `E1007`로 거부한다.
- ZIP 암호화(전통 암호화, AES)는 금지한다(`E1008`).
- ZIP64는 허용한다(대용량 엔트리 대응). 단 §3.2 상한이 우선한다.
- 파일명은 **UTF-8**로 기록하고 general purpose bit 11(EFS)을 설정한다. EFS 미설정 + 비ASCII 이름은 `E1009`.
- 경로 구분자는 `/`만 사용한다. `\`가 포함된 엔트리는 `E1010`.
- 디렉터리 엔트리(`.../`)는 허용하나 필수가 아니다. 파서는 디렉터리 엔트리에 의존하지 않는다.
- **심볼릭 링크, 하드 링크, 장치 파일, 특수 파일 금지**(`E1011`). 외부 속성(external attributes)으로 UNIX 파일 모드가 있더라도 일반 파일/디렉터리만 허용한다.
- 동일 경로의 **중복 엔트리 금지**(`E1012`).
- 패키지 루트에 `manifest.json`이 **반드시** 있어야 한다(`E1003`). 루트에 단일 최상위 폴더로 감싸는 형태(`MyApp/manifest.json`)는 **불허**한다.

### 3.2 자원 상한 (압축 폭탄 방지)

| 항목 | 상한 | 위반 코드 |
| --- | --- | --- |
| 엔트리 수 | 20,000 | `E1013` |
| 단일 엔트리 해제 크기 | 128 MiB | `E1014` |
| 전체 해제 크기 | 512 MiB | `E1015` |
| 단일 엔트리 압축비 | 200:1 | `E1016` |
| 전체 압축비 | 100:1 | `E1016` |
| 경로 길이 | 512 byte (UTF-8) | `E1010` |

상한은 선언된 값이 아니라 **실제 스트리밍 해제량**으로 집계한다. 해제 중 상한 초과가 확인되면 즉시 중단하고 부분 산출물을 삭제한다.

### 3.3 파일명 규칙

다음은 `E1010`으로 거부한다.

- 절대 경로, 드라이브 문자(`C:`), UNC 경로
- `..` 또는 `.` 세그먼트, 빈 세그먼트(`a//b`), 후행 `/`가 아닌 경로의 말미 공백/점
- Windows 예약 이름(`CON`, `PRN`, `AUX`, `NUL`, `COM1..9`, `LPT1..9`, 확장자 포함)
- 문자 `< > : " | ? *` 및 코드포인트 0x00–0x1F
- Windows에서 대소문자만 다른 두 경로(`A.txt`/`a.txt`) — 두 OS에서 동일하게 설치되도록 `E1012`로 처리

---

## 4. 표준 디렉터리 구조

```
MyApp.ipkg
├── manifest.json          (필수)
├── metadata/              (필수: icon, preview)
│   ├── icon.png           512x512 권장 PNG
│   ├── preview.png        16:9 권장 PNG
│   └── screenshots/*.png  (선택, 최대 8)
├── app/                   (필수)
│   ├── index.html         entry
│   ├── styles/
│   ├── scripts/
│   ├── components/
│   └── assets/
└── resources/             (선택: 폰트, 이미지 등 앱 실행 자원)
```

- `manifest.entry`는 `app/` 하위여야 한다(스키마에서 강제).
- `metadata/icon`, `metadata/preview`, `window/icon`는 패키지 내 실존 파일이어야 한다(`E1019`).
- 아이콘/미리보기 형식: PNG 또는 WebP. 실행 시점에 디코딩 실패하면 설치를 중단하지 않고 기본 아이콘으로 대체한다(경고 `W2001`).
- 앱 코드가 `app/` 상위 경로를 참조하는 것은 런타임에서 금지한다(§12.3).

---

## 5. manifest.json

`format_version` 1의 전체 스키마는 `schemas/manifest.schema.json`에 있다.

| 필드 | 타입 | 필수 | 의미 |
| --- | --- | --- | --- |
| `format` | `"IPKG"` | ✔ | 상수 |
| `format_version` | 정수 `1` | ✔ | 규격 major 버전 |
| `app.id` | string | ✔ | 역DNS 소문자, 2개 이상 세그먼트, 최대 128자 |
| `app.name` | string | ✔ | 표시 이름, 1–80자 |
| `app.version` | semver | ✔ | 패키지 버전 |
| `app.description` | string | ✔ | 1–4000자 |
| `app.author` | string | ✔ | 제작자 표시명 |
| `app.license` | string | ✔ | SPDX 식별자 또는 라이선스명 |
| `app.homepage` | https URL | ✖ | |
| `app.keywords` | string[] | ✖ | 최대 12개, 검색 보조 |
| `runtime.type` | `"chromium"` | ✔ | v1은 chromium만 허용 |
| `runtime.minimum_version` | semver | ✔ | 요구 런타임 최소 버전 |
| `sdk.minimum_version` | semver | ✔ | 요구 NAPP Web SDK 최소 버전 |
| `entry` | 상대 경로 | ✔ | `app/` 하위 시작 페이지 |
| `platforms` | enum[] | ✔ | 지원 플랫폼, 최소 1개 |
| `permissions` | enum[] | ✔ | 요청 권한(빈 배열 허용) |
| `metadata.icon` | 상대 경로 | ✔ | |
| `metadata.preview` | 상대 경로 | ✔ | |
| `metadata.screenshots` | 상대 경로[] | ✖ | 최대 8 |
| `window.title` | string | ✖ | 창 제목, 1–80자. 없으면 `app.name` |
| `window.width` | integer | ✖ | 시작 너비, 480–3840 (화면 작업 영역에 맞춰 줄어든다) |
| `window.height` | integer | ✖ | 시작 높이, 360–2160 |
| `window.icon` | 상대 경로 | ✖ | 창·작업 표시줄·트레이 아이콘. PNG 또는 WebP, 256px 이상 권장 |
| `window.tray` | boolean | ✖ | `true` 면 창을 닫아도 트레이에 남는다 |
| `extensions` | object | ✖ | 향후 확장 예약. v1 런처는 보존만 하고 해석하지 않는다 |

`permissions` 허용 값: `storage`, `window`, `notifications`, `dialog`, `filesystem`, `clipboard`, `external-links`, `system-info`.
선언되지 않은 권한에 해당하는 SDK 호출은 런타임이 `PermissionDenied`로 거부한다.

`window` 블록은 선택 사항이며, 사용하려면 `permissions` 에 `window` 가 있어야 한다(없으면 설치를 거부한다).
설치할 때 사용자가 `window` 권한을 거부하면 앱은 기본 제목·기본 크기·기본 아이콘·트레이 없이 실행된다.
트레이를 쓰는 앱에서 창을 닫으면 프로세스는 살아 있고, 트레이 메뉴의 **종료**로 끝낸다.
`window`·`tray`는 런타임 1.3.0 이상이 필요하고, `window.icon`은 런타임 1.4.0 이상이 필요하다(이전 런타임은 `icon` 키를 알 수 없는 필드로 거부하므로 함께 올려야 한다).

`window.icon`은 런타임 호스트의 창 아이콘, 작업 표시줄 단추, Alt+Tab, 트레이 아이콘에 모두 쓰인다.
지정하지 않거나 이미지를 디코딩하지 못하면 `napp.ico`(NAPP 기본 아이콘)로 대체하고 `W2001` 경고만 남긴다(실행은 계속된다).
`metadata.icon`은 스토어 타일·바로가기용이고, `window.icon`은 실행 중인 창의 아이콘이다. 둘을 다르게 두면 스토어 미리보기와 실제 창을 각각 꾸밀 수 있다.

### 5.1 platform 식별자

`{os}-{arch}` 형식. v1 허용 값: `windows-x64`, `windows-arm64`, `linux-x64`, `linux-arm64`, `macos-x64`, `macos-arm64`.
`macos-*`는 규격상 예약이며, macOS 런처가 배포되기 전까지 설치 대상이 아니다.

---

## 6. 검증 절차와 오류 코드

런처/IPKG Studio는 아래 순서를 **그대로** 수행한다. 앞 단계 실패 시 뒤 단계는 수행하지 않는다.

1. 파일 크기 상한(§3.2) 및 ZIP 시그니처 확인 → `E1001`, `E1002`
2. 엔트리 목록 검사: 경로 규칙, 중복, 링크/특수파일, 압축 방식 → `E1007`–`E1012`
3. `manifest.json` 존재 및 JSON 파싱 → `E1003`, `E1004`
4. 매니페스트 JSON Schema 검증 → `E1005`
5. 의미 검증: entry 실존, icon/preview/window.icon 실존, 플랫폼 비어있지 않음 → `E1018`, `E1019`, `E1020`
6. SHA-256 검증(다운로드 경로일 때, 저장소 JSON의 값과 비교) → `E1006`
7. 버전/ID 일관성(저장소 JSON의 id·version과 매니페스트 일치) → `E1021`
8. 플랫폼 호환성 → `E1022`
9. (서명 도입 시) Ed25519 검증 → `E1023`
10. 안전 압축 해제(§7) → `E1015`, `E1017`

| 코드 | 의미 |
| --- | --- |
| `E1001` | ZIP 시그니처 불일치 / 손상된 컨테이너 |
| `E1002` | 허용되지 않는 압축 방식 |
| `E1003` | `manifest.json` 없음 |
| `E1004` | 매니페스트 JSON 문법 오류 |
| `E1005` | 매니페스트 스키마 위반 |
| `E1006` | SHA-256 불일치 |
| `E1007` | 암호화된 엔트리 |
| `E1008` | (예약) 암호화 방식 오류 |
| `E1009` | 파일명 인코딩 오류 |
| `E1010` | 안전하지 않은 경로 |
| `E1011` | 링크/특수 파일 |
| `E1012` | 중복 경로 |
| `E1013`–`E1016` | 자원 상한 초과 (§3.2) |
| `E1017` | 해제 실패(디스크, 권한) |
| `E1018` | `entry` 파일 없음 |
| `E1019` | icon/preview/window.icon 파일 없음 |
| `E1020` | 플랫폼/권한 값 오류 |
| `E1021` | 저장소 JSON과 패키지 정보 불일치 |
| `E1022` | 현재 OS/아키텍처 미지원 |
| `E1023` | 서명 검증 실패 |
| `W2001` | 아이콘 디코딩 실패(경고, 설치는 계속) |

---

## 7. 설치 후 레이아웃

앱 프로그램 파일과 앱 데이터는 **물리적으로 분리**한다. 제거/업데이트 시 프로그램 파일만 교체한다.

Windows:

```
%APPDATA%\ILLIERTION\NAPP\config\        설정, 저장소 목록, 레지스트리
%LOCALAPPDATA%\ILLIERTION\NAPP\
  ├── apps\<appId>\<version>\            해제된 패키지 (읽기 전용 취급)
  ├── apps\<appId>\current              현재 버전을 가리키는 포인터 파일(버전 문자열)
  ├── data\<appId>\                     앱 데이터 (제거 시 사용자 선택)
  ├── cache\                            저장소/아이콘/다운로드 캐시
  ├── staging\                          설치·업데이트 임시 영역
  └── runtime\<runtimeVersion>\         NAPP Web Runtime
```

Linux (XDG):

```
$XDG_CONFIG_HOME/illiertion/napp/config/
$XDG_DATA_HOME/illiertion/napp/{apps,data,cache,staging,runtime}/
```

설치 트랜잭션 규칙:

1. `staging/<appId>/<version>.<random>`에 해제
2. 검증 완료 후 `apps/<appId>/<version>/`로 **원자적 rename**
3. `current` 포인터 갱신(마지막 단계)
4. 실패 시 staging만 삭제하고 `current`는 건드리지 않는다 → 기존 설치가 그대로 동작

`config`의 `registry.json`(런처 내부 형식, 규격 외)은 설치된 앱 ID, 버전, 출처 저장소 ID, 설치 시각, 사용 권한 승인 상태를 기록한다.
`registry.json`은 사용자 편집 대상이 아니며 손상 시 앱 디렉터리를 스캔해 복구한다.

---

## 8. 무결성과 서명

- **SHA-256**: `.ipkg` 파일 전체 바이트에 대한 다이제스트. 소문자 16진수 64자. 저장소 JSON과 런처 캐시가 이 값을 보관한다.
- SHA-256은 **전송 무결성**만 보장하며 작성자 인증을 대신하지 않는다. 이 사실을 UI에 명시한다.
- **Ed25519**(향후, Phase 7):
  - 서명 대상: `.ipkg` 파일 해시 + 매니페스트의 `app.id`, `app.version`, `entry`, `permissions`를 정규화한 바이트열
  - 서명은 저장소 JSON(`package.signature`) 또는 별도 `.sig`로 배포
  - 신뢰 공개키는 런처에 내장된 trust store로 관리하고, 저장소별 신뢰 수준(공식/서드파티/미검증)을 UI에 표시
  - 서명 부재는 "미검증"이며 차단 사유가 아니다. 서명 존재 + 검증 실패만 차단한다

---

## 9. 저장소 규격

### 9.1 접근 규칙

- 모든 요청은 **HTTPS 전용**.
- 허용 호스트(리다이렉트 포함): `raw.githubusercontent.com`, `github.com`, `objects.githubusercontent.com`, 그리고 사용자가 등록한 저장소의 호스트.
- GitHub Releases URL은 302 리다이렉트를 반환한다. 리다이렉트는 최대 5회, 각 hop의 호스트를 allowlist로 검사한다.
- `repository.json` TTL 기본 6시간, 개별 앱 JSON 6시간, 아이콘 7일. 오프라인 시 마지막 정상 캐시를 사용하고 UI에 "오프라인/캐시됨"을 표시한다.
- GitHub Raw는 CDN 캐시(수 분)가 있으므로, 갱신 직후 반영 지연을 오류로 취급하지 않는다.

### 9.2 repository.json

`schemas/repository.schema.json`. `apps[].id`는 유일해야 하며, 중복 시 해당 저장소 전체를 오류로 처리한다(`repository.id` + `app.id` 충돌은 저장소 단위로 격리).
`apps[].category`는 `categories.json`의 id를 참조한다.

### 9.3 개별 앱 JSON

`schemas/app.schema.json`.

- `release.latest`는 `release.versions`의 키 중 하나여야 한다(`E1021` 계열 저장소 검증 오류).
- 모든 배포 버전을 보존한다. 이전 버전 설치는 허용한다(다운그레이드는 사용자 확인 필요).
- `release.versions.<v>.yanked: true`인 버전은 신규 설치와 업데이트 후보에서 제외한다.
- `release.versions.<v>.platforms`가 있으면 `compatibility.platforms`보다 우선한다.

### 9.4 categories.json

`schemas/categories.schema.json`. `repository.json`의 `categories` 필드로 경로를 지정하며, 생략 시 카테고리 UI를 사용하지 않는다.

### 9.5 저장소 신뢰와 앱 ID 소유권

- 앱 ID의 **최초 설치 출처 저장소**가 이후 업데이트 출처가 된다(origin binding).
- 다른 저장소가 같은 앱 ID를 제공하면 "다른 출처" 경고 후 사용자가 명시적으로 전환해야 한다.
- 저장소는 사용자가 추가/비활성/삭제할 수 있고, 신뢰 수준(공식/서드파티)은 사용자 설정이 아니라 저장소 등록 시 부여한다.

---

## 10. 버전과 업데이트 판정

- 버전 비교는 **Semantic Versioning 2.0.0** precedence 규칙을 따른다(빌드 메타데이터 `+`는 비교에서 무시).
- 업데이트 존재: `installed.version < latest` 이고 `latest`가 현재 플랫폼을 지원하고 `yanked`가 아닐 때.
- 런타임 최소 버전 미충족 시 "런타임 업데이트 필요" 상태로 표시하고 설치 버튼을 비활성화한다.
- 앱 데이터(`data/<appId>`)는 업데이트/다운그레이드에서 **보존**한다. 스키마 마이그레이션은 앱 책임이며 SDK가 버전 필드를 제공한다.
- 자동 업데이트 정책(Phase 7): 기본 "알림만", 사용자가 앱별/전체 자동 적용을 선택할 수 있다.

---

## 10.1 NAPP Web Runtime 배포 형식 (구현 완료)

런타임은 앱과 별도로 설치되며, 런처와 같은 방식으로 검증 가능한 디렉터리로 배포한다.

```
<runtime>/1.0.0/
├── runtime.json          (아래 필수)
├── napp-runtime-host.exe
├── libcef.dll, *.pak, icudtl.dat, locales/  (CEF 리소스)
└── Qt6*.dll, platforms/, imageformats/      (Qt 런타임)
```

`runtime.json`:

| 필드 | 타입 | 필수 | 의미 |
| --- | --- | --- | --- |
| `format` | `"NAPP-RUNTIME"` | ✔ | 상수 |
| `version` | semver | ✔ | 런타임 버전(활성화/호환성 판정 기준) |
| `chromium_version` | string | ✔ | 표시용 Chromium 버전 |
| `platform` | platform id | ✔ | "windows-x64" 등. 현재 플랫폼과 다르면 등록 거부 |
| `host` | 상대 경로 | ✔ | 호스트 실행 파일 이름(기본 `napp-runtime-host.exe`) |
| `engine` | string | ✖ | `cef` |
| `cef_version` | string | ✖ | CEF 배포물 버전 |

설치 위치: `runtime/<version>/`. 활성 버전은 `config/runtime-current`에 기록한다.
호스트 인자는 런처가 결정한다: `--app-id`, `--app-name`, `--app-root`, `--entry`,
`--data-dir`, `--permissions`, `--runtime-config`. 호스트는 인자를 다시 검증하며,
상대 경로 데이터 디렉터리를 받으면 절대 경로로 정규화한다(CEF는 상대 캐시 경로를 거부하고
인메모리 저장소로 폴백하므로 격리가 깨진다).

앱 실행 격리(구현): 앱 1개당 호스트 프로세스 1개, `data/<appId>/web` 프로필, `napp://<appId>/`
스킴만 로드, 스킴 핸들러가 `app/` 밖 경로를 403으로 차단한다.

---

## 11. 버전 관리 및 하위 호환

- `format_version`은 정수 major만 증가한다. 가산적 변경(새 선택 필드, 새 권한 값)은 `extensions` 또는 하위 허용 규칙으로 처리하며 major를 올리지 않는다.
- v1 런처는 `extensions`를 보존하고 무시한다. 알 수 없는 최상위 필드는 스키마상 금지이므로, 미래 필드는 반드시 `extensions`에 넣는다.
- 새 `permissions` 값이 추가되면 v1 런처는 해당 패키지를 **설치 거부**한다(권한을 조용히 무시하지 않는다). 이는 `E1020`으로 처리한다.

---

## 12. 보안 요구사항 (규격 수준)

### 12.1 패키지

- §3.2 상한, §3.3 경로 규칙, §6 검증 순서를 우회하는 설치 경로를 만들지 않는다.
- 해제는 반드시 "검증된 경로를 다시 계산해 루트 내부임을 확인"한 뒤 수행한다(경로 탈출 방지 이중 확인).

### 12.2 저장 공간·세션 격리

- 앱마다 별도 저장 공간과 별도 쿠키/캐시 파티션을 사용한다. 앱 A가 앱 B의 데이터에 접근할 수 있는 API를 제공하지 않는다.

### 12.3 실행 격리

- 앱 페이지는 `app/` 디렉터리 밖(`resources/` 포함 상위)을 URL로 참조할 수 없다.
- 앱 간 이동, 외부 사이트로의 탐색은 기본 차단하며, `external-links` 권한이 있으면 **시스템 기본 브라우저**로 위임한다(앱 내부 탐색 아님).
- API Bridge는 앱 페이지에만 주입하고, 외부 오리진 문서에는 주입하지 않는다.

---

## 13. 미결 사항 (승인 필요)

1. `permissions`에 `filesystem`(사용자가 선택한 파일 읽기/쓰기)을 v1에 포함할지, Phase 6으로 미룰지.
2. Ed25519 도입 시점과 신뢰 공개키 배포/폐기 경로.
3. 앱 1개 최대 크기 512 MiB 상한의 적정성(게임/미디어 앱 고려).
4. `metadata.preview` 필수 여부(최소 패키지 제작 부담 vs 스토어 품질).
5. 다국어 표시 이름/설명(`extensions.localization`) 지원 시점.
