using System;
using System.IO;
using System.IO.Compression;
using System.Text;
using System.Reflection;
using System.Diagnostics;
using System.Security.Cryptography;
using System.Collections.Generic;
using System.Runtime.InteropServices;

[assembly: AssemblyTitle("Spotify Lyrics HUD Companion Setup")]
[assembly: AssemblyDescription("Native in-process Spotify lyrics overlay")]
[assembly: AssemblyCompany("Lyrics HUD integration contributors")]
[assembly: AssemblyProduct("Spotify Lyrics HUD Companion")]
[assembly: AssemblyVersion("2.0.3.0")]
[assembly: AssemblyFileVersion("2.0.3.0")]

internal static class Installer
{
    private static readonly Assembly Self = Assembly.GetExecutingAssembly();
    private static readonly Dictionary<string, string> Hashes = new Dictionary<string, string>(StringComparer.Ordinal);
    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool AttachConsole(int process);
    internal static string Version { get { return Self.GetName().Version.ToString(3); } }
    internal static bool Offline { get { return HasResource("spicetify-windows-x64.zip"); } }
    internal static string ReadText(string name)
    {
        using (var content = new MemoryStream())
        {
            CopyVerified(name, content);
            return Encoding.UTF8.GetString(content.ToArray()).TrimStart('\uFEFF');
        }
    }

    private static Stream OpenResource(string name)
    {
        Stream stream = Self.GetManifestResourceStream("payload." + name);
        if (stream == null) throw new InvalidDataException("Missing payload: " + name);
        return new DeflateStream(stream, CompressionMode.Decompress);
    }
    private static bool HasResource(string name)
    {
        using (Stream stream = Self.GetManifestResourceStream("payload." + name)) return stream != null;
    }
    private static void LoadManifest()
    {
        string content;
        using (var reader = new StreamReader(OpenResource("manifest.sha256"), Encoding.UTF8)) content = reader.ReadToEnd();
        foreach (string row in content.Split(new char[] { '\r', '\n' }, StringSplitOptions.RemoveEmptyEntries))
        {
            int split = row.IndexOf("  ", StringComparison.Ordinal);
            if (split != 64) throw new InvalidDataException("Invalid payload manifest");
            string name = row.Substring(split + 2);
            if (name != Path.GetFileName(name) || name.IndexOfAny(Path.GetInvalidFileNameChars()) >= 0)
                throw new InvalidDataException("Unsafe payload name");
            Hashes.Add(name, row.Substring(0, split));
        }
        if (Hashes.Count != 19) throw new InvalidDataException("Unexpected payload count");
    }
    private static void CopyVerified(string name, Stream destination)
    {
        using (Stream source = OpenResource(name))
        using (SHA256 hash = SHA256.Create())
        {
            // Hash while extracting. Large payloads never become large managed arrays.
            byte[] buffer = new byte[65536];
            int count;
            while ((count = source.Read(buffer, 0, buffer.Length)) != 0)
            {
                hash.TransformBlock(buffer, 0, count, buffer, 0);
                if (destination != null) destination.Write(buffer, 0, count);
            }
            hash.TransformFinalBlock(new byte[0], 0, 0);
            string actual = BitConverter.ToString(hash.Hash).Replace("-", "").ToLowerInvariant();
            if (!String.Equals(actual, Hashes[name], StringComparison.OrdinalIgnoreCase))
                throw new InvalidDataException("Payload hash mismatch: " + name);
        }
    }
    private static void ExtractVerified(string name, string file)
    {
        bool created = false;
        try
        {
            using (var output = new FileStream(file, FileMode.CreateNew, FileAccess.Write))
            { created = true; CopyVerified(name, output); }
        }
        catch { if (created) File.Delete(file); throw; }
    }
    internal static int RunAction(string action, Action<string> output = null, string spotifyDirectory = null)
    {
        if (action != "Install" && action != "Uninstall" && action != "Diagnose" && action != "Verify")
            throw new ArgumentException("Unknown setup action.");
        if (output != null) output("Verifying and preparing the installation files…");
        string tempBase = Path.GetFullPath(Path.GetTempPath());
        string temp = Path.Combine(tempBase, "SpotifyNetEaseSetup-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(temp);
        try
        {
            foreach (string name in Hashes.Keys)
                if (HasResource(name)) ExtractVerified(name, Path.Combine(temp, name));
            string powershell = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.System), "WindowsPowerShell", "v1.0", "powershell.exe");
            if (!File.Exists(powershell)) throw new FileNotFoundException("Windows PowerShell 5.1 is required.");
            ProcessStartInfo info = new ProcessStartInfo();
            info.FileName = powershell;
            info.Arguments = "-NoLogo -NoProfile -ExecutionPolicy Bypass -File \"" + Path.Combine(temp, "ConsoleActions.ps1") + "\" -Action " + action;
            info.UseShellExecute = false;
            info.WorkingDirectory = temp;
            if (output != null)
            {
                info.CreateNoWindow = true;
                info.RedirectStandardOutput = true;
                info.RedirectStandardError = true;
                info.RedirectStandardInput = true;
                info.StandardOutputEncoding = Encoding.UTF8;
                info.StandardErrorEncoding = Encoding.UTF8;
                info.EnvironmentVariables["LYRICS_SETUP_GUI"] = "1";
                if (!String.IsNullOrEmpty(spotifyDirectory))
                    info.EnvironmentVariables["LYRICS_SETUP_SPOTIFY_PATH"] = spotifyDirectory;
                info.Arguments = "-NonInteractive " + info.Arguments;
            }
            // Do not inherit a PowerShell 7-only module path into Windows PowerShell 5.1.
            info.EnvironmentVariables["PSModulePath"] =
                Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments), "WindowsPowerShell", "Modules") + ";" +
                Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles), "WindowsPowerShell", "Modules") + ";" +
                Path.Combine(Path.GetDirectoryName(powershell), "Modules");
            using (var child = new Process { StartInfo = info })
            {
                if (output != null)
                {
                    child.OutputDataReceived += delegate(object sender, DataReceivedEventArgs e) { if (e.Data != null) output(e.Data); };
                    child.ErrorDataReceived += delegate(object sender, DataReceivedEventArgs e) { if (e.Data != null) output(e.Data); };
                }
                child.Start();
                if (output != null)
                {
                    child.StandardInput.Close();
                    child.BeginOutputReadLine();
                    child.BeginErrorReadLine();
                }
                child.WaitForExit();
                return child.ExitCode;
            }
        }
        finally
        {
            // Delete only the unique staging directory created by this invocation.
            string resolved = Path.GetFullPath(temp);
            if (String.Equals(Path.GetDirectoryName(resolved).TrimEnd(Path.DirectorySeparatorChar), tempBase.TrimEnd(Path.DirectorySeparatorChar), StringComparison.OrdinalIgnoreCase)
                && Path.GetFileName(resolved).StartsWith("SpotifyNetEaseSetup-", StringComparison.Ordinal))
            {
                try { Directory.Delete(resolved, true); }
                catch
                {
                    const string warning = "Some temporary files are still in use and remain in the system temp folder.";
                    if (output != null) output(warning);
                    else Console.WriteLine(warning);
                }
            }
        }
    }
    private static void Instructions()
    {
        Console.WriteLine("Spotify Lyrics HUD Setup " + Version);
        Console.WriteLine("Run without arguments for the graphical installer.");
        Console.WriteLine("Close Spotify, including the tray icon, before Install / Repair or Disable integration.");
        Console.WriteLine("Open Spotify after installation; the A button beside the microphone toggles desktop lyrics.");
        Console.WriteLine("Hover over lyrics for timing, typography, opacity and colors. Drag the edge to resize.");
        Console.WriteLine("LRCLIB -> QQ Music -> NetEase fallback. The native HUD exits with Spotify.");
        Console.WriteLine("Standard and administrator sessions are supported. No resident launcher or updater.");
        Console.WriteLine("Use the source icon in setup to export source and licenses.");
        Console.WriteLine("HUD / integration: MIT; Spicetify CLI: LGPL-2.1. Upstream HUD: Fanbeixi.");
    }
    internal static void ExportSources(string destination)
    {
        Directory.CreateDirectory(destination);
        string[] names = { "spicetify-source-v2.46.4.tar.gz", "HUD-INTEGRATION-SOURCE.zip", "HUD-LICENSE.txt", "SPICETIFY-LICENSE", "MARKETPLACE-LICENSE.txt", "LICENSE", "NOTICE.txt" };
        foreach (string name in names)
            if (File.Exists(Path.Combine(destination, name))) throw new IOException("File already exists: " + name);
        foreach (string name in names) ExtractVerified(name, Path.Combine(destination, name));
    }
    [STAThread]
    public static int Main(string[] args)
    {
        try
        {
            if (args.Length > 0 && args[0] != "--ui-self-test")
            {
                AttachConsole(-1);
                Console.OutputEncoding = new UTF8Encoding(false);
                Console.InputEncoding = Encoding.UTF8;
            }
            LoadManifest();
            if (args.Length == 0) return SetupWindow.Run();
            if (args.Length > 0)
            {
                if (args[0] == "--ui-self-test") return SetupWindow.SelfTest(args.Length > 1 ? args[1] : null);
                if (args[0] == "--self-test")
                {
                    foreach (string name in Hashes.Keys)
                    {
                        if (!HasResource(name))
                        {
                            if (name != "spicetify-windows-x64.zip") throw new InvalidDataException("Missing payload: " + name);
                            Console.WriteLine("PASS optional pinned dependency " + name);
                            continue;
                        }
                        CopyVerified(name, null); Console.WriteLine("PASS SHA256 " + name);
                    }
                    int result = RunAction("Verify", Console.WriteLine);
                    Console.WriteLine(result == 0 ? "PASS single-file extraction / PowerShell verification / cleanup" : "FAIL payload verification");
                    return result;
                }
                if (args[0] == "--diagnose") return RunAction("Diagnose");
                if (args[0] == "--install") return RunAction("Install");
                if (args[0] == "--uninstall") return RunAction("Uninstall");
                if (args[0] == "--help") { Instructions(); return 0; }
                Console.WriteLine("Usage: Spotify-Lyrics-HUD-Setup-v2.0.3.exe [--self-test | --diagnose | --help | --install | --uninstall]");
                return 2;
            }
            return 2;
        }
        catch (Exception error)
        {
            if (args.Length == 0) System.Windows.MessageBox.Show(error.Message, "Setup could not start", System.Windows.MessageBoxButton.OK, System.Windows.MessageBoxImage.Error);
            else Console.Error.WriteLine("[ERROR] " + error.Message);
            return 1;
        }
    }
}
