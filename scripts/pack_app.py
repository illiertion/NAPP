#!/usr/bin/env python3
"""IPKG 앱을 플랫폼 전용 앱 패키지로 만든다.

웹 앱이지만 결과물은 각 OS 가 기대하는 모습이 된다:
  windows-x64  앱 아이콘을 단 .exe (네이티브 스텁) + 시작 메뉴/바탕 화면 바로가기,
               설치/제거 스크립트, 선택적으로 한 파일짜리 Setup.exe, zip
  macos-*      <Name>.app 번들 (Info.plist + AppRun + .icns)
  linux-*      AppDir (AppRun + .desktop + 아이콘) + install.sh/uninstall.sh

스텁은 NAPP 을 찾아 동봉한 .ipkg 를 설치하고 `NAPP --run-app <id>` 로 넘긴다.
앱 코드는 수정하지 않고, 패키지 하나로 배포할 수 있다.

사용:
  python scripts/pack_app.py path/to/app.ipkg
  python scripts/pack_app.py app.ipkg --platform windows-x64 --installer
  python scripts/pack_app.py app.ipkg --platform all --out dist-apps

Windows 스텁은 csc.exe (.NET Framework, Windows 기본 탑재) 로 컴파일한다.
"""

from __future__ import annotations

import argparse
import json
import os
import platform as host_platform
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import time
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PACKAGING = ROOT / "packaging"
DEFAULT_OUT = ROOT / "dist-apps"

APP_ID = re.compile(r"^[a-z][a-z0-9]*(\.[a-z0-9][a-z0-9-]*)+$")

# ICO 에 담을 크기. 256px 은 PNG 엔트리로 넣는다(Vista+ 방식, 화질 손실 없음).
ICO_SIZES = (16, 32, 48, 64, 128, 256)

# icns 에 PNG 를 그대로 넣을 수 있는 타입 (크기 -> chunk id)
ICNS_PNG_TYPES = [(16, b"icp4"), (32, b"icp5"), (64, b"icp6"), (128, b"ic07"),
                  (256, b"ic08"), (512, b"ic09"), (1024, b"ic10")]


# ---------------------------------------------------------------------------
# IPKG reading
# ---------------------------------------------------------------------------


class App:
    def __init__(self, package: Path, manifest: dict, icon: bytes) -> None:
        self.package = package
        self.manifest = manifest
        self.icon = icon

        app = manifest["app"]
        self.app_id: str = app["id"]
        self.name: str = app.get("name") or app["id"]
        self.version: str = app["version"]
        self.description: str = app.get("description", "")
        self.author: str = app.get("author", "")
        self.homepage: str = app.get("homepage", "")
        self.license: str = app.get("license", "MIT")
        self.platforms: list[str] = manifest.get("platforms", ["windows-x64"])
        self.permissions: list[str] = manifest.get("permissions", [])

    @property
    def package_file(self) -> str:
        return f"{self.app_id}-{self.version}.ipkg"

    @property
    def exe_stem(self) -> str:
        return safe_file_name(self.name)


def safe_file_name(name: str) -> str:
    result = name.strip()
    for invalid in '<>:"/\\|?*':
        result = result.replace(invalid, "-")
    result = "".join(ch for ch in result if ord(ch) >= 0x20)
    return result.rstrip(". ") or "App"


def read_package(package: Path) -> App:
    if not package.is_file():
        raise SystemExit(f"패키지가 없습니다: {package}")
    with zipfile.ZipFile(package) as archive:
        names = archive.namelist()
        if "manifest.json" not in names:
            raise SystemExit(f"manifest.json 이 없습니다: {package}")
        manifest = json.loads(archive.read("manifest.json").decode("utf-8"))

        app_id = manifest.get("app", {}).get("id", "")
        if not APP_ID.match(app_id):
            raise SystemExit(f"잘못된 앱 ID: {app_id!r}")

        icon_path = manifest.get("metadata", {}).get("icon")
        if not icon_path or icon_path not in names:
            raise SystemExit(f"아이콘이 패키지에 없습니다: {icon_path!r}")
        icon = archive.read(icon_path)

    return App(package, manifest, icon)


# ---------------------------------------------------------------------------
# Icons
# ---------------------------------------------------------------------------


def build_ico(icon: bytes, target: Path) -> bool:
    """앱 아이콘 PNG 를 Windows .ico 로 만든다. Windows 가 아니면 False."""
    with tempfile.TemporaryDirectory() as tmp:
        tmp_path = Path(tmp)
        source = tmp_path / "source.png"
        source.write_bytes(icon)

        script = tmp_path / "resize.ps1"
        script.write_text(
            "param([string]$Source, [string]$OutDir)\n"
            "Add-Type -AssemblyName System.Drawing\n"
            "$src = New-Object System.Drawing.Bitmap $Source\n"
            "foreach ($size in @(16,32,48,64,128,256)) {\n"
            "  $bmp = New-Object System.Drawing.Bitmap $size, $size\n"
            "  $g = [System.Drawing.Graphics]::FromImage($bmp)\n"
            "  $g.InterpolationMode = "
            "[System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic\n"
            "  $g.SmoothingMode = "
            "[System.Drawing.Drawing2D.SmoothingMode]::HighQuality\n"
            "  $g.PixelOffsetMode = "
            "[System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality\n"
            "  $g.Clear([System.Drawing.Color]::Transparent)\n"
            "  $scale = [Math]::Min($size / $src.Width, $size / $src.Height)\n"
            "  $w = [int][Math]::Round($src.Width * $scale)\n"
            "  $h = [int][Math]::Round($src.Height * $scale)\n"
            "  $g.DrawImage($src, [int](($size-$w)/2), [int](($size-$h)/2), $w, $h)\n"
            "  $g.Dispose()\n"
            "  $bmp.Save((Join-Path $OutDir (\"icon-{0}.png\" -f $size)), "
            "[System.Drawing.Imaging.ImageFormat]::Png)\n"
            "  $bmp.Dispose()\n"
            "}\n"
            "$src.Dispose()\n",
            encoding="utf-8",
        )

        try:
            completed = subprocess.run(
                ["powershell.exe", "-NoProfile", "-ExecutionPolicy", "Bypass",
                 "-File", str(script), "-Source", str(source), "-OutDir", str(tmp_path)],
                capture_output=True, timeout=120,
            )
        except (OSError, subprocess.SubprocessError):
            return False
        if completed.returncode != 0:
            return False

        images: list[tuple[int, bytes]] = []
        for size in ICO_SIZES:
            resized = tmp_path / f"icon-{size}.png"
            if resized.is_file():
                images.append((size, resized.read_bytes()))
        if not images:
            return False

    target.write_bytes(pack_ico(images))
    return True


def pack_ico(images: list[tuple[int, bytes]]) -> bytes:
    """PNG 엔트리들을 ICO 컨테이너로 묶는다. scripts/pack-ico.py 와 같은 형식."""
    count = len(images)
    offset = 6 + 16 * count
    directory = bytearray()
    payload = bytearray()
    for size, data in images:
        directory += struct.pack(
            "<BBBBHHII",
            0 if size >= 256 else size,
            0 if size >= 256 else size,
            0, 0, 1, 32,
            len(data),
            offset,
        )
        payload += data
        offset += len(data)
    return bytes(struct.pack("<HHH", 0, 1, count)) + bytes(directory) + bytes(payload)


def build_icns(icon: bytes, target: Path) -> bool:
    """PNG 를 감싼 최소한의 .icns 를 만든다. macOS 전용 타입."""
    try:
        width, height = png_size(icon)
    except ValueError:
        return False
    edge = max(width, height)

    chunks = bytearray()
    for size, chunk_id in ICNS_PNG_TYPES:
        # 원본 크기에 가장 가까운 타입 하나만 넣는다. Finder 가 나머지 칸은
        # 그 아이콘을 확대/축소해 채우므로, 한 크기만 있어도 동작한다.
        if size == nearest_icns_size(edge):
            chunks += chunk_id + struct.pack(">I", 8 + len(icon)) + icon
            break
    if not chunks:
        size, chunk_id = ICNS_PNG_TYPES[-1]
        chunks += chunk_id + struct.pack(">I", 8 + len(icon)) + icon

    body = b"icns" + struct.pack(">I", 8 + len(chunks)) + bytes(chunks)
    target.write_bytes(body)
    return True


def nearest_icns_size(edge: int) -> int:
    best = ICNS_PNG_TYPES[0][0]
    for size, _ in ICNS_PNG_TYPES:
        if abs(size - edge) < abs(best - edge):
            best = size
    return best


def png_size(data: bytes) -> tuple[int, int]:
    if len(data) < 24 or data[:8] != b"\x89PNG\r\n\x1a\n" or data[12:16] != b"IHDR":
        raise ValueError("not a PNG")
    return struct.unpack(">II", data[16:24])


# ---------------------------------------------------------------------------
# Shared generated files
# ---------------------------------------------------------------------------


def substitute(template: str, app: App, **extra: str) -> str:
    values = {
        "APP_ID": app.app_id,
        "APP_NAME": app.name,
        "APP_VERSION": app.version,
        "PUBLISHER": app.author or "Unknown",
        "HOMEPAGE": app.homepage,
        "PACKAGE_FILE": app.package_file,
    }
    values.update(extra)
    result = template
    for key, value in values.items():
        result = result.replace("{{" + key + "}}", cs_escape(value))
    return result


def cs_escape(value: str) -> str:
    return (value.replace("\\", "\\\\").replace('"', '\\"')
            .replace("\r", " ").replace("\n", " "))


def readme(app: App, platform: str, notes: list[str]) -> str:
    lines = [
        f"{app.name} {app.version}",
        "=" * min(72, len(app.name) + len(app.version) + 1),
        "",
        app.description,
        "",
        f"Platform:   {platform}",
        f"App ID:     {app.app_id}",
        f"Version:    {app.version}",
        f"Author:     {app.author}",
        f"License:    {app.license}",
    ]
    if app.homepage:
        lines.append(f"Homepage:   {app.homepage}")
    lines += ["", "Install", "-" * 60]
    lines += notes
    lines += [
        "",
        "About",
        "-" * 60,
        f"{app.name} is a NAPP app. The package ships the app itself as",
        f"{app.package_file}; the launcher stub installs it into NAPP and starts it.",
        "The app's files and settings live in the NAPP data folder, so removing this",
        "folder never deletes the app's saved data — use NAPP → Library → Remove for",
        "that.",
        "",
    ]
    return "\n".join(lines)


def zip_directory(source: Path, target: Path) -> None:
    if target.exists():
        target.unlink()
    shutil.make_archive(str(target.with_suffix("")), "zip", source.parent, source.name)


# ---------------------------------------------------------------------------
# Windows
# ---------------------------------------------------------------------------


def find_csc() -> Path | None:
    windir = Path(os.environ.get("WINDIR") or r"C:\Windows")
    for sub in ("Microsoft.NET\\Framework64\\v4.0.30319", "Microsoft.NET\\Framework\\v4.0.30319"):
        candidate = windir / sub / "csc.exe"
        if candidate.is_file():
            return candidate
    found = shutil.which("csc.exe")
    return Path(found) if found else None


def compile_csc(csc: Path, source: Path, output: Path, icon: Path | None,
                winexe: bool) -> None:
    command = [str(csc), "-nologo", "-optimize+", "-platform:anycpu",
               "-target:" + ("winexe" if winexe else "exe"), f"-out:{output}"]
    if icon is not None:
        command.append(f"-win32icon:{icon}")
    command.append("-r:System.Windows.Forms.dll")
    command.append("-r:System.Drawing.dll")
    command.append("-r:System.IO.Compression.dll")
    command.append("-r:System.IO.Compression.FileSystem.dll")
    command.append(str(source))
    completed = subprocess.run(command, capture_output=True, text=True)
    if completed.returncode != 0:
        raise SystemExit("csc.exe 실패:\n" + (completed.stdout or "") + (completed.stderr or ""))


def windows_sdk() -> tuple[Path | None, list[str]]:
    r"""rc.exe 와 함께 쓸 Windows SDK include 폴더를 찾는다.

    rc.exe 만으로는 winver.h 를 찾지 못한다. SDK Include\<version>\{um,shared} 를
    /i 로 넘겨야 한다.
    """
    program_files = os.environ.get("ProgramFiles(x86)") or r"C:\Program Files (x86)"
    kits_bin = Path(program_files) / "Windows Kits" / "10" / "bin"
    kits_include = Path(program_files) / "Windows Kits" / "10" / "Include"

    rc_tool: Path | None = None
    if kits_bin.is_dir():
        for arch in ("x64", "x86"):
            matches = sorted(kits_bin.glob(f"*/{arch}/rc.exe"), reverse=True)
            if matches:
                rc_tool = matches[0]
                break
    if rc_tool is None:
        found = shutil.which("rc.exe")
        rc_tool = Path(found) if found else None

    includes: list[str] = []
    if kits_include.is_dir():
        for version_dir in sorted(kits_include.iterdir(), reverse=True):
            for sub in ("um", "shared", "ucrt", "winrt"):
                candidate = version_dir / sub
                if candidate.is_dir():
                    includes.append(str(candidate))
            if includes:
                break
    return rc_tool, includes


def build_resource(icon: Path, description: str, product: str, original: str,
                   version: str, out_dir: Path, stem: str) -> Path | None:
    """cmake/version.rc.in 템플릿으로 아이콘 + VERSIONINFO 리소스를 만든다.

    CMake 쪽과 같은 템플릿을 쓰므로 두 쪽의 파일 속성 정보가 갈라지지 않는다.
    rc.exe 를 쓸 수 없으면 None 을 돌려주고, 호출 쪽이 아이콘만 넣는다.
    """
    rc_tool, includes = windows_sdk()
    template_path = ROOT / "cmake" / "version.rc.in"
    if rc_tool is None or not template_path.is_file():
        return None
    template = template_path.read_text(encoding="utf-8")

    # FILEVERSION wants four comma-separated numbers.
    numbers = [int(part) if part.isdigit() else 0 for part in version.split(".")[:3]]
    numbers += [0] * (3 - len(numbers))

    values = {
        "NAPP_RC_ICON": icon.name,
        "NAPP_RC_FILEVERSION": ",".join(str(n) for n in numbers) + ",0",
        "NAPP_RC_VERSION": version,
        "NAPP_RC_COMPANY": "ILLIERTION",
        "NAPP_RC_COPYRIGHT": "Copyright (c) ILLIERTION",
        "NAPP_RC_PRODUCT": product,
        "NAPP_RC_DESCRIPTION": description,
        "NAPP_RC_INTERNAL_NAME": Path(original).stem,
        "NAPP_RC_ORIGINAL_FILENAME": original,
        "NAPP_RC_FILETYPE": "VFT_APP",
    }
    text = template
    for key, value in values.items():
        text = text.replace("@" + key + "@", rc_escape(value))

    out_dir.mkdir(parents=True, exist_ok=True)
    # rc.exe 는 따옴표 안의 아이콘 경로를 .rc 기준으로 찾는다.
    shutil.copy2(icon, out_dir / icon.name)
    rc_file = out_dir / f"{stem}.rc"
    res_file = out_dir / f"{stem}.res"

    # rc.exe 인코딩은 세대마다 다르다. UTF-8 BOM 먼저, 실패하면 UTF-16LE BOM.
    # 앱 이름이 한글이어도 어느 쪽이든 통과한다.
    for encoding in ("utf-8-sig", "utf-16"):
        rc_file.write_bytes(text.encode(encoding))
        command = [str(rc_tool), "/fo", str(res_file)]
        for include in includes:
            command += ["/i", include]
        command.append(str(rc_file))
        completed = subprocess.run(command, capture_output=True, text=True)
        if completed.returncode == 0 and res_file.is_file():
            return res_file
        res_file.unlink(missing_ok=True)

    print("  ! rc.exe 리소스 컴파일 실패, 아이콘만 넣습니다:",
          (completed.stderr or completed.stdout or "").strip())
    return None


def rc_escape(value: str) -> str:
    return value.replace("\\", "\\\\").replace('"', '\\"').replace("\r", " ").replace("\n", " ")


def compile_with_resource(csc: Path, source: Path, output: Path, res_file: Path | None,
                          icon: Path | None, winexe: bool) -> None:
    """res 가 있으면 /win32res 로 아이콘+버전을 함께 넣고, 없으면 /win32icon."""
    if res_file is not None:
        command = [str(csc), "-nologo", "-optimize+", "-platform:anycpu",
                   "-target:" + ("winexe" if winexe else "exe"), f"-out:{output}",
                   f"-win32res:{res_file}"]
        command.append("-r:System.Windows.Forms.dll")
        command.append("-r:System.Drawing.dll")
        command.append("-r:System.IO.Compression.dll")
        command.append("-r:System.IO.Compression.FileSystem.dll")
        command.append(str(source))
        completed = subprocess.run(command, capture_output=True, text=True)
        if completed.returncode != 0:
            raise SystemExit("csc.exe 실패:\n" + (completed.stdout or "") + (completed.stderr or ""))
        return
    compile_csc(csc, source, output, icon, winexe)


def build_windows(app: App, out_root: Path, make_installer: bool, make_zip: bool = True) -> Path:
    stem = f"{app.app_id}-{app.version}-windows-x64"
    stage = out_root / stem
    if stage.exists():
        shutil.rmtree(stage)
    stage.mkdir(parents=True)

    with tempfile.TemporaryDirectory() as tmp:
        tmp_path = Path(tmp)

        ico = stage / f"{app.exe_stem}.ico"
        if not build_ico(app.icon, ico):
            print("  ! Windows 에서만 .ico 를 만들 수 있습니다. NAPP 기본 아이콘을 씁니다.")
            ico = None

        stub_source = tmp_path / "AppStub.cs"
        stub_source.write_text(
            substitute((PACKAGING / "AppStub.cs").read_text(encoding="utf-8"), app),
            encoding="utf-8-sig")

        csc = find_csc()
        if csc is None:
            raise SystemExit("csc.exe 를 찾지 못했습니다 (.NET Framework 필요)")

        executable = stage / f"{app.exe_stem}.exe"
        stub_res = None
        if ico is not None:
            stub_res = build_resource(
                ico,
                description=f"{app.name} — NAPP app",
                product="NAPP",
                original=f"{app.exe_stem}.exe",
                version=app.version,
                out_dir=tmp_path / "stub-res",
                stem="stub",
            )
        compile_with_resource(csc, stub_source, executable, stub_res, ico, winexe=True)

        shutil.copy2(app.package, stage / app.package_file)

        (stage / "install.ps1").write_text(windows_install_script(app), encoding="utf-8")
        (stage / "uninstall.ps1").write_text(windows_uninstall_script(app), encoding="utf-8")

        notes = [
            f"1. Unzip anywhere, then run {app.exe_stem}.exe.",
            f"2. It installs {app.package_file} into NAPP (or updates it) and starts",
            "   the app. NAPP must already be installed; the stub tells you where to",
            "   get it otherwise.",
            "",
            "To add Start Menu and desktop shortcuts, run install.ps1.",
            "To remove those shortcuts and the NAPP app, run uninstall.ps1.",
        ]
        (stage / "README.txt").write_text(readme(app, "windows-x64", notes), encoding="utf-8")

        if make_installer:
            setup_source = tmp_path / "AppSetup.cs"
            setup_source.write_text(
                substitute((PACKAGING / "AppSetup.cs").read_text(encoding="utf-8"), app),
                encoding="utf-8-sig")
            setup_exe = out_root / f"{app.app_id}-{app.version}-Setup.exe"
            setup_res = None
            if ico is not None:
                setup_res = build_resource(
                    ico,
                    description=f"{app.name} Setup",
                    product="NAPP",
                    original=setup_exe.name,
                    version=app.version,
                    out_dir=tmp_path / "setup-res",
                    stem="setup",
                )
            compile_with_resource(csc, setup_source, setup_exe, setup_res, ico, winexe=True)
            attach_payload(setup_exe, stage, app.version)
            print(f"  setup     {setup_exe}")

    archive = out_root / f"{stem}.zip"
    if make_zip:
        zip_directory(stage, archive)
        print(f"  archive   {archive}")
    print(f"  package   {stage}")
    return stage


def attach_payload(setup_exe: Path, stage: Path, version: str) -> None:
    """Setup.exe 뒤에 페이로드 zip + 32바이트 트레일러를 붙인다. make-dist.ps1 와 같은 형식."""
    with tempfile.TemporaryDirectory() as tmp:
        payload = Path(tmp) / "payload.zip"
        shutil.make_archive(str(payload.with_suffix("")), "zip", stage.parent, stage.name)

        # Windows can still hold the freshly compiled .exe for a moment (Defender
        # scanning it), so appending retries briefly instead of failing outright.
        # 방금 컴파일한 .exe 를 Windows 가 잠시 잡고 있을 수 있다(Defender 검사).
        # 그래서 실패하지 않고 잠깐 재시도한다.
        output = source = None
        for attempt in range(10):
            try:
                output = open(setup_exe, "r+b")
                source = open(payload, "rb")
                break
            except OSError:
                if output:
                    output.close()
                if source:
                    source.close()
                output = source = None
                time.sleep(0.25)
        if output is None or source is None:
            raise SystemExit(f"Setup.exe 에 페이로드를 붙일 수 없습니다: {setup_exe}")

        with output, source:
            output.seek(0, os.SEEK_END)
            offset = output.tell()
            shutil.copyfileobj(source, output)
            output.write(struct.pack("<q", offset))
            # 16-byte ASCII version field, shown by the installer before it
            # unpacks anything. Same layout as scripts/make-dist.ps1.
            field = version.encode("ascii", "replace")[:16]
            output.write(field + b"\0" * (16 - len(field)))
            output.write(b"NAPPPAY1")


def windows_install_script(app: App) -> str:
    return f"""# Start Menu / desktop shortcuts for {app.name}.
# powershell -ExecutionPolicy Bypass -File install.ps1
$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$shell = New-Object -ComObject WScript.Shell
$target = Join-Path $here '{app.exe_stem}.exe'
$icon = Join-Path $here '{app.exe_stem}.ico'
if (-not (Test-Path $icon)) {{ $icon = $target }}

function New-Shortcut([string]$path) {{
    $link = $shell.CreateShortcut($path)
    $link.TargetPath = $target
    $link.WorkingDirectory = $here
    $link.IconLocation = $icon
    $link.Description = '{app.name} {app.version}'
    $link.Save()
}}

$menu = Join-Path $env:APPDATA 'Microsoft\\Windows\\Start Menu\\Programs\\{app.exe_stem}'
New-Item -ItemType Directory -Force -Path $menu | Out-Null
New-Shortcut (Join-Path $menu '{app.exe_stem}.lnk')
New-Shortcut (Join-Path ([Environment]::GetFolderPath('Desktop')) '{app.exe_stem}.lnk')
Write-Host "Shortcuts created for {app.name}."
"""


def windows_uninstall_script(app: App) -> str:
    return f"""# Removes the shortcuts and the NAPP app for {app.name}.
# powershell -ExecutionPolicy Bypass -File uninstall.ps1
$ErrorActionPreference = 'SilentlyContinue'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path

Remove-Item (Join-Path $env:APPDATA 'Microsoft\\Windows\\Start Menu\\Programs\\{app.exe_stem}') -Recurse -Force
Remove-Item (Join-Path ([Environment]::GetFolderPath('Desktop')) '{app.exe_stem}.lnk') -Force

# The app itself lives in the NAPP data folder; ask NAPP to remove it.
$napp = $env:NAPP_HOME
if ($napp) {{ $napp = Join-Path $napp 'NAPP.exe' }}
if (-not $napp -or -not (Test-Path $napp)) {{
    $key = Get-ItemProperty 'HKCU:\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\NAPP'
    if ($key -and $key.InstallLocation) {{
        $candidate = Join-Path $key.InstallLocation 'NAPP.exe'
        if (Test-Path $candidate) {{ $napp = $candidate }}
    }}
}}

Write-Host "{app.name} shortcuts were removed."
if ($napp) {{
    Write-Host "Remove the app itself from NAPP -> Library -> {app.name} -> Remove."
}} else {{
    Write-Host "NAPP was not found; the app and its data are still in the NAPP data folder."
}}
"""


# ---------------------------------------------------------------------------
# macOS
# ---------------------------------------------------------------------------


def build_macos(app: App, out_root: Path, platform: str, make_zip: bool = True) -> Path:
    stem = f"{app.app_id}-{app.version}-{platform}"
    stage = out_root / stem
    if stage.exists():
        shutil.rmtree(stage)
    bundle = stage / f"{app.name}.app"
    macos_dir = bundle / "Contents" / "MacOS"
    resources = bundle / "Contents" / "Resources"
    macos_dir.mkdir(parents=True)
    resources.mkdir(parents=True)

    shutil.copy2(app.package, resources / app.package_file)
    build_icns(app.icon, resources / f"{app.app_id}.icns")

    (bundle / "Contents" / "Info.plist").write_text(
        macos_info_plist(app, platform), encoding="utf-8")

    launcher = macos_dir / app.exe_stem
    launcher.write_text(macos_launcher(app), encoding="utf-8")
    launcher.chmod(0o755)

    notes = [
        f"1. Copy {app.name}.app into /Applications (or run it from anywhere).",
        "2. On first open macOS may warn that the app is not signed — right-click",
        "   and choose Open. NAPP must already be installed.",
        "",
        "To remove, delete the .app. The app's saved data stays in the NAPP data",
        "folder until you remove it from NAPP -> Library.",
    ]
    (stage / "README.txt").write_text(readme(app, platform, notes), encoding="utf-8")

    archive = out_root / f"{stem}.zip"
    if make_zip:
        zip_directory(stage, archive)
        print(f"  archive   {archive}")
    print(f"  package   {stage}")
    return stage


def macos_info_plist(app: App, platform: str) -> str:
    identifier = app.app_id
    return f"""<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleIdentifier</key>
    <string>{identifier}</string>
    <key>CFBundleName</key>
    <string>{xml_escape(app.name)}</string>
    <key>CFBundleDisplayName</key>
    <string>{xml_escape(app.name)}</string>
    <key>CFBundleExecutable</key>
    <string>{xml_escape(app.exe_stem)}</string>
    <key>CFBundleIconFile</key>
    <string>{app.app_id}.icns</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>{app.version}</string>
    <key>CFBundleVersion</key>
    <string>{app.version}</string>
    <key>CFBundleInfoDictionaryVersion</key>
    <string>6.0</string>
    <key>LSMinimumSystemVersion</key>
    <string>11.0</string>
    <key>NSHighResolutionCapable</key>
    <true/>
    <key>NSHumanReadableCopyright</key>
    <string>{xml_escape(app.author)}</string>
    <key>NAPPPlatform</key>
    <string>{platform}</string>
</dict>
</plist>
"""


def macos_launcher(app: App) -> str:
    return f"""#!/bin/sh
# Generated by scripts/pack_app.py — native launcher for {app.name}.
# Finds NAPP, installs the bundled package when its version changed, then hands
# over to `NAPP --run-app`.
set -eu

HERE="$(cd "$(dirname "$0")/../Resources" && pwd)"
APP_ID="{app.app_id}"
APP_VERSION="{app.version}"
PACKAGE="$HERE/{app.package_file}"
MARKER="$HERE/$APP_ID.installed"

find_napp() {{
    if [ -n "${{NAPP_HOME:-}}" ] && [ -x "$NAPP_HOME/NAPP" ]; then
        echo "$NAPP_HOME/NAPP"
        return 0
    fi
    for candidate in /Applications/NAPP.app/Contents/MacOS/NAPP \\
                     "$HOME/Applications/NAPP.app/Contents/MacOS/NAPP" \\
                     "$(dirname "$HERE")/NAPP"; do
        if [ -x "$candidate" ]; then
            echo "$candidate"
            return 0
        fi
    done
    return 1
}}

if ! NAPP="$(find_napp)"; then
    osascript -e 'display alert "{app.name} needs NAPP" message "Install NAPP from {app.homepage or "https://github.com/ILLIERTION/NAPP"} and open {app.name} again." as informational' >/dev/null 2>&1 || true
    exit 2
fi

if [ -f "$PACKAGE" ] && [ "$(cat "$MARKER" 2>/dev/null || true)" != "$APP_VERSION" ]; then
    "$NAPP" --install-package "$PACKAGE" || exit 3
    printf '%s' "$APP_VERSION" > "$MARKER"
fi

exec "$NAPP" --run-app "$APP_ID"
"""


def xml_escape(value: str) -> str:
    return (value.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
            .replace('"', "&quot;"))


# ---------------------------------------------------------------------------
# Linux
# ---------------------------------------------------------------------------


def build_linux(app: App, out_root: Path, platform: str, make_zip: bool = True) -> Path:
    stem = f"{app.app_id}-{app.version}-{platform}"
    stage = out_root / stem
    if stage.exists():
        shutil.rmtree(stage)
    stage.mkdir(parents=True)

    shutil.copy2(app.package, stage / app.package_file)

    icons = stage / "icons"
    icons.mkdir()
    (icons / f"{app.app_id}.png").write_bytes(app.icon)

    apprun = stage / "AppRun"
    apprun.write_text(linux_apprun(app), encoding="utf-8")
    apprun.chmod(0o755)

    (stage / f"{app.app_id}.desktop").write_text(linux_desktop(app), encoding="utf-8")

    install = stage / "install.sh"
    install.write_text(linux_install_script(app), encoding="utf-8")
    install.chmod(0o755)

    uninstall = stage / "uninstall.sh"
    uninstall.write_text(linux_uninstall_script(app), encoding="utf-8")
    uninstall.chmod(0o755)

    notes = [
        "1. Unpack anywhere, then run ./AppRun.",
        "2. To add a menu entry and a desktop icon, run ./install.sh.",
        "   To remove them again, run ./uninstall.sh.",
        "",
        "NAPP must already be installed. Set NAPP_HOME to its folder if it is not",
        "on PATH. The package is also AppDir shaped, so appimagetool can turn it",
        "into a single .AppImage.",
    ]
    (stage / "README.txt").write_text(readme(app, platform, notes), encoding="utf-8")

    archive = out_root / f"{stem}.zip"
    if make_zip:
        zip_directory(stage, archive)
        print(f"  archive   {archive}")
    print(f"  package   {stage}")
    return stage


def linux_apprun(app: App) -> str:
    return f"""#!/bin/sh
# Generated by scripts/pack_app.py — native launcher for {app.name}.
set -eu

HERE="$(cd "$(dirname "$0")" && pwd)"
APP_ID="{app.app_id}"
APP_VERSION="{app.version}"
PACKAGE="$HERE/{app.package_file}"
MARKER="$HERE/$APP_ID.installed"

find_napp() {{
    for candidate in "${{NAPP_HOME:-}}/NAPP" \\
                     "$HOME/.local/bin/napp" \\
                     "$HOME/.local/bin/NAPP" \\
                     "$(command -v napp 2>/dev/null || true)" \\
                     "$(command -v NAPP 2>/dev/null || true)"; do
        if [ -n "$candidate" ] && [ -x "$candidate" ]; then
            echo "$candidate"
            return 0
        fi
    done
    return 1
}}

if ! NAPP="$(find_napp)"; then
    message="{app.name} needs NAPP. Install it from {app.homepage or "https://github.com/ILLIERTION/NAPP"} and run {app.name} again."
    if command -v zenity >/dev/null 2>&1; then
        zenity --info --title="{app.name}" --text="$message" 2>/dev/null || true
    elif command -v notify-send >/dev/null 2>&1; then
        notify-send "{app.name}" "$message" 2>/dev/null || true
    else
        echo "$message" >&2
    fi
    exit 2
fi

if [ -f "$PACKAGE" ] && [ "$(cat "$MARKER" 2>/dev/null || true)" != "$APP_VERSION" ]; then
    "$NAPP" --install-package "$PACKAGE" || exit 3
    printf '%s' "$APP_VERSION" > "$MARKER"
fi

exec "$NAPP" --run-app "$APP_ID"
"""


def linux_desktop(app: App) -> str:
    category = "Utility" if "developer" not in app.permissions else "Development"
    return f"""[Desktop Entry]
Type=Application
Name={app.name}
GenericName={app.name}
Comment={app.description}
Exec=AppRun
Icon={app.app_id}
Terminal=false
Categories={category};
StartupNotify=true
StartupWMClass={app.app_id}
"""


def linux_install_script(app: App) -> str:
    return f"""#!/bin/sh
# Adds {app.name} to the desktop menu. Run from this folder.
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"
APP_ID="{app.app_id}"

mkdir -p "$HOME/.local/share/applications" "$HOME/.local/share/icons/hicolor/256x256/apps"
cp "$HERE/icons/$APP_ID.png" "$HOME/.local/share/icons/hicolor/256x256/apps/$APP_ID.png"

sed "s|^Exec=.*|Exec=$HERE/AppRun|" "$HERE/$APP_ID.desktop" \\
    > "$HOME/.local/share/applications/$APP_ID.desktop"
sed -i "s|^Icon=.*|Icon=$APP_ID|" "$HOME/.local/share/applications/$APP_ID.desktop" 2>/dev/null || true
chmod +x "$HOME/.local/share/applications/$APP_ID.desktop"

command -v update-desktop-database >/dev/null 2>&1 \\
    && update-desktop-database "$HOME/.local/share/applications" || true
echo "{app.name} added to the application menu."
"""


def linux_uninstall_script(app: App) -> str:
    return f"""#!/bin/sh
# Removes the menu entry for {app.name}.
set -eu
APP_ID="{app.app_id}"
rm -f "$HOME/.local/share/applications/$APP_ID.desktop"
rm -f "$HOME/.local/share/icons/hicolor/256x256/apps/$APP_ID.png"
command -v update-desktop-database >/dev/null 2>&1 \\
    && update-desktop-database "$HOME/.local/share/applications" || true
echo "{app.name} removed from the application menu."
"""


# ---------------------------------------------------------------------------
# Driver
# ---------------------------------------------------------------------------


def target_platforms(app: App, requested: str) -> list[str]:
    if requested == "all":
        return list(app.platforms)
    return [requested]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("package", help="packaged app (.ipkg)")
    parser.add_argument("--platform", default="host",
                        help="windows-x64 | macos-arm64 | linux-x64 | all | host (default)")
    parser.add_argument("--out", default=str(DEFAULT_OUT), help="output folder")
    parser.add_argument("--installer", action="store_true",
                        help="also build a single-file Setup.exe (Windows)")
    parser.add_argument("--no-zip", action="store_true", help="skip the .zip archive")
    arguments = parser.parse_args()

    package = Path(arguments.package).resolve()
    out_root = Path(arguments.out).resolve()
    out_root.mkdir(parents=True, exist_ok=True)

    app = read_package(package)
    print(f"{app.name} {app.version} ({app.app_id})")
    print(f"  permissions: {', '.join(app.permissions) or '(none)'}")

    if arguments.platform == "host":
        machine = host_platform.machine().lower()
        arch = "arm64" if machine in ("arm64", "aarch64") else "x64"
        system = host_platform.system().lower()
        host = {"windows": f"windows-{arch}", "darwin": f"macos-{arch}",
                "linux": f"linux-{arch}"}.get(system, "windows-x64")
        platforms = [host]
    else:
        platforms = target_platforms(app, arguments.platform)

    unknown = [name for name in platforms if name not in app.platforms]
    if unknown:
        print(f"  ! 매니페스트가 지원하지 않는 플랫폼입니다: {', '.join(unknown)}")

    for name in platforms:
        print(f"[{name}]")
        if name.startswith("windows"):
            build_windows(app, out_root, arguments.installer, not arguments.no_zip)
        elif name.startswith("macos"):
            build_macos(app, out_root, name, not arguments.no_zip)
        elif name.startswith("linux"):
            build_linux(app, out_root, name, not arguments.no_zip)
        else:
            print(f"  ? 지원하지 않는 플랫폼: {name}")

    return 0


if __name__ == "__main__":
    sys.exit(main())