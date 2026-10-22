// Thin stable ABI adapter; contains no USB implementation or camera payload.
using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Web.Script.Serialization;
public sealed class X2dNativeFailure : Exception {
    public string Code;
    public int Win32;
    public X2dNativeFailure(string code,int error) : base(code) { Code=code;Win32=error; }
}
public static class X2dNativeBridge {
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    static extern IntPtr LoadLibraryEx(string path, IntPtr reserved, uint flags);
    [DllImport("kernel32.dll", CharSet=CharSet.Ansi, SetLastError=true)]
    static extern IntPtr GetProcAddress(IntPtr module, string name);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate IntPtr CallFn(byte[] input, int length);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate void FreeFn(IntPtr buffer);
    static CallFn call; static FreeFn free; static string loaded;
    public static void Configure(string path) {
        path=Path.GetFullPath(path);
        if(call!=null) { if(loaded!=path)throw new InvalidOperationException("NATIVE_ABI_ALREADY_LOADED"); return; }
        // Dependencies are resolved only beside the DLL and in System32.
        IntPtr module=LoadLibraryEx(path,IntPtr.Zero,0x100|0x800);
        if(module==IntPtr.Zero)throw new InvalidOperationException("NATIVE_LOAD_WIN32_"+Marshal.GetLastWin32Error());
        IntPtr c=GetProcAddress(module,"x2d_call"), f=GetProcAddress(module,"x2d_free");
        if(c==IntPtr.Zero||f==IntPtr.Zero)throw new InvalidOperationException("NATIVE_ABI_MISSING");
        call=(CallFn)Marshal.GetDelegateForFunctionPointer(c,typeof(CallFn));
        free=(FreeFn)Marshal.GetDelegateForFunctionPointer(f,typeof(FreeFn)); loaded=path;
    }
    public static string[] Invoke(string operation,string[] commands) {
        if(call==null)throw new InvalidOperationException("NATIVE_ABI_NOT_LOADED");
        var json=new JavaScriptSerializer();
        byte[] input=Encoding.UTF8.GetBytes(json.Serialize(new Dictionary<string,object>{{"operation",operation},{"commands",commands}}));
        IntPtr p=call(input,input.Length);
        if(p==IntPtr.Zero)throw new InvalidOperationException("NATIVE_ABI_EMPTY");
        try {
            return Decode(p);
        } finally { free(p); }
    }
    static string[] Decode(IntPtr p) {
            const int limit=32*1024*1024; // 512 replies * 8192 ASCII chars * worst-case 6-byte JSON escaping.
            int n=0;while(n<limit && Marshal.ReadByte(p,n)!=0)n++;
            if(n==limit)throw new InvalidOperationException("NATIVE_REPLY_LIMIT");
            byte[] bytes=new byte[n];Marshal.Copy(p,bytes,0,n);
            var json=new JavaScriptSerializer { MaxJsonLength=limit };
            var result=json.Deserialize<Dictionary<string,object>>(Encoding.UTF8.GetString(bytes));
            string error=(string)result["error"];
            if(error.Length!=0)throw new X2dNativeFailure(error,Convert.ToInt32(result["win32"]));
            var values=(ArrayList)result["values"];var output=new string[values.Count];
            for(int i=0;i<output.Length;i++)output[i]=(string)values[i];return output;
    }
    public static string SelfTestShell(){return Invoke("selftest",new string[0])[0];}
}
namespace TuningPatch { public static class PatchUsb {
    public static string SelfTestShell(){return X2dNativeBridge.SelfTestShell();}
    public static string[] RunReviewedBatch(string[] commands){return X2dNativeBridge.Invoke("batch",commands);}
}}
namespace StackPublic { public static class StackUsb {
    public static string SelfTestShell(){return X2dNativeBridge.SelfTestShell();}
    public static string[] RunReviewedBatch(string[] commands){return X2dNativeBridge.Invoke("batch",commands);}
    public static string DrainReplies(){return X2dNativeBridge.Invoke("drain",new string[0])[0];}
}}
namespace CaptureSequence { public static class Usb {
    public static string SelfTestShell(){return X2dNativeBridge.SelfTestShell();}
    public static string[] RunReviewedBatch(string[] commands){return X2dNativeBridge.Invoke("capture",commands);}
}}
namespace RawStackFault { public static class Usb {
    public static string SelfTestShell(){return X2dNativeBridge.SelfTestShell();}
    public static string[] ReadFaultOnly(string[] commands){return X2dNativeBridge.Invoke("fault",commands);}
}}
