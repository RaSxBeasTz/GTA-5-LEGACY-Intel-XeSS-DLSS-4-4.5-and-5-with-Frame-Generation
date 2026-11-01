using Microsoft.UI;

using Microsoft.UI.Windowing;

using Microsoft.UI.Xaml;

using Microsoft.UI.Xaml.Controls;

using Microsoft.Win32;

using System;

using System.Diagnostics;

using System.IO;

using System.Linq;

using System.Runtime.InteropServices;

using System.Text.Json;

using Windows.Storage.Pickers;

using WinRT.Interop;


namespace ZinkGTAUI;

public sealed partial class MainWindow : Window

{

    readonly ModBackend backend = new();

    readonly DispatcherTimer statusTimer = new() { Interval=TimeSpan.FromSeconds(1) };
    readonly DispatcherTimer autoApplyTimer = new() { Interval=TimeSpan.FromMilliseconds(650) };

    bool loading=true,dialogOpen,arcAvailable,providerScanning;

    string provider="NVIDIA";
    int nvidiaAdapterIndex=-1,intelAdapterIndex=-1;
    string nvidiaAdapterName="",intelAdapterName="";
    bool applying;
    int progressRevision;
    int renderWidth,renderHeight;

    int nvidiaMaxMultiplier;

    bool nvidiaDlss,nvidiaRtx40;
    System.Threading.Tasks.Task? nvidiaProbeTask;
    string? nvidiaProbeError;
    System.Threading.Tasks.Task EnsureNvidiaProbe()=>nvidiaProbeTask??=ProbeNvidia();

    public MainWindow()

    {

        InitializeComponent();

        AboutVersionText.Text=$"App {ReadAppReleaseVersion()} · Mod {ReadBundledModBuild()}";
        BuildIdentityText.Text=ReadAppBuildIdentity();

        var windowId=Win32Interop.GetWindowIdFromWindow(WindowNative.GetWindowHandle(this));

        var appWindow=AppWindow.GetFromWindowId(windowId);
        appWindow.Resize(new Windows.Graphics.SizeInt32(1200,760));
        if(appWindow.Presenter is OverlappedPresenter presenter)presenter.Maximize();
        ExtendsContentIntoTitleBar=true;SetTitleBar(AppTitleBar);
        appWindow.TitleBar.ButtonBackgroundColor=Colors.Transparent;
        appWindow.TitleBar.ButtonInactiveBackgroundColor=Colors.Transparent;
        appWindow.TitleBar.ButtonForegroundColor=Colors.White;
        appWindow.SetIcon(Path.Combine(AppContext.BaseDirectory,"Assets","Zink.ico"));

        Root.Loaded+=(_,_)=>InitializeBackend();

        statusTimer.Tick+=(_,_)=>UpdateStatus();
        autoApplyTimer.Tick+=AutoApplyTimer_Tick;
        WireAutomaticSettings();

        Closed+=(_,_)=>{statusTimer.Stop();autoApplyTimer.Stop();};

    }

    async void InitializeBackend()

    {

        try {

            var preferences=backend.LoadPreferences();
            if(!File.Exists(Path.Combine(preferences.GameDirectory,"GTA5.exe"))&&ModBackend.DiscoverGameDirectory() is string discoveredGame){preferences.GameDirectory=discoveredGame;App.Log("Auto-detected GTA: "+discoveredGame);}
            if(!File.Exists(preferences.GameSettingsPath)&&ModBackend.DiscoverSettingsPath() is string discoveredSettings){preferences.GameSettingsPath=discoveredSettings;App.Log("Auto-detected GTA settings: "+discoveredSettings);}

            if(!File.Exists(Path.Combine(backend.DataRoot,"preferences.json"))) {

                backend.ReadInstalled(preferences);

                // Old builds forced1080p even when their INI said1440p. Reflect the real game dimensions until the new bridge is installed.

                if(!File.Exists(Path.Combine(preferences.GameDirectory,"GTAXeSS-AppBridge.json"))&&File.Exists(preferences.GameSettingsPath)) {

                    var video=System.Xml.Linq.XDocument.Load(preferences.GameSettingsPath).Root?.Element("video");

                    var width=video?.Element("ScreenWidth")?.Attribute("value")?.Value;

                    preferences.Resolution=width=="3840"?"4K":width=="2560"?"1440P":"1080P";

                }

            }

            SetControls(preferences);AutoApplyToggle.IsOn=true;RefreshProfiles();LoadHardware(provider);await EnsureNvidiaProbe();UpdateProvider();loading=false;await ScanProviderEnvironment(false);UpdateStatus();statusTimer.Start();

        } catch(Exception ex) {loading=false;ShowError(ex);statusTimer.Start();}

    }

    ModSettings GetControls()=>new() {

        GameDirectory=GamePathBox.Text.Trim(),GameSettingsPath=GameSettingsBox.Text.Trim(),

        Resolution=(ResolutionChoice.SelectedItem as ComboBoxItem)?.Tag?.ToString()??"1080P",

        SourceFPS=int.Parse((SourceFpsChoice.SelectedItem as ComboBoxItem)?.Tag?.ToString()??"0"),

        Provider=provider,NvidiaAdapterIndex=nvidiaAdapterIndex,NvidiaAdapterName=nvidiaAdapterName,IntelAdapterIndex=intelAdapterIndex,IntelAdapterName=intelAdapterName,NvidiaMultiplier=int.Parse((MultiplierChoice.SelectedItem as ComboBoxItem)?.Tag?.ToString()??"2"),NvidiaDlssVersion=(DlssVersionChoice.SelectedItem as ComboBoxItem)?.Tag?.ToString()??"4.5",NvidiaDlss5=provider=="NVIDIA"&&(DlssVersionChoice.SelectedItem as ComboBoxItem)?.Tag?.ToString()=="5",NvidiaDlss5Rtx40Experimental=Dlss5Rtx40Toggle.IsOn,NvidiaDlaa=provider=="NVIDIA"&&((QualityChoice.SelectedItem as ComboBoxItem)?.Tag?.ToString()??"NATIVE")!="NATIVE",UpscalingQuality=(QualityChoice.SelectedItem as ComboBoxItem)?.Tag?.ToString()??"NATIVE",InputWidth=renderWidth,InputHeight=renderHeight,AutoStartMod=ModEnabledToggle.IsOn,

        FrameGeneration=FrameGenerationToggle.IsOn,IntelArcOnly=IntelArcToggle.IsOn,IntelDlss5DirectMl=provider=="Intel"&&IntelDlss5Toggle.IsOn,Diagnostics=DiagnosticsToggle.IsOn,

        Hud=HudToggle.IsOn,ShowFPS=FpsToggle.IsOn,ShowFrameTimes=FrameTimesToggle.IsOn,ShowUsage=UsageToggle.IsOn,ShowStatus=StatusToggle.IsOn,AutoApply=AutoApplyToggle.IsOn,VisualEnhancement=VisualEnhancementToggle.IsOn,VisualModDownloadUrl=VisualModDownloadUrlBox.Text.Trim()

    };

    void SetControls(ModSettings s) {

        DlssVersionChoice.SelectedIndex=s.EffectiveDlssVersion=="5"?2:s.EffectiveDlssVersion=="4"?0:1;Dlss5Rtx40Toggle.IsOn=s.NvidiaDlss5Rtx40Experimental;provider=s.Provider;nvidiaAdapterIndex=s.NvidiaAdapterIndex;nvidiaAdapterName=s.NvidiaAdapterName;intelAdapterIndex=s.IntelAdapterIndex;intelAdapterName=s.IntelAdapterName;renderWidth=s.InputWidth;renderHeight=s.InputHeight;var quality=s.UpscalingQuality=="NATIVE"&&s.NvidiaDlaa?"AA":s.UpscalingQuality;QualityChoice.SelectedIndex=Math.Max(0,Array.IndexOf(new[]{"NATIVE","AA","QUALITY","BALANCED","PERFORMANCE","ULTRA_PERFORMANCE","ULTRA_QUALITY","ULTRA_QUALITY_PLUS"},quality));MultiplierChoice.SelectedIndex=provider=="Intel"?1:s.NvidiaMultiplier==6?2:s.NvidiaMultiplier==4?1:0;PopulateGpuChoices();UpdateProvider();

        GamePathBox.Text=s.GameDirectory;GameSettingsBox.Text=s.GameSettingsPath;

        ResolutionChoice.SelectedIndex=s.Resolution=="4K"?2:s.Resolution=="1440P"?1:0;

        var indices=new[]{0,30,60,90,120};SourceFpsChoice.SelectedIndex=Math.Max(0,Array.IndexOf(indices,s.SourceFPS));

        FrameGenerationToggle.IsOn=s.FrameGeneration;IntelArcToggle.IsOn=s.IntelArcOnly;IntelDlss5Toggle.IsOn=s.IntelDlss5DirectMl;DiagnosticsToggle.IsOn=s.Diagnostics;ModEnabledToggle.IsOn=s.AutoStartMod;

        HudToggle.IsOn=s.Hud;FpsToggle.IsOn=s.ShowFPS;FrameTimesToggle.IsOn=s.ShowFrameTimes;UsageToggle.IsOn=s.ShowUsage;StatusToggle.IsOn=s.ShowStatus;AutoApplyToggle.IsOn=s.AutoApply;VisualEnhancementToggle.IsOn=s.VisualEnhancement;VisualModDownloadUrlBox.Text=s.VisualModDownloadUrl;

    }

    void ShowMessage(string title,string message,InfoBarSeverity severity=InfoBarSeverity.Success){MessageBar.Title=title;MessageBar.Message=message;MessageBar.Severity=severity;MessageBar.IsOpen=true;App.Log(title+": "+message);}

    void ShowError(Exception ex)=>ShowMessage("Could not complete the action",ex.Message,InfoBarSeverity.Error);

    void WireAutomaticSettings(){
        foreach(var combo in new[]{ResolutionChoice,QualityChoice,DlssVersionChoice,MultiplierChoice,SourceFpsChoice})combo.SelectionChanged+=SettingsChanged;
        foreach(var toggle in new[]{Dlss5Rtx40Toggle,IntelDlss5Toggle,FrameGenerationToggle,IntelArcToggle,HudToggle,FpsToggle,FrameTimesToggle,UsageToggle,StatusToggle,DiagnosticsToggle,ModEnabledToggle,AutoApplyToggle,VisualEnhancementToggle})toggle.Toggled+=SettingsChanged;
    }
    void SettingsChanged(object sender,RoutedEventArgs e){
        if(loading||applying||providerScanning)return;
        if(ReferenceEquals(sender,IntelDlss5Toggle))UpdateProvider();
        if(ReferenceEquals(sender,AutoApplyToggle)&&!AutoApplyToggle.IsOn){autoApplyTimer.Stop();ApplyProgressRing.IsActive=false;ApplyProgressPanel.Visibility=Visibility.Collapsed;return;}
        if(!AutoApplyToggle.IsOn)return;
        ++progressRevision;autoApplyTimer.Stop();autoApplyTimer.Start();
        ApplyProgressText.Text="Waiting to apply changes…";ApplyProgressRing.IsActive=true;ApplyProgressPanel.Visibility=Visibility.Visible;
    }
    async void AutoApplyTimer_Tick(object? sender,object e){autoApplyTimer.Stop();await ApplySettings(true);}
    async System.Threading.Tasks.Task FinishProgressIndicator(int revision,string text){
        ApplyProgressRing.IsActive=false;ApplyProgressText.Text=text;await System.Threading.Tasks.Task.Delay(1800);
        if(revision==progressRevision&&!applying&&!providerScanning)ApplyProgressPanel.Visibility=Visibility.Collapsed;
    }

    async System.Threading.Tasks.Task<bool> ApplySettings(bool automatic=false){
        if(applying)return false;var revision=++progressRevision;applying=true;autoApplyTimer.Stop();ApplyProgressPanel.Visibility=Visibility.Visible;ApplyProgressRing.IsActive=true;ApplyProgressText.Text="Applying changes to GTA and mod…";UpdateStatus();
        try{
            await EnsureNvidiaProbe();var settings=GetControls();ValidateNvidia(settings);var marker=Path.Combine(settings.GameDirectory,"GTAXeSS-AppBridge.json");
            if(ModBackend.IsGameRunning()){
                if(!File.Exists(marker))throw new InvalidOperationException("GTA is running without the app bridge. Close GTA once; Load GTA + mod will install it automatically.");
                bool pending=backend.ApplyLive(settings);ShowMessage(automatic?"Changes applied automatically":"Settings saved",pending?"Live settings were updated. Provider, resolution, upscaling, DLSS version or diagnostics changes will finish on the next GTA launch.":"The running mod was updated. Live controls apply on upcoming frames.");
            }else{
                await System.Threading.Tasks.Task.Run(()=>backend.ResolveInput(settings,Path.Combine(AppContext.BaseDirectory,"Backend")));bool needsInstall=!File.Exists(marker);if(!needsInstall){try{using var bridge=JsonDocument.Parse(File.ReadAllText(marker));needsInstall=!bridge.RootElement.TryGetProperty("build",out var build)||build.GetString()!=ReadBundledModBuild()||!bridge.RootElement.TryGetProperty("liveSettings",out _);}catch{needsInstall=true;}}
                string? installBackup=null;if(needsInstall)installBackup=backend.Install(settings,Path.Combine(AppContext.BaseDirectory,"Backend"));var intelNrStatus=await backend.ConfigureIntelDlss5Async(settings);var visualProgress=new Progress<string>(message=>ApplyProgressText.Text=message);var visualStatus=await backend.ConfigureVisualModAsync(settings,visualProgress);var backup=backend.Apply(settings);QualityChoice.SelectedIndex=Array.IndexOf(new[]{"NATIVE","AA","QUALITY","BALANCED","PERFORMANCE","ULTRA_PERFORMANCE","ULTRA_QUALITY","ULTRA_QUALITY_PLUS"},settings.UpscalingQuality);renderWidth=settings.InputWidth;renderHeight=settings.InputHeight;var output=settings.Dimensions;ShowMessage(needsInstall?"Mod installed and settings applied":automatic?"Changes applied automatically":"Settings applied",$"GTA renders at {renderWidth} x {renderHeight}; output {output.Width} x {output.Height}. {intelNrStatus} {visualStatus} The mod is {(settings.AutoStartMod?"enabled and will load automatically":"installed but disabled")}. Pre-install backup: {installBackup??"already installed"}. Settings backup: {backup}");
            }
            _=FinishProgressIndicator(revision,"✓ Changes applied");
            return true;
        }catch(Exception ex){ApplyProgressRing.IsActive=false;ApplyProgressText.Text="Changes were not applied";ShowError(ex);return false;}finally{applying=false;UpdateStatus();}
    }
    async void Apply_Click(object sender,RoutedEventArgs e)=>await ApplySettings();
    async void Install_Click(object sender,RoutedEventArgs e){
        if(applying)return;applying=true;UpdateStatus();try{await EnsureNvidiaProbe();var settings=GetControls();ValidateNvidia(settings);await System.Threading.Tasks.Task.Run(()=>backend.ResolveInput(settings,Path.Combine(AppContext.BaseDirectory,"Backend")));var backup=backend.Install(settings,Path.Combine(AppContext.BaseDirectory,"Backend"));var intelNrStatus=await backend.ConfigureIntelDlss5Async(settings);var visualProgress=new Progress<string>(message=>ApplyProgressText.Text=message);var visualStatus=await backend.ConfigureVisualModAsync(settings,visualProgress);backend.Apply(settings);ShowMessage("Mod installed and settings applied","Load GTA to activate the mod automatically. "+intelNrStatus+" "+visualStatus+" Previous runtime: "+backup);}catch(Exception ex){ShowError(ex);}finally{applying=false;UpdateStatus();}
    }
    async void BrowseGame_Click(object sender,RoutedEventArgs e) {

        try {var picker=new FolderPicker();InitializeWithWindow.Initialize(picker,WindowNative.GetWindowHandle(this));picker.FileTypeFilter.Add("*");var folder=await picker.PickSingleFolderAsync();if(folder!=null){GamePathBox.Text=folder.Path;UpdateStatus();}}catch(Exception ex){ShowError(ex);}

    }

    async void BrowseSettings_Click(object sender,RoutedEventArgs e) {

        try {var picker=new FileOpenPicker();InitializeWithWindow.Initialize(picker,WindowNative.GetWindowHandle(this));picker.FileTypeFilter.Add(".xml");var file=await picker.PickSingleFileAsync();if(file!=null)GameSettingsBox.Text=file.Path;}catch(Exception ex){ShowError(ex);}

    }

    async void SaveProfile_Click(object sender,RoutedEventArgs e) {

        try {if(!await ApplySettings())return;backend.SaveProfile(ProfileNameBox.Text.Trim(),GetControls());RefreshProfiles();ShowMessage("Profile and settings saved","Supported controls are applied; render changes take effect on the next game launch.");}catch(Exception ex){ShowError(ex);}

    }

    void RefreshProfiles(){var prior=loading;loading=true;ProfileChoice.Items.Clear();foreach(var name in backend.Profiles().OrderBy(n=>n))ProfileChoice.Items.Add(name);loading=prior;}

    void ProfileChoice_SelectionChanged(object sender,SelectionChangedEventArgs e){if(loading||ProfileChoice.SelectedItem is not string name)return;try{SetControls(backend.LoadProfile(name));ProfileNameBox.Text=name;ShowMessage("Profile loaded","Press Apply to update GTA.",InfoBarSeverity.Informational);}catch(Exception ex){ShowError(ex);}}

    void Reset_Click(object sender,RoutedEventArgs e){var current=GetControls();var defaults=new ModSettings{GameDirectory=current.GameDirectory,GameSettingsPath=current.GameSettingsPath};SetControls(defaults);ShowMessage("Defaults loaded","NVIDIA DLSS Quality, 1080p output and uncapped source FPS are selected. Changes apply automatically.",InfoBarSeverity.Informational);SettingsChanged(sender,e);}

    void OpenFolder(string path){Directory.CreateDirectory(path);Process.Start(new ProcessStartInfo(path){UseShellExecute=true});}

    void Backups_Click(object sender,RoutedEventArgs e){try{OpenFolder(Path.Combine(backend.DataRoot,"backups"));}catch(Exception ex){ShowError(ex);}}

    async void Restore_Click(object sender,RoutedEventArgs e){
        if(applying)return;try{var dialog=new ContentDialog{XamlRoot=Root.XamlRoot,Title="Restore GTA folder?",Content="This restores the files and GTA settings captured immediately before the most recent mod installation. A safety backup of the current state is created first. GTA must be closed.",PrimaryButtonText="Restore",CloseButtonText="Cancel",DefaultButton=ContentDialogButton.Close};if(await dialog.ShowAsync()!=ContentDialogResult.Primary)return;applying=true;UpdateStatus();var safety=backend.RestoreBeforeInstall(GamePathBox.Text.Trim());ShowMessage("GTA folder restored","The pre-install files and settings were restored. Safety backup of the replaced mod state: "+safety);}catch(Exception ex){ShowError(ex);}finally{applying=false;UpdateStatus();}
    }

    void Logs_Click(object sender,RoutedEventArgs e){try{var dir=GamePathBox.Text.Trim();if(!File.Exists(Path.Combine(dir,"GTA5.exe")))throw new InvalidOperationException("Choose the GTA folder first.");OpenFolder(dir);}catch(Exception ex){ShowError(ex);}}

    async void Launch_Click(object sender,RoutedEventArgs e){try{
        bool wasRunning=ModBackend.IsGameRunning();if(!await ApplySettings())return;
        if(ModBackend.IsGameRunning()){bool requested=backend.RequestActivation(GetControls());ShowMessage("GTA is already running",requested?"Live controls applied and mod activation requested. Render changes remain queued for the next launch.":"Effects are disabled in the active configuration. Queued render changes apply after restarting GTA.");return;}
        if(wasRunning&&!await ApplySettings())return;
        var settings=GetControls();if(settings.Provider=="NVIDIA")ModBackend.EnsureNvidiaHighPerformanceGpu(settings.GameDirectory);ModBackend.EnsureGtaLaunchOptions(settings.GameDirectory,settings);
        Process.Start(new ProcessStartInfo("steam://rungameid/271590"){UseShellExecute=true});ShowMessage("Loading GTA",ModEnabledToggle.IsOn?(settings.Provider=="NVIDIA"?"The mod is enabled and GTA is assigned to the Windows high-performance GPU so the RTX DLSS runtime can start. F9 still toggles it.":"The mod is enabled and will activate automatically in gameplay. F9 still toggles it."):"GTA is loading with automatic mod activation turned off. You can enable it later in Game installation.");
    }catch(Exception ex){ShowError(ex);}}
    void Root_KeyDown(object sender,Microsoft.UI.Xaml.Input.KeyRoutedEventArgs e){if(e.OriginalSource is TextBox)return;if(e.Key==Windows.System.VirtualKey.PageDown||e.Key==Windows.System.VirtualKey.PageUp){ContentScroll.ChangeView(null,Math.Clamp(ContentScroll.VerticalOffset+(e.Key==Windows.System.VirtualKey.PageDown?1:-1)*ContentScroll.ViewportHeight*0.8,0,ContentScroll.ScrollableHeight),null);e.Handled=true;}}
    static string ReadBundledModBuild(){try{using var json=JsonDocument.Parse(File.ReadAllText(Path.Combine(AppContext.BaseDirectory,"Backend","manifest.json")));return json.RootElement.TryGetProperty("Build",out var build)?build.GetString()??"unknown":"unknown";}catch{return "unknown";}}
    static string ReadAppBuildIdentity(){
        var assembly=System.Reflection.Assembly.GetExecutingAssembly();
        return assembly.GetCustomAttributes(typeof(System.Reflection.AssemblyInformationalVersionAttribute),false).OfType<System.Reflection.AssemblyInformationalVersionAttribute>().FirstOrDefault()?.InformationalVersion
            ?? assembly.GetName().Version?.ToString()??"unknown build";
    }
    static string ReadAppReleaseVersion(){var identity=ReadAppBuildIdentity();var marker=identity.IndexOf('+');return marker<0?identity:identity[..marker];}
    FrameworkElement BuildAboutContent(){
        var version=ReadAppReleaseVersion();var historyPath=Path.Combine(AppContext.BaseDirectory,"CHANGELOG.md");var history=File.Exists(historyPath)?File.ReadAllText(historyPath):"Release history is unavailable in this installation.";
        var stack=new StackPanel{Spacing=12,MaxWidth=780};stack.Children.Add(new TextBlock{Text=$"Zink GTA Enhancement\nApp {version} · Mod {ReadBundledModBuild()}\n{ReadAppBuildIdentity()}\nFor GTA V Legacy",FontSize=18,FontWeight=Microsoft.UI.Text.FontWeights.SemiBold,TextWrapping=TextWrapping.Wrap});stack.Children.Add(new TextBlock{Text="The exact build identity is generated automatically every time the app is compiled. CHANGELOG.md supplies the readable feature and fix history included with that build.",Foreground=new Microsoft.UI.Xaml.Media.SolidColorBrush(Windows.UI.Color.FromArgb(255,170,180,198)),TextWrapping=TextWrapping.Wrap});stack.Children.Add(new TextBlock{Text=history,TextWrapping=TextWrapping.Wrap,IsTextSelectionEnabled=true,FontFamily=new Microsoft.UI.Xaml.Media.FontFamily("Segoe UI"),FontSize=13});return new ScrollViewer{Content=stack,MaxHeight=620,HorizontalScrollMode=ScrollMode.Disabled,VerticalScrollMode=ScrollMode.Enabled,VerticalScrollBarVisibility=ScrollBarVisibility.Auto};
    }
    async void Navigate_Click(object sender,RoutedEventArgs e) {

        if(sender is not Button button)return;string target=button.Tag?.ToString()??"Home";
        if(target is not "Hotkeys" and not "About")foreach(var item in NavItems.Children.OfType<Button>()){item.Background=new Microsoft.UI.Xaml.Media.SolidColorBrush(Windows.UI.Color.FromArgb((byte)(item==button?28:0),255,255,255));item.FontWeight=item==button?Microsoft.UI.Text.FontWeights.SemiBold:Microsoft.UI.Text.FontWeights.Normal;}

        if(target is "Hotkeys" or "About") {

            if(dialogOpen)return;dialogOpen=true;

            try {await new ContentDialog{XamlRoot=Root.XamlRoot,Title=target=="About"?$"About · App {ReadAppReleaseVersion()}":target,CloseButtonText="Close",Content=target=="Hotkeys"?"F9: toggle the upscaling / frame-generation mod.\nF11: mark an artifact and save image stages with DLSS 5.\nF8: run the diagnostic comparison (not needed for normal use).\n\nThese hotkeys are fixed in the current mod.":BuildAboutContent()}.ShowAsync();}catch(Exception ex){ShowError(ex);}finally{dialogOpen=false;}return;

        }

        PageTitle.Text=target=="Home"?"Overview":target;

        FrameworkElement? section=target switch{"Graphics"=>GraphicsSection,"Upscaling"=>UpscalingSection,"Frame Generation"=>FrameGenerationSection,"Advanced"=>AdvancedSection,"Profiles"=>ProfilesSection,_=>null};

        double offset=section==null?0:section.TransformToVisual((UIElement)ContentScroll.Content).TransformPoint(new Windows.Foundation.Point()).Y;

        ContentScroll.ChangeView(null,offset,null);

    }

    void UpdateStatus() {

        if(loading)return;

        try {

            string dir=GamePathBox.Text.Trim();bool found=File.Exists(Path.Combine(dir,"GTA5.exe"));bool running=ModBackend.IsGameRunning();bool bridge=File.Exists(Path.Combine(dir,"GTAXeSS-AppBridge.json"));

            GameStatus.Text=!found?"Game folder not selected":running?"GTA is running":"GTA detected · closed";

            ModStatus.Text=bridge?(provider=="Intel"&&IntelArcToggle.IsOn&&!arcAvailable?"Intel Arc is not currently available":"App bridge installed"):"Install / update mod to connect";SidebarStatus.Text=bridge?"Connected to installed mod":"Setup required";

            ApplyButton.IsEnabled=found&&!applying;InstallButton.IsEnabled=found&&!running&&!applying;

            GameBuildInfo.Text=found?"GTA "+(FileVersionInfo.GetVersionInfo(Path.Combine(dir,"GTA5.exe")).FileVersion??"version unavailable"):"GTA build unavailable";

            var telemetry=ModBackend.ReadTelemetry(dir);

            LiveFps.Text=telemetry.TryGetValue("SourceFPS",out var fps)?$"{fps:F1} FPS":"—";

            LiveFg.Text=telemetry.TryGetValue("XeFGFPS",out var fg)?$"{fg:F1} FPS":"—";

            LiveUsage.Text=telemetry.TryGetValue("CPU",out var cpu)&&telemetry.TryGetValue("GPU",out var gpu)?$"CPU {cpu:F0}% · GPU {gpu:F0}%":"Live readings appear while the mod is running.";

            var runtime=ModBackend.ReadRuntimeStatus(dir);
            RuntimeStateText.Text=runtime.State;ActiveProviderText.Text=runtime.Provider;ActiveUpscalerText.Text=runtime.Upscaler;UpscalerRuntimeText.Text=runtime.UpscalerRuntime;ActiveFgText.Text=runtime.FrameGeneration;FgRuntimeText.Text=runtime.FrameGenerationRuntime;NeuralRuntimeText.Text=runtime.Neural;RuntimeEvidenceText.Text=runtime.Detail;
            var badge=runtime.IsLive?Windows.UI.Color.FromArgb(110,42,126,70):running?Windows.UI.Color.FromArgb(100,138,101,24):Windows.UI.Color.FromArgb(80,60,70,88);RuntimeStateBadge.Background=new Microsoft.UI.Xaml.Media.SolidColorBrush(badge);RuntimeStateBadge.BorderBrush=new Microsoft.UI.Xaml.Media.SolidColorBrush(runtime.IsLive?Windows.UI.Color.FromArgb(170,105,214,139):Windows.UI.Color.FromArgb(90,255,255,255));

            if(File.Exists(GameSettingsBox.Text.Trim())) {

                var video=System.Xml.Linq.XDocument.Load(GameSettingsBox.Text.Trim()).Root?.Element("video");

                ResolutionStatus.Text="Saved GTA resolution: "+video?.Element("ScreenWidth")?.Attribute("value")?.Value+" × "+video?.Element("ScreenHeight")?.Attribute("value")?.Value;

            }else ResolutionStatus.Text="Select GTA settings.xml.";

        }catch(Exception ex){ModStatus.Text="Status unavailable: "+ex.Message;}

    }

    async void Provider_Click(object sender,RoutedEventArgs e){if(sender is Button b){CaptureGpuChoice();provider=b.Tag?.ToString()=="NVIDIA"?"NVIDIA":"Intel";QualityChoice.SelectedIndex=provider=="NVIDIA"?2:0;MultiplierChoice.SelectedIndex=provider=="Intel"?1:0;PopulateGpuChoices();UpdateProvider();await ScanProviderEnvironment(true);SettingsChanged(sender,e);}}
    async void RescanProvider_Click(object sender,RoutedEventArgs e)=>await ScanProviderEnvironment(true);
    async void RescanGpus_Click(object sender,RoutedEventArgs e){CaptureGpuChoice();PopulateGpuChoices();LoadHardware(provider);await ScanProviderEnvironment(true);}
    void Dlss5Rtx40_Toggled(object sender,RoutedEventArgs e){if(!loading)UpdateProvider();}

    void UpdateProvider(){

        if(ProviderStatus==null)return;

        bool nv=provider=="NVIDIA";DlssVersionChoice.IsEnabled=nv&&nvidiaDlss;Dlss5Rtx40Toggle.IsEnabled=nv&&nvidiaRtx40;IntelDlss5Toggle.Visibility=nv?Visibility.Collapsed:Visibility.Visible;IntelDlss5Help.Visibility=!nv&&IntelDlss5Toggle.IsOn?Visibility.Visible:Visibility.Collapsed;((ComboBoxItem)DlssVersionChoice.Items[2]).IsEnabled=nv&&(nvidiaMaxMultiplier>=6||(nvidiaRtx40&&Dlss5Rtx40Toggle.IsOn));ProviderStatus.Text="Selected: "+(nv?"NVIDIA RTX":"Intel XeSS");

        IntelProviderButton.BorderThickness=new Thickness(nv?1:3);NvidiaProviderButton.BorderThickness=new Thickness(nv?3:1);

        UpscalingTitle.Text=nv?"NVIDIA DLSS":"Intel XeSS";QualityChoice.Header=nv?"DLSS quality":"XeSS quality";

        foreach(ComboBoxItem item in QualityChoice.Items){bool intelOnly=item.Tag?.ToString() is "ULTRA_QUALITY" or "ULTRA_QUALITY_PLUS";item.Visibility=nv&&intelOnly?Visibility.Collapsed:Visibility.Visible;item.IsEnabled=!nv||(!intelOnly&&(nvidiaDlss||item.Tag?.ToString()=="NATIVE"));}IntelArcToggle.IsEnabled=!nv;

        if(!nv&&IntelDlss5Toggle.IsOn)FrameGenerationToggle.IsOn=false;FrameGenerationToggle.IsEnabled=nv||!IntelDlss5Toggle.IsOn;FrameGenerationToggle.Header=!nv&&IntelDlss5Toggle.IsOn?"Frame generation off · Intel DLSS 5 test":nv?"Allow NVIDIA frame generation":"Allow Intel XeSS FG";

        MultiplierChoice.IsEnabled=nv&&nvidiaMaxMultiplier>=2;

        foreach(ComboBoxItem item in MultiplierChoice.Items){int m=int.Parse(item.Tag.ToString()!);item.IsEnabled=nv?m<=nvidiaMaxMultiplier:m==4;}

    }

    void ValidateNvidia(ModSettings s){if(s.Provider!="NVIDIA")return;if(nvidiaProbeError!=null)throw new InvalidOperationException("NVIDIA capability check failed: "+nvidiaProbeError+" Close and reopen the app to retry.");if(s.EffectiveDlssVersion=="5"&&nvidiaMaxMultiplier<6&&!(nvidiaRtx40&&s.NvidiaDlss5Rtx40Experimental))throw new InvalidOperationException("DLSS 5 requires RTX 50, or the explicit unsupported RTX 40 experimental option.");if(s.FrameGeneration&&s.NvidiaMultiplier>nvidiaMaxMultiplier)throw new InvalidOperationException("This NVIDIA GPU / driver does not support the selected frame-generation multiplier.");if(s.UpscalingQuality!="NATIVE"&&!nvidiaDlss)throw new InvalidOperationException("DLSS is not supported by the current NVIDIA GPU / driver.");}

    async System.Threading.Tasks.Task ProbeNvidia(){

        try{

            var folder=Path.Combine(AppContext.BaseDirectory,"Backend");var exe=Path.Combine(folder,"NvidiaProbe.exe");

            var info=new ProcessStartInfo(exe){UseShellExecute=false,CreateNoWindow=true,RedirectStandardOutput=true,RedirectStandardError=true};info.ArgumentList.Add(folder);

            using var process=Process.Start(info)??throw new InvalidOperationException("Cannot start NVIDIA capability check.");

            var output=process.StandardOutput.ReadToEndAsync();var error=process.StandardError.ReadToEndAsync();

            try{using var timeout=new System.Threading.CancellationTokenSource(30000);await process.WaitForExitAsync(timeout.Token);}catch(System.OperationCanceledException){process.Kill();throw new InvalidOperationException("NVIDIA capability check timed out.");}

            var line=(await output).Split('\n').FirstOrDefault(l=>l.StartsWith("{\"dlssSupported\""))??throw new InvalidOperationException("No supported NVIDIA capability report.");

            if(process.ExitCode!=0)throw new InvalidOperationException((await error).Trim());
            using var json=JsonDocument.Parse(line);var root=json.RootElement;nvidiaDlss=root.GetProperty("dlssSupported").GetBoolean();nvidiaMaxMultiplier=root.GetProperty("fgSupported").GetBoolean()?root.GetProperty("maxMultiplier").GetInt32():0;

            NvidiaCapabilityText.Text=$"NVIDIA driver check: DLSS {(nvidiaDlss?"supported":"unavailable")} · FG up to {nvidiaMaxMultiplier}×. GTA must render on the NVIDIA GPU.";

        }catch(Exception ex){nvidiaProbeError=ex.Message;nvidiaMaxMultiplier=0;nvidiaDlss=false;NvidiaCapabilityText.Text="NVIDIA unavailable: "+ex.Message;App.Log("NVIDIA capability: "+ex.Message);}

    }

    sealed class GpuChoiceItem { public int AdapterIndex {get;init;} public DisplayDevice Device {get;init;} public ulong DedicatedVram {get;init;} public string Label {get;init;}=""; public override string ToString()=>Label; }
    System.Collections.Generic.List<GpuChoiceItem> EnumerateGpus(){
        var result=new System.Collections.Generic.List<GpuChoiceItem>();
        try{var probe=Path.Combine(AppContext.BaseDirectory,"Backend","GpuProbe.exe");if(File.Exists(probe)){using var process=Process.Start(new ProcessStartInfo(probe){UseShellExecute=false,CreateNoWindow=true,RedirectStandardOutput=true})!;var output=process.StandardOutput.ReadToEnd();process.WaitForExit(5000);foreach(var line in output.Split('\n',StringSplitOptions.RemoveEmptyEntries)){var parts=line.Trim().Split('|',3);if(parts.Length!=3||!int.TryParse(parts[0],out var index)||!ulong.TryParse(parts[1],out var vram))continue;var name=parts[2].Trim();result.Add(new GpuChoiceItem{AdapterIndex=index,DedicatedVram=vram,Device=new DisplayDevice{DeviceString=name},Label=$"Adapter {index} · {name}"});}if(result.Count>0)return result;}}
        catch(Exception ex){App.Log("DXGI GPU scan fallback: "+ex.Message);}
        var unique=new System.Collections.Generic.HashSet<string>(StringComparer.OrdinalIgnoreCase);for(uint i=0;i<16;i++){var display=new DisplayDevice{cb=Marshal.SizeOf<DisplayDevice>()};if(!EnumDisplayDevices(null,i,ref display,0))break;if(string.IsNullOrWhiteSpace(display.DeviceString)||display.DeviceString.Contains("Basic Display",StringComparison.OrdinalIgnoreCase)||!unique.Add(string.IsNullOrWhiteSpace(display.DeviceID)?display.DeviceString:display.DeviceID))continue;result.Add(new GpuChoiceItem{AdapterIndex=result.Count,Device=display,DedicatedVram=ReadGpuVram(display),Label=$"Adapter {result.Count} · {display.DeviceString}"});}return result;
    }
    static bool MatchesProvider(GpuChoiceItem gpu,string selected)=>selected=="NVIDIA"?gpu.Device.DeviceString.Contains("NVIDIA",StringComparison.OrdinalIgnoreCase):gpu.Device.DeviceString.Contains("Intel",StringComparison.OrdinalIgnoreCase);
    void PopulateGpuChoices(){if(GpuChoice==null)return;var wasLoading=loading;loading=true;var wantedIndex=provider=="NVIDIA"?nvidiaAdapterIndex:intelAdapterIndex;var wantedName=provider=="NVIDIA"?nvidiaAdapterName:intelAdapterName;GpuChoice.Items.Clear();GpuChoice.Items.Add(new GpuChoiceItem{AdapterIndex=-1,Label="Automatic (recommended)"});foreach(var gpu in EnumerateGpus())GpuChoice.Items.Add(gpu);var match=GpuChoice.Items.OfType<GpuChoiceItem>().FirstOrDefault(g=>g.AdapterIndex==wantedIndex&&wantedIndex>=0)??GpuChoice.Items.OfType<GpuChoiceItem>().FirstOrDefault(g=>!string.IsNullOrWhiteSpace(wantedName)&&g.Device.DeviceString.Equals(wantedName,StringComparison.OrdinalIgnoreCase));GpuChoice.SelectedItem=match??GpuChoice.Items[0];CaptureGpuChoice();loading=wasLoading;}
    void CaptureGpuChoice(){if(GpuChoice?.SelectedItem is not GpuChoiceItem item)return;if(provider=="NVIDIA"){nvidiaAdapterIndex=item.AdapterIndex;nvidiaAdapterName=item.AdapterIndex>=0?item.Device.DeviceString:"";}else{intelAdapterIndex=item.AdapterIndex;intelAdapterName=item.AdapterIndex>=0?item.Device.DeviceString:"";}}
    void GpuChoice_SelectionChanged(object sender,SelectionChangedEventArgs e){CaptureGpuChoice();if(!loading)SettingsChanged(sender,e);}

    static ulong ReadGpuVram(DisplayDevice gpu){if(string.IsNullOrWhiteSpace(gpu.DeviceKey))return 0;var registryPath=gpu.DeviceKey.Replace(@"\Registry\Machine\", "",StringComparison.OrdinalIgnoreCase);using var gpuKey=Registry.LocalMachine.OpenSubKey(registryPath);var raw=gpuKey?.GetValue("HardwareInformation.qwMemorySize")??gpuKey?.GetValue("HardwareInformation.MemorySize");try{if(raw is byte[] bytes&&bytes.Length>=8)return BitConverter.ToUInt64(bytes);if(raw!=null)return Convert.ToUInt64(raw);}catch{}return 0;}

    async System.Threading.Tasks.Task ScanProviderEnvironment(bool announce){
        if(providerScanning||ProviderScanText==null)return;providerScanning=true;autoApplyTimer.Stop();var scanRevision=++progressRevision;var selected=provider;var scanningText=$"Scanning {selected} GPU, display, GTA installation, settings and runtimes…";ProviderScanText.Text=scanningText;ApplyProgressPanel.Visibility=Visibility.Visible;ApplyProgressRing.IsActive=true;ApplyProgressText.Text=scanningText;
        try{
            if(!File.Exists(Path.Combine(GamePathBox.Text.Trim(),"GTA5.exe"))&&ModBackend.DiscoverGameDirectory() is string game)GamePathBox.Text=game;
            if(!File.Exists(GameSettingsBox.Text.Trim())&&ModBackend.DiscoverSettingsPath() is string settingsPath)GameSettingsBox.Text=settingsPath;
            var gpus=EnumerateGpus();var chosen=GpuChoice.SelectedItem as GpuChoiceItem;var gpu=chosen?.AdapterIndex>=0?chosen:gpus.FirstOrDefault(g=>MatchesProvider(g,selected));
            var issues=new System.Collections.Generic.List<string>();var facts=new System.Collections.Generic.List<string>();
            if(gpu==null||string.IsNullOrWhiteSpace(gpu.Device.DeviceString))issues.Add(selected+" GPU not detected");else{var vram=gpu.DedicatedVram>0?gpu.DedicatedVram:ReadGpuVram(gpu.Device);facts.Add($"GPU: {gpu.Device.DeviceString} · adapter {gpu.AdapterIndex}"+(vram>0?$" · {vram/1073741824.0:F1} GB VRAM":"")+(chosen?.AdapterIndex>=0?" · manually selected":" · automatic"));}
            var mode=new DevMode{dmSize=(ushort)Marshal.SizeOf<DevMode>()};if(EnumDisplaySettings(null,-1,ref mode)){facts.Add($"Desktop: {mode.dmPelsWidth} × {mode.dmPelsHeight} · {mode.dmDisplayFrequency} Hz");if(mode.dmPelsWidth>=3840&&ResolutionChoice.SelectedIndex<2)ResolutionChoice.SelectedIndex=2;else if(mode.dmPelsWidth>=2560&&ResolutionChoice.SelectedIndex<1)ResolutionChoice.SelectedIndex=1;}
            var gameDir=GamePathBox.Text.Trim();var settingsFile=GameSettingsBox.Text.Trim();
            if(!File.Exists(Path.Combine(gameDir,"GTA5.exe")))issues.Add("GTA V Legacy folder not found");else{var version=FileVersionInfo.GetVersionInfo(Path.Combine(gameDir,"GTA5.exe")).FileVersion??"unknown";facts.Add("GTA: found · "+version);}
            if(!File.Exists(settingsFile))issues.Add("GTA settings.xml not found");else{try{var video=System.Xml.Linq.XDocument.Load(settingsFile).Root?.Element("video");var width=video?.Element("ScreenWidth")?.Attribute("value")?.Value;var height=video?.Element("ScreenHeight")?.Attribute("value")?.Value;var windowed=video?.Element("Windowed")?.Attribute("value")?.Value;var adapter=video?.Element("AdapterIndex")?.Attribute("value")?.Value;var description=video?.Element("VideoCardDescription")?.Value;facts.Add($"GTA display: {width} × {height} · mode {(windowed=="0"?"fullscreen":windowed=="2"?"borderless":"windowed")} · adapter {adapter} {description}".Trim());}catch(Exception ex){issues.Add("GTA settings invalid: "+ex.Message);}}
            var payload=Path.Combine(AppContext.BaseDirectory,"Backend");var required=selected=="NVIDIA"?new[]{"NVIDIA RTX.dll","sl.interposer.dll","sl.dlss.dll","sl.dlss_g.dll","nvngx_dlss.dll","nvngx_dlssg.dll","NvidiaProbe.exe"}:new[]{"Intel ARC.dll","libxess.dll","libxess_dx11.dll","libxess_fg.dll","libxell.dll"};var missing=required.Where(name=>!File.Exists(Path.Combine(payload,name))).ToArray();if(missing.Length>0)issues.Add("Missing app runtime: "+string.Join(", ",missing));else facts.Add($"{selected} runtime: {required.Length} required files ready");
            if(selected=="NVIDIA"){await EnsureNvidiaProbe();if(nvidiaProbeError!=null)issues.Add("NVIDIA driver probe: "+nvidiaProbeError);else facts.Add($"Driver capability: DLSS {(nvidiaDlss?"ready":"unavailable")} · FG up to {nvidiaMaxMultiplier}×");}else{facts.Add(arcAvailable?"Intel Arc policy: ready":"Intel Arc policy: GPU not confirmed");if(IntelArcToggle.IsOn&&!arcAvailable)issues.Add("Require Intel Arc is enabled but no Intel Arc GPU was detected");}
            if(issues.Count==0&&File.Exists(settingsFile)&&File.Exists(Path.Combine(gameDir,"GTA5.exe"))&&!ModBackend.IsGameRunning()){var candidate=GetControls();await System.Threading.Tasks.Task.Run(()=>backend.ResolveInput(candidate,payload));renderWidth=candidate.InputWidth;renderHeight=candidate.InputHeight;facts.Add($"SDK render size: {renderWidth} × {renderHeight} → {candidate.Dimensions.Width} × {candidate.Dimensions.Height}");}
            ProviderScanText.Text=string.Join("\n",facts.Select(x=>"✓ "+x).Concat(issues.Select(x=>"⚠ "+x)));LoadHardware(selected);UpdateProvider();
            if(announce)ShowMessage($"{selected} scan complete",issues.Count==0?"GPU, desktop, GTA, settings, runtime files and SDK render size are ready.":string.Join(" · ",issues),issues.Count==0?InfoBarSeverity.Success:InfoBarSeverity.Warning);
            providerScanning=false;_=FinishProgressIndicator(scanRevision,issues.Count==0?$"✓ {selected} scan complete":$"⚠ {selected} scan completed with warnings");
        }catch(Exception ex){ProviderScanText.Text="Scan failed: "+ex.Message;if(announce)ShowError(ex);providerScanning=false;_=FinishProgressIndicator(scanRevision,$"{selected} scan failed");}
    }

    void LoadHardware(string? preferredProvider=null) {

        try {

            var gpus=EnumerateGpus();

            arcAvailable=gpus.Any(g=>g.Device.DeviceString.Contains("Intel",StringComparison.OrdinalIgnoreCase)&&g.Device.DeviceString.Contains("Arc",StringComparison.OrdinalIgnoreCase));
            nvidiaRtx40=gpus.Any(g=>g.Device.DeviceString.Contains("NVIDIA",StringComparison.OrdinalIgnoreCase)&&g.Device.DeviceString.Contains("RTX 40",StringComparison.OrdinalIgnoreCase));

            var mainGpu=preferredProvider=="Intel"?gpus.FirstOrDefault(g=>g.Device.DeviceString.Contains("Intel",StringComparison.OrdinalIgnoreCase)&&g.Device.DeviceString.Contains("Arc",StringComparison.OrdinalIgnoreCase)):preferredProvider=="NVIDIA"?gpus.FirstOrDefault(g=>g.Device.DeviceString.Contains("NVIDIA",StringComparison.OrdinalIgnoreCase)):gpus.OrderByDescending(g=>g.Device.DeviceString.Contains("NVIDIA",StringComparison.OrdinalIgnoreCase)&&g.Device.DeviceString.Contains("RTX",StringComparison.OrdinalIgnoreCase)).ThenByDescending(g=>g.Device.DeviceString.Contains("Intel",StringComparison.OrdinalIgnoreCase)&&g.Device.DeviceString.Contains("Arc",StringComparison.OrdinalIgnoreCase)).ThenByDescending(g=>(g.Device.StateFlags&4)!=0).FirstOrDefault();
            ulong vram=mainGpu==null?0:mainGpu.DedicatedVram>0?mainGpu.DedicatedVram:ReadGpuVram(mainGpu.Device);
            GpuInfo.Text=mainGpu==null||string.IsNullOrWhiteSpace(mainGpu.Device.DeviceString)?"GPU information unavailable":vram>0?$"{mainGpu.Device.DeviceString}\n{vram/1073741824.0:F1} GB dedicated VRAM":mainGpu.Device.DeviceString;

            using var key=Registry.LocalMachine.OpenSubKey(@"HARDWARE\DESCRIPTION\System\CentralProcessor\0");CpuInfo.Text=key?.GetValue("ProcessorNameString")?.ToString()?.Trim()??"CPU information unavailable";

            var memory=new MemoryStatus{Length=(uint)Marshal.SizeOf<MemoryStatus>()};if(GlobalMemoryStatusEx(ref memory)){var usable=memory.TotalPhysical/1073741824.0;var installed=Math.Ceiling(usable);RamInfo.Text=$"{installed:F0} GB RAM installed · {usable:F1} GB usable";}else RamInfo.Text="RAM information unavailable";

            var mode=new DevMode{dmSize=(ushort)Marshal.SizeOf<DevMode>()};DisplayInfo.Text=EnumDisplaySettings(null,-1,ref mode)?$"Desktop {mode.dmPelsWidth} × {mode.dmPelsHeight} · {mode.dmDisplayFrequency} Hz":$"Desktop {GetSystemMetrics(0)} × {GetSystemMetrics(1)}";
            App.Log($"Hardware panel: GPU={GpuInfo.Text.Replace('\n',' ')}; RAM={RamInfo.Text}; DISPLAY={DisplayInfo.Text}");

        }catch(Exception ex){App.Log("Hardware: "+ex.Message);GpuInfo.Text="Hardware information unavailable";}

    }

    [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)]struct DisplayDevice{public int cb;[MarshalAs(UnmanagedType.ByValTStr,SizeConst=32)]public string DeviceName;[MarshalAs(UnmanagedType.ByValTStr,SizeConst=128)]public string DeviceString;public uint StateFlags;[MarshalAs(UnmanagedType.ByValTStr,SizeConst=128)]public string DeviceID;[MarshalAs(UnmanagedType.ByValTStr,SizeConst=128)]public string DeviceKey;}

    [StructLayout(LayoutKind.Sequential)]struct MemoryStatus{public uint Length,MemoryLoad;public ulong TotalPhysical,AvailablePhysical,TotalPageFile,AvailablePageFile,TotalVirtual,AvailableVirtual,AvailableExtendedVirtual;}

    [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)]struct DevMode{[MarshalAs(UnmanagedType.ByValTStr,SizeConst=32)]public string dmDeviceName;public ushort dmSpecVersion,dmDriverVersion,dmSize,dmDriverExtra;public uint dmFields;public int dmPositionX,dmPositionY;public uint dmDisplayOrientation,dmDisplayFixedOutput;public short dmColor,dmDuplex,dmYResolution,dmTTOption,dmCollate;[MarshalAs(UnmanagedType.ByValTStr,SizeConst=32)]public string dmFormName;public ushort dmLogPixels;public uint dmBitsPerPel,dmPelsWidth,dmPelsHeight,dmDisplayFlags,dmDisplayFrequency,dmICMMethod,dmICMIntent,dmMediaType,dmDitherType,dmReserved1,dmReserved2,dmPanningWidth,dmPanningHeight;}

    [DllImport("user32.dll",CharSet=CharSet.Unicode)]static extern bool EnumDisplayDevices(string? name,uint index,ref DisplayDevice device,uint flags);

    [DllImport("kernel32.dll")]static extern bool GlobalMemoryStatusEx(ref MemoryStatus memory);

    [DllImport("user32.dll")]static extern int GetSystemMetrics(int index);

    [DllImport("user32.dll",CharSet=CharSet.Unicode)]static extern bool EnumDisplaySettings(string? deviceName,int modeNum,ref DevMode mode);

}







