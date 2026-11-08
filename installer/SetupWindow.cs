// MIT. A compact WPF shell over the verified single-file installer backend.
using System;
using System.Collections.Concurrent;
using System.Diagnostics;
using System.Globalization;
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
    private readonly SetupText copy = new SetupText();
    private readonly Func<string, Action<string>, string, int> backend;
    private readonly ConcurrentQueue<string> pending = new ConcurrentQueue<string>();
    private readonly DispatcherTimer logTimer;
    private readonly StringBuilder log = new StringBuilder();
    private readonly string integrationLicense = Installer.ReadText("LICENSE");
    private readonly string upstreamLicense = Installer.ReadText("HUD-LICENSE.txt");
    private int page, previousPage;
    private bool busy, lastSucceeded;
    private bool chinese;
    private string spotifyDirectory, currentAction = "Install", lastFailure;
    private readonly Regex ansi = new Regex(@"\x1B\[[0-?]*[ -/]*[@-~]", RegexOptions.Compiled);
    private T Find<T>(string name) where T : FrameworkElement { return (T)window.FindName(name); }
    private Button Button(string name) { return Find<Button>(name); }
    private string Text(string key) { return copy.Get(key, chinese); }
    private string ActionKey { get { return currentAction == "Diagnose" ? "check" : currentAction == "Uninstall" ? "disable" : "install"; } }
    private void Title(string key)
    {
        Find<TextBlock>("StepTitle").Text = Text(key + ".title");
        Find<TextBlock>("StepDescription").Text = Text(key + ".description");
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
        Button("Language").Click += delegate { chinese = !chinese; ApplyLanguage(); };
        Button("ProjectLink").Click += delegate {
            try { Process.Start(new ProcessStartInfo("https://github.com/E77zL/spotify_lyrics") { UseShellExecute = true }); }
            catch (Exception error) { MessageBox.Show(window, error.Message, Text("dialog.projectError")); }
        };
        Button("BrowseSpotify").Click += delegate {
            var dialog = new Microsoft.Win32.OpenFileDialog {
                Title = Text("dialog.selectSpotify"), Filter = Text("dialog.spotifyFilter"), CheckFileExists = true
            };
            if (dialog.ShowDialog(window) == true)
            {
                spotifyDirectory = Path.GetDirectoryName(dialog.FileName);
                ApplyLanguage();
            }
        };
        logTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(100) };
        logTimer.Tick += delegate { FlushLog(); };
        window.Closed += delegate { logTimer.Stop(); };
        ApplyLanguage();
    }
    private void ApplyLanguage()
    {
        copy.Apply(window, chinese);
        window.Title = Text("window.title");
        Find<TextBlock>("VersionLabel").Text = "v" + Installer.Version;
        Find<TextBlock>("LicenseText").Text = integrationLicense +
            "\n\n" + Text("license.upstream") + "\n\n" + upstreamLicense +
            "\n\n" + Text("license.components.title") + "\n\n" + Text("license.components.body");
        if (spotifyDirectory != null)
        {
            Find<TextBlock>("FolderLabel").Text = spotifyDirectory;
            Find<TextBlock>("FolderLabel").ToolTip = spotifyDirectory;
        }
        ShowPage(page);
    }
    private string SelectedAction()
    {
        return Find<RadioButton>("DisableOption").IsChecked == true ? "Uninstall" :
            Find<RadioButton>("DiagnoseOption").IsChecked == true ? "Diagnose" : "Install";
    }
    private void RefreshAction()
    {
        string action = SelectedAction();
        Button("Next").Content = Text(action == "Diagnose" ? "button.check" : action == "Uninstall" ? "button.disable" : "button.install");
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
        Button("Next").Content = Text(page == 0 ? "button.next" : page == 2 ? lastSucceeded ? "button.finish" : "button.retry" : "button.done");
        Find<TextBlock>("StepCount").Text = page == 3 ? Text("guide.counter") : "0" + (page + 1) + " / 03";
        if (page == 0) Title("license");
        else if (page == 1) { Title("options"); RefreshAction(); }
        else if (page == 3) Title("guide");
        else RefreshProgress();
    }
    private void RefreshProgress()
    {
        if (busy)
        {
            Find<TextBlock>("StepTitle").Text = Text("working." + ActionKey + ".title");
            Find<TextBlock>("StepDescription").Text = Text("working.description");
            Find<TextBlock>("StatusLabel").Text = Text(currentAction == "Diagnose" ? "working.check.status" : "working.change.status");
        }
        else if (lastSucceeded)
        {
            Find<TextBlock>("StepTitle").Text = Text("success." + ActionKey + ".title");
            Find<TextBlock>("StepDescription").Text = Text(currentAction == "Install" ? "success.install.description" : "success.kept.description");
            Find<TextBlock>("StatusLabel").Text = Text("success." + ActionKey + ".status");
        }
        else
        {
            Title("error");
            Find<TextBlock>("StatusLabel").Text = lastFailure ?? Text("error.status");
        }
    }
    private void SetBusy(bool value)
    {
        busy = value;
        foreach (string name in new[] { "CloseWindow", "Help", "ProjectLink" })
            Button(name).IsEnabled = !value;
        ShowPage(page);
    }
    private void FlushLog()
    {
        string line;
        var batch = new StringBuilder();
        while (pending.TryDequeue(out line))
        {
            line = ansi.Replace(line, "").Trim();
            if (line.Length == 0) continue;
            batch.AppendLine(line);
        }
        if (batch.Length == 0) return;
        log.Append(batch);
        var control = Find<TextBox>("Log");
        if (log.Length > 24000)
        {
            log.Remove(0, log.Length - 20000);
            control.Text = log.ToString();
        }
        else control.AppendText(batch.ToString());
        control.ScrollToEnd();
    }
    private async Task Execute(string action)
    {
        log.Clear();
        Find<TextBox>("Log").Clear();
        lastSucceeded = false;
        lastFailure = null;
        currentAction = action;
        page = 2;
        SetBusy(true);
        Find<ProgressBar>("Progress").IsIndeterminate = true;
        logTimer.Start();
        int result = -1;
        try { result = await Task.Run(delegate { return backend(action, pending.Enqueue, spotifyDirectory); }); }
        catch (Exception error) { lastFailure = error.Message; pending.Enqueue(error.Message); }
        finally
        {
            logTimer.Stop();
            FlushLog();
            lastSucceeded = result == 0;
            Find<ProgressBar>("Progress").IsIndeterminate = false;
            Find<ProgressBar>("Progress").Value = lastSucceeded ? 100 : 0;
            if (!lastSucceeded && lastFailure == null)
                foreach (string line in log.ToString().Split('\n'))
                    if (line.Contains("[ERROR]")) lastFailure = line.Substring(line.IndexOf("[ERROR]", StringComparison.Ordinal) + 7).Trim();
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
                CultureInfo previousCulture = CultureInfo.CurrentUICulture;
                System.Threading.Thread.CurrentThread.CurrentUICulture = new CultureInfo("zh-CN");
                setup = new SetupWindow(delegate(string action, Action<string> output, string folder) {
                    output("\u001b[32mPayload verified\u001b[0m"); release.Wait();
                    if (fail) throw new IOException("Simulated action failure");
                    output("[OK] Environment checked"); return 0;
                });
                System.Threading.Thread.CurrentThread.CurrentUICulture = previousCulture;
                Require(!setup.chinese, "New sessions start in English");
                Require(setup.window.Title == "Lyrics Setup", "Window title");
                Require(setup.window.FindName("BrandLabel") == null && setup.Find<TextBlock>("VersionLabel").Text == "v" + Installer.Version, "Version-only title bar");
                setup.window.Show();
                Require(!setup.Button("Next").IsEnabled, "License must be accepted");
                Require(setup.window.FindName("Export") == null, "Source button removed");
                Require(setup.Find<TextBlock>("LicenseText").Text.IndexOf("Fanbeixi", StringComparison.OrdinalIgnoreCase) >= 0, "Upstream attribution");
                SavePreview(setup.window, outputDirectory, "license-en");
                Click(setup.Button("Language"));
                Require(setup.chinese && setup.Find<TextBlock>("StepTitle").Text == setup.Text("license.title"), "Chinese license page");
                Require(!setup.Button("Next").IsEnabled, "Switch preserves license gating");
                SavePreview(setup.window, outputDirectory, "license-zh");
                setup.Find<CheckBox>("AcceptLicense").IsChecked = true;
                Require(setup.Button("Next").IsEnabled, "License enables Next");
                setup.Button("Next").RaiseEvent(new RoutedEventArgs(System.Windows.Controls.Primitives.ButtonBase.ClickEvent));
                Require(setup.page == 1 && setup.SelectedAction() == "Install", "Next opens options");
                SavePreview(setup.window, outputDirectory, "options-zh");
                Click(setup.Button("Language"));
                SavePreview(setup.window, outputDirectory, "options-en");
                setup.Find<RadioButton>("DiagnoseOption").IsChecked = true;
                setup.spotifyDirectory = @"C:\Music\Spotify";
                Click(setup.Button("Language"));
                Require(setup.SelectedAction() == "Diagnose" && setup.Find<CheckBox>("AcceptLicense").IsChecked == true &&
                    setup.Find<TextBlock>("FolderLabel").Text == setup.spotifyDirectory, "Switch retains action, consent and folder");
                Click(setup.Button("Language"));
                Require(setup.Button("Next").Content.ToString() == setup.Text("button.check"), "Selected operation translated");
                Task work = setup.Execute("Diagnose");
                Require(setup.busy && !setup.Button("CloseWindow").IsEnabled && !setup.Button("Next").IsEnabled, "Busy guard");
                Click(setup.Button("Language"));
                Require(setup.busy && setup.chinese && !setup.Button("Next").IsEnabled &&
                    setup.Find<TextBlock>("StepTitle").Text == setup.Text("working.check.title"), "Busy language switch retains operation");
                release.Set(); await work;
                Require(setup.lastSucceeded && !setup.busy, "Success state");
                Require(!setup.Find<TextBox>("Log").Text.Contains("\u001b") && setup.Find<TextBox>("Log").Text.Contains("[OK]"), "Readable output");
                SavePreview(setup.window, outputDirectory, "complete-zh");
                Click(setup.Button("Language"));
                SavePreview(setup.window, outputDirectory, "complete-en");
                fail = true; await setup.Execute("Install");
                Require(!setup.lastSucceeded && setup.Button("Next").Content.ToString() == setup.Text("button.retry"), "Failure offers retry");
                Require(setup.Find<TextBlock>("StatusLabel").Text.Contains("Simulated"), "Visible failure");
                SavePreview(setup.window, outputDirectory, "error-en");
                Click(setup.Button("Language"));
                Require(setup.Find<TextBlock>("StatusLabel").Text.Contains("Simulated"), "Switch retains backend error");
                SavePreview(setup.window, outputDirectory, "error-zh");
                fail = false;
                foreach (string action in new[] { "Install", "Uninstall" })
                {
                    await setup.Execute(action);
                    Require(setup.lastSucceeded && setup.Find<TextBlock>("StatusLabel").Text == setup.Text("success." + setup.ActionKey + ".status"), "Action completion translated");
                    Click(setup.Button("Language"));
                    Require(setup.Find<TextBlock>("StepTitle").Text == setup.Text("success." + setup.ActionKey + ".title"), "Completed action switches language");
                }
                setup.ShowPage(1);
                setup.Button("Help").RaiseEvent(new RoutedEventArgs(System.Windows.Controls.Primitives.ButtonBase.ClickEvent));
                Require(setup.page == 3, "Help opens");
                SavePreview(setup.window, outputDirectory, "guide-zh");
                Click(setup.Button("Language"));
                SavePreview(setup.window, outputDirectory, "guide-en");
                setup.Button("Back").RaiseEvent(new RoutedEventArgs(System.Windows.Controls.Primitives.ButtonBase.ClickEvent));
                Require(setup.page == 1, "Help returns");
                release.Dispose();
                Console.WriteLine("PASS bilingual pages, license gating, retained selection/path/error, busy language switching, async output, retry and guide navigation");
                result = 0;
            }
            catch (Exception error) { Console.Error.WriteLine(error); }
            finally { if (setup != null) setup.window.Close(); app.Shutdown(result); }
        };
        app.Run(); return result;
    }
    private static void Click(Button button) { button.RaiseEvent(new RoutedEventArgs(System.Windows.Controls.Primitives.ButtonBase.ClickEvent)); }
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
