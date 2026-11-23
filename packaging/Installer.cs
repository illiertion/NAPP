// NAPP setup — a single .exe installer.
//
// scripts/make-dist.ps1 compiles this file, appends the payload ZIP to the
// executable and stores a 16-byte trailer: the ZIP offset (Int64, little
// endian) followed by the ASCII magic "NAPPPAY1". The installer copies that
// payload out of its own file and unpacks it, so no extra files are needed.
//
// Usage:
//   NAPP-Setup.exe                          (GUI: Install or Portable)
//   NAPP-Setup.exe --install "D:\Apps\NAPP" [--desktop] [--launch] [--quiet]
//   NAPP-Setup.exe --portable "D:\NAPP" [--launch] [--quiet]

using System;
using System.Diagnostics;
using System.Drawing;
using System.Globalization;
using System.IO;
using System.IO.Compression;
using System.Reflection;
using System.Text;
using System.Windows.Forms;
using Microsoft.Win32;

namespace NappSetup
{
    internal static class Program
    {
        internal const string Magic = "NAPPPAY1";
        internal const int VersionBytes = 16;
        // offset (Int64) | version (16 ASCII, space padded) | magic (8 ASCII)
        internal const int TrailerBytes = 8 + VersionBytes + 8;

        internal const string Usage =
            "NAPP-Setup.exe [--install <folder> | --portable <folder>] [--desktop] [--launch] [--quiet]";

        [STAThread]
        private static int Main(string[] args)
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);

            Options options;
            try
            {
                options = Options.Parse(args);
            }
            catch (ArgumentException error)
            {
                Console.Error.WriteLine(error.Message);
                return 2;
            }

            Payload payload;
            try
            {
                payload = Payload.Locate();
            }
            catch (Exception error)
            {
                MessageBox.Show(error.Message, "NAPP Setup", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return 3;
            }

            if (options.Mode == "gui")
            {
                using (var form = new SetupForm(payload))
                {
                    return form.ShowDialog() == DialogResult.OK ? 0 : 1;
                }
            }

            try
            {
                Setup.Run(payload, options.Mode == "portable", options.Target,
                    options.Desktop, options.Launch, null);
                return 0;
            }
            catch (Exception error)
            {
                Console.Error.WriteLine(error.Message);
                if (!options.Quiet)
                {
                    MessageBox.Show("Setup could not finish.\r\n\r\n" + error.Message,
                        "NAPP Setup", MessageBoxButtons.OK, MessageBoxIcon.Error);
                }
                return 4;
            }
        }
    }

    internal sealed class Options
    {
        public string Mode = "gui";
        public string Target = "";
        public bool Desktop;
        public bool Launch;
        public bool Quiet;

        public static Options Parse(string[] args)
        {
            var options = new Options();
            for (var index = 0; index < args.Length; index++)
            {
                switch (args[index].ToLowerInvariant())
                {
                    case "--install":
                        options.Mode = "install";
                        options.Target = Value(args, ref index, "--install");
                        break;
                    case "--portable":
                        options.Mode = "portable";
                        options.Target = Value(args, ref index, "--portable");
                        break;
                    case "--desktop":
                        options.Desktop = true;
                        break;
                    case "--launch":
                        options.Launch = true;
                        break;
                    case "--quiet":
                        options.Quiet = true;
                        break;
                    default:
                        throw new ArgumentException("Unknown argument: " + args[index] + "\r\n" + Program.Usage);
                }
            }
            if (options.Quiet && options.Mode == "gui") options.Mode = "install";
            if (options.Mode != "gui" && options.Target.Length == 0)
                throw new ArgumentException("--" + options.Mode + " needs a folder.\r\n" + Program.Usage);
            if (options.Mode == "install" && options.Target.Length == 0)
                options.Target = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                    "Programs\\NAPP");
            return options;
        }

        private static string Value(string[] args, ref int index, string name)
        {
            if (index + 1 >= args.Length) throw new ArgumentException(name + " needs a folder.");
            index++;
            return args[index];
        }
    }

    /// <summary>The ZIP appended to this executable.</summary>
    internal sealed class Payload
    {
        private readonly long offset;
        private readonly long length;

        public string Version { get; private set; }

        private Payload(long offset, long length)
        {
            this.offset = offset;
            this.length = length;
            Version = "0.0.0";
        }

        public static Payload Locate()
        {
            var executable = Application.ExecutablePath;
            using (var stream = File.OpenRead(executable))
            {
                if (stream.Length <= Program.TrailerBytes)
                    throw new InvalidDataException("This setup file is incomplete: the payload is missing.");

                stream.Seek(-Program.TrailerBytes, SeekOrigin.End);
                var trailer = new byte[Program.TrailerBytes];
                var read = 0;
                while (read < trailer.Length)
                {
                    var chunk = stream.Read(trailer, read, trailer.Length - read);
                    if (chunk <= 0) throw new InvalidDataException("This setup file is incomplete.");
                    read += chunk;
                }

                var magic = Encoding.ASCII.GetString(trailer, 8 + Program.VersionBytes, 8);
                if (magic != Program.Magic)
                    throw new InvalidDataException(
                        "This setup file is incomplete: it was not built by scripts\\make-dist.ps1.");

                var offset = BitConverter.ToInt64(trailer, 0);
                var length = stream.Length - Program.TrailerBytes - offset;
                if (offset <= 0 || length <= 0)
                    throw new InvalidDataException("This setup file is damaged.");

                var payload = new Payload(offset, length);
                var version = Encoding.ASCII.GetString(trailer, 8, Program.VersionBytes).TrimEnd(' ', '\0');
                payload.Version = version.Length > 0 ? version : "0.0.0";
                return payload;
            }
        }

        public string CopyTo(string destination, Action<double> progress)
        {
            using (var source = File.OpenRead(Application.ExecutablePath))
            using (var target = File.Create(destination))
            {
                source.Seek(offset, SeekOrigin.Begin);
                var buffer = new byte[1024 * 512];
                long copied = 0;
                while (copied < length)
                {
                    var wanted = (int)Math.Min(buffer.Length, length - copied);
                    var chunk = source.Read(buffer, 0, wanted);
                    if (chunk <= 0) throw new EndOfStreamException("The embedded payload ended early.");
                    target.Write(buffer, 0, chunk);
                    copied += chunk;
                    if (progress != null) progress((double)copied / length);
                }
            }
            return destination;
        }

    }

    internal static class Setup
    {
        public static void Run(Payload payload, bool portable, string target,
            bool desktopShortcut, bool launch, Action<string, double> report)
        {
            target = Path.GetFullPath(Environment.ExpandEnvironmentVariables(target));
            if (report != null) report("Preparing the setup payload…", 0);

            var temporary = Path.Combine(Path.GetTempPath(),
                "napp-setup-" + Guid.NewGuid().ToString("N") + ".zip");
            try
            {
                payload.CopyTo(temporary, (fraction) =>
                {
                    if (report != null) report("Reading the setup payload…", fraction * 0.35);
                });

                PrepareTarget(target, portable, report);
                Extract(temporary, target, (fraction) =>
                {
                    if (report != null) report("Copying NAPP " + payload.Version + "…", 0.35 + fraction * 0.6);
                });
                WriteRecord(target, portable ? "portable" : "install", payload.Version);

                var executable = Path.Combine(target, "NAPP.exe");
                if (portable)
                {
                    File.WriteAllText(Path.Combine(target, "PORTABLE.txt"),
                        "NAPP " + payload.Version + " portable\r\n" +
                        "Settings and app data live in %LOCALAPPDATA%\\illiertion\\napp.\r\n",
                        Encoding.UTF8);
                }
                else
                {
                    Register(executable, target, payload.Version, desktopShortcut);
                }

                if (report != null) report("NAPP " + payload.Version + " is ready.", 1);
                if (launch) Start(executable);
            }
            finally
            {
                try
                {
                    if (File.Exists(temporary)) File.Delete(temporary);
                }
                catch (IOException)
                {
                    // The next setup run overwrites it; leaving one temp file is harmless.
                }
            }
        }

        public static string DefaultTarget(bool portable)
        {
            return portable
                ? Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Desktop), "NAPP-Portable")
                : Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                    "Programs\\NAPP");
        }

        private static void PrepareTarget(string target, bool portable, Action<string, double> report)
        {
            if (Directory.Exists(target) && Directory.GetFileSystemEntries(target).Length > 0)
            {
                var existing = Path.Combine(target, "NAPP.exe");
                var message = "The folder already exists and is not empty:\r\n" + target +
                              "\r\n\r\nReplace its contents?";
                if (report == null || !File.Exists(existing))
                {
                    var answer = MessageBox.Show(message, "NAPP Setup", MessageBoxButtons.YesNo,
                        MessageBoxIcon.Warning);
                    if (answer != DialogResult.Yes) throw new OperationCanceledException("The setup was cancelled.");
                }
                else if (MessageBox.Show(message, "NAPP Setup", MessageBoxButtons.YesNo, MessageBoxIcon.Warning) !=
                         DialogResult.Yes)
                {
                    throw new OperationCanceledException("The setup was cancelled.");
                }
            }
            Directory.CreateDirectory(target);
        }

        private static void Extract(string zipPath, string target, Action<double> progress)
        {
            var root = target.EndsWith(Path.DirectorySeparatorChar.ToString(), StringComparison.Ordinal)
                ? target
                : target + Path.DirectorySeparatorChar;

            using (var archive = ZipFile.OpenRead(zipPath))
            {
                long total = 0;
                foreach (var entry in archive.Entries) total += entry.Length;
                long done = 0;

                foreach (var entry in archive.Entries)
                {
                    var relative = entry.FullName.Replace('/', Path.DirectorySeparatorChar);
                    var destination = Path.GetFullPath(Path.Combine(target, relative));
                    if (!destination.StartsWith(root, StringComparison.OrdinalIgnoreCase))
                        throw new InvalidDataException("The payload contains an unsafe path: " + entry.FullName);

                    if (relative.EndsWith(Path.DirectorySeparatorChar.ToString(), StringComparison.Ordinal) ||
                        entry.Name.Length == 0)
                    {
                        Directory.CreateDirectory(destination);
                        continue;
                    }

                    var folder = Path.GetDirectoryName(destination);
                    if (!string.IsNullOrEmpty(folder)) Directory.CreateDirectory(folder);
                    using (var source = entry.Open())
                    using (var output = File.Create(destination))
                        source.CopyTo(output);

                    done += entry.Length;
                    if (progress != null && total > 0) progress((double)done / total);
                }
            }
        }

        private static void WriteRecord(string target, string mode, string version)
        {
            var record = new StringBuilder();
            record.Append("{\r\n");
            record.Append("  \"mode\": \"").Append(mode).Append("\",\r\n");
            record.Append("  \"target\": \"").Append(Json(target)).Append("\",\r\n");
            record.Append("  \"version\": \"").Append(Json(version)).Append("\",\r\n");
            record.Append("  \"installedAt\": \"")
                .Append(DateTime.Now.ToString("s", CultureInfo.InvariantCulture)).Append("\"\r\n");
            record.Append("}\r\n");
            File.WriteAllText(Path.Combine(target, "install.json"), record.ToString(), new UTF8Encoding(false));
        }

        private static void Register(string executable, string target, string version, bool desktopShortcut)
        {
            var startMenu = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
                "Microsoft\\Windows\\Start Menu\\Programs\\NAPP.lnk");
            CreateShortcut(startMenu, executable);

            if (desktopShortcut)
            {
                var desktop = Environment.GetFolderPath(Environment.SpecialFolder.Desktop);
                CreateShortcut(Path.Combine(desktop, "NAPP.lnk"), executable);
            }

            using (var key = Registry.CurrentUser.CreateSubKey(
                "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\NAPP"))
            {
                if (key == null) throw new InvalidOperationException("Could not write the uninstall entry.");
                key.SetValue("DisplayName", "NAPP");
                key.SetValue("DisplayVersion", version);
                key.SetValue("Publisher", "ILLIERTION");
                key.SetValue("DisplayIcon", executable);
                key.SetValue("InstallLocation", target);
                key.SetValue("NoModify", 1, RegistryValueKind.DWord);
                key.SetValue("NoRepair", 1, RegistryValueKind.DWord);
                key.SetValue("UninstallString",
                    "powershell.exe -NoProfile -ExecutionPolicy Bypass -File \"" +
                    Path.Combine(target, "Uninstall.ps1") + "\"");
            }

            // .ipkg opens with this copy of the launcher (per user, no admin).
            using (var extension = Registry.CurrentUser.CreateSubKey("Software\\Classes\\.ipkg"))
                if (extension != null) extension.SetValue(null, "NAPP.ipkg");
            using (var type = Registry.CurrentUser.CreateSubKey("Software\\Classes\\NAPP.ipkg"))
                if (type != null) type.SetValue(null, "NAPP Package");
            using (var icon = Registry.CurrentUser.CreateSubKey("Software\\Classes\\NAPP.ipkg\\DefaultIcon"))
                if (icon != null) icon.SetValue(null, "\"" + executable + "\",0");
            using (var command = Registry.CurrentUser.CreateSubKey(
                "Software\\Classes\\NAPP.ipkg\\shell\\open\\command"))
                if (command != null) command.SetValue(null, "\"" + executable + "\" \"%1\"");
        }

        private static void CreateShortcut(string path, string executable)
        {
            var folder = Path.GetDirectoryName(path);
            if (!string.IsNullOrEmpty(folder)) Directory.CreateDirectory(folder);

            var shellType = Type.GetTypeFromProgID("WScript.Shell");
            if (shellType == null) throw new InvalidOperationException("WScript.Shell is not available.");
            var shell = Activator.CreateInstance(shellType);
            var shortcut = shellType.InvokeMember("CreateShortcut", BindingFlags.InvokeMethod, null, shell,
                new object[] { path });
            var type = shortcut.GetType();
            type.InvokeMember("TargetPath", BindingFlags.SetProperty, null, shortcut, new object[] { executable });
            type.InvokeMember("WorkingDirectory", BindingFlags.SetProperty, null, shortcut,
                new object[] { Path.GetDirectoryName(executable) });
            type.InvokeMember("IconLocation", BindingFlags.SetProperty, null, shortcut,
                new object[] { executable + ",0" });
            type.InvokeMember("Description", BindingFlags.SetProperty, null, shortcut,
                new object[] { "NAPP — ILLIERTION web application platform" });
            type.InvokeMember("Save", BindingFlags.InvokeMethod, null, shortcut, null);
        }

        public static void Start(string executable)
        {
            if (!File.Exists(executable)) return;
            Process.Start(new ProcessStartInfo
            {
                FileName = executable,
                WorkingDirectory = Path.GetDirectoryName(executable),
                UseShellExecute = true
            });
        }

        private static string Json(string value)
        {
            return value.Replace("\\", "\\\\").Replace("\"", "\\\"");
        }
    }

    internal sealed class SetupForm : Form
    {
        private readonly Payload payload;
        private readonly RadioButton installRadio;
        private readonly RadioButton portableRadio;
        private readonly TextBox pathBox;
        private readonly CheckBox desktopCheck;
        private readonly CheckBox launchCheck;
        private readonly Button actionButton;
        private readonly Button cancelButton;
        private readonly Label status;
        private readonly ProgressBar progress;

        public SetupForm(Payload payload)
        {
            this.payload = payload;

            Text = "NAPP " + payload.Version + " Setup";
            ClientSize = new Size(592, 396);
            StartPosition = FormStartPosition.CenterScreen;
            FormBorderStyle = FormBorderStyle.FixedDialog;
            MaximizeBox = false;
            MinimizeBox = false;
            Font = new Font("Segoe UI", 9f);

            var heading = new Label
            {
                Text = "Choose how to set up NAPP",
                Font = new Font("Segoe UI", 14f, FontStyle.Bold),
                AutoSize = true,
                Location = new Point(22, 18)
            };
            Controls.Add(heading);

            var subtitle = new Label
            {
                Text = "Install for this Windows account, or keep everything in a folder you can move around.",
                AutoSize = false,
                Size = new Size(540, 34),
                Location = new Point(24, 52),
                ForeColor = Color.FromArgb(90, 96, 106)
            };
            Controls.Add(subtitle);

            installRadio = new RadioButton
            {
                Text = "Install (recommended)",
                Checked = true,
                AutoSize = true,
                Location = new Point(26, 96)
            };
            Controls.Add(installRadio);

            var installHint = new Label
            {
                Text = "Start Menu shortcut, optional desktop shortcut and an uninstall entry. No administrator rights.",
                AutoSize = false,
                Size = new Size(520, 20),
                Location = new Point(46, 118),
                ForeColor = Color.FromArgb(110, 116, 126)
            };
            Controls.Add(installHint);

            portableRadio = new RadioButton
            {
                Text = "Portable",
                AutoSize = true,
                Location = new Point(26, 150)
            };
            Controls.Add(portableRadio);

            var portableHint = new Label
            {
                Text = "Copy the program files only. Nothing is added to the registry or Start Menu.",
                AutoSize = false,
                Size = new Size(520, 20),
                Location = new Point(46, 172),
                ForeColor = Color.FromArgb(110, 116, 126)
            };
            Controls.Add(portableHint);

            var pathLabel = new Label { Text = "Folder", AutoSize = true, Location = new Point(26, 206) };
            Controls.Add(pathLabel);

            pathBox = new TextBox
            {
                Location = new Point(74, 203),
                Size = new Size(400, 24),
                Text = Setup.DefaultTarget(false)
            };
            Controls.Add(pathBox);

            var browse = new Button { Text = "Browse…", Location = new Point(482, 202), Size = new Size(82, 26) };
            Controls.Add(browse);

            desktopCheck = new CheckBox
            {
                Text = "Create a desktop shortcut",
                AutoSize = true,
                Location = new Point(26, 238)
            };
            Controls.Add(desktopCheck);

            launchCheck = new CheckBox
            {
                Text = "Launch NAPP when setup finishes",
                AutoSize = true,
                Checked = true,
                Location = new Point(26, 262)
            };
            Controls.Add(launchCheck);

            status = new Label
            {
                Text = "Ready to install NAPP " + payload.Version + ".",
                AutoSize = false,
                Size = new Size(540, 20),
                Location = new Point(26, 294),
                ForeColor = Color.FromArgb(90, 96, 106)
            };
            Controls.Add(status);

            progress = new ProgressBar
            {
                Location = new Point(26, 318),
                Size = new Size(538, 14),
                Minimum = 0,
                Maximum = 1000
            };
            Controls.Add(progress);

            actionButton = new Button
            {
                Text = "Install",
                Location = new Point(356, 346),
                Size = new Size(104, 32)
            };
            Controls.Add(actionButton);

            cancelButton = new Button
            {
                Text = "Cancel",
                Location = new Point(470, 346),
                Size = new Size(94, 32),
                DialogResult = DialogResult.Cancel
            };
            Controls.Add(cancelButton);
            CancelButton = cancelButton;

            browse.Click += (sender, args) =>
            {
                using (var dialog = new FolderBrowserDialog())
                {
                    dialog.Description = "Select the NAPP folder";
                    dialog.SelectedPath = pathBox.Text;
                    if (dialog.ShowDialog() == DialogResult.OK) pathBox.Text = dialog.SelectedPath;
                }
            };

            installRadio.CheckedChanged += (sender, args) => ApplyMode();
            actionButton.Click += (sender, args) => Begin();
        }

        private void ApplyMode()
        {
            var portable = portableRadio.Checked;
            desktopCheck.Enabled = !portable;
            pathBox.Text = Setup.DefaultTarget(portable);
            actionButton.Text = portable ? "Copy" : "Install";
        }

        private void Begin()
        {
            actionButton.Enabled = false;
            cancelButton.Enabled = false;
            installRadio.Enabled = false;
            portableRadio.Enabled = false;

            Action<string, double> report = (message, fraction) =>
            {
                status.Text = message;
                progress.Value = Math.Max(0, Math.Min(1000, (int)(fraction * 1000)));
                Application.DoEvents();
            };

            try
            {
                Setup.Run(payload, portableRadio.Checked, pathBox.Text, desktopCheck.Checked,
                    launchCheck.Checked, report);
                MessageBox.Show("NAPP " + payload.Version + " is ready.\r\n\r\n" + pathBox.Text,
                    "NAPP Setup", MessageBoxButtons.OK, MessageBoxIcon.Information);
                DialogResult = DialogResult.OK;
                Close();
            }
            catch (OperationCanceledException)
            {
                status.Text = "Setup was cancelled.";
                Reset();
            }
            catch (Exception error)
            {
                MessageBox.Show("Setup could not finish.\r\n\r\n" + error.Message,
                    "NAPP Setup", MessageBoxButtons.OK, MessageBoxIcon.Error);
                status.Text = "Setup failed.";
                progress.Value = 0;
                Reset();
            }
        }

        private void Reset()
        {
            actionButton.Enabled = true;
            cancelButton.Enabled = true;
            installRadio.Enabled = true;
            portableRadio.Enabled = true;
            actionButton.Text = portableRadio.Checked ? "Copy" : "Install";
        }
    }
}
