using System;

using System.Collections.Generic;

using System.Diagnostics;

using System.Globalization;

using System.IO;

using System.Linq;

using System.IO.Compression;

using System.Net.Http;

using Microsoft.Win32;

using System.Security.Cryptography;

using System.Text;

using System.Text.Json;

using System.Text.RegularExpressions;

using System.Xml.Linq;



namespace ZinkGTAUI;



public sealed class ModSettings

{

    public string GameDirectory { get; set; } = @"G:\SteamLibrary\steamapps\common\Grand Theft Auto V";

    public string GameSettingsPath { get; set; } = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments), "Rockstar Games", "GTA V", "settings.xml");

    public string Resolution { get; set; } = "1080P";

    public string Provider { get; set; } = "NVIDIA";

    public int NvidiaAdapterIndex { get; set; } = -1;
    public string NvidiaAdapterName { get; set; } = "";
    public int IntelAdapterIndex { get; set; } = -1;
    public string IntelAdapterName { get; set; } = "";
    public int SelectedAdapterIndex => Provider=="NVIDIA"?NvidiaAdapterIndex:IntelAdapterIndex;
    public string SelectedAdapterName => Provider=="NVIDIA"?NvidiaAdapterName:IntelAdapterName;

    public int NvidiaMultiplier { get; set; } = 2;

    public bool NvidiaDlaa { get; set; }
    public bool NvidiaDlss5 { get; set; }
    public bool NvidiaDlss5Rtx40Experimental { get; set; }
    public bool IntelDlss5DirectMl { get; set; }
    public string NvidiaDlssVersion { get; set; } = "";
    public string EffectiveDlssVersion => string.IsNullOrEmpty(NvidiaDlssVersion)?(NvidiaDlss5?"5":"4.5"):NvidiaDlssVersion;
    public string UpscalingQuality { get; set; } = "QUALITY";
    public int InputWidth { get; set; } = 1280;
    public int InputHeight { get; set; } = 720;
    public bool AutoStartMod { get; set; } = true;

    public bool FrameGeneration { get; set; }

    public bool IntelArcOnly { get; set; } = true;

    public bool Diagnostics { get; set; } = true;

    public int SourceFPS { get; set; }

    public bool Hud { get; set; } = true;

    public bool ShowFPS { get; set; } = true;

    public bool ShowFrameTimes { get; set; }

    public bool ShowUsage { get; set; } = true;

    public bool ShowStatus { get; set; } = true;

    public bool AutoApply { get; set; } = true;

    public bool VisualEnhancement { get; set; }

    public string VisualModDownloadUrl { get; set; } = "";

    public (int Width, int Height) Dimensions => Resolution.ToUpperInvariant() switch

    {

        "1080P" => (1920,1080), "1440P" => (2560,1440), "4K" => (3840,2160),

        _ => throw new InvalidOperationException("Choose 1080p, 1440p or 4K.")

    };

    public void Validate()

    {

        _ = Dimensions;
        if(!new[]{"4","4.5","5"}.Contains(EffectiveDlssVersion))throw new InvalidOperationException("Choose DLSS 4, DLSS 4.5 or DLSS 5.");
        NvidiaDlss5=EffectiveDlssVersion=="5";
        if(Provider=="NVIDIA"&&NvidiaDlss5&&UpscalingQuality=="NATIVE")UpscalingQuality="AA";
        if(!new[]{"NATIVE","AA","QUALITY","BALANCED","PERFORMANCE","ULTRA_PERFORMANCE","ULTRA_QUALITY","ULTRA_QUALITY_PLUS"}.Contains(UpscalingQuality))throw new InvalidOperationException("Choose a supported upscaling quality.");

        if(Provider is not ("Intel" or "NVIDIA"))throw new InvalidOperationException("Choose Intel or NVIDIA.");
        if(NvidiaAdapterIndex < -1 || IntelAdapterIndex < -1)throw new InvalidOperationException("Choose a detected graphics card or Automatic.");
        if(Provider=="Intel"&&IntelDlss5DirectMl)FrameGeneration=false;

        if(Provider=="NVIDIA" && UpscalingQuality is "ULTRA_QUALITY" or "ULTRA_QUALITY_PLUS")throw new InvalidOperationException("Choose a supported DLSS quality.");
        if(Provider=="NVIDIA" && NvidiaMultiplier is not (2 or 4 or 6))throw new InvalidOperationException("Choose NVIDIA x2, x4 or x6.");

        if (SourceFPS != 0 && SourceFPS is not (30 or 60 or 90 or 120)) throw new InvalidOperationException("Unsupported source FPS limit.");

        if (!File.Exists(Path.Combine(GameDirectory, "GTA5.exe"))) throw new InvalidOperationException("Select the GTA V Legacy folder containing GTA5.exe.");

        if (!File.Exists(GameSettingsPath)) throw new InvalidOperationException("Select GTA V Legacy's existing settings.xml. Run the game once if it has not been created.");

        var video=XDocument.Load(GameSettingsPath).Root?.Element("video");

        if(video?.Element("ScreenWidth")?.Attribute("value")==null||video.Element("ScreenHeight")?.Attribute("value")==null)throw new InvalidOperationException("Select the GTA V Legacy settings.xml with ScreenWidth and ScreenHeight values.");

    }

}



// Preserve comments, unknown sections/keys, and every occurrence of a known key.

public sealed class IniDocument

{

    readonly List<string> lines;

    public IniDocument(string content) { lines = content.Replace("\r\n", "\n").Split('\n').ToList(); }

    public string Get(string section, string key, string fallback)

    {

        string current="";

        foreach(var line in lines) {

            var t=line.Trim(); if(t.StartsWith('[') && t.EndsWith(']')) {current=t[1..^1];continue;}

            int eq=t.IndexOf('='); if(eq<0 || t.StartsWith(';') || t.StartsWith('#'))continue;

            if(current.Equals(section,StringComparison.OrdinalIgnoreCase)&&t[..eq].Trim().Equals(key,StringComparison.OrdinalIgnoreCase))return t[(eq+1)..].Trim();

        }

        return fallback;

    }

    public void Set(string section,string key,string value)

    {

        string current=""; int sectionEnd=-1; bool found=false;

        for(int i=0;i<lines.Count;i++) {

            var t=lines[i].Trim(); if(t.StartsWith('[')&&t.EndsWith(']')) {

                if(current.Equals(section,StringComparison.OrdinalIgnoreCase))sectionEnd=i;

                current=t[1..^1]; continue;

            }

            int eq=t.IndexOf('='); if(eq<0||t.StartsWith(';')||t.StartsWith('#'))continue;

            if(current.Equals(section,StringComparison.OrdinalIgnoreCase)&&t[..eq].Trim().Equals(key,StringComparison.OrdinalIgnoreCase)){lines[i]=$"{key}={value}";found=true;}

        }

        if(found)return;

        if(current.Equals(section,StringComparison.OrdinalIgnoreCase))sectionEnd=lines.Count;

        if(sectionEnd<0){lines.Add("");lines.Add($"[{section}]");lines.Add($"{key}={value}");}

        else lines.Insert(sectionEnd,$"{key}={value}");

    }

    public override string ToString()=>string.Join("\r\n",lines);

}



public sealed class ModBackend

{

    public string DataRoot { get; }

    readonly Func<bool> gameRunning;

    public ModBackend(string? dataRoot=null,Func<bool>? running=null) {

        DataRoot=dataRoot??Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"ZinkGTAUI");

        gameRunning=running??IsGameRunning; Directory.CreateDirectory(DataRoot);

    }

    public static bool IsGameRunning() {

        foreach(var name in new[]{"GTA5","GTA5_Enhanced"}) {

            var processes=Process.GetProcessesByName(name);try{foreach(var process in processes){try{if(!process.HasExited)return true;}catch(System.ComponentModel.Win32Exception){return true;}catch(InvalidOperationException){}}}finally{foreach(var process in processes)process.Dispose();}

        }

        return false;

    }

    public ModSettings LoadPreferences() {

        var path=Path.Combine(DataRoot,"preferences.json");

        return File.Exists(path)?JsonSerializer.Deserialize<ModSettings>(File.ReadAllText(path))??new():new();

    }

    public static string? DiscoverGameDirectory() {
        var candidates=new List<string>();
        void Add(string? p){if(!string.IsNullOrWhiteSpace(p)&&!candidates.Contains(p,StringComparer.OrdinalIgnoreCase))candidates.Add(p);}
        foreach(var keyPath in new[]{@"SOFTWARE\WOW6432Node\Valve\Steam",@"SOFTWARE\Valve\Steam"})using(var key=Registry.LocalMachine.OpenSubKey(keyPath)){var steam=key?.GetValue("InstallPath")?.ToString();if(steam!=null){Add(Path.Combine(steam,"steamapps","common","Grand Theft Auto V"));var libraries=Path.Combine(steam,"steamapps","libraryfolders.vdf");if(File.Exists(libraries))foreach(Match m in Regex.Matches(File.ReadAllText(libraries),"\\\"path\\\"\\s*\\\"([^\\\"]+)\\\""))Add(Path.Combine(m.Groups[1].Value.Replace("\\\\","\\"),"steamapps","common","Grand Theft Auto V"));}}
        using(var key=Registry.LocalMachine.OpenSubKey(@"SOFTWARE\WOW6432Node\Rockstar Games\Grand Theft Auto V"))Add(key?.GetValue("InstallFolder")?.ToString());
        var epic=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData),"Epic","EpicGamesLauncher","Data","Manifests");if(Directory.Exists(epic))foreach(var file in Directory.EnumerateFiles(epic,"*.item")){try{using var json=JsonDocument.Parse(File.ReadAllText(file));var root=json.RootElement;if(root.TryGetProperty("DisplayName",out var title)&&title.GetString()?.Contains("Grand Theft Auto V",StringComparison.OrdinalIgnoreCase)==true&&root.TryGetProperty("InstallLocation",out var location))Add(location.GetString());}catch{}}
        foreach(var drive in DriveInfo.GetDrives().Where(d=>d.IsReady&&d.DriveType==DriveType.Fixed)){Add(Path.Combine(drive.RootDirectory.FullName,"SteamLibrary","steamapps","common","Grand Theft Auto V"));Add(Path.Combine(drive.RootDirectory.FullName,"Program Files (x86)","Steam","steamapps","common","Grand Theft Auto V"));Add(Path.Combine(drive.RootDirectory.FullName,"Games","Grand Theft Auto V"));}
        return candidates.FirstOrDefault(p=>File.Exists(Path.Combine(p,"GTA5.exe")));
    }

    public static string? DiscoverSettingsPath() {
        var docs=Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments);var candidates=new[]{Path.Combine(docs,"Rockstar Games","GTA V","settings.xml"),Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile),"OneDrive","Documents","Rockstar Games","GTA V","settings.xml")};return candidates.FirstOrDefault(File.Exists);
    }

    public static IReadOnlyList<string> EnsureNvidiaHighPerformanceGpu(string gameDirectory) {
        if(!OperatingSystem.IsWindows())return Array.Empty<string>();
        var configured=new List<string>();
        using var key=Registry.CurrentUser.CreateSubKey(@"Software\Microsoft\DirectX\UserGpuPreferences",true)
            ??throw new InvalidOperationException("Windows GPU preferences could not be opened.");
        foreach(var name in new[]{"GTA5.exe","GTA5_Enhanced.exe","PlayGTAV.exe","GTAVLauncher.exe"}) {
            var path=Path.GetFullPath(Path.Combine(gameDirectory,name));
            if(!File.Exists(path))continue;
            key.SetValue(path,"GpuPreference=2;",RegistryValueKind.String);
            configured.Add(path);
        }
        if(configured.Count==0)throw new InvalidOperationException("No GTA executable was found for the NVIDIA GPU preference.");
        return configured;
    }

    public static void EnsureGtaLaunchOptions(string gameDirectory,ModSettings settings) {
        var path=Path.Combine(gameDirectory,"commandline.txt");
        var content=File.Exists(path)?File.ReadAllText(path):"";
        content=Regex.Replace(content,@"(?im)^\s*-(?:windowed|borderless|fullscreen)\s*$","");
        content=Regex.Replace(content,@"(?im)^\s*-adapter(?:\s+|=)\d+\s*$","");
        content=Regex.Replace(content,@"(?im)^\s*-(?:width|height)(?:\s+|=)\d+\s*$","");
        content=content.Trim();
        var output=settings.Dimensions;var scaled=settings.InputWidth>0&&settings.InputHeight>0&&(settings.InputWidth!=output.Width||settings.InputHeight!=output.Height);
        var width=scaled?settings.InputWidth:output.Width;var height=scaled?settings.InputHeight:output.Height;
        // A scaled source must remain windowed at the SDK input size. The native
        // mod surface covers the monitor after activation; forcing GTA fullscreen
        // makes it substitute the desktop mode and prevents source discovery.
        // GTA's DXGI ordering on hybrid Intel/NVIDIA systems exposes the discrete
        // RTX adapter as index 0.  Index 1 selects the Intel device and makes the
        // NVIDIA D3D12 bridge a cross-adapter pipeline, which cannot share GTA's
        // native D3D11 colour textures.
        var adapter=settings.SelectedAdapterIndex>=0?settings.SelectedAdapterIndex:settings.Provider=="NVIDIA"?0:-1;
        var required=$"{(scaled?"-windowed":"-fullscreen")}\r\n-width {width}\r\n-height {height}"+(adapter>=0?$"\r\n-adapter {adapter}":"");
        AtomicWrite(path,Encoding.UTF8.GetBytes((content.Length==0?"":content+"\r\n")+required+"\r\n"));
    }

    public void SavePreferences(ModSettings value)=>AtomicWrite(Path.Combine(DataRoot,"preferences.json"),Encoding.UTF8.GetBytes(JsonSerializer.Serialize(value,new JsonSerializerOptions{WriteIndented=true})));

    public ModSettings ReadInstalled(ModSettings value) {

        var path=Path.Combine(value.GameDirectory,"GTAXeSS.ini"); if(!File.Exists(path))return value;

        var ini=new IniDocument(File.ReadAllText(path));

        string resolution=ini.Get("XeSS","OutputResolution","1080P").ToUpperInvariant();

        value.Resolution=resolution is "1440P" or "2560X1440"?"1440P":resolution is "4K" or "2160P" or "3840X2160"?"4K":"1080P";

        bool Read(string section,string key,bool d)=>ini.Get(section,key,d?"1":"0")=="1";

        value.Provider=ini.Get("Graphics","Provider","Intel")=="NVIDIA"?"NVIDIA":"Intel";
        value.NvidiaAdapterIndex=int.TryParse(ini.Get("Graphics","NvidiaAdapterIndex","-1"),out var nvidiaAdapter)?nvidiaAdapter:-1;
        value.NvidiaAdapterName=ini.Get("Graphics","NvidiaAdapterName","");
        value.IntelAdapterIndex=int.TryParse(ini.Get("Graphics","IntelAdapterIndex","-1"),out var intelAdapter)?intelAdapter:-1;
        value.IntelAdapterName=ini.Get("Graphics","IntelAdapterName","");

        value.NvidiaMultiplier=int.TryParse(ini.Get("NVIDIA","Multiplier","2"),out var multiplier)&&new[]{2,4,6}.Contains(multiplier)?multiplier:2;

        value.NvidiaDlaa=Read("NVIDIA","DLAA",false);
        value.NvidiaDlss5=Read("NVIDIA","DLSS5",false);value.NvidiaDlss5Rtx40Experimental=Read("NVIDIA","DLSS5RTX40Experimental",false);value.NvidiaDlssVersion=ini.Get("NVIDIA","DLSSVersion",value.NvidiaDlss5?"5":"4.5");value.UpscalingQuality=ini.Get("Upscaling","Quality",value.NvidiaDlaa?"AA":"NATIVE");value.AutoStartMod=Read("App","AutoStart",true);value.InputWidth=int.TryParse(ini.Get("Upscaling","InputWidth","0"),out var iw)?iw:0;value.InputHeight=int.TryParse(ini.Get("Upscaling","InputHeight","0"),out var ih)?ih:0;

        value.FrameGeneration=Read("FrameGeneration","Enabled",false); value.IntelArcOnly=Read("Intel ARC","Enabled",true);value.IntelDlss5DirectMl=Read("Intel ARC","DLSS5DirectML",false);value.Diagnostics=Read("Diagnostics","Enabled",true);

        value.Hud=Read("HUD","Enabled",true);value.ShowFPS=Read("HUD","ShowFPS",true);value.ShowFrameTimes=Read("HUD","ShowFrameTimes",false);value.ShowUsage=Read("HUD","ShowUsage",true);value.ShowStatus=Read("HUD","ShowStatus",true);

        value.VisualEnhancement=Read("VisualEnhancement","Enabled",false);

        value.SourceFPS=int.TryParse(ini.Get("Pacing","SourceFPS","0"),out var fps)&&new[]{0,30,60,90,120}.Contains(fps)?fps:0;return value;

    }

    public string Apply(ModSettings settings) {

        settings.Validate(); EnsureClosed();

        var iniPath=Path.Combine(settings.GameDirectory,"GTAXeSS.ini"); var existing=File.Exists(iniPath)?File.ReadAllBytes(iniPath):null;

        var xmlBytes=File.ReadAllBytes(settings.GameSettingsPath);var xml=XDocument.Load(settings.GameSettingsPath,LoadOptions.PreserveWhitespace);

        var video=xml.Root?.Element("video")??throw new InvalidOperationException("This is not GTA V's settings.xml: missing video section.");

        var (w,h)=settings.Dimensions;

        void SetVideo(string key,int val) { var element=video.Element(key)??throw new InvalidOperationException($"GTA settings are missing {key}.");element.SetAttributeValue("value",val); }

        int renderW=settings.UpscalingQuality is "NATIVE" or "AA"?w:settings.InputWidth,renderH=settings.UpscalingQuality is "NATIVE" or "AA"?h:settings.InputHeight;
        if(renderW<640||renderH<360||renderW>w||renderH>h)throw new InvalidOperationException("Query a valid SDK render resolution before applying upscaling.");
        SetVideo("ScreenWidth",renderW);SetVideo("ScreenHeight",renderH);
        SetVideo("Windowed",renderW!=w||renderH!=h?1:0);
        var selectedAdapter=settings.SelectedAdapterIndex>=0?settings.SelectedAdapterIndex:settings.Provider=="NVIDIA"?0:-1;
        if(selectedAdapter>=0) {
            var adapter=video.Element("AdapterIndex");
            if(adapter==null){adapter=new XElement("AdapterIndex");video.Add(adapter);}
            adapter.SetAttributeValue("value",selectedAdapter);
        }
        // SDK-recommended inputs are often not physical display modes (e.g. 2259x1271).
        // The launch file also forces fullscreen so GTA cannot restore its old windowed mode.

        // The accepted native-colour path requires no game-side frame scaling.

        var sampling=xml.Root?.Element("graphics")?.Element("SamplingMode"); if(sampling!=null)sampling.SetAttributeValue("value",0);

        var ini=new IniDocument(existing==null?"":File.ReadAllText(iniPath));

        if(settings.Provider=="NVIDIA")ini.Set("Redesign","DirectSharedColor","1");

        ini.Set("XeSS","OutputResolution",settings.Resolution);ini.Set("XeSS","Quality",settings.UpscalingQuality=="NATIVE"?"AUTO":settings.UpscalingQuality);
        ini.Set("Upscaling","Quality",settings.UpscalingQuality);ini.Set("Upscaling","InputWidth",renderW.ToString(CultureInfo.InvariantCulture));ini.Set("Upscaling","InputHeight",renderH.ToString(CultureInfo.InvariantCulture));

        void Set(string section,string key,bool val)=>ini.Set(section,key,val?"1":"0");

        ini.Set("Graphics","Provider",settings.Provider);ini.Set("Graphics","NvidiaAdapterIndex",settings.NvidiaAdapterIndex.ToString(CultureInfo.InvariantCulture));ini.Set("Graphics","NvidiaAdapterName",settings.NvidiaAdapterName);ini.Set("Graphics","IntelAdapterIndex",settings.IntelAdapterIndex.ToString(CultureInfo.InvariantCulture));ini.Set("Graphics","IntelAdapterName",settings.IntelAdapterName);ini.Set("NVIDIA","Multiplier",settings.NvidiaMultiplier.ToString(CultureInfo.InvariantCulture));Set("NVIDIA","DLAA",settings.Provider=="NVIDIA"&&settings.UpscalingQuality!="NATIVE");Set("NVIDIA","DLSS5",settings.Provider=="NVIDIA"&&settings.NvidiaDlss5);Set("NVIDIA","DLSS5RTX40Experimental",settings.NvidiaDlss5Rtx40Experimental);ini.Set("NVIDIA","DLSSVersion",settings.EffectiveDlssVersion);
        Set("App","AutoStart",settings.AutoStartMod);ini.Set("App","Revision",Guid.NewGuid().ToString("N"));

        Set("FrameGeneration","Enabled",settings.FrameGeneration);Set("Intel ARC","Enabled",settings.Provider=="Intel"&&settings.IntelArcOnly);Set("Intel ARC","DLSS5DirectML",settings.Provider=="Intel"&&settings.IntelDlss5DirectMl);Set("Diagnostics","Enabled",settings.Diagnostics);ini.Set("Pacing","SourceFPS",settings.SourceFPS.ToString(CultureInfo.InvariantCulture));

        Set("HUD","Enabled",settings.Hud);Set("HUD","ShowFPS",settings.ShowFPS);Set("HUD","ShowFrameTimes",settings.ShowFrameTimes);Set("HUD","ShowUsage",settings.ShowUsage);Set("HUD","ShowStatus",settings.ShowStatus);

        Set("VisualEnhancement","Enabled",settings.VisualEnhancement);

        string backup=CreateBackup("settings"); if(existing!=null)File.WriteAllBytes(Path.Combine(backup,"GTAXeSS.ini"),existing);File.WriteAllBytes(Path.Combine(backup,"settings.xml"),xmlBytes);

        try { EnsureClosed();AtomicWrite(iniPath,Encoding.Unicode.GetBytes("\uFEFF"+ini));EnsureClosed();using var buffer=new MemoryStream();xml.Save(buffer,SaveOptions.DisableFormatting);AtomicWrite(settings.GameSettingsPath,buffer.ToArray()); }

        catch {if(existing!=null)AtomicWrite(iniPath,existing);else if(File.Exists(iniPath))File.Delete(iniPath);AtomicWrite(settings.GameSettingsPath,xmlBytes);throw;}

        SavePreferences(settings);return backup;

    }

    public void ResolveInput(ModSettings s,string payload){
        s.Validate();
        var (w,h)=s.Dimensions;if(s.UpscalingQuality is "NATIVE" or "AA"){s.InputWidth=w;s.InputHeight=h;return;}
        var info=new ProcessStartInfo(Path.Combine(payload,"NvidiaProbe.exe")){UseShellExecute=false,CreateNoWindow=true,RedirectStandardOutput=true,RedirectStandardError=true};foreach(var argument in new[]{payload,s.Provider,w.ToString(CultureInfo.InvariantCulture),h.ToString(CultureInfo.InvariantCulture),s.UpscalingQuality})info.ArgumentList.Add(argument);
        using var process=Process.Start(info)??throw new InvalidOperationException("Cannot query SDK render resolution.");var output=process.StandardOutput.ReadToEndAsync();var error=process.StandardError.ReadToEndAsync();if(!process.WaitForExit(30000)){process.Kill();throw new InvalidOperationException("Upscaling SDK query timed out.");}
        var line=output.GetAwaiter().GetResult().Split('\n').FirstOrDefault(l=>l.StartsWith("{\"inputWidth\""));if(process.ExitCode!=0||line==null)throw new InvalidOperationException("SDK quality query failed: "+error.GetAwaiter().GetResult().Trim());using var json=JsonDocument.Parse(line);s.InputWidth=json.RootElement.GetProperty("inputWidth").GetInt32();s.InputHeight=json.RootElement.GetProperty("inputHeight").GetInt32();
        // Keep the exact SDK size. GTA clamps window heights below 560 pixels.
        if(s.InputHeight<560){s.UpscalingQuality=NextGtaQuality(s.UpscalingQuality,s.Provider);ResolveInput(s,payload);}
    }
    public static string NextGtaQuality(string quality,string provider){var modes=provider=="NVIDIA"?new[]{"ULTRA_PERFORMANCE","PERFORMANCE","BALANCED","QUALITY","AA"}:new[]{"ULTRA_PERFORMANCE","PERFORMANCE","BALANCED","QUALITY","ULTRA_QUALITY","ULTRA_QUALITY_PLUS","AA"};var index=Array.IndexOf(modes,quality);if(index<0||index==modes.Length-1)throw new InvalidOperationException("No GTA-compatible input resolution for this quality.");return modes[index+1];}
    public bool ApplyLive(ModSettings s){
        s.Validate();var path=Path.Combine(s.GameDirectory,"GTAXeSS.ini");var ini=new IniDocument(File.ReadAllText(path));var installed=ReadInstalled(new ModSettings{GameDirectory=s.GameDirectory});
        bool pending=installed.EffectiveDlssVersion!=s.EffectiveDlssVersion||installed.NvidiaDlss5!=(s.Provider=="NVIDIA"&&s.NvidiaDlss5)||installed.Provider!=s.Provider||installed.SelectedAdapterIndex!=s.SelectedAdapterIndex||installed.Resolution!=s.Resolution||installed.UpscalingQuality!=s.UpscalingQuality||installed.IntelArcOnly!=(s.Provider=="Intel"&&s.IntelArcOnly)||installed.IntelDlss5DirectMl!=(s.Provider=="Intel"&&s.IntelDlss5DirectMl)||installed.Diagnostics!=s.Diagnostics||installed.VisualEnhancement!=s.VisualEnhancement;
        foreach(var item in new[]{("Enabled",s.Hud),("ShowFPS",s.ShowFPS),("ShowFrameTimes",s.ShowFrameTimes),("ShowUsage",s.ShowUsage),("ShowStatus",s.ShowStatus)})ini.Set("HUD",item.Item1,item.Item2?"1":"0");
        ini.Set("VisualEnhancement","Enabled",s.VisualEnhancement?"1":"0");
        ini.Set("Pacing","SourceFPS",s.SourceFPS.ToString(CultureInfo.InvariantCulture));ini.Set("FrameGeneration","Enabled",s.FrameGeneration?"1":"0");ini.Set("App","FGCommand",installed.FrameGeneration==s.FrameGeneration?"-1":s.FrameGeneration?"1":"0");if(installed.Provider=="NVIDIA"&&s.Provider=="NVIDIA")ini.Set("NVIDIA","Multiplier",s.NvidiaMultiplier.ToString(CultureInfo.InvariantCulture));ini.Set("App","AutoStart",s.AutoStartMod?"1":"0");ini.Set("App","Revision",Guid.NewGuid().ToString("N"));
        string backup=CreateBackup("live-settings");File.Copy(path,Path.Combine(backup,"GTAXeSS.ini"));AtomicWrite(path,Encoding.Unicode.GetBytes("\uFEFF"+ini));SavePreferences(s);return pending;
    }

    public async System.Threading.Tasks.Task<string> ConfigureIntelDlss5Async(ModSettings settings){
        EnsureClosed();
        const string tag="v0.1.35.1";
        const string mainHash="29532C7DFF93A7171E5825238FC0BDD0CFB9828CDE7D4FCEF5608537D589DC23";
        const string portHash="74A24EB103EF05A54EDE6598D70F4C7093A2C683F59C40972D7A635E36CE8AE9";
        const string mainUrl="https://github.com/janblade/OptiScaler-F5-DLSSNR-Multipass/releases/download/v0.1.35.1/OptiScaler-DLSSNR-F5-v0.1.35.1-with-dlss-fg.zip";
        const string portUrl="https://github.com/janblade/OptiScaler-F5-DLSSNR-Multipass/releases/download/v0.1.35.1/Optional-Vendor-Neutral-DLSS5-Forwarder.zip";
        var marker=Path.Combine(settings.GameDirectory,"Zink-Intel-DLSS5.json");
        var proxy=Path.Combine(settings.GameDirectory,"dxgi.dll");
        var managedNames=new[]{"dxgi.dll","OptiScaler.ini","nvngx.dll_dlssnr.dll","nr_port.ini","Zink-Intel-DLSS5-LICENSE.txt"};
        if(settings.Provider!="Intel"||!settings.IntelDlss5DirectMl){
            if(!File.Exists(marker))return "Intel DLSS 5 DirectML is off.";
            var safety=CreateBackup("disable-intel-dlss5");
            foreach(var name in managedNames){var path=Path.Combine(settings.GameDirectory,name);if(File.Exists(path)){File.Copy(path,Path.Combine(safety,name),true);File.Delete(path);}}
            var folder=Path.Combine(settings.GameDirectory,"OptiScaler");if(Directory.Exists(folder)){var saved=Path.Combine(safety,"OptiScaler");CopyTree(folder,saved);Directory.Delete(folder,true);}
            File.Copy(marker,Path.Combine(safety,"Zink-Intel-DLSS5.json"),true);File.Delete(marker);
            return "Intel DLSS 5 DirectML was disabled; the removed files are safety-backed up.";
        }
        settings.FrameGeneration=false;
        if(File.Exists(proxy)&&!File.Exists(marker))throw new InvalidOperationException("Intel DLSS 5 needs dxgi.dll, but another mod already owns that filename. Remove or chain that loader before enabling this option; Zink did not overwrite it.");
        var foreignFolder=Path.Combine(settings.GameDirectory,"OptiScaler");
        if(Directory.Exists(foreignFolder)&&!File.Exists(marker))throw new InvalidOperationException("An unmanaged OptiScaler folder already exists in GTA. Remove it or manage it manually before enabling Intel DLSS 5.");
        var downloads=Path.Combine(DataRoot,"downloads");Directory.CreateDirectory(downloads);
        async System.Threading.Tasks.Task<string> Download(string name,string url,string hash){
            var path=Path.Combine(downloads,name);if(File.Exists(path)&&Convert.ToHexString(SHA256.HashData(await File.ReadAllBytesAsync(path))).Equals(hash,StringComparison.OrdinalIgnoreCase))return path;
            using var client=new HttpClient{Timeout=TimeSpan.FromMinutes(15)};client.DefaultRequestHeaders.UserAgent.ParseAdd("Zink-GTA/2.9");var bytes=await client.GetByteArrayAsync(url);
            if(!Convert.ToHexString(SHA256.HashData(bytes)).Equals(hash,StringComparison.OrdinalIgnoreCase))throw new InvalidOperationException("The Intel DLSS 5 package failed SHA-256 verification. Nothing was installed.");AtomicWrite(path,bytes);return path;
        }
        var main=await Download($"OptiScaler-{tag}.zip",mainUrl,mainHash);var port=await Download($"Vendor-Neutral-DLSS5-{tag}.zip",portUrl,portHash);
        var stage=Path.Combine(DataRoot,"staging","IntelDlss5-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(stage);
        try{
            var mainStage=Path.Combine(stage,"main");var portStage=Path.Combine(stage,"port");ZipFile.ExtractToDirectory(main,mainStage);ZipFile.ExtractToDirectory(port,portStage);
            var sourceDll=Path.Combine(mainStage,"OptiScaler.dll");var sourceIni=Path.Combine(mainStage,"OptiScaler.ini");var portDll=Path.Combine(portStage,"nvngx.dll_dlssnr.dll");var portIni=Path.Combine(portStage,"nr_port.ini");
            if(!File.Exists(sourceDll)||!File.Exists(sourceIni)||!File.Exists(portDll)||!File.Exists(portIni))throw new InvalidOperationException("The verified Intel DLSS 5 packages are incomplete.");
            if(!File.Exists(Path.Combine(settings.GameDirectory,"nvngx_dlssnr.dll")))throw new InvalidOperationException("The required DLSS Neural Rendering 310.9 model is not installed. Install/update the Zink mod first.");
            var optiIni=new IniDocument(await File.ReadAllTextAsync(sourceIni));
            optiIni.Set("FrameGen","Enabled","false");optiIni.Set("FrameGen","FGInput","none");
            optiIni.Set("Inputs","EnableXeSSInputs","false");
            optiIni.Set("DlssNr","Enabled","true");optiIni.Set("DlssNr","NativeMotion","true");optiIni.Set("DlssNr","NativeDepthFinder","true");optiIni.Set("DlssNr","NativeInput","true");optiIni.Set("DlssNr","NativeUpscaler","false");optiIni.Set("DlssNr","FinishedPicture","true");optiIni.Set("DlssNr","Passes","1");optiIni.Set("DlssNr","WorkingScale","0.5");
            AtomicWrite(proxy,await File.ReadAllBytesAsync(sourceDll));AtomicWrite(Path.Combine(settings.GameDirectory,"OptiScaler.ini"),Encoding.UTF8.GetBytes(optiIni.ToString()));AtomicWrite(Path.Combine(settings.GameDirectory,"nvngx.dll_dlssnr.dll"),await File.ReadAllBytesAsync(portDll));AtomicWrite(Path.Combine(settings.GameDirectory,"nr_port.ini"),await File.ReadAllBytesAsync(portIni));
            var optiTarget=Path.Combine(settings.GameDirectory,"OptiScaler");if(Directory.Exists(optiTarget))Directory.Delete(optiTarget,true);CopyTree(Path.Combine(mainStage,"OptiScaler"),optiTarget,path=>!path.Contains($"{Path.DirectorySeparatorChar}streamline{Path.DirectorySeparatorChar}",StringComparison.OrdinalIgnoreCase));
            var licence=await File.ReadAllTextAsync(Path.Combine(mainStage,"LICENSE"));AtomicWrite(Path.Combine(settings.GameDirectory,"Zink-Intel-DLSS5-LICENSE.txt"),Encoding.UTF8.GetBytes("OptiScaler-F5-DLSSNR-Multipass "+tag+"\r\nSource: https://github.com/janblade/OptiScaler-F5-DLSSNR-Multipass\r\n\r\n"+licence));
            AtomicWrite(marker,Encoding.UTF8.GetBytes(JsonSerializer.Serialize(new{source="janblade/OptiScaler-F5-DLSSNR-Multipass",version=tag,backend="vendor-neutral DirectML",model="DLSS Neural Rendering 310.9",frameGeneration=false,proxy="dxgi.dll",installedUtc=DateTime.UtcNow,mainSha256=mainHash,portSha256=portHash},new JsonSerializerOptions{WriteIndented=true})));
            return "Intel Arc DLSS 5 DirectML installed in NR-only mode; XeSS upscaling remains active and frame generation is off.";
        }finally{try{Directory.Delete(stage,true);}catch{}}
    }

    static void CopyTree(string source,string destination,Func<string,bool>? include=null){
        foreach(var directory in Directory.EnumerateDirectories(source,"*",SearchOption.AllDirectories)){if(include!=null&&!include(directory+Path.DirectorySeparatorChar))continue;Directory.CreateDirectory(Path.Combine(destination,Path.GetRelativePath(source,directory)));}
        Directory.CreateDirectory(destination);foreach(var file in Directory.EnumerateFiles(source,"*",SearchOption.AllDirectories)){if(include!=null&&!include(file))continue;var target=Path.Combine(destination,Path.GetRelativePath(source,file));Directory.CreateDirectory(Path.GetDirectoryName(target)!);File.Copy(file,target,true);}
    }

    public async System.Threading.Tasks.Task<string> ConfigureVisualModAsync(ModSettings settings,IProgress<string>? progress=null){
        var bundledArchive=Path.Combine(AppContext.BaseDirectory,"VisualMod","NaturalVision Evolved - Single Player Package.zip");
        var downloadedArchive=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile),"Downloads","NaturalVision Evolved - Single Player Package.zip");
        var cachedArchive=Path.Combine(DataRoot,"downloads","NaturalVision Evolved - Single Player Package.zip");
        var nveArchive=File.Exists(bundledArchive)?bundledArchive:File.Exists(downloadedArchive)?downloadedArchive:cachedArchive;
        var nveMarker=Path.Combine(settings.GameDirectory,"Zink-NVE.json");
        if(!settings.VisualEnhancement&&File.Exists(nveMarker))return await ConfigureNaturalVisionAsync(settings,nveArchive,progress);
        if(settings.VisualEnhancement&&!File.Exists(nveArchive)&&!string.IsNullOrWhiteSpace(settings.VisualModDownloadUrl)){nveArchive=await DownloadNaturalVisionAsync(settings.VisualModDownloadUrl,cachedArchive,progress);settings.VisualModDownloadUrl="";}
        if(File.Exists(nveArchive))return await ConfigureNaturalVisionAsync(settings,nveArchive,progress);
        if(settings.VisualEnhancement)throw new InvalidOperationException("Paste the authorised NaturalVision Evolved package link from Razed Mods / Cfx Marketplace, then turn the Visual Mod setting on.");
        EnsureClosed();
        const string visualVVersion="1.0.630";
        const string archiveHash="96E666AD5DCFB9840317E9DD15A0151C358C51FEB2A85265494B190D070A112C";
        const string downloadUrl="https://gta5mod.net/download-mod/275787";
        var config=Path.Combine(settings.GameDirectory,"fxReloader.ini");
        var marker=Path.Combine(settings.GameDirectory,"Zink-VisualV.json");
        var live=Path.Combine(settings.GameDirectory,"fxReloader");
        if(!settings.VisualEnhancement){
            if(File.Exists(marker))WriteFxConfig(config,false);
            return File.Exists(marker)?"VisualV loose-file redirect disabled; the next GTA launch uses stock weather and lighting.":"VisualV graphics pack is not installed.";
        }
        if(!File.Exists(Path.Combine(settings.GameDirectory,"fxReloader.asi")))throw new InvalidOperationException("Install / update the mod before enabling the VisualV graphics pack.");
        if((File.Exists(config)||Directory.Exists(live))&&!File.Exists(marker))throw new InvalidOperationException("An existing third-party fxReloader installation was found. Remove it or manage it separately before enabling the bundled visual pack.");
        var cache=Path.Combine(DataRoot,"downloads");Directory.CreateDirectory(cache);var archive=Path.Combine(cache,$"Legacy-VisualV-{visualVVersion}.7z");
        if(!File.Exists(archive)||!Convert.ToHexString(SHA256.HashData(await File.ReadAllBytesAsync(archive))).Equals(archiveHash,StringComparison.OrdinalIgnoreCase)){
            using var client=new HttpClient{Timeout=TimeSpan.FromMinutes(5)};client.DefaultRequestHeaders.UserAgent.ParseAdd("Zink-GTA-Visual-Mod/2.8");
            var bytes=await client.GetByteArrayAsync(downloadUrl);
            if(!Convert.ToHexString(SHA256.HashData(bytes)).Equals(archiveHash,StringComparison.OrdinalIgnoreCase))throw new InvalidOperationException("The VisualV Legacy download did not match the creator release SHA-256. Nothing was installed.");
            AtomicWrite(archive,bytes);
        }
        var staging=Path.Combine(DataRoot,"staging","VisualV-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(staging);
        try{
            var tar=new ProcessStartInfo("tar.exe"){UseShellExecute=false,CreateNoWindow=true,RedirectStandardOutput=true,RedirectStandardError=true};
            foreach(var argument in new[]{"-xf",archive,"-C",staging,"installation via modPackage/VisualV.oiv"})tar.ArgumentList.Add(argument);
            using(var process=Process.Start(tar)??throw new InvalidOperationException("Windows archive support could not be started.")){
                var error=process.StandardError.ReadToEndAsync();if(!process.WaitForExit(120000)){process.Kill();throw new InvalidOperationException("VisualV Legacy archive extraction timed out.");}
                if(process.ExitCode!=0)throw new InvalidOperationException("VisualV Legacy archive extraction failed: "+error.GetAwaiter().GetResult().Trim());
            }
            var oiv=Path.Combine(staging,"installation via modPackage","VisualV.oiv");var package=Path.Combine(staging,"package");
            if(!File.Exists(oiv))throw new InvalidOperationException("The verified VisualV Legacy archive does not contain VisualV.oiv.");
            ZipFile.ExtractToDirectory(oiv,package);var data=Path.Combine(package,"content","update.rpf","common","data");
            var visual=Path.Combine(data,"visualsettings.dat");
            var sourceTimecycle=Path.Combine(data,"timecycle");
            var expectedTimecycles=new[]{"timecycle_mods_1.xml","timecycle_mods_2.xml","timecycle_mods_3.xml","timecycle_mods_4.xml","underwater_deep.xml","w_blizzard.xml","w_clear.xml","w_clearing.xml","w_clouds.xml","w_extrasunny.xml","w_foggy.xml","w_halloween.xml","w_neutral.xml","w_overcast.xml","w_rain.xml","w_rainhalloween.xml","w_smog.xml","w_snow.xml","w_snowhalloween.xml","w_snowlight.xml","w_thunder.xml","w_xmas.xml"};
            if(!File.Exists(visual)||expectedTimecycles.Any(name=>!File.Exists(Path.Combine(sourceTimecycle,name))))throw new InvalidOperationException("The verified VisualV Legacy package is missing required visual or timecycle data.");
            Directory.CreateDirectory(live);
            AtomicWrite(Path.Combine(live,"visualsettings.dat"),await File.ReadAllBytesAsync(visual));
            var liveTimecycle=Path.Combine(live,"timecycle");Directory.CreateDirectory(liveTimecycle);
            foreach(var name in expectedTimecycles)AtomicWrite(Path.Combine(liveTimecycle,name),await File.ReadAllBytesAsync(Path.Combine(sourceTimecycle,name)));
            AtomicWrite(marker,Encoding.UTF8.GetBytes(JsonSerializer.Serialize(new{source="https://www.gta5-mods.com/misc/visualv",downloadMirror=downloadUrl,version=visualVVersion,sha256=archiveHash,installedUtc=DateTime.UtcNow,mode="VisualV Legacy full timecycle and visual settings redirect",timecycleOverride=true,timecycleFiles=expectedTimecycles.Length,singlePlayerOnly=true},new JsonSerializerOptions{WriteIndented=true})));
            WriteFxConfig(config,true);
            return $"VisualV {visualVVersion} Legacy installed with all {expectedTimecycles.Length} Legacy timecycle files and visual settings. Active on the next GTA launch; Story Mode only.";
        }finally{try{Directory.Delete(staging,true);}catch{}}
    }

    async System.Threading.Tasks.Task<string> DownloadNaturalVisionAsync(string address,string destination,IProgress<string>? progress){
        if(!Uri.TryCreate(address.Trim(),UriKind.Absolute,out var uri)||uri.Scheme!=Uri.UriSchemeHttps)throw new InvalidOperationException("Paste an authorised HTTPS NaturalVision Evolved package link from Razed Mods / Cfx Marketplace.");
        Directory.CreateDirectory(Path.GetDirectoryName(destination)!);var partial=destination+".part";long existing=File.Exists(partial)?new FileInfo(partial).Length:0;
        using var client=new HttpClient{Timeout=System.Threading.Timeout.InfiniteTimeSpan};client.DefaultRequestHeaders.UserAgent.ParseAdd("Zink-GTA-NVE-Installer/3.2");
        using var request=new HttpRequestMessage(HttpMethod.Get,uri);if(existing>0)request.Headers.Range=new System.Net.Http.Headers.RangeHeaderValue(existing,null);
        progress?.Report(existing>0?$"Resuming NaturalVision Evolved download at {existing/1073741824d:0.00} GB…":"Downloading NaturalVision Evolved in the background…");
        using var response=await client.SendAsync(request,HttpCompletionOption.ResponseHeadersRead);
        if(existing>0&&response.StatusCode!=System.Net.HttpStatusCode.PartialContent){existing=0;try{File.Delete(partial);}catch{};response.EnsureSuccessStatusCode();}else response.EnsureSuccessStatusCode();
        var remaining=response.Content.Headers.ContentLength;var total=remaining.HasValue?existing+remaining.Value:0;long received=existing,lastReported=-1;
        await using(var input=await response.Content.ReadAsStreamAsync())await using(var output=new FileStream(partial,existing>0?FileMode.Append:FileMode.Create,FileAccess.Write,FileShare.Read,1024*1024,true)){
            var buffer=new byte[1024*1024];int count;while((count=await input.ReadAsync(buffer))>0){await output.WriteAsync(buffer.AsMemory(0,count));received+=count;var percent=total>0?received*100/total:-1;if(percent!=lastReported){lastReported=percent;progress?.Report(total>0?$"Downloading NaturalVision Evolved… {percent}% · {received/1073741824d:0.00} / {total/1073741824d:0.00} GB":$"Downloading NaturalVision Evolved… {received/1073741824d:0.00} GB");}}
        }
        progress?.Report("Download complete. Verifying the NaturalVision Evolved archive…");File.Move(partial,destination,true);return destination;
    }

    async System.Threading.Tasks.Task<string> ConfigureNaturalVisionAsync(ModSettings settings,string archive,IProgress<string>? progress){
        EnsureClosed();
        const string expectedArchiveHash="193AB8B4B814B52DFAE955434B02E03B05574B34CB4162249421A92AFFBE58FD";
        const string expectedLoaderHash="C39D574CAA9DB4462B20110E87A8229E48DEEB2819883800C8E54F1C6C80548C";
        var root=Path.Combine(DataRoot,"NVE");var packages=Path.Combine(root,"packages");var marker=Path.Combine(settings.GameDirectory,"Zink-NVE.json");
        var helper=Path.Combine(DataRoot,"tools","OIVInstaller","CodeWalker.OIVInstaller.exe");
        var loaderArchive=Directory.EnumerateFiles(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile),"Downloads"),"*RageOpenV*.zip").OrderByDescending(File.GetLastWriteTimeUtc).FirstOrDefault();
        var names=new[]{"1. NaturalVision Installer - PART ONE.oiv","2. NaturalVision Installer - PART TWO.oiv","3. NaturalVision Installer - PART THREE.oiv"};
        var installedNames=new[]{"NaturalVision Evolved - Part One","NaturalVision Evolved - Part Two","NaturalVision Evolved - Part Three"};
        if(!settings.VisualEnhancement){
            if(!File.Exists(marker))return "NaturalVision Evolved is off.";
            if(!File.Exists(helper))throw new InvalidOperationException("The local OIV installer is missing, so NVE cannot be safely removed.");
            for(int i=installedNames.Length-1;i>=0;i--){progress?.Report($"Removing NVE part {i+1} of 3…");await RunOivHelper(helper,"--uninstall",installedNames[i],settings.GameDirectory);}
            var loader=Path.Combine(settings.GameDirectory,"RageOpenV.asi");if(File.Exists(loader))File.Move(loader,loader+".disabled",true);
            if(File.Exists(Path.Combine(settings.GameDirectory,"fxReloader.ini")))WriteFxConfig(Path.Combine(settings.GameDirectory,"fxReloader.ini"),false);
            var safety=CreateBackup("disable-nve");File.Copy(marker,Path.Combine(safety,"Zink-NVE.json"),true);File.Delete(marker);
            return "NaturalVision Evolved was disabled and its three packages were reverted from the mods folder.";
        }
        if(File.Exists(marker)&&File.Exists(Path.Combine(settings.GameDirectory,"RageOpenV.asi"))&&Directory.Exists(Path.Combine(settings.GameDirectory,"mods")))return "NaturalVision Evolved is already installed and enabled.";
        progress?.Report("Verifying the 16 GB NaturalVision Evolved archive…");
        using(var stream=new FileStream(archive,FileMode.Open,FileAccess.Read,FileShare.Read)){if(!Convert.ToHexString(await SHA256.HashDataAsync(stream)).Equals(expectedArchiveHash,StringComparison.OrdinalIgnoreCase))throw new InvalidOperationException("The downloaded NVE archive does not match the verified local package. Nothing was installed.");}
        if(loaderArchive==null)throw new InvalidOperationException("Download RageOpenV from its official GTA5-Mods page before enabling NVE.");
        using(var stream=new FileStream(loaderArchive,FileMode.Open,FileAccess.Read,FileShare.Read)){if(!Convert.ToHexString(await SHA256.HashDataAsync(stream)).Equals(expectedLoaderHash,StringComparison.OrdinalIgnoreCase))throw new InvalidOperationException("The RageOpenV archive hash does not match the publisher-listed build.");}
        if(!File.Exists(helper))throw new InvalidOperationException("The local headless OIV installer is missing. Rebuild the CodeWalker OIV helper before enabling NVE.");
        if(!File.Exists(Path.Combine(settings.GameDirectory,"dinput8.dll")))throw new InvalidOperationException("The ScriptHookV ASI loader (dinput8.dll) is required before NVE can load.");
        Directory.CreateDirectory(packages);
        progress?.Report("Extracting NVE's three installation packages…");
        using(var zip=ZipFile.OpenRead(archive))foreach(var name in names){var entry=zip.GetEntry(name)??throw new InvalidOperationException("The NVE archive is missing "+name);var target=Path.Combine(packages,name);if(!File.Exists(target)||new FileInfo(target).Length!=entry.Length){await using var input=entry.Open();await using var output=new FileStream(target,FileMode.Create,FileAccess.Write,FileShare.None);await input.CopyToAsync(output);}}
        var loaderTarget=Path.Combine(settings.GameDirectory,"RageOpenV.asi");
        if(!File.Exists(loaderTarget)){progress?.Report("Installing the verified GTA mods-folder loader…");using var zip=ZipFile.OpenRead(loaderArchive);var entry=zip.GetEntry("RageOpenV.asi")??throw new InvalidOperationException("RageOpenV.asi is missing from the verified archive.");await using var input=entry.Open();using var memory=new MemoryStream();await input.CopyToAsync(memory);AtomicWrite(loaderTarget,memory.ToArray());}
        var oldVisual=Path.Combine(settings.GameDirectory,"fxReloader.ini");if(File.Exists(oldVisual))WriteFxConfig(oldVisual,false);
        for(int i=0;i<names.Length;i++){progress?.Report($"Installing NaturalVision Evolved part {i+1} of 3…");await RunOivHelper(helper,"--install",Path.Combine(packages,names[i]),settings.GameDirectory);}
        progress?.Report("Verifying the NVE installation and creating restore data…");
        if(!Directory.Exists(Path.Combine(settings.GameDirectory,"mods")))throw new InvalidOperationException("The NVE packages completed but no mods folder was created; the install is not being marked active.");
        AtomicWrite(marker,Encoding.UTF8.GetBytes(JsonSerializer.Serialize(new{sourceArchive=archive,archiveSha256=expectedArchiveHash,loader="RageOpenV 1.0",loaderSha256=expectedLoaderHash,installer="CodeWalker OIV Installer",parts=names,installedUtc=DateTime.UtcNow,upscalerPluginInstalled=false,storyModeOnly=true},new JsonSerializerOptions{WriteIndented=true})));
        return "NaturalVision Evolved is installed in the isolated mods folder. Its optional DLSS/FSR plug-in was not installed.";
    }

    static async System.Threading.Tasks.Task RunOivHelper(string helper,string command,string package,string gameDirectory){
        var info=new ProcessStartInfo(helper){UseShellExecute=false,CreateNoWindow=true,RedirectStandardOutput=true,RedirectStandardError=true};
        foreach(var arg in new[]{"--game",gameDirectory,"--force",command,package})info.ArgumentList.Add(arg);
        using var process=Process.Start(info)??throw new InvalidOperationException("The NVE package installer could not start.");var output=process.StandardOutput.ReadToEndAsync();var error=process.StandardError.ReadToEndAsync();
        await process.WaitForExitAsync();var text=(await output)+"\n"+(await error);if(process.ExitCode!=0)throw new InvalidOperationException("NVE package installation failed: "+text.Trim());
    }

    static void WriteFxConfig(string path,bool enabled)=>AtomicWrite(path,Encoding.UTF8.GetBytes($"; Managed by Zink GTA Visual Mod. Story Mode only.\r\nreloadHotkey = F10\r\nwatchFiles = true\r\nwatchFolder = fxReloader\r\nredirectFromOwnFolder = {(enabled?"true":"false")}\r\nunsafeReloadFromHotkeyThread = false\r\n"));
    public bool RequestActivation(ModSettings settings){
        if(!settings.AutoStartMod)return false;
        var installed=ReadInstalled(new ModSettings{GameDirectory=settings.GameDirectory});if(!installed.FrameGeneration&&installed.UpscalingQuality=="NATIVE")return false;
        var path=Path.Combine(settings.GameDirectory,"GTAXeSS.ini");var ini=new IniDocument(File.ReadAllText(path));ini.Set("App","FGCommand","1");ini.Set("App","Revision",Guid.NewGuid().ToString("N"));AtomicWrite(path,Encoding.Unicode.GetBytes("\uFEFF"+ini));return true;
    }
    public string Install(ModSettings settings,string payloadRoot) {

        settings.Validate();EnsureClosed();

        if(!File.Exists(Path.Combine(settings.GameDirectory,"ScriptHookV.dll")))throw new InvalidOperationException("Install ScriptHookV for GTA V Legacy before installing this mod.");

        var manifest=JsonSerializer.Deserialize<BundleManifest>(File.ReadAllText(Path.Combine(payloadRoot,"manifest.json")))??throw new InvalidOperationException("Missing backend manifest.");

        if(manifest.Protocol!=1||manifest.Files.Count==0)throw new InvalidOperationException("Unsupported backend bundle.");

        foreach(var item in manifest.Files) {

            if(Path.GetFileName(item.Key)!=item.Key||item.Key.Contains(':')||item.Key is "GTAXeSS.ini" or "settings.xml")throw new InvalidOperationException("Invalid bundled filename.");

            string source=Path.Combine(payloadRoot,item.Key);

            if(!File.Exists(source)||!Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(source))).Equals(item.Value,StringComparison.OrdinalIgnoreCase))throw new InvalidOperationException("Backend integrity check failed: "+item.Key);

        }

        string backup=CreateBackup("install");var originals=new Dictionary<string,byte[]?>();

        foreach(var name in manifest.Files.Keys) {var target=Path.Combine(settings.GameDirectory,name);originals[name]=File.Exists(target)?File.ReadAllBytes(target):null;if(originals[name]!=null)File.WriteAllBytes(Path.Combine(backup,name),originals[name]!);}
        var bridgePath=Path.Combine(settings.GameDirectory,"GTAXeSS-AppBridge.json");var iniPath=Path.Combine(settings.GameDirectory,"GTAXeSS.ini");bool bridgeExisted=File.Exists(bridgePath),iniExisted=File.Exists(iniPath);if(bridgeExisted)File.Copy(bridgePath,Path.Combine(backup,"GTAXeSS-AppBridge.json"));if(iniExisted)File.Copy(iniPath,Path.Combine(backup,"GTAXeSS.ini"));File.Copy(settings.GameSettingsPath,Path.Combine(backup,"settings.xml"));
        File.WriteAllText(Path.Combine(backup,"install-record.json"),JsonSerializer.Serialize(new InstallRestoreRecord{GameDirectory=settings.GameDirectory,SettingsPath=settings.GameSettingsPath,OriginalFiles=originals.ToDictionary(x=>x.Key,x=>x.Value!=null),BridgeExisted=bridgeExisted,IniExisted=iniExisted},new JsonSerializerOptions{WriteIndented=true}));

        try {

            foreach(var name in manifest.Files.Keys){EnsureClosed();AtomicWrite(Path.Combine(settings.GameDirectory,name),File.ReadAllBytes(Path.Combine(payloadRoot,name)));}

            AtomicWrite(Path.Combine(settings.GameDirectory,"GTAXeSS-AppBridge.json"),Encoding.UTF8.GetBytes(JsonSerializer.Serialize(new{protocol=1,build=manifest.Build,resolutions=new[]{"1080P","1440P","4K"},providers=new[]{"Intel","NVIDIA"},liveSettings=true,upscaling=true})));

        }catch {foreach(var item in originals){var target=Path.Combine(settings.GameDirectory,item.Key);if(item.Value!=null)AtomicWrite(target,item.Value);else if(File.Exists(target))File.Delete(target);}throw;}

        return backup;

    }

    public string RestoreBeforeInstall(string gameDirectory) {
        EnsureClosed();var root=Path.Combine(DataRoot,"backups");if(!Directory.Exists(root))throw new InvalidOperationException("No pre-install backup is available.");
        string? selected=null;InstallRestoreRecord? record=null;foreach(var folder in Directory.EnumerateDirectories(root).OrderByDescending(x=>x)){var path=Path.Combine(folder,"install-record.json");if(!File.Exists(path))continue;try{var candidate=JsonSerializer.Deserialize<InstallRestoreRecord>(File.ReadAllText(path));if(candidate!=null&&Path.GetFullPath(candidate.GameDirectory).Equals(Path.GetFullPath(gameDirectory),StringComparison.OrdinalIgnoreCase)){selected=folder;record=candidate;break;}}catch{}}
        if(selected==null||record==null)throw new InvalidOperationException("No matching pre-install backup was found for this GTA folder. Install the mod once with this app before using restore.");
        var safety=CreateBackup("pre-restore");foreach(var name in record.OriginalFiles.Keys.Append("GTAXeSS-AppBridge.json").Append("GTAXeSS.ini")){var current=Path.Combine(gameDirectory,name);if(File.Exists(current))File.Copy(current,Path.Combine(safety,name),true);}if(File.Exists(record.SettingsPath))File.Copy(record.SettingsPath,Path.Combine(safety,"settings.xml"),true);
        foreach(var item in record.OriginalFiles){var target=Path.Combine(gameDirectory,item.Key);var source=Path.Combine(selected,item.Key);if(item.Value){if(!File.Exists(source))throw new InvalidOperationException("Backup is incomplete: "+item.Key);AtomicWrite(target,File.ReadAllBytes(source));}else if(File.Exists(target))File.Delete(target);}
        void RestoreOptional(string name,bool existed){var target=Path.Combine(gameDirectory,name);var source=Path.Combine(selected,name);if(existed){if(!File.Exists(source))throw new InvalidOperationException("Backup is incomplete: "+name);AtomicWrite(target,File.ReadAllBytes(source));}else if(File.Exists(target))File.Delete(target);}
        RestoreOptional("GTAXeSS-AppBridge.json",record.BridgeExisted);RestoreOptional("GTAXeSS.ini",record.IniExisted);var xml=Path.Combine(selected,"settings.xml");if(File.Exists(xml))AtomicWrite(record.SettingsPath,File.ReadAllBytes(xml));
        var visualMarker=Path.Combine(gameDirectory,"Zink-VisualV.json");
        if(File.Exists(visualMarker)){
            foreach(var name in new[]{"Zink-VisualV.json","fxReloader.ini"}){var source=Path.Combine(gameDirectory,name);if(File.Exists(source))File.Copy(source,Path.Combine(safety,name),true);}
            var visualFolder=Path.Combine(gameDirectory,"fxReloader");
            if(Directory.Exists(visualFolder)){var saved=Path.Combine(safety,"fxReloader");foreach(var source in Directory.EnumerateFiles(visualFolder,"*",SearchOption.AllDirectories)){var relative=Path.GetRelativePath(visualFolder,source);var destination=Path.Combine(saved,relative);Directory.CreateDirectory(Path.GetDirectoryName(destination)!);File.Copy(source,destination,true);}Directory.Delete(visualFolder,true);}
            File.Delete(visualMarker);var visualConfig=Path.Combine(gameDirectory,"fxReloader.ini");if(File.Exists(visualConfig))File.Delete(visualConfig);
        }
        return safety;
    }

    public void SaveProfile(string name,ModSettings settings) {ValidateProfileName(name);Directory.CreateDirectory(Path.Combine(DataRoot,"profiles"));AtomicWrite(Path.Combine(DataRoot,"profiles",name+".json"),Encoding.UTF8.GetBytes(JsonSerializer.Serialize(settings,new JsonSerializerOptions{WriteIndented=true})));}

    public ModSettings LoadProfile(string name) {ValidateProfileName(name);return JsonSerializer.Deserialize<ModSettings>(File.ReadAllText(Path.Combine(DataRoot,"profiles",name+".json")))??throw new InvalidOperationException("Invalid profile.");}

    public IEnumerable<string> Profiles()=>Directory.Exists(Path.Combine(DataRoot,"profiles"))?Directory.EnumerateFiles(Path.Combine(DataRoot,"profiles"),"*.json").Select(Path.GetFileNameWithoutExtension).OfType<string>():Array.Empty<string>();

    static void ValidateProfileName(string name) {if(string.IsNullOrWhiteSpace(name)||name.Length>60||!Regex.IsMatch(name,@"^[\p{L}\p{N} _-]+$")||new[]{"CON","PRN","AUX","NUL","COM1","LPT1"}.Contains(name,StringComparer.OrdinalIgnoreCase))throw new InvalidOperationException("Use a profile name with letters, numbers, spaces, - or _.");}

    string CreateBackup(string kind){string folder=Path.Combine(DataRoot,"backups",DateTime.Now.ToString("yyyyMMdd-HHmmss-fff")+"-"+kind+"-"+Guid.NewGuid().ToString("N")[..6]);Directory.CreateDirectory(folder);return folder;}

    void EnsureClosed(){if(gameRunning())throw new InvalidOperationException("Close GTA before applying settings or updating the mod.");}

    static void AtomicWrite(string path,byte[] bytes){
        var fullPath=Path.GetFullPath(path);Directory.CreateDirectory(Path.GetDirectoryName(fullPath)!);string temp=fullPath+".zink-"+Guid.NewGuid().ToString("N")+".tmp";
        try{File.WriteAllBytes(temp,bytes);File.Move(temp,fullPath,true);}
        catch(UnauthorizedAccessException ex){throw new UnauthorizedAccessException($"Windows denied write access to '{fullPath}'. Close GTA if it is shutting down, then reopen Zink and accept the administrator prompt.",ex);}
        finally{try{if(File.Exists(temp))File.Delete(temp);}catch(UnauthorizedAccessException){}}
    }

    public static Dictionary<string,double> ReadTelemetry(string gameDirectory) {

        string path=Path.Combine(gameDirectory,"GTAXeSS-Performance.log");if(!IsGameRunning()||!File.Exists(path)||DateTime.Now-File.GetLastWriteTime(path)>TimeSpan.FromSeconds(5))return new();

        using var stream=new FileStream(path,FileMode.Open,FileAccess.Read,FileShare.ReadWrite|FileShare.Delete);stream.Seek(Math.Max(0,stream.Length-8192),SeekOrigin.Begin);using var reader=new StreamReader(stream);var last=reader.ReadToEnd().Split('\n',StringSplitOptions.RemoveEmptyEntries).LastOrDefault()??"";

        var result=new Dictionary<string,double>();foreach(Match match in Regex.Matches(last,@"\b(SourceFPS|XeFGFPS|CPU|RenderThreadCPU|GPU)=([0-9.]+)"))if(double.TryParse(match.Groups[2].Value,NumberStyles.Float,CultureInfo.InvariantCulture,out var val))result[match.Groups[1].Value]=val;return result;

    }

    public static RuntimeStatus ReadRuntimeStatus(string gameDirectory) {
        var status=new RuntimeStatus();
        var iniPath=Path.Combine(gameDirectory,"GTAXeSS.ini");
        if(!File.Exists(iniPath)){status.State="Not installed";status.Detail="Install or update the mod to read live runtime information.";return status;}
        var ini=new IniDocument(File.ReadAllText(iniPath));
        status.Provider=ini.Get("Graphics","Provider","Intel");
        var quality=ini.Get("Upscaling","Quality","NATIVE").Replace('_',' ');
        var running=IsGameRunning();
        var telemetry=ReadTelemetry(gameDirectory);
        status.IsLive=running&&telemetry.Count>0;
        status.State=!running?"GTA is closed":status.IsLive?"Live · verified":"Starting · waiting for frames";

        static string VersionOf(string path){try{var v=FileVersionInfo.GetVersionInfo(path);return v.FileVersion??v.ProductVersion??"unknown";}catch{return "unknown";}}
        static string Tail(string path){try{using var s=new FileStream(path,FileMode.Open,FileAccess.Read,FileShare.ReadWrite|FileShare.Delete);s.Seek(Math.Max(0,s.Length-65536),SeekOrigin.Begin);using var r=new StreamReader(s);return r.ReadToEnd();}catch{return "";}}
        string mainLog=Tail(Path.Combine(gameDirectory,"GTAXeSS.log"));
        if(string.IsNullOrEmpty(mainLog)){var candidate=Directory.EnumerateFiles(gameDirectory,"GTAXeSS*.log").Where(p=>!p.EndsWith("Performance.log",StringComparison.OrdinalIgnoreCase)&&!p.EndsWith("NVIDIA.log",StringComparison.OrdinalIgnoreCase)&&!p.EndsWith("DLSS5.log",StringComparison.OrdinalIgnoreCase)).OrderByDescending(File.GetLastWriteTimeUtc).FirstOrDefault();if(candidate!=null)mainLog=Tail(candidate);}

        if(status.Provider.Equals("NVIDIA",StringComparison.OrdinalIgnoreCase)){
            var selected=ini.Get("NVIDIA","DLSSVersion",ini.Get("NVIDIA","DLSS5","0")=="1"?"5":"4.5");
            var nvidiaLog=Tail(Path.Combine(gameDirectory,"GTAXeSS-NVIDIA.log"));
            var nrLog=Tail(Path.Combine(gameDirectory,"GTAXeSS-DLSS5.log"));
            var preset=Regex.Matches(nvidiaLog,@"DLSS SR preset=(\d+)").Cast<Match>().LastOrDefault()?.Groups[1].Value;
            var presetName=preset switch{"11"=>"K","13"=>quality=="ULTRA PERFORMANCE"?"L":"M",_=>"driver-selected"};
            bool nrEvaluated=Regex.IsMatch(nrLog,@"(?m)^EVALUATED result=0x1 frames=[1-9]");
            var runtimeMatch=Regex.Matches(nrLog,@"RUNTIME file=([^\s]+)").Cast<Match>().LastOrDefault();
            var srVersion=VersionOf(Path.Combine(gameDirectory,"nvngx_dlss.dll"));
            status.Upscaler=$"DLSS {selected} · {quality} · preset {presetName}";
            status.UpscalerRuntime=$"DLSS SR runtime {srVersion}";
            status.Neural=selected=="5"?(nrEvaluated?"RTX 40 experimental NR path · evaluating (not official DLSS 5 support)":runtimeMatch!=null?$"Experimental neural runtime loaded · {runtimeMatch.Groups[1].Value}":"Neural Rendering · waiting or safe fallback"):"Neural Rendering off";
            status.NeuralIsActive=nrEvaluated&&status.IsLive;
            status.FrameGenerationRuntime=$"Streamline DLSS-G {VersionOf(Path.Combine(gameDirectory,"sl.dlss_g.dll"))}";
        }else{
            status.Provider="Intel XeSS";
            status.Upscaler=$"XeSS · {quality}";
            status.UpscalerRuntime=$"XeSS runtime {VersionOf(Path.Combine(gameDirectory,"libxess.dll"))}";
            bool directMl=ini.Get("Intel ARC","DLSS5DirectML","0")=="1";
            var optiLog=Tail(Path.Combine(gameDirectory,"OptiScaler.log"));
            bool vendorNeutral=directMl&&Regex.IsMatch(optiLog,@"vendor-neutral|DirectML",RegexOptions.IgnoreCase);
            bool nrRunning=vendorNeutral&&Regex.IsMatch(optiLog,@"DLSS.?NR|Neural Rendering|Native input",RegexOptions.IgnoreCase);
            status.Neural=directMl?(nrRunning?"DLSS 5 Neural Rendering · DirectML vendor-neutral backend active":"DLSS 5 DirectML configured · waiting for verified model frames"):"Intel XeSS reconstruction";
            status.NeuralIsActive=directMl?nrRunning&&status.IsLive:status.IsLive;
            status.FrameGenerationRuntime=$"XeSS-FG runtime {VersionOf(Path.Combine(gameDirectory,"libxess_fg.dll"))}";
        }

        bool fg=ini.Get("FrameGeneration","Enabled","0")=="1";
        int requested=status.Provider.StartsWith("NVIDIA",StringComparison.OrdinalIgnoreCase)&&int.TryParse(ini.Get("NVIDIA","Multiplier","2"),out var nvMultiplier)?nvMultiplier:4;
        int actual=0;
        var frameMatch=Regex.Matches(mainLog,@"frames_presented=(\d+)[^\r\n]*frame_gen_result=0").Cast<Match>().LastOrDefault();
        if(frameMatch!=null)int.TryParse(frameMatch.Groups[1].Value,out actual);
        if(actual<1&&telemetry.TryGetValue("SourceFPS",out var source)&&source>1&&telemetry.TryGetValue("XeFGFPS",out var output))actual=Math.Clamp((int)Math.Round(output/source),1,6);
        status.FrameGeneration=!fg?"Off":status.IsLive&&actual>1?$"Active · {actual}× ({actual-1} generated per source frame)":status.IsLive?"Enabled · source frames currently passing":"Enabled · waiting for live frames";
        status.FrameGenerationIsActive=fg&&status.IsLive&&actual>1;
        status.Detail=status.IsLive?$"Observed from live telemetry and runtime logs · requested {requested}×":running?"GTA is running, but the mod has not produced fresh telemetry yet.":"Launch GTA to replace configured values with verified live state.";
        return status;
    }

}

public sealed class RuntimeStatus {
    public bool IsLive{get;set;} public bool NeuralIsActive{get;set;} public bool FrameGenerationIsActive{get;set;}
    public string State{get;set;}="Unavailable"; public string Provider{get;set;}="—"; public string Upscaler{get;set;}="—"; public string UpscalerRuntime{get;set;}="—"; public string Neural{get;set;}="—"; public string FrameGeneration{get;set;}="—"; public string FrameGenerationRuntime{get;set;}="—"; public string Detail{get;set;}="";
}

public sealed class BundleManifest {public int Protocol{get;set;} public string Build{get;set;}="";public Dictionary<string,string> Files{get;set;}=new();}
public sealed class InstallRestoreRecord {public string GameDirectory{get;set;}="";public string SettingsPath{get;set;}="";public Dictionary<string,bool> OriginalFiles{get;set;}=new();public bool BridgeExisted{get;set;}public bool IniExisted{get;set;}}










