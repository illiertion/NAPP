// Single-file setup for a packaged NAPP app.
//
// scripts/pack_app.py compiles this and appends the app payload (a ZIP) plus a
// 32-byte trailer, exactly like packaging/Installer.cs does for NAPP itself:
//   int64  payload offset
//   16B    ASCII version
//   8B     "NAPPPAY1"
//
// The result is one .exe that installs the app per-user — Start Menu and desktop
// shortcuts, an uninstall entry — or unpacks it to any folder chosen at run time.
// No administrator rights and no bundled runtime are needed.
//
// scripts/pack_app.py 가 컴파일한 뒤 앱 페이로드(ZIP)와 32바이트 트레일러를 덧붙인다.
// NAPP 자체 설치 프로그램(packaging/Installer.cs)과 같은 형식이다. 결과물 하나로
// 사용자 범위 설치(시작 메뉴·바탕 화면 바로가기·제거 항목) 또는 임의 폴더에 압축
// 해제가 가능하다. 관리자 권한도, 런타임 동봉도 필요 없다.
//
// Placeholders replaced at pack time: {{APP_ID}}, {{APP_NAME}}, {{APP_VERSION}},
// {{PUBLISHER}}, {{HOMEPAGE}}.

using System;
using System.IO;
using System.IO.Compression;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using System.Windows.Forms;

internal static class AppSetup
{
    private const string AppId = "{{APP_ID}}";
    private const string AppName = "{{APP_NAME}}";
    private const string AppVersion = "{{APP_VERSION}}";
    private const string Publisher = "{{PUBLISHER}}";
    private const string Homepage = "{{HOMEPAGE}}";
    private const string Magic = "NAPPPAY1";

    private static string _payloadZip;

    [STAThread]
    private static int Main(string[] args)
    {
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);

        // NOTE: this file is compiled with the .NET Framework csc.exe, which only
        // understands C# 5. No string interpolation, no inline out declarations,
        // no null-conditional operators.
        // 이 파일은 C# 5 까지만 아는 .NET Framework csc.exe 로 컴파일한다.
        // 문자열 보간, inline out 선언, null 조건 연산자를 쓰지 않는다.
        string payloadError;
        if (!LocatePayload(out payloadError))
        {
            MessageBox.Show(payloadError, AppName, MessageBoxButtons.OK, MessageBoxIcon.Error);
            return 1;
        }

        bool portable = HasFlag(args, "--portable");
        bool install = HasFlag(args, "--install");
        bool quiet = HasFlag(args, "--quiet");
        string target = ValueOf(args, "--target");

        try
        {
            if (install || (!portable && !quiet && AskInstall()))
            {
                string destination = target;
                if (string.IsNullOrEmpty(destination))
                    destination = Path.Combine(
                        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                        "Programs", SafeFileName(AppName));
                return Install(destination, quiet);
            }

            if (string.IsNullOrEmpty(target))
            {
                using (var dialog = new FolderBrowserDialog())
                {
                    dialog.Description = "Choose where to unpack " + AppName + ".";
                    if (dialog.ShowDialog() != DialogResult.OK)
                        return 1;
                    target = dialog.SelectedPath;
                }
            }
            return Unpack(target);
        }
        finally
        {
            if (_payloadZip != null && File.Exists(_payloadZip))
                TryDelete(_payloadZip);
        }
    }

    // ---- payload ------------------------------------------------------------

    private static bool LocatePayload(out string error)
    {
        error = null;
        string self = Assembly.GetExecutingAssembly().Location;
        var info = new FileInfo(self);
        if (info.Length < 32)
        {
            error = "This setup program has no payload attached.";
            return false;
        }

        using (var stream = File.OpenRead(self))
        {
            stream.Seek(-32, SeekOrigin.End);
            var trailer = new byte[32];
            stream.Read(trailer, 0, 32);
            long offset = BitConverter.ToInt64(trailer, 0);
            string magic = Encoding.ASCII.GetString(trailer, 24, 8);
            if (magic != Magic || offset < 0 || offset + 32 > info.Length)
            {
                error = "This setup program is damaged (payload trailer not found).";
                return false;
            }

            _payloadZip = Path.Combine(Path.GetTempPath(), "napp-app-setup-" + Guid.NewGuid().ToString("N") + ".zip");
            stream.Seek(offset, SeekOrigin.Begin);
            var payload = new byte[info.Length - offset - 32];
            stream.Read(payload, 0, payload.Length);
            File.WriteAllBytes(_payloadZip, payload);
        }
        return true;
    }

    // ---- modes --------------------------------------------------------------

    private static bool AskInstall()
    {
        return MessageBox.Show(
            "Install " + AppName + " for this user?\n\n" +
            "Yes — install and add Start Menu / desktop shortcuts.\n" +
            "No — unpack to a folder you choose.",
            AppName + " Setup", MessageBoxButtons.YesNo, MessageBoxIcon.Question) == DialogResult.Yes;
    }

    private static int Install(string destination, bool quiet)
    {
        if (Directory.Exists(destination) && Directory.GetFileSystemEntries(destination).Length > 0)
        {
            if (!quiet && MessageBox.Show(
                    "Files already exist in\n" + destination + "\n\nOverwrite them?",
                    AppName + " Setup", MessageBoxButtons.YesNo, MessageBoxIcon.Warning) != DialogResult.Yes)
                return 1;
        }

        UnpackTo(destination);

        string executable = Path.Combine(destination, SafeFileName(AppName) + ".exe");
        string icon = Path.Combine(destination, SafeFileName(AppName) + ".ico");
        if (!File.Exists(icon))
            icon = executable;

        string startMenu = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.StartMenu),
            "Programs", SafeFileName(AppName));
        Directory.CreateDirectory(startMenu);
        CreateShortcut(Path.Combine(startMenu, SafeFileName(AppName) + ".lnk"),
                       executable, icon, destination);
        CreateShortcut(Path.Combine(
                           Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory),
                           SafeFileName(AppName) + ".lnk"),
                       executable, icon, destination);

        WriteUninstallEntry(destination, executable);
        if (!quiet)
            MessageBox.Show(AppName + " is installed.", AppName + " Setup",
                            MessageBoxButtons.OK, MessageBoxIcon.Information);
        return 0;
    }

    private static int Unpack(string destination)
    {
        UnpackTo(destination);
        MessageBox.Show(AppName + " was unpacked to\n" + destination,
                        AppName + " Setup", MessageBoxButtons.OK, MessageBoxIcon.Information);
        return 0;
    }

    private static string UnpackTo(string destination)
    {
        Directory.CreateDirectory(destination);
        string root = Path.GetFullPath(destination)
            .TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;

        // Extracted by hand instead of ZipFile.ExtractToDirectory: the .NET
        // Framework overload has no "overwrite" flag, and a payload entry must
        // never escape the destination folder.
        // ZipFile.ExtractToDirectory 대신 직접 푼다. .NET Framework 오버로드에는
        // 덮어쓰기 인수가 없고, 페이로드 항목이 대상 폴더 밖으로 나가면 안 된다.
        using (ZipArchive archive = ZipFile.OpenRead(_payloadZip))
        {
            foreach (ZipArchiveEntry entry in archive.Entries)
            {
                string relative = entry.FullName.Replace('/', Path.DirectorySeparatorChar);
                string target = Path.GetFullPath(Path.Combine(root, relative));
                if (!target.StartsWith(root, StringComparison.OrdinalIgnoreCase))
                    continue;

                if (string.IsNullOrEmpty(entry.Name))
                {
                    Directory.CreateDirectory(target);
                    continue;
                }
                string parent = Path.GetDirectoryName(target);
                if (!string.IsNullOrEmpty(parent))
                    Directory.CreateDirectory(parent);
                entry.ExtractToFile(target, true);
            }
        }
        return destination;
    }

    // ---- shell integration --------------------------------------------------

    // WScript.Shell through late binding: one COM call, no interop assembly to
    // ship and no dependency beyond the .NET Framework already on Windows.
    // 지연 바인딩으로 WScript.Shell 을 부른다. COM 호출 하나면 되고, 동봉할 인터롭
    // 어셈블리도 Windows 에 이미 있는 .NET Framework 외의 의존성도 없다.
    private static void CreateShortcut(string linkPath, string target, string icon, string workingDirectory)
    {
        try
        {
            Type shellType = Type.GetTypeFromProgID("WScript.Shell");
            if (shellType == null)
                return;
            object shell = Activator.CreateInstance(shellType);
            object shortcut = shellType.InvokeMember(
                "CreateShortcut", BindingFlags.InvokeMethod, null, shell, new object[] { linkPath });
            SetMember(shortcut, "TargetPath", target);
            SetMember(shortcut, "WorkingDirectory", workingDirectory);
            SetMember(shortcut, "IconLocation", icon);
            SetMember(shortcut, "Description", AppName + " " + AppVersion);
            shellType.InvokeMember("Save", BindingFlags.InvokeMethod, null, shortcut, null);
            Marshal.FinalReleaseComObject(shortcut);
            Marshal.FinalReleaseComObject(shell);
        }
        catch (Exception)
        {
            // A shortcut is a convenience, never a reason to fail the install.
            // 바로가기는 편의 기능일 뿐, 설치 실패 이유가 되지 않는다.
        }
    }

    private static void SetMember(object target, string name, object value)
    {
        try
        {
            target.GetType().InvokeMember(
                name, BindingFlags.SetProperty, null, target, new[] { value });
        }
        catch (Exception)
        {
        }
    }

    private static void WriteUninstallEntry(string destination, string executable)
    {
        try
        {
            string keyPath = "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\" + AppId;
            using (var key = Microsoft.Win32.Registry.CurrentUser.CreateSubKey(keyPath))
            {
                if (key == null)
                    return;
                key.SetValue("DisplayName", AppName);
                key.SetValue("DisplayVersion", AppVersion);
                key.SetValue("Publisher", Publisher);
                if (!string.IsNullOrEmpty(Homepage))
                    key.SetValue("HelpLink", Homepage);
                key.SetValue("InstallLocation", destination);
                key.SetValue("DisplayIcon", executable);
                key.SetValue("NoModify", 1, Microsoft.Win32.RegistryValueKind.DWord);
                key.SetValue("NoRepair", 1, Microsoft.Win32.RegistryValueKind.DWord);
                key.SetValue("UninstallString",
                    "powershell.exe -NoProfile -ExecutionPolicy Bypass -File \"" +
                    Path.Combine(destination, "uninstall.ps1") + "\"");
            }
        }
        catch (Exception)
        {
        }
    }

    // ---- helpers ------------------------------------------------------------

    private static bool HasFlag(string[] args, string flag)
    {
        foreach (string argument in args)
            if (string.Equals(argument, flag, StringComparison.OrdinalIgnoreCase))
                return true;
        return false;
    }

    private static string ValueOf(string[] args, string flag)
    {
        for (int index = 0; index + 1 < args.Length; ++index)
            if (string.Equals(args[index], flag, StringComparison.OrdinalIgnoreCase))
                return args[index + 1];
        return null;
    }

    private static string SafeFileName(string name)
    {
        var builder = new StringBuilder(name.Trim());
        foreach (char invalid in Path.GetInvalidFileNameChars())
            builder.Replace(invalid, '-');
        while (builder.Length > 0 && (builder[builder.Length - 1] == '.' || builder[builder.Length - 1] == ' '))
            builder.Length--;
        return builder.Length > 0 ? builder.ToString() : "App";
    }

    private static void TryDelete(string path)
    {
        try
        {
            File.Delete(path);
        }
        catch (Exception)
        {
        }
    }
}