# NAPP 아키텍처 (Draft 1)

- 작성일: 2026-11-20
- 상태: **승인 대기** — 코드 작성 전 확정이 필요한 구조 결정 포함
- 함께 볼 문서: `specifications/IPKG_SPECIFICATION.md`, `docs/IMPLEMENTATION_PLAN.md`

---

## 1. 컴포넌트 경계

```
┌──────────────────────────────────────────────────────────────┐
│ NAPP Launcher (Qt 6 / C++20, 단일 네이티브 프로세스)          │
│                                                              │
│  ui/            홈, 스토어, 앱 관리, 런타임 관리, 설정        │
│  core/          앱 레지스트리, 설치 트랜잭션, 실행 조정        │
│  store/         저장소 인덱스, 앱 JSON, 버전/업데이트 판정     │
│  package/       IPKG 파서·검증·해제, SHA-256, 서명            │
│  network/       비동기 다운로드, 캐시, allowlist, 재시도       │
│  security/      경로 검증, 권한 저장소, trust store           │
│  runtime/       런타임 설치/버전 관리, 프로세스 수명주기       │
│  settings/      QSettings 기반 구성, 테마/언어                 │
└───────────────┬──────────────────────────────────────────────┘
                │ 로컬 IPC (QLocalSocket, JSON-RPC 2.0)
                ▼
┌──────────────────────────────────────────────────────────────┐
│ NAPP Web Runtime Host (CEF 기반, 앱별 프로세스)               │
│                                                              │
│  runtime-host   CefInitialize 1회 + CefBrowser 1개           │
│                 + API Bridge(native → JS) + RequestContext   │
└──────────────────────────────────────────────────────────────┘
                ▲
                │ window.NAPP (주입된 브리지)
                │
        IPKG 앱 (HTML/CSS/JS/WASM, app/ 디렉터리)
```

`ipkg-studio`는 런처와 완전히 독립 실행된다. 런처의 `package/` 모듈은 **공유 정적/동적 라이브러리**(`libnappipkg`)로 분리해 Studio가 같은 검증 코드를 재사용한다. 이는 "규격을 한 곳에서만 구현"하기 위한 결정이다.

---

## 2. 런처 설계

### 2.1 UI 계층

| 계층 | 기술 | 이유 |
| --- | --- | --- |
| 창/셸/네비게이션/목록 | Qt Widgets | 네이티브 위젯, 성숙한 접근성·단축키·DPI 처리 |
| 스토어 상세, 대시보드 카드 등 시각 요소 | Qt Quick (QQuickWidget) | 애니메이션과 카드 레이아웃 |
| NAPP 앱 실행 화면 | CEF (별도 프로세스 임베드) | 웹 앱 실행 |

Widgets와 Quick을 **혼용**하되, 한 화면 안에서 두 계층을 겹쳐 쓰지 않는다. QQuickWidget는 렌더링 비용이 있으므로 목록형 화면은 Widgets로 유지한다.
모델은 Qt 모델(`QAbstractListModel`)로 두고, Widgets/QML 어느 쪽에서도 같은 모델을 본다 → UI 교체가 로직을 건드리지 않는다.

### 2.2 로직 계층

- 모든 네트워크·파일 작업은 비동기(`QNetworkAccessManager`, `QtConcurrent` + 취소 토큰). UI 스레드에서 블로킹 I/O 금지.
- `AppRegistry`(설치 상태), `RepositoryService`(원격 인덱스), `InstallService`(트랜잭션), `RuntimeService`(런타임)를 각각 독립 클래스로 두고 시그널로 UI에 통보한다.
- 설치 파이프라인은 상태 기계(다운로드 → 해시 → 검증 → 해제 → 등록)로 구현하고 각 단계가 취소·재시도 가능하다.

---

## 3. 런타임 모델 — 가장 중요한 결정

### 3.1 요구사항 충돌 (정확히 짚어야 하는 부분)

원 요구사항은 ① 하나의 Chromium Runtime을 여러 앱이 공유하고 ② 앱별 독립 브라우저 인스턴스·데이터 격리·백그라운드 관리가 되어야 한다고 말한다.
그러나 **CEF는 프로세스당 `CefInitialize`를 한 번만 호출할 수 있고**, 한 프로세스 안의 여러 `CefBrowser`는 같은 브라우저 프로세스를 공유한다. 즉:

| 모델 | 런타임 초기화 | 앱 간 OS 수준 격리 | 메모리 특성 | 구현 난이도 |
| --- | --- | --- | --- | --- |
| **A. 단일 호스트 프로세스 + 다중 CefBrowser** | 1회 | ❌ (논리적 격리만: RequestContext/파티션) | 기준선이 가장 낮음 | 낮음 |
| **B. 앱별 호스트 프로세스 + 공유 바이너리** | 프로세스마다 1회 | ✅ (별도 프로세스, 별도 파티션) | 앱 수에 비례해 증가 | 중간 |
| **C. 하이브리드** | 둘 다 | 기본 B, 경량 앱 A | 정책 의존 | 높음 |

**결정: 모델 B로 구현 완료** (앱별 호스트 프로세스 + 공유 CEF 바이너리). 모델 A는 측정 후 필요할 때만 검토한다.

구현 세부(2026-11-20): CEF 158(Chromium 158.0.8127.18), Qt 메인 스레드에서 external message pump로 구동,
`sandbox`는 아직 비활성(`no_sandbox=1`, Phase 7 과제). `runtime/cef-host` 참고.

## 3.2 런처 UI 계층 (QML + Widgets 셸)

- **QML(Qt Quick)**: 모든 화면·컴포넌트 — 내비게이션, 스토어/내 앱/런타임/설정, 상세 오버레이, 진행 상태.
- **Widgets**: 창(`QMainWindow` + `QQuickWidget`), 네이티브 대화상자(파일 선택·권한 확인·오류), 팔레트/테마.
- **경계**: QML은 서비스 객체를 직접 만지지 않고 `LauncherBackend`(`Q_PROPERTY`/`Q_INVOKABLE`)와
  `MapListModel`(역할 기반 목록)만 사용한다. 비즈니스 로직은 기존 서비스 계층에 그대로 남는다.
- **배포**: QML은 `qrc:/qml`에 임베드되고, `windeployqt --qmldir launcher/qml`이 QtQuick 모듈을 함께 배포한다.

- "공유 런타임"의 실체는 **디스크상의 동일 CEF 바이너리를 모든 앱 프로세스가 사용**하는 것이다. 코드 페이지는 OS가 공유하므로 설치 용량과 일부 RAM이 절약되지만, 프로세스별 기본 오버헤드는 사라지지 않는다.
- 모델 A는 앱 하나가 크래시하면 전체가 죽고, 악성/버그 앱이 다른 앱 데이터에 접근할 수 있는 경로가 생긴다. 보안 요구(§17)와 충돌한다.
- 따라서 문서·UI에서 "RAM 절감"을 수치로 약속하지 않는다. Phase 5/9에서 실제 측정값으로만 보고한다.

### 3.2 런타임 프로세스 수명주기

1. 런처가 앱 실행 요청 → `runtime-host --app-id <id> --profile <dataDir> --url <custom-scheme URL>` 스폰
2. 호스트는 `CefInitialize` 1회 후 창을 소유(별도 최상위 창)하고 `CefBrowser` 생성
3. 앱 종료/창 닫힘 → `CefShutdown` → 프로세스 종료 → 런처가 상태 갱신
4. 백그라운드 관리: 사용하지 않는 앱 프로세스는 최소화(CEF 창 hide + `WasHidden` 통지, 타임아웃 후 종료)
5. 단일 인스턴스 보장: 앱 ID별 락 파일로 중복 실행 방지(두 번째 실행은 기존 창 포커스)

### 3.3 앱 리소스 제공

- 파일 시스템 경로를 `file://`로 직접 열지 않는다. 앱은 `napp://<appId>/...` 커스텀 스킴으로 로드한다.
- 스킴 핸들러가 `app/` 경계 밖 접근을 거부하고, 응답에 보안 헤더(CSP, COOP/COEP)를 붙인다. WASM 스레드(SharedArrayBuffer)를 쓰려면 COOP/COEP가 필수이므로 여기서 처리한다.
- 앱별 `request_context`(쿠키/캐시/localStorage)를 `data/<appId>/web`에 매핑한다.

### 3.4 API Bridge

- 설계: `window.NAPP`는 런타임이 주입하는 JS 객체 + CEF message router(`CefMessageRouterBrowserSide`)로 native 호출.
- 각 요청은 `{id, method, params}` → 호스트가 앱의 `manifest.permissions`로 권한 검사 → C++ 핸들러 실행 → `{id, result|error}` 응답.
- 호스트는 현재 브라우저의 앱 ID를 **스폰 인자에서만** 결정한다(페이지가 자기 ID를 주장할 수 없음).
- 요청은 반드시 비동기(Promise)이며, 파일 선택 등은 네이티브 대화상자를 호스트 창에 모달로 띄운다.
- 브리지는 앱 페이지에만 주입하고 외부 오리진 문서에는 주입하지 않는다.

---

## 4. 패키지 계층 (`libnappipkg`)

- ZIP 구현: **miniz**(단일 파일, MIT) 사용 예정. Qt 6 공개 API에 ZIP 리더가 없고, `QZipReader`는 private 헤더라 사용하지 않는다.
- 구성: `ArchiveReader`(스트리밍 엔트리 열거/읽기), `PathValidator`(§3.3), `ManifestParser`(Qt JSON + SchemaValidator), `SchemaValidator`, `PackageVerifier`(해시·서명·자원 상한), `Installer`(트랜잭션).
- JSON Schema 검증은 런처에 내장된 스키마 사본으로 수행한다(네트워크 의존 금지, 버전 고정). 검증기 선택은 미결(§7).
- SHA-256: `QCryptographicHash`로 충분(스트리밍 지원).
- Ed25519: Qt에 API가 없다 → 미결(§7).

---

## 5. 저장소·네트워크 계층

- `RepositoryService`: 저장소 목록(사용자 설정) → 각 `repository.json` 병렬 갱신 → 앱 JSON 지연 로드/캐시.
- `HttpClient`: 리다이렉트 수동 처리 + hop별 allowlist 검사, 타임아웃, 재시도(지수 백오프, 최대 3회), `Range` 미사용.
- `DownloadManager`: 임시 파일(`staging/*.part`) → 해시 검증 → 원자적 rename. 취소 시 `.part` 삭제.
- `CacheStore`: JSON/아이콘 TTL 캐시(파일 + 메타 인덱스). 오프라인 모드에서 마지막 정상 응답 사용.
- 모든 응답은 크기 상한(`maxResponseBytes`)으로 스트리밍 중단 가능해야 한다.

---

## 6. 설정·데이터 경로

- `QStandardPaths::AppConfigLocation` / `AppDataLocation`을 사용하되, 앱·런타임 데이터는 `AppLocalDataLocation`(Windows `LOCALAPPDATA`) 기준으로 분리.
- 한글/공백이 포함된 경로(`C:\Users\박찬율\Desktop\NAPP Launcher`)에서 동작 확인 필요 — 빌드는 이미 통과했다. CEF 서브프로세스 경로 전달 시 UTF-8/Wide 변환을 명시적으로 처리한다.

---

## 7. 확정이 필요한 기술 결정 (승인 대상)

| # | 결정 | 권고 | 대안 | 영향 |
| --- | --- | --- | --- | --- |
| 1 | 런타임 프로세스 모델 | 앱별 호스트 프로세스 + 공유 CEF 바이너리 | 단일 호스트 다중 브라우저 | 보안·안정성 전반 |
| 2 | Ed25519 구현 | OpenSSL 3 (`EVP_PKEY_ED25519`) | libsodium (작음, 이식 쉬움) | Windows 배포 DLL, 라이선스 표기 |
| 3 | ZIP 구현 | miniz 단일 파일 | libzip | 의존성·빌드 단순성 |
| 4 | JSON Schema 검증기 | 런처 내장 경량 검증기(직접 구현, 사용 필드 한정) | nlohmann/json-schema, valijson | C++ 의존성 추가 여부 |
| 5 | UI 비중 | Widgets 셸 + Quick 상세 화면 | 전면 Quick | 개발 속도·메모리 |
| 6 | CEF 배포물 | 최소 배포(minimal, 심볼 제외) | standard | 설치 크기 |
| 7 | 최소 지원 OS | Windows 10 1809+ / glibc 2.31+ (Ubuntu 20.04+) | 더 넓게 | CEF 요구사항 |

CEF는 아직 이 PC에 없다. Phase 5 착수 시 `cef_binary_*_windows64` / `linux64` 배포물을 내려받아 `third_party/cef`에 두고, 라이선스(`BSD-3-Clause` + Chromium 고지) 동봉 여부를 확인해야 한다. 그 전까지 Phase 1–4는 Qt만으로 완결된다.

---

## 8. 미해결 위험

| 위험 | 완화 |
| --- | --- |
| CEF 도입 후 빌드 복잡도 급증(수 GB, 서브프로세스 설정) | Phase 5에서 격리 브랜치로 검증, 실패 시 런타임 대안(QtWebEngine) 재검토 |
| 앱 1개당 프로세스 오버헤드 | 유휴 앱 자동 종료 정책, 측정 기반 정책 조정 |
| GitHub 의존(가용성·캐시 지연) | TTL 캐시 + 오프라인 모드 + 저장소 추가 지원 |
| Qt 6.12 + MSVC 14.51 조합 | 환경 프로브로 확인됨(빌드 성공), CI에서 고정 |
| 서명 없는 초기 배포의 신뢰 문제 | 저장소 신뢰 수준 표시, 서명은 Phase 7에 강제 아님 |
