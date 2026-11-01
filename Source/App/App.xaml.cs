using Microsoft.UI.Xaml;
using System;
using System.IO;

namespace ZinkGTAUI;

public partial class App : Application
{
    private Window? _window;
    public static string LogPath { get; } = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "ZinkGTAUI", "AppStartup.log");

    public static void Log(string text)
    {
        try
        {
            Directory.CreateDirectory(Path.GetDirectoryName(LogPath)!);
            File.AppendAllText(LogPath, $"[{DateTime.Now:yyyy-MM-dd HH:mm:ss.fff}] {text}\r\n");
        }
        catch { }
    }

    public App()
    {
        Log("App constructor entered");
        InitializeComponent();
        Log("App InitializeComponent completed");
        UnhandledException += (_, e) => Log("WinUI UnhandledException: " + e.Exception);
        AppDomain.CurrentDomain.UnhandledException += (_, e) => Log("AppDomain UnhandledException: " + e.ExceptionObject);
    }

    protected override void OnLaunched(LaunchActivatedEventArgs args)
    {
        Log("OnLaunched entered");
        _window = new MainWindow();
        Log("MainWindow constructed");
        _window.Activate();
        Log("MainWindow activated");
    }
}
