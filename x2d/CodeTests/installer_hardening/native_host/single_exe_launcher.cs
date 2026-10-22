// 单文件交付入口：展开内嵌完整包后运行正式界面。这里不访问相机。
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Reflection;
using System.Security.Cryptography;
using System.Windows.Forms;

internal static class SingleInstaller {
    const string PayloadHash = "__PAYLOAD_SHA256__";
    const string PackageName = "__PACKAGE_NAME__";
    const int FileCount = __FILE_COUNT__;
    const string Prefix = "x2d-installer-";

    static bool OrdinaryTree(string directory) {
        if((File.GetAttributes(directory)&FileAttributes.ReparsePoint)!=0) return false;
        foreach(string file in Directory.GetFiles(directory))
            if((File.GetAttributes(file)&FileAttributes.ReparsePoint)!=0) return false;
        foreach(string child in Directory.GetDirectories(directory))
            if(!OrdinaryTree(child)) return false;
        return true;
    }

    static void Cleanup(string directory,string parent) {
        if(String.IsNullOrEmpty(directory) || !Directory.Exists(directory)) return;
        string root=Path.GetFullPath(directory).TrimEnd(Path.DirectorySeparatorChar);
        string owner=Path.GetFullPath(parent).TrimEnd(Path.DirectorySeparatorChar);
        Guid id;
        string name=Path.GetFileName(root);
        if(!String.Equals(Path.GetDirectoryName(root),owner,StringComparison.OrdinalIgnoreCase) ||
           !name.StartsWith(Prefix,StringComparison.Ordinal) ||
           !Guid.TryParseExact(name.Substring(Prefix.Length),"N",out id) ||
           !OrdinaryTree(root)) return;
        // 只删除本次创建、已核对绝对父目录和链接状态的会话目录。
        Directory.Delete(root,true);
    }

    static string Extract(string root) {
        root=Path.GetFullPath(root);
        if(!Directory.Exists(root) || Directory.GetFileSystemEntries(root).Length!=0 ||
           (File.GetAttributes(root)&FileAttributes.ReparsePoint)!=0)
            throw new InvalidOperationException("临时安装目录不可用。");
        using(var stream=Assembly.GetExecutingAssembly().GetManifestResourceStream("X2D.Package")) {
            if(stream==null) throw new InvalidOperationException("安装器内置文件缺失，请重新下载。");
            // 单文件载体传输完整性；不检查/锁定用户的 TXT 文件。
            using(var sha=SHA256.Create()) {
                string hash=BitConverter.ToString(sha.ComputeHash(stream)).Replace("-","").ToLowerInvariant();
                if(hash!=PayloadHash) throw new InvalidOperationException("安装器下载不完整，请重新下载。");
            }
            stream.Position=0;
            using(var zip=new ZipArchive(stream,ZipArchiveMode.Read,false)) {
                if(zip.Entries.Count!=FileCount) throw new InvalidOperationException("内置安装文件数量不符。");
                long total=0;
                var paths=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
                foreach(var entry in zip.Entries) {
                    string name=entry.FullName;
                    if(!name.StartsWith(PackageName+"/",StringComparison.Ordinal) ||
                       name.IndexOf('\\')>=0 || name.IndexOf(':')>=0 ||
                       Array.Exists(name.Split('/'),p=>p.Length==0 || p=="." || p=="..") ||
                       !paths.Add(name) || entry.Length>256L*1024*1024)
                        throw new InvalidOperationException("内置安装路径无效。");
                    total+=entry.Length;
                    if(total>512L*1024*1024) throw new InvalidOperationException("内置安装文件大小无效。");
                    string path=Path.GetFullPath(Path.Combine(root,name));
                    if(!path.StartsWith(root+Path.DirectorySeparatorChar,StringComparison.OrdinalIgnoreCase))
                        throw new InvalidOperationException("内置安装路径越界。");
                    Directory.CreateDirectory(Path.GetDirectoryName(path));
                    using(var input=entry.Open())
                    using(var output=new FileStream(path,FileMode.CreateNew,FileAccess.Write,FileShare.None))
                        input.CopyTo(output);
                }
            }
        }
        string gui=Path.Combine(root,PackageName,"HasselbladTuningPatchInstaller.exe");
        if(!File.Exists(gui)) throw new InvalidOperationException("安装界面缺失。");
        return gui;
    }

    [STAThread] static int Main() {
        string parent=Path.GetFullPath(Path.GetTempPath());
        string root=null;
        bool canClean=true;
        try {
            root=Path.Combine(parent,Prefix+Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(root);
            string gui=Extract(root);
            // 前台窗口由正式 GUI 显示；辅助进程保持不可见。
            using(var child=Process.Start(new ProcessStartInfo(gui) {
                UseShellExecute=false,WorkingDirectory=Path.GetDirectoryName(gui)
            })) {
                child.WaitForExit();
                canClean=child.ExitCode==0;
                return child.ExitCode;
            }
        } catch(Exception) {
            MessageBox.Show("安装器无法展开或运行。请确认下载完整、临时目录有足够空间。安装结果未确认，请保留当前现场。",
                            "哈苏特调补丁",MessageBoxButtons.OK,MessageBoxIcon.Error);
            return 1;
        } finally {
            // GUI 异常退出时保留现场，避免删除仍被其工作进程使用的文件。
            if(canClean) { try { Cleanup(root,parent); } catch {} }
        }
    }
}
