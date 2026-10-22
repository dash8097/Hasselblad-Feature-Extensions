using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Reflection;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.Collections.Generic;

[assembly: AssemblyTitle("哈苏特调补丁")]
[assembly: AssemblyProduct("哈苏特调补丁")]
[assembly: AssemblyVersion("5.8.52.0")]
[assembly: AssemblyFileVersion("5.8.52.0")]

// 计数来自已完成的工作；阶段区间是工作权重，不按等待时间推进。
internal sealed class InstallerProgress {
    public int Value { get; private set; }
    public void Reset() { Value=0; }
    public bool Observe(string line,out string detail) {
        detail="";
        if(!line.StartsWith("PATCH_PROGRESS=",StringComparison.Ordinal)) return false;
        var fields=line.Substring(15).Split('|');long done,total;
        if(fields.Length!=3 || !Int64.TryParse(fields[1],out done) || !Int64.TryParse(fields[2],out total) || done<0 || total<=0 || done>total) return false;
        int start,end;string title;
        switch(fields[0]) {
            case "preflight":start=0;end=5;title="检查安装包";break;
            case "snapshot":start=5;end=15;title="检查相机组件";break;
            case "transfer":start=15;end=75;title="传输载荷";break;
            case "install":start=75;end=90;title="安装／恢复文件";break;
            case "verify":start=90;end=99;title="核对完成文件";break;
            case "cleanup-pre":start=15;end=20;title="回收旧暂存";break;
            case "cleanup-post":start=99;end=99;title="清理安装收尾";break;
            case "diagnose":start=0;end=99;title="采集诊断";break;
            default:return false;
        }
        Value=Math.Max(Value,start+(int)((decimal)done*(end-start)/total));
        detail=title+" "+(int)((decimal)done*100/total)+"% · "+(fields[0]=="transfer" ? (done/1024)+" / "+(total/1024)+" KB" : done+" / "+total);
        return true;
    }
    public void Finish(bool success) { if(success) Value=100; }
}

// Windows portable client. Hardware actions only run after an explicit button click.
internal sealed class RawStackInstaller : Form {
    const string InstallerVersion = "H3 内部候选";
    const string Github = "https://github.com/dash8097/Hasselblad-Feature-Extensions";
    const string OfficialFirmware = "https://www.hasselblad.com/x-system/firmware/";
    const string ThirdPartyNotice = "安装前必须确认未安装其他人的补丁包。叠加安装可能导致严重故障，无法保证恢复。\n安装过他人的包：先通过官方固件升级清除并还原，重启后重新检查；不通过请勿安装。";
    readonly string root = AppDomain.CurrentDomain.BaseDirectory;
    readonly TextBox log = new TextBox();
    readonly Label state = new Label();
    readonly ProgressBar progress = new ProgressBar();
    readonly InstallerProgress proportional = new InstallerProgress();
    readonly Label elapsed = new Label();
    readonly Stopwatch operationClock = new Stopwatch();
    readonly System.Windows.Forms.Timer progressTimer = new System.Windows.Forms.Timer();
    readonly CheckBox[] featureBoxes = new CheckBox[7];
    int operationMask = 255;
    readonly Button check = new Button(), install = new Button(), verify = new Button(), diagnose = new Button(), uninstall = new Button();
    readonly object evidenceLock = new object();
    readonly PatchUpdateState update = new PatchUpdateState();
    Dictionary<string, object> context, report;
    readonly StringBuilder operationLog = new StringBuilder();
    string primaryFailure = "", currentAction = "";
    Form diagnosticWindow;
    TextBox diagnosticText, diagnosticRaw;
    Button diagnosticAgain;
#if INTERNAL_UI_TESTS
    Func<string,Task<int>> offlineActionFixture;
#endif
    bool running;
    readonly bool stock;
    readonly bool universal;
    readonly bool manualReboot;
    string lastResult = "";
    RawStackInstaller() {
        stock = File.Exists(Path.Combine(root, "stock-package.txt"));
        universal = File.Exists(Path.Combine(root, "universal-package.txt"));
        manualReboot = stock;
        var manifestPath = Path.Combine(root, "payload", "install-manifest.json");
        if(File.Exists(manifestPath)) {
            var manifest = new System.Web.Script.Serialization.JavaScriptSerializer().Deserialize<Dictionary<string, object>>(File.ReadAllText(manifestPath));
            object value;
            if(manifest.TryGetValue("manual_reboot_required", out value)) manualReboot = value is bool && (bool)value;
        }
        Text = "哈苏特调补丁 · X2D";
        AutoScaleMode = AutoScaleMode.Dpi;
        ClientSize = new Size(720, 624); MinimumSize = MaximumSize = new Size(736, 663);
        FormBorderStyle = FormBorderStyle.FixedSingle; MaximizeBox = false;
        StartPosition = FormStartPosition.CenterScreen;
        Font = new Font("Microsoft YaHei UI", 10); BackColor = Color.FromArgb(247, 248, 250);
        ForeColor = Color.FromArgb(32, 37, 44);
        Controls.Add(new Label { Text = "H", TextAlign = ContentAlignment.MiddleCenter,
            Font = new Font("Segoe UI", 25, FontStyle.Bold), ForeColor = Color.White,
            BackColor = Color.FromArgb(32, 37, 44), Location = new Point(28, 28), Size = new Size(56, 56) });
        Controls.Add(new Label { Text = "哈苏特调补丁", Font = new Font(Font.FontFamily, 20, FontStyle.Bold),
            AutoSize = true, Location = new Point(100, 25) });
        Controls.Add(new Label { Text = "X2D 100C  /  固件 4.2.0  /  " + InstallerVersion,
            AutoSize = true, ForeColor = Color.FromArgb(109, 116, 126), Location = new Point(102, 65) });
        Controls.Add(new Label { Text = "全功能测试包：以下功能整套安装。",
            AutoSize = true, Font = new Font(Font.FontFamily, 9), Location = new Point(28, 113), ForeColor = Color.FromArgb(91, 99, 110) });
        string[] featureLabels = new string[] {"四亿／十六亿像素（含高清回放）", "25MP", "RAW／ISO 堆栈", "人眼与 AFC", "对焦增强", "屏幕调参与显示扩展", "快门动画与声音（默认关闭）"};
        int[] featureBits = {1,2,4,8,16,64,128};
        for(int i=0;i<featureBoxes.Length;i++) {
            var box = new CheckBox {Text=featureLabels[i],Name="Feature"+i,Tag=featureBits[i],Checked=true,Enabled=false,AutoCheck=false,AutoSize=false,
                Location=new Point(28+(i%2)*334,154+(i/2)*36),Size=new Size(326,30),UseVisualStyleBackColor=true};
            featureBoxes[i]=box; Controls.Add(box);
        }
        Controls.Add(new Label {Text="全功能测试；九区停用。快门动画、声音初始关闭。",AutoSize=true,
            Location=new Point(28,304),Font=new Font(Font.FontFamily,9),ForeColor=Color.FromArgb(109,116,126)});
        var card = new Panel { BackColor = Color.White, Location = new Point(28, 330), Size = new Size(664, 136) };
        card.Paint += delegate(object sender, PaintEventArgs e) { using(var pen = new Pen(Color.FromArgb(228, 231, 236))) e.Graphics.DrawRectangle(pen, 0, 0, card.Width-1, card.Height-1); };
        Controls.Add(card);
        card.Controls.Add(new Label { Text = "连接相机，然后选择操作", Font = new Font(Font.FontFamily, 12, FontStyle.Bold),
            AutoSize = true, Location = new Point(20, 18) });
        state.Text = "等待操作。请关闭 Phocus，并通过 USB 连接相机。";
        state.Location = new Point(20, 50); state.Size = new Size(624, 44);
        state.ForeColor = Color.FromArgb(91, 99, 110); card.Controls.Add(state);
        progress.Location = new Point(20,96); progress.Size = new Size(624,10); card.Controls.Add(progress);
        elapsed.Location = new Point(20,110); elapsed.Size = new Size(624,20); elapsed.Font = new Font(Font.FontFamily,9);
        elapsed.ForeColor = Color.FromArgb(109,116,126); card.Controls.Add(elapsed);
        progress.Style=ProgressBarStyle.Continuous;
        progressTimer.Interval=1000; progressTimer.Tick += delegate { if(running) elapsed.Text="总进度 "+proportional.Value+"% · 已用时 "+(int)operationClock.Elapsed.TotalSeconds+" 秒 · 请保持 USB 连接"; };
        Disposed += delegate { progressTimer.Dispose(); };
        AddButton(install, "检测并安装", 28, 256, true, async delegate {
            await Run(universal ? new string[] { update.Action } : new string[] { "Check", "Stage", "Activate", manualReboot ? "PollInstall" : "Verify" });
        });
        AddButton(uninstall, "卸载", 302, 164, false, async delegate { await Run(new string[] { "Uninstall" }); });
        AddButton(diagnose, "诊断", 484, 168, false, async delegate { await Run(new string[] { "Diagnose" }); });
        uninstall.Enabled = universal;
        // 原始回执仍保留供诊断；首页只显示操作状态。
        log.Multiline = true; log.ReadOnly = true; log.Visible = false; Controls.Add(log);
        Controls.Add(new Label { Text = "其他来源的补丁请先通过官方固件还原。完成后按提示手动重启。",
            AutoSize = true, Font = new Font(Font.FontFamily, 9), ForeColor = Color.FromArgb(109,116,126), Location = new Point(28, 542) });
        var githubLink = new LinkLabel { Text = "公开 GitHub：" + Github, Name = "PublicGithubLink", AutoSize = true,
            Font = new Font("Segoe UI", 9), Location = new Point(28, 571), LinkColor = Color.FromArgb(44,83,132),
            ActiveLinkColor = Color.FromArgb(24,56,96), VisitedLinkColor = Color.FromArgb(44,83,132) };
        githubLink.LinkClicked += delegate { try { Process.Start(new ProcessStartInfo(Github) { UseShellExecute = true }); }
            catch { state.Text = "公开项目地址：" + Github; } }; Controls.Add(githubLink);
        Controls.Add(new Label { Text = "交流群入口在 GitHub 页面。日志在诊断中查看。", Name = "GithubCommunityNotice",
            AutoSize = true, Font = new Font(Font.FontFamily, 9), ForeColor = Color.FromArgb(109,116,126), Location = new Point(28, 599) });
        FormClosing += delegate(object sender, FormClosingEventArgs e) { if (running) { e.Cancel = true; state.Text = "正在处理，请等待本次事务结束再关闭。"; } };
    }
    void AddButton(Button button, string text, int x, int width, bool primary, EventHandler handler) {
        button.Text = text; button.Location = new Point(x, 486); button.Size = new Size(width, 42);
        button.FlatStyle = FlatStyle.Flat; button.FlatAppearance.BorderSize = primary ? 0 : 1;
        button.FlatAppearance.BorderColor = Color.FromArgb(222, 226, 232);
        button.UseVisualStyleBackColor = false;
        button.BackColor = primary ? Color.FromArgb(32, 37, 44) : Color.White;
        button.ForeColor = primary ? Color.White : Color.FromArgb(32, 37, 44);
        button.FlatAppearance.MouseOverBackColor = primary ? Color.FromArgb(47, 54, 64) : Color.FromArgb(241, 243, 246);
        button.FlatAppearance.MouseDownBackColor = primary ? Color.FromArgb(18, 22, 28) : Color.FromArgb(228, 231, 236);
        button.EnabledChanged += delegate {
            // Windows 会自行绘制禁用文字，禁用时必须同时改为浅色背景。
            button.BackColor = !button.Enabled ? Color.FromArgb(228, 231, 236) : primary ? Color.FromArgb(32, 37, 44) : Color.White;
            button.ForeColor = !button.Enabled ? SystemColors.GrayText : primary ? Color.White : Color.FromArgb(32, 37, 44);
        };
        button.Font = new Font(Font.FontFamily, 10, primary ? FontStyle.Bold : FontStyle.Regular);
        button.Cursor = Cursors.Hand; button.Click += handler; Controls.Add(button);
    }
    void Append(string line) {
        if (line == null || IsDisposed) return;
        if(line.StartsWith("PATCH_PROGRESS=",StringComparison.Ordinal)) { QueueProgress(line); return; }
        lock(evidenceLock) {
            if(update.Observe(line)) return;
            var parsed = PatchDiagnosticReport.Parse(line, "PATCH_CONTEXT=");
            if(parsed != null) { context = parsed; object stage; if(parsed.TryGetValue("stage",out stage)) QueueProgress(Convert.ToString(stage)); return; }
            parsed = PatchDiagnosticReport.Parse(line, "PATCH_DIAGNOSTIC=");
            if(parsed != null) { if(report == null) report = parsed; return; }
            line = PatchDiagnosticReport.Protect(line);
            if(operationLog.Length < 65536) operationLog.AppendLine(line);
        }
        if (line == "PATCH_MANUAL_REBOOT_REQUIRED" || line == "PATCH_ALREADY_CURRENT" || line == "PATCH_VERSION_VERIFIED" || line == "PATCH_INSTALL_COMPLETE" || line == "PATCH_UNINSTALLED_MANUAL_REBOOT_REQUIRED" || line == "PATCH_ALREADY_UNINSTALLED" || line == "PATCH_UPDATE_AVAILABLE") lastResult = line;
        if (line == "PATCH_DETECTED_ROUTE=Stock") line = "检测到原厂状态，将自动安装完整补丁。";
        else if (line == "PATCH_DETECTED_ROUTE=Upgrade") line = "检测到旧补丁；点击安装／更新将自动卸载，重启后继续更新。";
        else if (line == "PATCH_DETECTED_ROUTE=KnownProject") line = "检测到本项目开发版或已知组件组合，可一键卸载；更新将先卸载，重启后继续。";
        else if (line == "PATCH_DETECTED_ROUTE=CurrentStock" || line == "PATCH_DETECTED_ROUTE=CurrentUpgrade") line = "检测到完整补丁；核对是否为当前版本。";
        else if (line == "STACK_FILES_VERIFIED_MANUAL_REBOOT_REQUIRED") line = "安装文件已完整核对；请等待本次安装结束。";
        else if (line == "PATCH_MANUAL_REBOOT_REQUIRED") line = "补丁安装完成，请手动关机后重新开机。重启后点击“检测并安装”。";
        else if (line == "PATCH_UNINSTALLED_MANUAL_REBOOT_REQUIRED") line = "补丁文件已还原，电源键所需服务保留。请正常关机后重新开机，再继续更新或核对原厂状态。";
        string displayLine = line;
        BeginInvoke((Action)delegate { log.AppendText(displayLine + Environment.NewLine); });
    }
    void QueueProgress(string value) {
        if(IsDisposed || !IsHandleCreated) return;
        try { BeginInvoke((Action)delegate {
            if(!running || IsDisposed) return;
            if(value.StartsWith("PATCH_PROGRESS=",StringComparison.Ordinal)) {
                string detail;if(!proportional.Observe(value,out detail)) return;
                progress.Value=proportional.Value;
                state.Text="总进度 "+progress.Value+"% · "+detail+"\n请保持连接；全部核对通过后才显示 100%。";
            } else {
                state.Text="总进度 "+proportional.Value+"% · "+PatchDiagnosticReport.Protect(value)+"\n等待期间保持当前进度，请保持连接。";
            }
        }); } catch(InvalidOperationException) { if(!IsDisposed) throw; }
    }
    async Task<int> Invoke(string action) {
#if INTERNAL_UI_TESTS
        if(offlineActionFixture != null) return await offlineActionFixture(action);
#endif
        if(universal || !stock) {
            lock(evidenceLock) { context = new Dictionary<string,object> { {"protocol",1},{"action",action},{"stage","电脑端安装必需文件检查"},{"transport","未打开 USB"},{"recovery","未启动相机操作"},{"time",DateTimeOffset.Now.ToString("yyyy-MM-dd HH:mm:ss zzz")} }; }
            PatchDiagnosticReport.CheckPackage(root);
        }
        var start = new ProcessStartInfo(Path.Combine(root,"X2dEngine.exe")) {
            Arguments = "\"" + root.TrimEnd(Path.DirectorySeparatorChar) + "\"",
            UseShellExecute = false, CreateNoWindow = true, WindowStyle = ProcessWindowStyle.Hidden,
            RedirectStandardOutput = true, RedirectStandardError = true, RedirectStandardInput = true,
            StandardOutputEncoding = Encoding.UTF8, StandardErrorEncoding = Encoding.UTF8, WorkingDirectory = root
        };
        string pending="";
        if(action=="ResumeUpdate") { lock(evidenceLock) {
            if(!update.Pending) throw new InvalidOperationException("没有待续接的更新。");
            pending=update.PreviousBootFingerprint;
        }}
        string request=new System.Web.Script.Serialization.JavaScriptSerializer().Serialize(new {action=action,feature_mask=255,pending=pending});
        return await Task.Run(delegate {
            using (var p = new Process { StartInfo = start }) {
                p.OutputDataReceived += delegate(object sender, DataReceivedEventArgs e) { Append(e.Data); };
                p.ErrorDataReceived += delegate(object sender, DataReceivedEventArgs e) { Append(e.Data); };
                p.Start(); p.BeginOutputReadLine(); p.BeginErrorReadLine(); p.StandardInput.WriteLine(request); p.StandardInput.Close(); p.WaitForExit(); return p.ExitCode;
            }
        });
    }
    int SelectedFeatures() {return 255;}
    async Task Run(string[] actions) {
        if(!running && !update.Pending) operationMask=SelectedFeatures();
        if(operationMask==0 && (actions[0]=="Install" || actions[0]=="ResumeUpdate")) {state.Text="请选择至少一个扩展功能。";return;}

        if (running) return; running = true; lastResult = ""; check.Enabled = install.Enabled = verify.Enabled = diagnose.Enabled = uninstall.Enabled = false;
        foreach(var box in featureBoxes) box.Enabled=false;
        operationClock.Restart(); proportional.Reset(); progress.Style=ProgressBarStyle.Continuous; progress.Value=0;
        elapsed.Text="总进度 0% · 已用时 0 秒 · 请保持 USB 连接"; progressTimer.Start();
        if(diagnosticAgain != null && !diagnosticAgain.IsDisposed) diagnosticAgain.Enabled = false;
        currentAction = actions[0];
        lock(evidenceLock) { context = report = null; operationLog.Clear(); update.BeginAttempt(); }
        if(currentAction != "Diagnose") primaryFailure = "";
        int exitCode = 0;
        if (Array.IndexOf(actions, "Install") >= 0 || Array.IndexOf(actions, "ResumeUpdate") >= 0 || Array.IndexOf(actions, "Stage") >= 0) Append(ThirdPartyNotice);
        try {
            foreach (var action in actions) {
                state.Text = action == "Uninstall" ? "正在卸载并还原原厂文件；请保持连接，完成后手动重启。" : action == "Diagnose" ? "正在只读采集诊断，不安装或触发拍摄…" : action == "Install" ? "正在识别并安装／更新；旧版会自动卸载，请保持连接。" : action == "ResumeUpdate" ? "正在重新核对固件、原厂文件与重启状态，然后继续安装。" : action == "Stage" ? "正在传输；中断后可重新安装并核对续传位置。" : action == "Activate" ? "正在切换入口并恢复取景…" : "正在核对…";
                exitCode = await Invoke(action);
                if (exitCode != 0) { state.Text = "操作未完成。详细诊断已打开，请保留连接并拍下摘要。"; break; }
            }
            lock(evidenceLock) { update.Complete(currentAction,exitCode); }
            if(exitCode == 0 && universal && currentAction != "Diagnose" && !update.Pending &&
                lastResult != "PATCH_ALREADY_CURRENT" && lastResult != "PATCH_MANUAL_REBOOT_REQUIRED" &&
                lastResult != "PATCH_UNINSTALLED_MANUAL_REBOOT_REQUIRED" && lastResult != "PATCH_ALREADY_UNINSTALLED" &&
                lastResult != "PATCH_VERSION_VERIFIED" && lastResult != "PATCH_UPDATE_AVAILABLE" &&
                actions[actions.Length-1] != "Check")
                throw new InvalidOperationException("未取得安装完成回执。请读取诊断；没有自动重发安装。");
            if(exitCode == 0) state.Text = update.Pending ? "旧版文件已还原。保持客户端打开，正常关机后重新开机，再点击“检测并安装”。" : lastResult == "PATCH_UPDATE_AVAILABLE" ? "已识别旧补丁；点击“检测并安装”即可自动处理。" : lastResult == "PATCH_UNINSTALLED_MANUAL_REBOOT_REQUIRED" ? "补丁文件已还原，原厂文件核对通过。请正常关机后重新开机。" : lastResult == "PATCH_ALREADY_UNINSTALLED" ? "原厂文件已就绪，无需重复卸载；若尚未重启，请手动关机开机。" : currentAction == "Diagnose" ? "只读诊断完成，请查看诊断窗口。" : lastResult == "PATCH_MANUAL_REBOOT_REQUIRED" || actions[actions.Length - 1] == "PollInstall" ? "文件安装完成。请手动重启相机，再点击“检测并安装”。" : lastResult == "PATCH_ALREADY_CURRENT" ? "已是当前补丁版本，核对通过，无需重复安装。" : actions[actions.Length - 1] == "Check" ? "Phocus、驱动、连接与安装条件核对完成。" : "安装版本核对完成。实际拍摄效果请手动试拍确认。";
        } catch (Exception ex) { exitCode = 1; Append(ex.Message); lock(evidenceLock) { if(report == null && context != null) context["reason"] = PatchDiagnosticReport.Protect(ex.Message); } state.Text = "客户端操作未完成，详细诊断已打开。"; }
        finally { foreach(var box in featureBoxes) box.Enabled=false; progressTimer.Stop(); operationClock.Stop(); proportional.Finish(exitCode==0); progress.Value=proportional.Value; elapsed.Text="总进度 "+progress.Value+"% · 本次操作用时 "+(int)operationClock.Elapsed.TotalSeconds+" 秒"; install.Text = "检测并安装"; running = false; check.Enabled = install.Enabled = verify.Enabled = diagnose.Enabled = true; uninstall.Enabled = universal; if(diagnosticAgain != null && !diagnosticAgain.IsDisposed) diagnosticAgain.Enabled = true; }
        if(exitCode != 0 || currentAction == "Diagnose") {
            try { ShowDiagnostics(exitCode); }
            catch(Exception ex) { Append("诊断窗口更新未完成："+ex.Message); if(exitCode != 0) state.Text = "操作未完成；诊断界面未能打开，原始回执已保留。"; }
        }
    }
    void ShowDiagnostics(int exitCode) {
        Dictionary<string, object> evidence; string raw;
        lock(evidenceLock) {
            evidence = report ?? context ?? new Dictionary<string, object>();
            raw = operationLog.ToString();
            if(!evidence.ContainsKey("action")) evidence["action"] = currentAction;
            if(!evidence.ContainsKey("reason") || String.IsNullOrEmpty(Convert.ToString(evidence["reason"]))) evidence["reason"] = exitCode == 0 ? "未取得结构化诊断，请查看原始输出" : "客户端进程退出，代码 0x"+unchecked((uint)exitCode).ToString("X8")+"；原生客户端未返回完整诊断，不能据此判断相机故障。\n"+(raw.Length == 0 ? "没有取得标准输出或错误输出。" : "本次进程输出：\n"+raw.Substring(0,Math.Min(raw.Length,2048)));
        }
        string previous = currentAction == "Diagnose" ? primaryFailure : "";
        string display = PatchDiagnosticReport.Format(evidence, previous, exitCode);
        if(exitCode != 0 && currentAction != "Diagnose" && primaryFailure.Length == 0) primaryFailure = display;
        if(diagnosticWindow == null || diagnosticWindow.IsDisposed) {
            diagnosticWindow = new Form { Text = "哈苏特调补丁 · " + InstallerVersion + " · X2D 详细诊断", ClientSize = new Size(900,860), MinimumSize = new Size(860,800), Font = Font, StartPosition = FormStartPosition.CenterParent };
            var title = new Label { Text = "操作诊断 · 请拍完整摘要", Font = new Font(Font.FontFamily,16,FontStyle.Bold), AutoSize = true, Location = new Point(22,16) }; diagnosticWindow.Controls.Add(title);
            var build = new Label { Text = InstallerVersion + " · X2D 固件 4.2.0 · 诊断 D1 · 包标识："+PackageFingerprint()+"\n只读诊断不重新安装；无法确认的状态明确标为未取得。", AutoSize = true, Location = new Point(22,54) }; diagnosticWindow.Controls.Add(build);
            var tabs = new TabControl { Location = new Point(22,112), Size = new Size(856,672), Anchor = AnchorStyles.Top|AnchorStyles.Bottom|AnchorStyles.Left|AnchorStyles.Right };
            var summary = new TabPage("详细摘要"); var output = new TabPage("本次原始输出（已脱敏）");
            diagnosticText = new TextBox { Multiline = true, ReadOnly = true, WordWrap = true, ScrollBars = ScrollBars.Vertical, Dock = DockStyle.Fill, Font = new Font(Font.FontFamily,10) };
            diagnosticRaw = new TextBox { Multiline = true, ReadOnly = true, WordWrap = true, ScrollBars = ScrollBars.Vertical, Dock = DockStyle.Fill, Font = new Font("Consolas",9) };
            summary.Controls.Add(diagnosticText); output.Controls.Add(diagnosticRaw); tabs.TabPages.Add(summary); tabs.TabPages.Add(output); diagnosticWindow.Controls.Add(tabs);
            var close = new Button { Text = "返回操作页", Location = new Point(22,805), Size = new Size(150,34), Anchor = AnchorStyles.Bottom|AnchorStyles.Left }; close.Click += delegate { diagnosticWindow.Close(); }; diagnosticWindow.Controls.Add(close);
            diagnosticAgain = new Button { Text = "重新读取现场（不安装）", Location = new Point(188,805), Size = new Size(220,34), Anchor = AnchorStyles.Bottom|AnchorStyles.Left, Enabled = !running }; diagnosticAgain.Click += async delegate { await Run(new string[] { "Diagnose" }); }; diagnosticWindow.Controls.Add(diagnosticAgain);
            var copy = new Button { Text = "复制完整结果", Location = new Point(426,805), Size = new Size(150,34), Anchor = AnchorStyles.Bottom|AnchorStyles.Left };
            copy.Click += delegate {
                try { Clipboard.SetText(diagnosticText.Text+Environment.NewLine+"本次原始输出（已脱敏）"+Environment.NewLine+diagnosticRaw.Text); }
                catch(Exception ex) { MessageBox.Show(diagnosticWindow,"复制未完成，可选中文字手动复制。"+Environment.NewLine+ex.Message,"诊断结果",MessageBoxButtons.OK,MessageBoxIcon.Information); }
            }; diagnosticWindow.Controls.Add(copy);
            var note = new Label { Text = "首次失败保留；日志不自动上传。", AutoSize = true, Location = new Point(598,813), Anchor = AnchorStyles.Bottom|AnchorStyles.Left }; diagnosticWindow.Controls.Add(note);
        }
        diagnosticText.Text = display.Replace("\r\n","\n").Replace("\n","\r\n");
        diagnosticRaw.Text = raw;
        if(!diagnosticWindow.Visible) diagnosticWindow.Show(this);
        diagnosticWindow.BringToFront();
    }
    string PackageFingerprint() {
        try { using(var sha = System.Security.Cryptography.SHA256.Create()) return BitConverter.ToString(sha.ComputeHash(File.ReadAllBytes(Path.Combine(root,"package-release.json")))).Replace("-","").Substring(0,16); }
        catch { return "未取得"; }
    }
#if INTERNAL_UI_TESTS
    static void PumpUntil(Func<bool> complete) {
        var watch=Stopwatch.StartNew();
        while(!complete()) {
            if(watch.ElapsedMilliseconds > 10000) throw new InvalidOperationException("离线诊断刷新检查超时");
            Application.DoEvents(); System.Threading.Thread.Sleep(5);
        }
        Application.DoEvents();
    }
    void CheckCompactUi(string output) {
        Show(); Application.DoEvents();
        var buttons = new List<Button>();
        foreach(Control c in Controls) if(c is Button && c.Visible) buttons.Add((Button)c);
        if(buttons.Count != 3 || buttons[0] != install || buttons[1] != uninstall || buttons[2] != diagnose ||
            install.Text != "检测并安装" || uninstall.Text != "卸载" || diagnose.Text != "诊断" || log.Visible || ClientSize.Height > 650)
            throw new Exception("首页必须为紧凑三按钮界面");
        var github=Controls["PublicGithubLink"] as LinkLabel; var community=Controls["GithubCommunityNotice"] as Label;
        if(github==null || !github.Visible || !github.Text.Contains(Github) || github.Links.Count==0 || community==null || !community.Visible || !community.Text.Contains("交流群入口在 GitHub 页面") || github.Right>ClientSize.Width || community.Bottom>ClientSize.Height)
            throw new Exception("主页缺少可见 GitHub 链接或交流群说明");
        var cases = new List<string> { "three-visible-buttons", "compact-home-without-log", "startup-without-USB", "visible-clickable-github-and-community-notice" };
        var actions = new List<string>(); var completion = new TaskCompletionSource<int>();
        offlineActionFixture = delegate(string action) { actions.Add(action); return completion.Task; };
        install.PerformClick(); Application.DoEvents(); install.PerformClick();
        if(!running || actions.Count != 1 || actions[0] != "Install" || install.Enabled || uninstall.Enabled || diagnose.Enabled)
            throw new Exception("按钮重复派发或忙碌门控失败");
        Append("PATCH_MANUAL_REBOOT_REQUIRED"); completion.SetResult(0); PumpUntil(delegate { return !running; });
        if(diagnosticWindow != null || !state.Text.Contains("手动重启") || install.Text != "检测并安装")
            throw new Exception("成功后出现错误弹窗或缺少重启提示");
        cases.Add("single-click-install-and-double-click-blocked"); cases.Add("successful-install-waits-reboot-without-error-window");
        offlineActionFixture = delegate(string action) { actions.Add(action); Append("PATCH_ALREADY_CURRENT"); return Task.FromResult(0); };
        install.PerformClick(); PumpUntil(delegate { return !running; });
        if(!state.Text.Contains("无需重复安装") || diagnosticWindow != null) throw new Exception("已安装版本误报失败");
        cases.Add("current-version-verified-without-reinstall");
        offlineActionFixture = delegate(string action) { actions.Add(action); Append("PATCH_UNINSTALLED_MANUAL_REBOOT_REQUIRED"); return Task.FromResult(0); };
        uninstall.PerformClick(); PumpUntil(delegate { return !running; });
        if(actions[actions.Count-1] != "Uninstall" || diagnosticWindow != null) throw new Exception("卸载派发错误");
        cases.Add("one-click-uninstall");
        offlineActionFixture = delegate(string action) { actions.Add(action); Append("PATCH_UNINSTALLED_MANUAL_REBOOT_REQUIRED"); Append("PATCH_UPDATE_RESTART_PENDING="+new string('a',64)); return Task.FromResult(0); };
        install.PerformClick(); PumpUntil(delegate { return !running; });
        if(!update.Pending || diagnosticWindow != null) throw new Exception("更新卸载完成未等待重启");
        offlineActionFixture = delegate(string action) { Append("PATCH_UPDATE_RESTART_STILL_PENDING"); return Task.FromResult(0); };
        install.PerformClick(); PumpUntil(delegate { return !running; });
        if(!update.Pending || diagnosticWindow != null || !state.Text.Contains("关机")) throw new Exception("未重启时再次点击误报安装错误");
        cases.Add("click-before-reboot-keeps-waiting-without-error-window");
        offlineActionFixture = delegate(string action) { actions.Add(action); Append("PATCH_MANUAL_REBOOT_REQUIRED"); return Task.FromResult(0); };
        install.PerformClick(); PumpUntil(delegate { return !running; });
        if(actions[actions.Count-1] != "ResumeUpdate" || update.Pending || install.Text != "检测并安装") throw new Exception("同一按钮续接失败");
        cases.Add("same-button-resumes-update");
        offlineActionFixture = delegate(string action) { actions.Add(action); return Task.FromResult(0); };
        install.PerformClick(); PumpUntil(delegate { return !running; });
        if(diagnosticWindow == null || !diagnosticText.Text.Contains("未取得安装完成回执")) throw new Exception("缺少回执仍声称成功");
        cases.Add("missing-completion-receipt-refused"); diagnosticWindow.Close();
        report = new Dictionary<string,object> { {"protocol",1}, {"reason","离线诊断样例"} };
        offlineActionFixture = delegate(string action) { actions.Add(action); return Task.FromResult(0); };
        diagnose.PerformClick(); PumpUntil(delegate { return !running; });
        if(actions[actions.Count-1] != "Diagnose" || !diagnosticWindow.Visible) throw new Exception("诊断未打开");
        cases.Add("diagnosis-only-opens-diagnostic-window");
        File.WriteAllText(output,new System.Web.Script.Serialization.JavaScriptSerializer().Serialize(new Dictionary<string,object> {
            {"offline",true},{"device_requests",0},{"cases",cases},{"installer_version",InstallerVersion} }),Encoding.UTF8);
    }
    void CheckProportionalProgress(string output) {
        Show(); Application.DoEvents();
        var completion = new TaskCompletionSource<int>();
        offlineActionFixture = delegate(string action) { return completion.Task; };
        install.PerformClick(); Application.DoEvents();
        Action<string,int> expect = delegate(string line,int value) {
            Append(line); Application.DoEvents();
            if(progress.Style != ProgressBarStyle.Continuous || progress.Value != value)
                throw new Exception("比例进度错误："+line+" actual="+progress.Value);
        };
        expect("PATCH_PROGRESS=snapshot|38|76",10);
        expect("PATCH_PROGRESS=transfer|50|100",45);
        expect("等待相机服务就绪",45);
        foreach(string line in new [] { "PATCH_PROGRESS=transfer|200|100","PATCH_PROGRESS=verify|1|0","PATCH_PROGRESS=unknown|1|2","PATCH_PROGRESS=verify|x|10" }) expect(line,45);
        expect("PATCH_PROGRESS=transfer|100|100",75);
        expect("PATCH_PROGRESS=install|3|5",84);
        expect("PATCH_PROGRESS=verify|1|3",93);
        expect("PATCH_PROGRESS=snapshot|0|76",93);
        expect("PATCH_PROGRESS=verify|71|71",99);
        expect("PATCH_PROGRESS=verify|9223372036854775807|9223372036854775807",99);
        completion.SetResult(1); PumpUntil(delegate { return !running; });
        if(progress.Value != 99 || diagnosticWindow == null) throw new Exception("失败未保留实际比例与诊断");
        diagnosticWindow.Close();
        completion = new TaskCompletionSource<int>(); install.PerformClick(); Application.DoEvents();
        if(progress.Value != 0) throw new Exception("新操作没有重新开始");
        expect("PATCH_PROGRESS=verify|71|71",99);
        Append("PATCH_MANUAL_REBOOT_REQUIRED"); completion.SetResult(0); PumpUntil(delegate { return !running; });
        if(progress.Value != 100 || !state.Text.Contains("手动重启")) throw new Exception("完整回执未完成进度");
        File.WriteAllText(output,new System.Web.Script.Serialization.JavaScriptSerializer().Serialize(new Dictionary<string,object> {
            {"offline",true},{"device_requests",0},{"cases",new [] {"weighted-byte-transfer","determinate-wait","invalid-counts-rejected","no-regression","large-count-no-overflow","failure-preserves-progress","new-operation-reset","100-only-after-success-receipt"}} }),Encoding.UTF8);
    }
    void CheckDiagnosticRefresh(string output) {
        Show(); Application.DoEvents(); currentAction="Install";
        report=new Dictionary<string,object> { {"protocol",1},{"reason","首次安装失败样例"} };
        ShowDiagnostics(1);
        var first=diagnosticWindow;currentAction="Diagnose";
        report=new Dictionary<string,object> { {"protocol",1},{"reason","诊断刷新成功样例"} };
        ShowDiagnostics(0);ShowDiagnostics(0);Application.DoEvents();
        if(first != diagnosticWindow || !diagnosticWindow.Visible || !diagnosticText.Text.Contains("首次安装失败样例") || !diagnosticText.Text.Contains("诊断刷新成功样例")) throw new Exception("可见窗口刷新或首次失败保留错误");
        var cases=new List<string> { "repeat-visible-window-refresh","first-failure-preserved" };
        int calls=0;var completion=new TaskCompletionSource<int>();
        offlineActionFixture=delegate(string action) { if(action != "Diagnose") throw new Exception("诊断按钮派发了非只读动作");calls++;return completion.Task; };
        diagnosticAgain.PerformClick();Application.DoEvents();
        if(!running || diagnosticAgain.Enabled || calls != 1) throw new Exception("读取期间未禁用刷新按钮");
        diagnosticAgain.PerformClick();Application.DoEvents();if(calls != 1) throw new Exception("重复派发诊断");
        cases.Add("refresh-click-dispatches-diagnose-only-once");
        completion.SetResult(0);PumpUntil(delegate { return !running; });
        if(!diagnosticAgain.Enabled || !diagnosticWindow.Visible || !state.Text.Contains("只读诊断完成")) throw new Exception("读取后窗口未更新");
        cases.Add("real-button-success-refresh");
        completion=new TaskCompletionSource<int>();diagnosticAgain.PerformClick();Application.DoEvents();
        diagnosticWindow.Close();completion.SetResult(1);PumpUntil(delegate { return !running; });
        if(!diagnosticWindow.Visible || diagnosticWindow.IsDisposed || !diagnosticText.Text.Contains("首次安装失败样例")) throw new Exception("读取期间关闭窗口未恢复");
        cases.Add("closed-during-read-window-recreated-failure-preserved");
        offlineActionFixture=delegate(string action) { if(action != "Diagnose") throw new Exception("动作错误");throw new InvalidOperationException("模拟诊断读取异常"); };
        diagnosticAgain.PerformClick();PumpUntil(delegate { return !running; });
        if(!diagnosticWindow.Visible || !diagnosticText.Text.Contains("模拟诊断读取异常") || !diagnosticAgain.Enabled) throw new Exception("读取异常未保留或窗口退出");
        cases.Add("read-exception-displayed-without-exit");
        diagnosticWindow.Close();ShowDiagnostics(1);Application.DoEvents();
        if(!diagnosticWindow.Visible) throw new Exception("关闭后无法重新打开诊断");cases.Add("closed-window-reopened");
        offlineActionFixture=null;
        File.WriteAllText(output,new System.Web.Script.Serialization.JavaScriptSerializer().Serialize(new Dictionary<string,object> { {"offline",true},{"device_requests",0},{"cases",cases},{"installer_version",InstallerVersion} }),Encoding.UTF8);
    }
#endif
    [STAThread] static void Main(string[] args) {
        Application.EnableVisualStyles(); Application.SetCompatibleTextRenderingDefault(false);
        using (var form = new RawStackInstaller()) {
#if INTERNAL_UI_TESTS
            if(args.Length == 2 && args[0] == "--progress-selftest") { form.CheckProportionalProgress(args[1]); return; }
            if(args.Length == 2 && args[0] == "--progress-preview") {
                form.Show(); Application.DoEvents(); form.running=true;
                form.Append("PATCH_PROGRESS=transfer|1590050|3180100"); Application.DoEvents();
                form.elapsed.Text="总进度 45% · 已用时 3 秒 · 请保持 USB 连接";
                using(var bitmap=new Bitmap(form.Width,form.Height)) { form.DrawToBitmap(bitmap,new Rectangle(Point.Empty,bitmap.Size)); bitmap.Save(args[1]); }
                form.running=false; return;
            }
            if(args.Length == 2 && args[0] == "--compact-ui-selftest") { form.CheckCompactUi(args[1]); return; }
            if(args.Length == 2 && args[0] == "--diagnostic-refresh-selftest") { form.CheckDiagnosticRefresh(args[1]); return; }
            if(args.Length == 2 && args[0] == "--restart-selftest") {
                form.Show(); Application.DoEvents();
                string[] markers = { "STACK_FILES_VERIFIED_MANUAL_REBOOT_REQUIRED", "PATCH_MANUAL_REBOOT_REQUIRED", "PATCH_UNINSTALLED_MANUAL_REBOOT_REQUIRED" };
                string[] expected = { "安装文件已完整核对", "补丁安装完成，请手动关机后重新开机", "电源键所需服务保留。请正常关机后重新开机" };
                var cases = new List<string>();
                for(int i=0;i<markers.Length;i++) {
                    form.log.Clear(); form.Append(markers[i]); Application.DoEvents();
                    if(!form.log.Text.Contains(expected[i]) || form.log.Text.Contains(markers[i]) || !form.operationLog.ToString().Contains(markers[i])) throw new InvalidOperationException("中文重启提示或原始回执丢失");
                    if(i>0 && form.lastResult != markers[i]) throw new InvalidOperationException("重启结果状态丢失");
                    cases.Add(markers[i]);
                }
                var proof = new Dictionary<string,object> { {"offline",true},{"device_requests",0},{"cases",cases},{"raw_receipts_preserved",true},{"chinese_restart_prompt",true} };
                File.WriteAllText(args[1],new System.Web.Script.Serialization.JavaScriptSerializer().Serialize(proof),Encoding.UTF8); return;
            }
            if(args.Length == 2 && args[0] == "--update-selftest") {
                var proof = new Dictionary<string,object> { {"offline",true},{"device_requests",0},{"cases",PatchUpdateState.SelfTest()} };
                File.WriteAllText(args[1],new System.Web.Script.Serialization.JavaScriptSerializer().Serialize(proof),Encoding.UTF8); return;
            }
            if(args.Length == 2 && args[0] == "--update-preview") {
                form.update.BeginAttempt(); form.update.Observe("PATCH_UPDATE_RESTART_PENDING="+new string('a',64)); form.update.Complete("Install",0);
                form.install.Text=form.update.ButtonText; form.state.Text="旧版文件已还原。保持客户端打开，正常关机后重新开机，再点击“检测并安装”。";
                form.Show(); Application.DoEvents();
                using(var bitmap=new Bitmap(form.Width,form.Height)) { form.DrawToBitmap(bitmap,new Rectangle(Point.Empty,bitmap.Size)); bitmap.Save(args[1]); } return;
            }
            if(args.Length == 1 && args[0] == "--diagnostic-selftest") { PatchDiagnosticReport.SelfTest(); return; }
            if(args.Length == 3 && args[0] == "--package-check") {
                var proof = new Dictionary<string,object> { {"offline",true},{"device_requests",0},{"required_files",PatchDiagnosticReport.RuntimeFiles} };
                try { PatchDiagnosticReport.CheckPackage(args[1]); proof["passed"]=true; }
                catch(Exception ex) { proof["passed"]=false; proof["reason"]=PatchDiagnosticReport.Protect(ex.Message); }
                File.WriteAllText(args[2],new System.Web.Script.Serialization.JavaScriptSerializer().Serialize(proof),Encoding.UTF8); return;
            }
            if(args.Length == 2 && args[0] == "--diagnostic-preview") {
                form.Show(); Application.DoEvents();
                form.currentAction="Install";
                form.report=new Dictionary<string,object> { {"protocol",1},{"action","Install"},{"route","upgrade"},{"stage","Verify"},{"reason","Installed file differs（人工故障样例）"},{"command","sha256sum /system/etc/x2d-raw-stack/run.sh"},{"target","/system/etc/x2d-raw-stack/run.sh"},{"expected",new string('a',64)},{"actual",new string('b',64)},{"camera_exit","未取得"},{"state","未确认"},{"readonly","未确认"},{"recovery","未确认；没有重发安装"},{"transport","收到回复"},{"camera_log","本样例没有相机日志"} };
                form.ShowDiagnostics(1); Application.DoEvents();
                using(var bitmap = new Bitmap(form.diagnosticWindow.Width, form.diagnosticWindow.Height)) { form.diagnosticWindow.DrawToBitmap(bitmap,new Rectangle(Point.Empty,bitmap.Size)); bitmap.Save(args[1]); }
                return;
            }
            if (args.Length == 2 && args[0] == "--preview") {
                form.Show(); Application.DoEvents();
                using (var bitmap = new Bitmap(form.Width, form.Height)) { form.DrawToBitmap(bitmap, new Rectangle(Point.Empty, bitmap.Size)); bitmap.Save(args[1]); }
                return;
            }
#else
            if(args.Length != 0) throw new ArgumentException("此程序不提供内部测试入口。");
#endif
            Application.Run(form);
        }
    }
}
