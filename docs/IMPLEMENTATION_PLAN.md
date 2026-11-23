# NAPP 구현 계획 (Draft 2)

- 작성일: 2026-11-20
- 상태: **Phase 1–5 구현 완료** (Phase 6부터 미착수)
- 실제 구현/검증 결과는 §0.1 참조

---

## 0. 현재 상태 (확인된 사실)

**작업 디렉터리**: `C:\Users\박찬율\Desktop\NAPP Launcher` — 기존 소스 없음(`.freebuff` 메타데이터만 존재), git 저장소 아님.

**확인된 환경**

| 항목 | 값 | 상태 |
| --- | --- | --- |
| OS | Windows 10 19045 x64 | ✔ |
| Qt | 6.12.0 `msvc2022_64` (`C:\Qt\6.12.0`) | ✔ |
| 컴파일러 | MSVC 19.51 (VS 18 Community, toolset 14.51) | ✔ |
| CMake | 4.3.1 | ✔ |
| Ninja | Qt Creator 동봉본 사용 가능 | ✔ |
| Qt Installer Framework | `C:\Qt\Tools\QtInstallerFramework` | ✔ |
| Python | 3.12.10 + `jsonschema` | ✔ (스펙 검증용) |
| git | 2.56 | ✔ |
| **CEF** | **없음** | ✖ Phase 5 착수 시 도입 |
| Linux 검증 환경 | 없음(WSL/Docker 미확인) | ✖ Phase 9 전 필요 |

**빌드 가능 여부 검증**: Qt Widgets/Network/JSON/Crypto를 쓰는 C++20 프로브(`build/envprobe`)를 CMake+Ninja+MSVC로 **빌드 및 실행 성공**(종료 코드 0). 한글·공백 포함 경로에서도 구성·링크가 통과했다. `Qt6Widgets.dll` 등이 PATH에 없으면 `0xC0000135`로 실행이 실패하므로, 배포 시 `windeployqt` 또는 설치 프로그램으로 DLL을 동봉해야 한다(빌드 문제 아님).

**스펙 산출물 검증**: `specifications/schemas/*.schema.json`에 대해 `examples/` 4개 문서가 검증을 통과했고, 의도적으로 잘못된 매니페스트(대문자 앱 ID, `../evil.html` entry, 미허용 권한)는 거부됨을 확인했다(아래 4장 참조).

---

## 0.1 Phase 1–5 구현 결과 (2026-11-20)

| Phase | 상태 | 산출물 | 검증 |
| --- | --- | --- | --- |
| 1 프로젝트 기반 | 완료 | `CMakeLists.txt`, `cmake/`, `libs/core`(Result/Error/SemVer/Paths/Platform/Logging/Json), 런처 셸, 로그/단일 인스턴스 | Debug·Release 빌드, 런처 실행 유지 |
| 2 IPKG 규격 | 완료 | `libs/ipkg`: ZipReader(miniz), PathRules, ManifestParser, PackageVerifier, Installer | 유닛 테스트 38건 (경로 탈출·zip 폭탄·암호화·심볼릭 링크·중복·해시·설치 원자성·업데이트·제거) |
| 3 GitHub 저장소 | 완료 | `HttpClient`(리다이렉트 allowlist·크기 상한), `CatalogCache`(TTL·오프라인), `RepositoryService`, 스토어/상세/홈 UI | 저장소 404 처리 확인, 캐시/스키마 검증 경로 구현, 공식 저장소 미배포로 실데이터 확인 불가 |
| 4 패키지 관리 | 완료 | `DownloadManager`(HttpClient.download), `InstallService`(다운로드→해시→검증→설치), `AppRegistry`, 라이브러리/설정 UI, 저장소 추가·삭제·활성화 | 설치/업데이트/제거/다운그레이드 유닛 테스트 |
| 5b QML UI 전환 | 완료 | `launcher/qml` 6개 화면 + `LauncherBackend`/`MapListModel`, Widgets는 셸·네이티브 대화상자 | 빌드, QML 오류 0건, `--ui-snapshot`으로 5개 화면 렌더 확인, windeployqt `--qmldir` 배포 |
| 5 Chromium 런타임 | 완료(CEF 158) | `runtime/cef-host`: Qt+CEF 호스트, `napp://` 스킴 핸들러(CSP/경로 봉쇄), 앱별 프로세스·프로필, `RuntimeManager`(스캔/등록/활성/제거) | `scripts/smoke-runtime.sh`: 두 앱 동시 실행, 각자 프로필, 엔트리 문서 서빙, 프로세스 생존 |

### 빌드/실행 방법

```bat
scripts\fetch-cef.bat                 REM CEF 158 배포물 다운로드 + 압축 해제
scripts\build.bat Debug --test        REM Qt 전용 빌드 + ctest (CEF 없이 Phase 1–4)
scripts\build-cef.bat --test          REM Release + CEF 런타임 호스트
build\cef\NAPP.exe --register-runtime build\cef\runtime-stage
build\cef\NAPP.exe                    REM 런처 실행
bash scripts/smoke-runtime.sh         REM 두 앱 동시 실행 검증
python specifications/tools/validate_spec.py
```

### 이번 단계에서 의도적으로 미구현/편차로 둔 항목 (완료로 보고하지 않음)

1. **CEF 샌드박스 비활성** (`settings.no_sandbox = 1`). 프로세스 분리는 되어 있지만 Chromium 샌드박스는 `cef_sandbox.lib` 연동이 필요하다. 외부 앱을 신뢰하기 전에 반드시 해결해야 하는 보안 항목(§17).
2. ~~UI는 Qt Widgets만 사용~~ → **2026-11-20 변경: 모든 화면을 QML(Qt Quick)로 구현**하고 Widgets는 창·네이티브 대화상자만 담당한다(`launcher/qml`, `LauncherBackend`). 사용법은 `docs/USAGE.md` §8.
3. **스토어 아이콘**: 저장소 아이콘을 내려받지 않고 글자 타일을 사용한다. `CatalogCache`에 아이콘 경로 지원만 있다.
4. **자동 업데이트 미구현**: 업데이트 가능 여부 표시와 수동 설치(버전 선택)만 동작한다.
5. **Ed25519 서명 검증 미구현**(Phase 7). SHA-256 무결성만 검증한다.
6. **NAPP Web SDK/브리지 없음**(Phase 6): 앱 페이지에 `window.NAPP`가 주입되지 않는다.
7. **categories.json 미사용**: 카테고리 필터는 앱 JSON의 `app.category`를 사용한다.
8. **권한 UI는 전체 허용/취소**만 제공하고 브리지에서의 세부 검사는 Phase 6에서 붙인다.
9. **Linux 빌드 미검증**: 이 환경에 Linux 툴체인이 없다.
10. 공식 저장소(`ILLIERTION/NAPP-Repository`)가 아직 없어 스토어는 404 오류를 표시한다(오류 처리 경로는 정상).

---

## 1. Phase 개요와 수용 기준

각 Phase는 **빌드 성공 + 해당 테스트 통과 + 산출물 확인** 후에만 다음으로 진행한다.

### Phase 0 — 규격 확정 (현재 위치)
- 산출물: `specifications/IPKG_SPECIFICATION.md`, 4개 JSON Schema, 예제, 아키텍처 문서
- 수용 기준: 예제 JSON이 스키마 검증 통과, 오류 코드/검증 순서 확정, 아키텍처 결정 §7 승인

### Phase 1 — 프로젝트 기반
- CMake 최상위 + `libnappipkg` + `launcher` + 빈 `ipkg-studio` 타깃
- 공통 로깅(`qInstallMessageHandler`, 파일 롤링), 예외 없는 오류 전달 타입(`Result<T>` 대신 `std::expected` 스타일)
- 런처 셸: 사이드바(홈/스토어/앱/런타임/설정) + QStackedWidget, 테마 토큰, 다국어 리소스 골격
- 수용 기준: Windows에서 Debug/Release 빌드, 셸 실행, 창 크기 상태 저장, 로그 파일 생성. 테스트: CMake `enable_testing()` + 최소 1개 gtest 불필요한 자체 테스트 실행 파일

### Phase 2 — IPKG 규격 구현 (`libnappipkg`)
- miniz 도입, `ArchiveReader`, `PathValidator`, `ManifestParser`, `SchemaValidator`, `PackageVerifier`
- 안전 해제기, 자원 상한, 오류 코드(`E10xx`) 매핑
- 수용 기준: 정상/악성 샘플 패키지 픽스처로 테스트 — 경로 탈출, zip 폭탄, 중복 엔트리, 잘못된 manifest, 상한 초과가 각각 지정된 코드로 거부되고, 정상 패키지는 설치 트리로 전개됨

### Phase 3 — 저장소 연동
- `HttpClient`(리다이렉트 allowlist, 타임아웃, 재시도, 크기 상한), `RepositoryService`, `CacheStore`
- 스토어 목록/검색/상세 UI, 카테고리, 로딩·오류·오프라인 상태
- 수용 기준: 공식 저장소 JSON을 실제로 가져와 목록 표시, 네트워크 차단 시 캐시로 표시, 잘못된 스키마 저장소가 앱 전체를 깨지 않음

### Phase 4 — 패키지 관리
- `DownloadManager`, 버전 선택, 설치/제거/업데이트 트랜잭션, `registry.json`, 앱 데이터 관리
- 수용 기준: 실제 `.ipkg` 설치·실행 파일 전개·제거, 업데이트 시 `data/` 보존, 실패 시 기존 설치 유지(롤백), 다운그레이드 확인 흐름

### Phase 5 — Chromium 런타임
- CEF 배포물 도입, `runtime-host`, 앱별 프로세스, `napp://` 스킴, 파티션 격리, 유휴 종료
- 수용 기준: 두 앱 동시 실행 + 각각 다른 앱 데이터, 한 앱 크래시가 다른 앱에 영향 없음, 런타임 설치/제거/버전 표시

### Phase 6 — NAPP Web SDK
- `NAPP.app/storage/window/notification/dialog/system/theme`, TypeScript 정의, 예제 앱
- 브리지 권한 검사 + 앱 ID 위조 방지 테스트
- 수용 기준: 예제 앱에서 storage 저장/조회, 창 제목 변경, 알림 표시가 동작하고, 권한 미선언 시 `PermissionDenied`

### Phase 7 — 업데이트·보안
- semver 비교, 업데이트 알림/자동 적용, Ed25519 서명, trust store, 손상 복구
- 수용 기준: 서명 실패 패키지 차단, 업데이트 실패 시 복구, 버전 비교 단위 테스트

### Phase 8 — IPKG Studio
- 프로젝트/매니페스트 편집, 아이콘·미리보기, 플랫폼 선택, 빌드·검증, 결과물 `.ipkg`
- 수용 기준: Studio로 만든 패키지가 런처에 설치되고, 검증 오류를 항목별로 표시

### Phase 9 — 배포·최적화
- Qt Installer Framework 설치 프로그램(런처 + 선택적 런타임), `windeployqt`, Linux AppImage/deb, 실측 RAM/CPU/디스크 표
- 수용 기준: 깨끗한 Windows/Linux에서 설치→앱 설치→실행→제거 전 과정, 측정값 문서화

---

## 2. 저장소 구조 (Phase 1에서 생성)

```
NAPP/
├── CMakeLists.txt
├── cmake/                     공통 함수, 옵션, 경고 수준
├── libs/ipkg/                 IPKG 파서·검증 (런처/Studio 공유)
├── launcher/
│   ├── src/{core,ui,store,package,network,runtime,security,settings}
│   ├── include/
│   └── resources/
├── runtime/                   CEF 기반 runtime-host (Phase 5)
├── ipkg-studio/
├── sdk/
│   ├── javascript/  types/  examples/  documentation/
├── specifications/            이미 존재 (규격·스키마·예제)
├── tests/                     픽스처 + 단위 테스트
├── docs/
└── third_party/               miniz, (CEF는 Phase 5)
```

기존 구현이 없으므로 이 구조로 새로 시작한다. 이후 재구성은 하지 않는다.

---

## 3. 개발 규율

1. C++20, Qt 6 공개 API만 사용(private 헤더 금지)
2. 헤더/소스 분리, 파일당 단일 책임, 1000줄 초과 파일 금지
3. UI는 서비스 시그널만 구독(비즈니스 로직을 위젯에 넣지 않음)
4. 네트워크/파일 I/O는 전부 비동기 + 취소 가능
5. 모든 실패는 `ErrorCode` + 사용자 메시지 + 로그 3종으로 전달
6. 스펙(JSON Schema)과 코드가 어긋나면 스펙을 먼저 고치고 코드를 맞춘다
7. 미구현 기능은 UI에 "준비 중"으로 표시하며 완료로 보고하지 않는다
8. Windows와 Linux 양쪽 빌드를 Phase 2부터 CI 수준으로 유지(가능해지면 GitHub Actions)

---

## 4. 현재까지 실행한 검증

| 검증 | 명령 | 결과 |
| --- | --- | --- |
| 툴체인 프로브 빌드·실행 | `build\envprobe.bat` (vcvars64 + `cmake -G Ninja` + build + run) | 빌드 성공, 실행 종료 코드 0 |
| 예제 JSON ↔ 스키마 | `python specifications/tools/validate_spec.py` | 4개 예제 통과, 실패 0 |
| 부정 통제(잘못된 매니페스트) | 대문자 앱 ID / `../evil.html` / 미허용 권한 주입 | 4개 위치에서 거부됨 |

`build/envprobe`는 환경 확인용 산출물이며 Phase 1에서 정식 CMake 구조로 대체된다.

---

## 5. 승인이 필요한 항목

1. `docs/ARCHITECTURE.md` §7의 기술 결정 7건(특히 런타임 프로세스 모델 = 앱별 호스트 프로세스)
2. `specifications/IPKG_SPECIFICATION.md` §13의 미결 사항 5건
3. Phase 1 착수 승인 후 첫 산출물: CMake 골격 + 런처 셸 + `libnappipkg` 인터페이스 + 스펙 검증 테스트
