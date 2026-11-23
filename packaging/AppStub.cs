// Native launcher stub for one packaged NAPP app.
//
// scripts/pack_app.py compiles this once per app, with the app's own .ico, so
// the result is an ordinary Windows executable: Explorer and the taskbar show
// the app icon, double-clicking starts the app, and no console window appears.
// The app itself is a web app, so the stub only locates NAPP, installs the
// bundled .ipkg when needed, and hands over to `NAPP --run-app`.
//
// scripts/pack_app.py 가 앱마다 한 번씩 컴파일한다. 결과물은 평범한 Windows 실행
// 파일이라 탐색기와 작업 표시줄에 앱 아이콘이 뜨고 더블클릭으로 실행되며 콘솔 창이
// 없다. 앱 본체는 웹 앱이므로 이 스텁은 NAPP 을 찾고, 필요하면 동봉한 .ipkg 를
// 설치하고, `NAPP --run-app` 에 넘기는 일만 한다.
//
// Placeholders replaced at pack time: {{APP_ID}}, {{APP_NAME}}, {{APP_VERSION}},
// {{HOMEPAGE}}, {{PACKAGE_FILE}}.
//
// Compiled with the .NET Framework csc.exe, so C# 5 only: no string
// interpolation, no inline out declarations, no null-conditional operators.
// .NET Framework csc.exe 로 컴파일하므로 C# 5 만 쓴다.

using System;
using System.Diagnostics;
using System.IO;
using System.Text;
using System.Windows.Forms;
using Microsoft.Win32;

internal static class AppStub
{
    private const string AppId = "{{APP_ID}}";
    private const string AppName = "{{APP_NAME}}";
    private const string AppVersion = "{{APP_VERSION}}";
    private const string Homepage = "{{HOMEPAGE}}";
    private const string PackageFile = "{{PACKAGE_FILE}}";

    private const string NappHomeVariable = "NAPP_HOME";
    private const string UninstallKey =
        "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\NAPP";

    [STAThread]
    private static int Main()
    {
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);

        string baseDirectory = AppDomain.CurrentDomain.BaseDirectory;
        string napp = FindNapp(baseDirectory);
        if (napp == null)
        {
            ShowMissingNapp();
            return 2;
        }

        // Install or update from the bundled package. The launcher rejects a
        // version that is already there, so a marker file records what was
        // installed and the work only happens when the bundled version changes.
        // 동봉 패키지로 설치하거나 갱신한다. 런처는 이미 설치된 버전을 거부하므로,
        // 어떤 버전을 설치했는지 마커에 적어 두고 버전이 바뀔 때만 설치한다.
        string package = Path.Combine(baseDirectory, PackageFile);
        string marker = Path.Combine(baseDirectory, AppId + ".installed");
        if (File.Exists(package) && ReadMarker(marker) != AppVersion)
        {
            int installed = Run(napp, "--install-package \"" + package + "\"", baseDirectory);
            if (installed != 0)
            {
                ShowError("NAPP could not install the bundled package (code " + installed + ").");
                return installed;
            }
            File.WriteAllText(marker, AppVersion, new UTF8Encoding(false));
        }

        int exitCode = Run(napp, "--run-app " + AppId, baseDirectory);
        return exitCode;
    }

    // NAPP_HOME wins (portable layouts and CI), then the per-user install
    // recorded by the setup program, then a NAPP.exe sitting next to this app.
    // NAPP_HOME 이 우선하고(포터블/CI), 다음은 설치 프로그램이 남긴 등록 정보,
    // 마지막으로 앱 옆에 있는 NAPP.exe 를 쓴다.
    private static string FindNapp(string baseDirectory)
    {
        string home = Environment.GetEnvironmentVariable(NappHomeVariable);
        if (!string.IsNullOrEmpty(home))
        {
            string candidate = Path.Combine(home, "NAPP.exe");
            if (File.Exists(candidate))
                return candidate;
        }

        try
        {
            using (RegistryKey key = Registry.CurrentUser.OpenSubKey(UninstallKey))
            {
                object location = key == null ? null : key.GetValue("InstallLocation");
                if (location != null)
                {
                    string candidate = Path.Combine(location.ToString(), "NAPP.exe");
                    if (File.Exists(candidate))
                        return candidate;
                }
            }
        }
        catch (Exception)
        {
            // A locked or unreadable registry must not stop a portable run.
            // 등록 정보를 읽지 못해도 포터블 실행은 막지 않는다.
        }

        string parent = Path.GetDirectoryName(baseDirectory.TrimEnd(Path.DirectorySeparatorChar));
        if (parent != null)
        {
            string candidate = Path.Combine(parent, "NAPP.exe");
            if (File.Exists(candidate))
                return candidate;
        }

        return null;
    }

    private static int Run(string executable, string arguments, string workingDirectory)
    {
        var startInfo = new ProcessStartInfo
        {
            FileName = executable,
            Arguments = arguments,
            WorkingDirectory = workingDirectory,
            UseShellExecute = false,
        };
        try
        {
            using (Process process = Process.Start(startInfo))
            {
                if (process == null)
                    return 1;
                process.WaitForExit();
                return process.ExitCode;
            }
        }
        catch (Exception exception)
        {
            ShowError("Could not start NAPP: " + exception.Message);
            return 1;
        }
    }

    private static string ReadMarker(string path)
    {
        try
        {
            return File.Exists(path) ? File.ReadAllText(path).Trim() : null;
        }
        catch (Exception)
        {
            return null;
        }
    }

    private static void ShowMissingNapp()
    {
        string message = AppName + " runs on the NAPP platform, which is not installed yet.";
        if (!string.IsNullOrEmpty(Homepage))
            message += "\n\nGet NAPP from:\n" + Homepage;
        message += "\n\nIf NAPP is already installed somewhere else, set the " +
                   NappHomeVariable + " environment variable to its folder.";
        MessageBox.Show(message, AppName, MessageBoxButtons.OK, MessageBoxIcon.Information);
    }

    private static void ShowError(string message)
    {
        MessageBox.Show(message, AppName, MessageBoxButtons.OK, MessageBoxIcon.Error);
    }
}