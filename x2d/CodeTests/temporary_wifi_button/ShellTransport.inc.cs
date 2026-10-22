        // 仅由本目录中已审查的主机方案调用。USB 操作类型固定为 HblShell。
        private static uint Crc(byte[] a,int start,int count) {
            uint c=0; for(int j=start;j<start+count;j++){c^=(uint)a[j]<<8;for(int k=0;k<8;k++)c=((c&32768)!=0?(c<<1)^0x1021:c<<1)&65535;} return c;
        }
        private static void Put32(byte[] b,int offset,uint v){Array.Copy(BitConverter.GetBytes(v),0,b,offset,4);}
        private static byte[] ShellRequest(string command,uint tag) {
            byte[] text=Encoding.UTF8.GetBytes(command);
            if(text.Length==0||text.Length>231||command.IndexOf('\0')>=0)throw new UsbFailure("SHELL_REQUEST_LIMIT");
            foreach(byte c in text)if(c<32||c>126)throw new UsbFailure("SHELL_ASCII_ONLY");
            byte[] p=new byte[257];p[0]=10;p[2]=8;p[3]=5;p[4]=252;
            Put32(p,5,65);Put32(p,13,tag);Array.Copy(text,0,p,25,text.Length);Put32(p,17,Crc(p,21,236));return p;
        }
        private static uint ValidateShellReply(byte[] p,int count,uint tag) {
            if((count!=260&&count!=1024)||p[0]!=9||p[1]!=0||p[2]!=5||p[3]!=8||p[4]!=252)throw new UsbFailure("SHELL_FRAME");
            if(BitConverter.ToUInt32(p,5)!=65||BitConverter.ToUInt32(p,13)!=tag)throw new UsbFailure("SHELL_HEADER");
            if(BitConverter.ToUInt32(p,17)!=Crc(p,21,236))throw new UsbFailure("SHELL_CRC");
            uint function=BitConverter.ToUInt32(p,9);
            if(function!=0&&function!=4)throw new UsbFailure("SHELL_REPLY_TYPE");
            if(BitConverter.ToUInt32(p,21)!=0)throw new UsbFailure("SHELL_REMOTE_FAILURE");
            return function;
        }
        public static string SelfTestShell() {
            if(Crc(Encoding.ASCII.GetBytes("123456789"),0,9)!=0x31c3)throw new Exception("CRC");
            byte[] q=ShellRequest("printf X2D_UI_PROBE_OK",99);
            if(BitConverter.ToUInt32(q,5)!=65||BitConverter.ToUInt32(q,9)!=0||q[256]!=0)throw new Exception("REQUEST");
            byte[] p=new byte[1024];Array.Copy(q,p,q.Length);p[0]=9;p[2]=5;p[3]=8;
            Put32(p,9,4);if(ValidateShellReply(p,p.Length,99)!=4)throw new Exception("NOTIFY");
            Put32(p,9,0);if(ValidateShellReply(p,p.Length,99)!=0)throw new Exception("FINAL");
            int rejected=0;try{ValidateShellReply(p,p.Length,98);}catch(UsbFailure){rejected++;}
            p[30]^=1;try{ValidateShellReply(p,p.Length,99);}catch(UsbFailure){rejected++;}
            try{ShellRequest(new string('x',232),1);}catch(UsbFailure){rejected++;}
            if(rejected!=3)throw new Exception("REJECTION");return "SHELL_OFFLINE_PASS";
        }
        private string Shell(string command,uint tag) {
            byte[] request=ShellRequest(command,tag);uint n;
            Check(WinUsb_WritePipe(usb,pipeOut,request,(uint)request.Length,out n,IntPtr.Zero),"SHELL_SEND");
            if(n!=request.Length)throw new UsbFailure("SHELL_SHORT_WRITE");
            StringBuilder output=new StringBuilder();string deferredError=null;
            for(int frame=0;frame<64;frame++) {
                byte[] packet=new byte[1024];
                try {
                    Check(WinUsb_ReadPipe(usb,pipeIn,packet,1024,out n,IntPtr.Zero),"SHELL_RECEIVE");
                    uint function=ValidateShellReply(packet,(int)n,tag);
                    if(function==0){if(deferredError!=null)throw new UsbFailure(deferredError);return output.ToString();}
                    int length=0;while(length<232&&packet[25+length]!=0)length++;
                    if(output.Length+length>8192)throw new UsbFailure("SHELL_OUTPUT_LIMIT");
                    for(int j=0;j<length;j++){byte c=packet[25+j];if(c!=10&&c!=13&&c!=9&&(c<32||c>126))deferredError="SHELL_OUTPUT_CHAR_AT_"+j+"_VALUE_"+c+"_LEN_"+length;}
                    if(deferredError==null)output.Append(Encoding.ASCII.GetString(packet,25,length));
                }finally{Array.Clear(packet,0,packet.Length);}
            }
            throw new UsbFailure("SHELL_FRAME_LIMIT");
        }
        public static string DrainReplies() {
            ReadResult r=new ReadResult();TemporaryUiUsb s=new TemporaryUiUsb();int frames=0;
            try{s.Open(r);for(;frames<64;frames++){
                byte[] p=new byte[1024];uint n;
                try{if(!WinUsb_ReadPipe(s.usb,s.pipeIn,p,1024,out n,IntPtr.Zero)){
                    int error=Marshal.GetLastWin32Error();if(error==121)return "DRAINED frames="+frames+" requests=0";
                    throw new UsbFailure("DRAIN_FAILED",error);
                }}finally{Array.Clear(p,0,p.Length);}
            }throw new UsbFailure("DRAIN_LIMIT");}finally{s.Dispose();}
        }
        public static string[] RunReviewedBatch(string[] commands) {
            if(commands==null||commands.Length==0||commands.Length>100)throw new UsbFailure("BATCH_LIMIT");
            foreach(string command in commands)ShellRequest(command,1);
            ReadResult r=new ReadResult();TemporaryUiUsb s=new TemporaryUiUsb();
            List<string> output=new List<string>();
            try {
                s.Open(r);ushort seq=(ushort)new Random().Next(1,65000);
                byte[] v=s.Read(27,seq,r);string ver=Encoding.ASCII.GetString(v);Array.Clear(v,0,v.Length);
                if(ver!="S5,4.2.0"&&ver!="S6,v4.2.0")throw new UsbFailure("FIRMWARE_MISMATCH");
                byte[] running=s.Read(28,(ushort)(seq+1),r);string rs=Encoding.ASCII.GetString(running);Array.Clear(running,0,running.Length);
                if(rs!="i1")throw new UsbFailure("CAMERA_NOT_RUNNING");
                uint tag=(uint)new Random().Next(1,int.MaxValue-200);
                for(int i=0;i<commands.Length;i++){
                    try{output.Add(s.Shell(commands[i],tag+(uint)i));}
                    catch(UsbFailure e){throw new UsbFailure("BATCH_STEP_"+i+"_"+e.Code,e.Win32);}
                }
            }finally{s.Dispose();}
            if(!s.released)throw new UsbFailure("USB_CLOSE");
            return output.ToArray();
        }
