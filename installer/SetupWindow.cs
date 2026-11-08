// MIT. A compact WPF shell over the verified single-file installer backend.
using System;
using System.Collections.Concurrent;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Markup;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Shell;
using System.Windows.Threading;

internal sealed class SetupWindow
{
    private readonly Window window;
    private readonly Func<string, Action<string>, string, int> backend;
    private readonly ConcurrentQueue<string> pending = new ConcurrentQueue<string>();
    private readonly DispatcherTimer logTimer;
    private readonly StringBuilder log = new StringBuilder();
    private int page, previousPage;
    private bool busy, lastSucceeded;
    private string spotifyDirectory;
    private readonly Regex ansi = new Regex(@"\x1B\[[0-?]*[ -/]*[@-~]", RegexOptions.Compiled);
    private T Find<T>(string name) where T : FrameworkElement { return (T)window.FindName(name); }
    private Button Button(string name) { return Find<Button>(name); }
    private void Title(string title, string description)
    {
        Find<TextBlock>("StepTitle").Text = title;
        Find<TextBlock>("StepDescription").Text = description;
    }
    internal SetupWindow(Func<string, Action<string>, string, int> runner)
    {
        backend = runner;
        using (Stream xaml = Assembly.GetExecutingAssembly().GetManifestResourceStream("setup.window.xaml"))
            window = (Window)XamlReader.Load(xaml);
        WindowChrome.SetWindowChrome(window, new WindowChrome {
            CaptionHeight = 0, ResizeBorderThickness = new Thickness(0), GlassFrameThickness = new Thickness(0),
            CornerRadius = new CornerRadius(8), UseAeroCaptionButtons = false
        });
        Find<TextBlock>("VersionLabel").Text = "v" + Installer.Version + (Installer.Offline ? "  /  OFFLINE" : "");
        Find<TextBlock>("LicenseText").Text = Installer.ReadText("LICENSE") +
            "\n\nUPSTREAM HUD — FANBEIXI\n\n" + Installer.ReadText("HUD-LICENSE.txt") +
            "\n\nTHIRD-PARTY COMPONENTS\n\nSpicetify CLI: LGPL-2.1. Spicetify Marketplace: MIT. " +
            "Full notices and corresponding source are included. Use the source icon below to export them. " +
            "This is an independent derivative project, not an official Spotify product.";
        Find<Border>("TitleBar").MouseLeftButtonDown += delegate(object sender, MouseButtonEventArgs e) {
            if (e.OriginalSource is TextBlock || e.OriginalSource is Border || e.OriginalSource is Grid)
                window.DragMove();
        };
        Button("Minimize").Click += delegate { window.WindowState = WindowState.Minimized; };
        Button("CloseWindow").Click += delegate { if (!busy) window.Close(); };
        window.Closing += delegate(object sender, System.ComponentModel.CancelEventArgs e) { e.Cancel = busy; };
        window.PreviewKeyDown += delegate(object sender, KeyEventArgs e) {
            if (e.Key == Key.Escape && !busy) { if (page == 3) ShowPage(previousPage); else window.Close(); e.Handled = true; }
        };
        Find<CheckBox>("AcceptLicense").Checked += delegate { if (page == 0) Button("Next").IsEnabled = true; };
        Find<CheckBox>("AcceptLicense").Unchecked += delegate { if (page == 0) Button("Next").IsEnabled = false; };
        foreach (string name in new[] { "InstallOption", "DisableOption", "DiagnoseOption" })
            Find<RadioButton>(name).Checked += delegate { if (page == 1) RefreshAction(); };
        Button("Next").Click += async delegate {
            if (page == 0) ShowPage(1);
            else if (page == 1) await Execute(SelectedAction());
            else if (page == 2) { if (lastSucceeded) window.Close(); else ShowPage(1); }
            else ShowPage(previousPage);
        };
        Button("Back").Click += delegate { ShowPage(page == 3 ? previousPage : page == 2 ? 1 : 0); };
        Button("Help").Click += delegate { if (!busy) { if (page != 3) previousPage = page; ShowPage(page == 3 ? previousPage : 3); } };
        Button("ProjectLink").Click += delegate {
            try { Process.Start(new ProcessStartInfo("https://github.com/E77zL/spotify_lyrics") { UseShellExecute = true }); }
            catch (Exception error) { MessageBox.Show(window, error.Message, "Could not open project"); }
        };
        Button("BrowseSpotify").Click += delegate {
            var dialog = new Microsoft.Win32.OpenFileDialog {
                Title = "Select your Spotify desktop application", Filter = "Spotify desktop|Spotify.exe", CheckFileExists = true
            };
            if (dialog.ShowDialog(window) == true)
            {
                spotifyDirectory = Path.GetDirectoryName(dialog.FileName);
                Find<TextBlock>("FolderLabel").Text = spotifyDirectory;
                Find<TextBlock>("FolderLabel").ToolTip = spotifyDirectory;
            }
        };
        Button("Export").Click += async delegate {
            using (var dialog = new System.Windows.Forms.FolderBrowserDialog { Description = "Choose a folder for source code and licenses", ShowNewFolderButton = true })
            {
                if (dialog.ShowDialog() != System.Windows.Forms.DialogResult.OK) return;
                SetBusy(true);
                try
                {
                    string path = dialog.SelectedPath;
                    await Task.Run(delegate { Installer.ExportSources(path); });
                    MessageBox.Show(window, "Source and licenses exported to:\n" + path, "Export complete", MessageBoxButton.OK, MessageBoxImage.Information);
                }
                catch (Exception error) { MessageBox.Show(window, error.Message, "Export failed", MessageBoxButton.OK, MessageBoxImage.Warning); }
                finally { SetBusy(false); }
            }
        };
        logTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(100) };
        logTimer.Tick += delegate { FlushLog(); };
        window.Closed += delegate { logTimer.Stop(); };
        ShowPage(0);
    }
    private string SelectedAction()
    {
        return Find<RadioButton>("DisableOption").IsChecked == true ? "Uninstall" :
            Find<RadioButton>("DiagnoseOption").IsChecked == true ? "Diagnose" : "Install";
    }
    private void RefreshAction()
    {
        string action = SelectedAction();
        Button("Next").Content = action == "Diagnose" ? "Check" : action == "Uninstall" ? "Disable" : "Install";
    }
    private void ShowPage(int next)
    {
        page = next;
        Find<Grid>("LicensePage").Visibility = page == 0 ? Visibility.Visible : Visibility.Collapsed;
        Find<StackPanel>("OptionsPage").Visibility = page == 1 ? Visibility.Visible : Visibility.Collapsed;
        Find<Grid>("ProgressPage").Visibility = page == 2 ? Visibility.Visible : Visibility.Collapsed;
        Find<ScrollViewer>("HelpPage").Visibility = page == 3 ? Visibility.Visible : Visibility.Collapsed;
        Button("Back").IsEnabled = page != 0 && !busy;
        Button("Next").IsEnabled = !busy && (page != 0 || Find<CheckBox>("AcceptLicense").IsChecked == true);
        Button("Next").Content = page == 0 ? "Next" : page == 2 ? lastSucceeded ? "Finish" : "Retry" : "Done";
        Find<TextBlock>("StepCount").Text = page == 3 ? "GUIDE" : "0" + (page + 1) + " / 03";
        if (page == 0) Title("License Agreement", "A little credit to the people who made this possible.");
        else if (page == 1) { Title("Make yourself at home", "Choose what you would like to do."); RefreshAction(); }
        else if (page == 3) Title("Quick guide", "Everything you need to get started.");
    }
    private void SetBusy(bool value)
    {
        busy = value;
        foreach (string name in new[] { "Next", "Back", "CloseWindow", "Export", "Help", "ProjectLink" })
            Button(name).IsEnabled = !value;
        if (!value) ShowPage(page);
    }
    private void FlushLog()
    {
        string line;
        bool changed = false;
        while (pending.TryDequeue(out line))
        {
            line = ansi.Replace(line, "").Trim();
            if (line.Length == 0) continue;
            log.AppendLine(line);
            changed = true;
        }
        if (!changed) return;
        if (log.Length > 24000) log.Remove(0, log.Length - 20000);
        var control = Find<TextBox>("Log");
        control.Text = log.ToString();
        control.ScrollToEnd();
    }
    private async Task Execute(string action)
    {
        log.Clear();
        Find<TextBox>("Log").Clear();
        lastSucceeded = false;
        ShowPage(2);
        SetBusy(true);
        Title(action == "Diagnose" ? "Checking your setup" : action == "Uninstall" ? "Disabling integration" : "A better view of your music",
              "Please keep this window open until setup finishes.");
        Find<TextBlock>("StatusLabel").Text = action == "Diagnose" ? "Inspecting Spotify and installed components…" : "Verifying files and applying your changes…";
        Find<TextBlock>("CleanupLabel").Text = "Setup removes its temporary files when it finishes.";
        Find<ProgressBar>("Progress").IsIndeterminate = true;
        logTimer.Start();
        int result = -1;
        string failure = null;
        try { result = await Task.Run(delegate { return backend(action, pending.Enqueue, spotifyDirectory); }); }
        catch (Exception error) { failure = error.Message; pending.Enqueue(error.Message); }
        finally
        {
            logTimer.Stop();
            FlushLog();
            lastSucceeded = result == 0;
            Find<ProgressBar>("Progress").IsIndeterminate = false;
            Find<ProgressBar>("Progress").Value = lastSucceeded ? 100 : 0;
            if (lastSucceeded)
            {
                Title(action == "Diagnose" ? "Environment checked" : action == "Uninstall" ? "Integration disabled" : "You're all set",
                      action == "Install" ? "Your next song deserves a better view." : "Your personal settings are kept.");
                Find<TextBlock>("StatusLabel").Text = action == "Install" ? "Open Spotify, then use the A button beside the microphone to show your lyrics." :
                    action == "Diagnose" ? "The results are shown below. No installation files were changed." : "The HUD integration and A button are disabled.";
            }
            else
            {
                Title("Let's get this sorted", "Setup could not complete. Review the details, then retry.");
                string lastError = null;
                foreach (string line in log.ToString().Split('\n'))
                    if (line.Contains("[ERROR]")) lastError = line.Substring(line.IndexOf("[ERROR]", StringComparison.Ordinal) + 7).Trim();
                Find<TextBlock>("StatusLabel").Text = failure ?? lastError ?? "The operation did not finish. Check the log below for details.";
            }
            SetBusy(false);
        }
    }
    internal static int Run()
    {
        var app = new Application();
        var setup = new SetupWindow(Installer.RunAction);
        return app.Run(setup.window);
    }
    // Exercise real UI transitions with an injected backend; never modify Spotify.
    internal static int SelfTest(string outputDirectory)
    {
        int result = 1;
        var app = new Application { ShutdownMode = ShutdownMode.OnExplicitShutdown };
        app.Startup += async delegate {
            SetupWindow setup = null;
            try
            {
                bool fail = false;
                var release = new System.Threading.ManualResetEventSlim(false);
                setup = new SetupWindow(delegate(string action, Action<string> output, string folder) {
                    output("\u001b[32mPayload verified\u001b[0m"); release.Wait();
                    if (fail) throw new IOException("Simulated action failure");
                    output("[OK] Environment checked"); return 0;
                });
                setup.window.Show();
                Require(!setup.Button("Next").IsEnabled, "License must be accepted");
                Require(setup.Find<TextBlock>("LicenseText").Text.IndexOf("Fanbeixi", StringComparison.OrdinalIgnoreCase) >= 0, "Upstream attribution");
                SavePreview(setup.window, outputDirectory, "license");
                setup.Find<CheckBox>("AcceptLicense").IsChecked = true;
                Require(setup.Button("Next").IsEnabled, "License enables Next");
                setup.Button("Next").RaiseEvent(new RoutedEventArgs(System.Windows.Controls.Primitives.ButtonBase.ClickEvent));
                Require(setup.page == 1 && setup.SelectedAction() == "Install", "Next opens options");
                SavePreview(setup.window, outputDirectory, "options");
                setup.Find<RadioButton>("DiagnoseOption").IsChecked = true;
                Require(setup.Button("Next").Content.ToString() == "Check", "Selected operation updates button");
                Task work = setup.Execute("Diagnose");
                Require(setup.busy && !setup.Button("CloseWindow").IsEnabled && !setup.Button("Next").IsEnabled, "Busy guard");
                release.Set(); await work;
                Require(setup.lastSucceeded && !setup.busy, "Success state");
                Require(!setup.Find<TextBox>("Log").Text.Contains("\u001b") && setup.Find<TextBox>("Log").Text.Contains("[OK]"), "Readable output");
                SavePreview(setup.window, outputDirectory, "complete");
                fail = true; await setup.Execute("Install");
                Require(!setup.lastSucceeded && setup.Button("Next").Content.ToString() == "Retry", "Failure offers retry");
                Require(setup.Find<TextBlock>("StatusLabel").Text.Contains("Simulated"), "Visible failure");
                SavePreview(setup.window, outputDirectory, "error");
                setup.ShowPage(1);
                setup.Button("Help").RaiseEvent(new RoutedEventArgs(System.Windows.Controls.Primitives.ButtonBase.ClickEvent));
                Require(setup.page == 3, "Help opens");
                setup.Button("Back").RaiseEvent(new RoutedEventArgs(System.Windows.Controls.Primitives.ButtonBase.ClickEvent));
                Require(setup.page == 1, "Help returns");
                release.Dispose();
                Console.WriteLine("PASS WPF license gating, action selection, async output, busy guard, success, retry, errors and help navigation");
                result = 0;
            }
            catch (Exception error) { Console.Error.WriteLine(error); }
            finally { if (setup != null) setup.window.Close(); app.Shutdown(result); }
        };
        app.Run(); return result;
    }
    private static void Require(bool condition, string message) { if (!condition) throw new InvalidOperationException(message); }
    private static void SavePreview(Window window, string directory, string name)
    {
        if (String.IsNullOrEmpty(directory)) return;
        Directory.CreateDirectory(directory);
        foreach (int scale in new[] { 1, 2 })
        {
            window.UpdateLayout();
            var target = new RenderTargetBitmap((int)window.Width * scale, (int)window.Height * scale, 96 * scale, 96 * scale, PixelFormats.Pbgra32);
            target.Render(window);
            var png = new PngBitmapEncoder(); png.Frames.Add(BitmapFrame.Create(target));
            using (Stream file = File.Create(Path.Combine(directory, name + "-" + scale + "x.png"))) png.Save(file);
        }
    }
}
