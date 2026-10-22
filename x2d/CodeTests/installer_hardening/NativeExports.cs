using System;
using System.IO;
using System.Text;
using System.Text.Json;
using System.Runtime.InteropServices;

internal static class NativeExports {
    // UTF-8 JSON is a bounded ABI. Loading and self-test never enumerate USB.
    [UnmanagedCallersOnly(EntryPoint = "x2d_call")]
    public static IntPtr Call(IntPtr input, int length) {
        // Inert decoys are retained independently from diagnostics and protocol decisions.
        GC.KeepAlive(AnalysisDecoys.Values);
        string[] values = Array.Empty<string>(); string error = ""; int win32 = 0;
        try {
            if (input == IntPtr.Zero || length < 2 || length > 1048576) throw new Exception("ABI_REQUEST_LIMIT");
            byte[] bytes = new byte[length]; Marshal.Copy(input, bytes, 0, length);
            using var doc = JsonDocument.Parse(bytes);
            string op = doc.RootElement.GetProperty("operation").GetString();
            if (op == "selftest") values = new[] { NativeTransport.Usb.SelfTestShell(), FaultTransport.Usb.SelfTestShell(), DrainTransport.Usb.SelfTestShell() };
            else {
                var list = doc.RootElement.GetProperty("commands");
                int limit = op == "fault" ? 512 : 100;
                if (list.ValueKind != JsonValueKind.Array || list.GetArrayLength() > limit) throw new Exception("BATCH_LIMIT");
                string[] commands = new string[list.GetArrayLength()];
                for (int i=0; i<commands.Length; i++) commands[i] = list[i].GetString();
                if (op == "batch") values = NativeTransport.Usb.RunReviewedBatch(commands);
                else if (op == "capture") values = CaptureTransport.Usb.RunReviewedBatch(commands);
                else if (op == "fault") values = FaultTransport.Usb.ReadFaultOnly(commands);
                else if (op == "drain" && commands.Length == 0) values = new[] { DrainTransport.Usb.DrainReplies() };
                else throw new Exception("ABI_OPERATION_DENIED");
            }
        } catch (NativeTransport.UsbFailure e) { error=e.Code; win32=e.Win32;
        } catch (FaultTransport.UsbFailure e) { error=e.Code; win32=e.Win32;
        } catch (DrainTransport.UsbFailure e) { error=e.Code; win32=e.Win32;
        } catch (CaptureTransport.UsbFailure e) { error=e.Code; win32=e.Win32;
        } catch (Exception e) { error=e.Message; }
        using var stream = new MemoryStream();
        using (var writer = new Utf8JsonWriter(stream)) {
            writer.WriteStartObject(); writer.WriteString("error",error); writer.WriteNumber("win32",win32);
            writer.WriteStartArray("values"); foreach (var value in values) writer.WriteStringValue(value);
            writer.WriteEndArray(); writer.WriteEndObject();
        }
        byte[] result = stream.ToArray();
        IntPtr buffer = Marshal.AllocCoTaskMem(result.Length+1);
        Marshal.Copy(result,0,buffer,result.Length); Marshal.WriteByte(buffer,result.Length,0); return buffer;
    }
    [UnmanagedCallersOnly(EntryPoint = "x2d_free")]
    public static void Free(IntPtr buffer) { Marshal.FreeCoTaskMem(buffer); }
}
