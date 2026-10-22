using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Management;
using System.Text;
using System.Text.RegularExpressions;
using System.Web.Script.Serialization;
using Microsoft.Win32;

// 只读主机元数据；不打开 USB 设备、不启动 Phocus、不安装驱动。
internal static class WindowsPreflight {
    internal sealed class Snapshot {
        public string[] Versions;
        public bool Running;
        public int DriverPackages;
        public bool WinUsbAvailable;
        public int BindingIssues;
    }
    internal static bool TestInf(string text) {
        return Regex.IsMatch(text,@"(?im)^\s*[^;\[\]\r\n]+=[^\r\n,]+,\s*USB\\VID_2756&PID_0009&MI_03\s*$") &&
            Regex.IsMatch(text,@"(?im)^\s*AddService\s*=\s*WinUSB\s*,") &&
            Regex.IsMatch(text,@"(?im)^\s*HKR\s*,[^\r\n]*DeviceInterfaceGUIDs[^\r\n]*\{CC135687-5267-4313-B0C7-C344609D6EF0\}");
    }
    internal static string BlockReason(Snapshot s) {
        if(s.Running) return "请先关闭 Phocus（包括 Phocus64、启动器），再重新检查或安装。";
        if(s.Versions.Length==0) return "未找到已安装的 Phocus 主程序和有效版本。请先安装官方 Phocus，再重新检查。";
        if(s.DriverPackages==0 || !s.WinUsbAvailable) return "未找到支持 X2D 控制接口的已安装 Phocus USB 驱动。请通过官方 Phocus 安装程序安装或修复驱动。";
        if(s.BindingIssues>0) return "当前 X2D 控制接口的驱动绑定或设备状态异常。请修复官方 USB 驱动后重新检查。";
        return "";
    }
    static void Add(HashSet<string> items,object value) {
        var s=value as string;if(!String.IsNullOrWhiteSpace(s)) items.Add(s);
    }
    static Snapshot Read() {
        var locations=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        var executables=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach(var name in new[]{"ProgramFiles","ProgramFiles(x86)"}) {
            string folder=Environment.GetEnvironmentVariable(name);
            if(!String.IsNullOrEmpty(folder)) locations.Add(Path.Combine(folder,"Hasselblad"));
        }
        foreach(var hive in new[]{RegistryHive.LocalMachine,RegistryHive.CurrentUser})
        foreach(var view in new[]{RegistryView.Registry64,RegistryView.Registry32}) {
            using(var root=RegistryKey.OpenBaseKey(hive,view)) {
                using(var key=root.OpenSubKey(@"SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall")) {
                    if(key!=null) foreach(string sub in key.GetSubKeyNames()) using(var app=key.OpenSubKey(sub)) {
                        if(app!=null && Regex.IsMatch(Convert.ToString(app.GetValue("DisplayName")),@"^Phocus(?:\s|$)",RegexOptions.IgnoreCase))
                            Add(locations,app.GetValue("InstallLocation"));
                    }
                }
                foreach(var exe in new[]{"Phocus.exe","Phocus64.exe"}) using(var key=root.OpenSubKey(@"SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\"+exe)) {
                    if(key!=null) Add(executables,key.GetValue(""));
                }
            }
        }
        foreach(var folder in locations) {
            if(!Directory.Exists(folder)) continue;
            var dirs=new List<string>{folder};
            foreach(var dir in Directory.GetDirectories(folder)) if(Regex.IsMatch(Path.GetFileName(dir),@"^Phocus(?:\s|$)",RegexOptions.IgnoreCase)) dirs.Add(dir);
            foreach(var dir in dirs) foreach(var exe in new[]{"Phocus.exe","Phocus64.exe"}) executables.Add(Path.Combine(dir,exe));
        }
        var versions=new SortedSet<string>();
        foreach(var exe in executables) {
            if(!File.Exists(exe)) continue;
            var info=FileVersionInfo.GetVersionInfo(exe);
            if(info.ProductName=="Phocus" && Regex.IsMatch(info.CompanyName??"","Hasselblad",RegexOptions.IgnoreCase) &&
               Regex.IsMatch(info.ProductVersion??"",@"^\d+(?:\.\d+){1,3}$")) versions.Add(info.ProductVersion);
        }
        bool running=false;
        foreach(var process in Process.GetProcesses()) using(process) {
            if(Regex.IsMatch(process.ProcessName,@"^(Phocus|Phocus64|PhocusLoader|PhocusRelaunch)$",RegexOptions.IgnoreCase)) running=true;
        }
        string windows=Environment.GetFolderPath(Environment.SpecialFolder.Windows);
        int packages=0,issues=0;
        foreach(var inf in Directory.GetFiles(Path.Combine(windows,"INF"),"oem*.inf"))
            using(var reader=new StreamReader(inf,true)) if(TestInf(reader.ReadToEnd())) packages++;
        // 实例标识只在内存过滤；不输出可能含序列号的路径。
        using(var search=new ManagementObjectSearcher("SELECT Present,PNPDeviceID,Service,ConfigManagerErrorCode FROM Win32_PnPEntity"))
        using(var results=search.Get()) foreach(ManagementObject entry in results) using(entry) {
            if(Convert.ToBoolean(entry["Present"]) && Convert.ToString(entry["PNPDeviceID"]).StartsWith(@"USB\VID_2756&PID_0009&MI_03\",StringComparison.OrdinalIgnoreCase) &&
               (!String.Equals(Convert.ToString(entry["Service"]),"WinUSB",StringComparison.OrdinalIgnoreCase) || Convert.ToInt32(entry["ConfigManagerErrorCode"])!=0)) issues++;
        }
        return new Snapshot{Versions=new List<string>(versions).ToArray(),Running=running,DriverPackages=packages,BindingIssues=issues,
            WinUsbAvailable=File.Exists(Path.Combine(windows,@"System32\drivers\WinUSB.sys"))};
    }
#if !INTERNAL_PREFLIGHT_TESTS
    static int Main(string[] args) {
        Console.OutputEncoding=new UTF8Encoding(false);
        if(args.Length!=0) return 2;
        try {
            Snapshot s=Read();string reason=BlockReason(s);
            Console.WriteLine(new JavaScriptSerializer().Serialize(new {snapshot=s,error=reason}));return reason.Length==0?0:1;
        } catch {
            Console.WriteLine("{\"error\":\"主机软件或驱动元数据读取失败；未打开 USB。\"}");return 1;
        }
    }
#endif
}
