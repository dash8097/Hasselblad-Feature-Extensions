using System;
using System.Collections.Generic;
using System.Text;
using System.Text.RegularExpressions;
using System.Web.Script.Serialization;

internal static class PatchDiagnosticReport {
    internal static readonly string[] RuntimeFiles = "HasselbladTuningPatchInstaller.exe|X2dEngine.exe|X2dHostCheck.exe|X2dTransport.dll|python311.dll|runtime/python311.zip|package-release.json|feature-catalog.json|diagnostic-commands.json|engine/mac_usb.pyd|engine/mac_diagnostics.pyd|engine/package_feature_selection.pyd|engine/mac_patch.pyd|engine/windows_engine.pyd".Split('|');
    static void InventoryDirectory(string root,string directory,HashSet<string> files) {
        if((System.IO.File.GetAttributes(directory)&System.IO.FileAttributes.ReparsePoint)!=0)
            throw new InvalidOperationException("安装目录包含链接，请完整解压到普通目录。");
        foreach(string path in System.IO.Directory.GetFiles(directory)) {
            if((System.IO.File.GetAttributes(path)&System.IO.FileAttributes.ReparsePoint)!=0)
                throw new InvalidOperationException("安装文件包含链接。");
            string relative=path.Substring(root.Length+1).Replace('\\','/');
            if(!files.Add(relative)) throw new InvalidOperationException("安装文件名称冲突。");
        }
        foreach(string child in System.IO.Directory.GetDirectories(directory)) InventoryDirectory(root,child,files);
    }
    internal static void CheckPackage(string root) {
        root=System.IO.Path.GetFullPath(root).TrimEnd(System.IO.Path.DirectorySeparatorChar);
        var actual=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        InventoryDirectory(root,root,actual);
        Dictionary<string,string> manifest;
        try { manifest=new JavaScriptSerializer{MaxJsonLength=4194304}.Deserialize<Dictionary<string,string>>(
                  System.IO.File.ReadAllText(System.IO.Path.Combine(root,"package-integrity.json"),Encoding.UTF8)); }
        catch { throw new InvalidOperationException("完整性清单无法读取，请重新解压。"); }
        if(manifest==null || manifest.Count==0 || manifest.Count>10000) throw new InvalidOperationException("安装清单无效。");
        foreach(string name in RuntimeFiles) if(!manifest.ContainsKey(name)) throw new InvalidOperationException("安装清单缺少必需文件："+name);
        var registered=new HashSet<string>(StringComparer.OrdinalIgnoreCase){"package-integrity.json"};
        foreach(var item in manifest) {
            string name=item.Key;
            if(String.IsNullOrEmpty(name) || System.IO.Path.IsPathRooted(name) || name.IndexOf('\\')>=0 || name.IndexOf(':')>=0 ||
               Array.Exists(name.Split('/'),part=>part.Length==0 || part=="." || part=="..") || !registered.Add(name)) throw new InvalidOperationException("安装清单路径无效。");
            if(item.Value==null || !Regex.IsMatch(item.Value,"\\A[0-9a-f]{64}\\z") || !actual.Contains(name))
                throw new InvalidOperationException("安装文件缺失或摘要无效："+name);
            using(var stream=System.IO.File.OpenRead(System.IO.Path.Combine(root,name)))
            using(var sha=System.Security.Cryptography.SHA256.Create()) {
                string hash=BitConverter.ToString(sha.ComputeHash(stream)).Replace("-","").ToLowerInvariant();
                if(hash!=item.Value)throw new InvalidOperationException("安装文件损坏或版本混用："+name);
            }
        }
        if(!actual.SetEquals(registered)) throw new InvalidOperationException("安装目录混入未登记文件，请重新完整解压。");
    }

    internal static string Protect(string text, int limit = 8192) {
        if (String.IsNullOrEmpty(text)) return "";
        text = Regex.Replace(text, @"[A-Z]:\\Users\\[^\\\s]+", "<用户目录>", RegexOptions.IgnoreCase);
        text = Regex.Replace(text, @"\\\\\?\\usb#[^\s]+", "<USB设备路径已隐去>", RegexOptions.IgnoreCase);
        text = Regex.Replace(text, @"((?:serial(?:number)?|serial_number|device_serial)\s*[:=]\s*)[^\s,;]+", "$1<已隐去>", RegexOptions.IgnoreCase);
        text = Regex.Replace(text, @"/ssd/[^\s""']+", "<照片路径已隐去>", RegexOptions.IgnoreCase);
        text = Regex.Replace(text, @"\bB\d{7,}\.(?:jpg|jpeg|heif|heic|3fr|raw)\b", "<照片名已隐去>", RegexOptions.IgnoreCase);
        text = Regex.Replace(text, @"printf '%s' '[A-Za-z0-9+/=]+'", "传输载荷分块（数据省略）");
        return text.Length > limit ? text.Substring(0,limit) + "\n[诊断内容已截断]" : text;
    }
    internal static Dictionary<string, object> Parse(string line, string prefix) {
        if (!line.StartsWith(prefix, StringComparison.Ordinal) || line.Length > 262144) return null;
        try {
            var json = Encoding.UTF8.GetString(Convert.FromBase64String(line.Substring(prefix.Length)));
            var value = new JavaScriptSerializer { MaxJsonLength = 196608 }.Deserialize<Dictionary<string, object>>(json);
            if (value == null || Get(value,"protocol") != "1") return null;
            return value;
        } catch { return null; }
    }
    internal static string Get(Dictionary<string, object> value, string key) {
        object item;
        return value != null && value.TryGetValue(key, out item) && item != null ? Protect(Convert.ToString(item), key == "camera_log" ? 98304 : 8192) : "未取得";
    }
    internal static string Format(Dictionary<string, object> report, string primary, int hostExit) {
        var text = new StringBuilder();
        if (!String.IsNullOrEmpty(primary)) text.AppendLine("先前操作首次失败（保留）\n" + primary + "\n");
        text.AppendLine("操作："+Get(report,"action")+"    流程："+Get(report,"route"));
        text.AppendLine("阶段："+Get(report,"stage"));
        if(report.ContainsKey("first_failure")) text.AppendLine("首次失败（恢复前）：\n"+Get(report,"first_failure"));
        text.AppendLine("原始错误 / 结果："+Get(report,"reason"));
        text.AppendLine("电脑退出码："+hostExit+"    相机退出码："+Get(report,"camera_exit"));
        text.AppendLine("USB："+Get(report,"transport"));
        text.AppendLine("目标："+Get(report,"target"));
        text.AppendLine("命令："+Get(report,"command"));
        text.AppendLine("期望："+Get(report,"expected"));
        text.AppendLine("实际："+Get(report,"actual"));
        text.AppendLine("运行状态："+Get(report,"state"));
        text.AppendLine("系统分区："+Get(report,"readonly"));
        text.AppendLine("恢复状态："+Get(report,"recovery"));
        text.AppendLine("时间："+Get(report,"time"));
        text.AppendLine("\n相机安装日志：\n"+Get(report,"camera_log"));
        text.AppendLine("\nUSB 中断或缺少日志时，只表示证据未取得；不会自动重连、重复安装或推定恢复成功。");
        return text.ToString();
    }
    internal static void SelfTest() {
        var d = new Dictionary<string, object> { {"protocol",1}, {"stage","Stage"}, {"reason","USB timeout 121"}, {"camera_exit","未取得"} };
        var encoded="PATCH_DIAGNOSTIC="+Convert.ToBase64String(Encoding.UTF8.GetBytes(new JavaScriptSerializer().Serialize(d)));
        if (Parse(encoded,"PATCH_DIAGNOSTIC=") == null || Parse("PATCH_DIAGNOSTIC=bad","PATCH_DIAGNOSTIC=") != null) throw new Exception("诊断协议检查失败");
        var text=Format(d,"首次失败：指纹不匹配",1);
        if (!text.Contains("指纹不匹配") || !text.Contains("USB timeout 121") || !text.Contains("相机退出码：未取得")) throw new Exception("首次失败或未知状态丢失");
        text=Protect(@"C:\Users\fixture-user\a.txt serial=fixture-id /ssd/122HASBL/B0000001.JPG printf '%s' 'YWJjZA=='");
        if (text.Contains("fixture-user") || text.Contains("fixture-id") || text.Contains("B0000001") || text.Contains("YWJjZA")) throw new Exception("脱敏失败");
        if (Parse("PATCH_DIAGNOSTIC="+new string('A',262144),"PATCH_DIAGNOSTIC=") != null) throw new Exception("超长报告未拒绝");
        d["camera_log"] = new string('R',70000)+"\nRESTORE_FAILURE_TAIL serial=private-id";
        encoded="PATCH_DIAGNOSTIC="+Convert.ToBase64String(Encoding.UTF8.GetBytes(new JavaScriptSerializer().Serialize(d)));
        var longReport=Parse(encoded,"PATCH_DIAGNOSTIC=");
        if (longReport == null || !Format(longReport,"",0).Contains("RESTORE_FAILURE_TAIL") || Format(longReport,"",0).Contains("private-id")) throw new Exception("长诊断丢失收尾原因或脱敏失败");
    }
}
