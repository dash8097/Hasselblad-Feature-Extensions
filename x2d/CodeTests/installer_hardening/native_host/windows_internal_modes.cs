using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;

// 同一个 EXE 的固定内部模式；后台进程与前台隔离，依然只接受原有操作协议。
internal static class WindowsInternalModes {
    [DllImport("X2dEngine.dll", CharSet=CharSet.Unicode, CallingConvention=CallingConvention.Cdecl)]
    static extern int x2d_engine_run(string root);

    internal static bool Dispatch(string[] args) {
        if(args.Length==0) return false;
        Console.SetOut(new StreamWriter(Console.OpenStandardOutput(),new UTF8Encoding(false)){AutoFlush=true});
        Console.SetError(new StreamWriter(Console.OpenStandardError(),new UTF8Encoding(false)){AutoFlush=true});
        int result=2;
        try {
            string root=AppDomain.CurrentDomain.BaseDirectory.TrimEnd(Path.DirectorySeparatorChar);
            if(args.Length==1 && args[0]=="--host-check")
                result=WindowsPreflight.Run(new string[0]);
            else if(args.Length==2 && args[0]=="--engine" &&
                    String.Equals(Path.GetFullPath(args[1]).TrimEnd(Path.DirectorySeparatorChar),
                                  root,StringComparison.OrdinalIgnoreCase)) {
                PatchDiagnosticReport.CheckPackage(root);
                result=x2d_engine_run(root);
            } else if(args.Length==1 && args[0]=="--verify-package") {
                PatchDiagnosticReport.CheckPackage(root);
                Console.WriteLine("PACKAGE_PREFLIGHT_OK");
                result=0;
            }
        } catch(Exception ex) {
            Console.Error.WriteLine(PatchDiagnosticReport.Protect(ex.Message));
            result=2;
        }
        Environment.Exit(result);
        return true;
    }
}
