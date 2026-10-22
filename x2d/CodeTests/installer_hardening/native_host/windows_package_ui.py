"""Windows 正式前台与原生包校验衔接；不修改冻结的旧界面模板。"""


def replace_once(text, before, after):
    assert text.count(before) == 1, "安装包界面模板变化：" + before[:100]
    return text.replace(before, after)


def diagnostic(text, required):
    # 用户要求：只检查安装必需文件存在，不扫描额外文件或读取说明 TXT。
    import json
    begin = text.index('    internal static readonly string[] RuntimeFiles = ')
    end = text.index('    internal static void CheckPackage(string root) {', begin)
    text = text[:begin] + '    internal static readonly string[] RuntimeFiles = ' + json.dumps('|'.join(required)) + ".Split('|');\n" + text[end:]
    begin = text.index('    internal static void CheckPackage(string root) {')
    end = text.index('    internal static string Protect(', begin)
    return text[:begin] + '''    internal static void CheckPackage(string root) {
        root=System.IO.Path.GetFullPath(root).TrimEnd(System.IO.Path.DirectorySeparatorChar);
        foreach(string name in RuntimeFiles)
            if(!System.IO.File.Exists(System.IO.Path.Combine(root,name)))
                throw new InvalidOperationException("安装包缺少必需文件："+name);
    }

''' + text[end:]


def frontend(text):
    # 安装完成后的查看入口是只读诊断。检测并安装会再次进入更新路由；
    # 仅旧版卸载后的 pending-update 流程才应继续提示该按钮。
    for before, after in (
        ('补丁安装完成，请手动关机后重新开机。重启后点击“检测并安装”。',
         '补丁安装完成，请手动关机后重新开机。重启后点击“诊断”查看运行状态。'),
        ('文件安装完成。请手动重启相机，再点击“检测并安装”。',
         '文件安装完成。请手动重启相机，再点击“诊断”查看运行状态。'),
        ('原厂文件已就绪，无需重复卸载；若尚未重启，请手动关机开机。',
         '原厂文件及卸载收尾已核对完成；具体清理结果请查看本次诊断。'),
    ):
        text = replace_once(text, before, after)
    text = replace_once(text,
        'new {action=action,feature_mask=operationMask,pending=pending}',
        'new Dictionary<string,object>{{"action",action},{"feature_mask",operationMask},{"pending",pending}}')
    # 正式包打开窗口即完成包自检，成功只表示本地文件有效。
    # 内部 UI fixture 不代表该检查通过；交付验证另外执行正式 EXE。
    needle = "    [STAThread] static void Main(string[] args) {"
    method = '''    async Task CheckLocalPackage() {
        if(running) return;
        running=true;
        check.Enabled=install.Enabled=verify.Enabled=diagnose.Enabled=uninstall.Enabled=false;
        foreach(var box in featureBoxes) box.Enabled=false;
        currentAction="PackageCheck";
        context=new Dictionary<string,object> {
            {"protocol",1},{"action","PackageCheck"},{"stage","电脑端安装包文件检查"},
            {"transport","未打开 USB"},{"recovery","未启动相机操作"},
            {"time",DateTimeOffset.Now.ToString("yyyy-MM-dd HH:mm:ss zzz")}
        };
        state.Text="正在检查安装包文件；不会连接相机。";
        try {
            await Task.Run(delegate { PatchDiagnosticReport.CheckPackage(root); });
            state.Text="安装包文件检查通过。选择安装、卸载或诊断后才会连接相机。";
        } catch(Exception ex) {
            context["reason"]=PatchDiagnosticReport.Protect(ex.Message);
            state.Text="安装包文件检查未通过，请查看详细诊断。";
            ShowDiagnostics(1);
        } finally {
            running=false;
            check.Enabled=install.Enabled=verify.Enabled=diagnose.Enabled=uninstall.Enabled=true;
            foreach(var box in featureBoxes) box.Enabled=!update.Pending;
        }
    }
'''
    text = replace_once(text, needle, method + needle)
    text = replace_once(text, needle, needle + '\n#if !INTERNAL_UI_TESTS\n        if(WindowsInternalModes.Dispatch(args)) return;\n#endif')
    return replace_once(
        text,
        "            Application.Run(form);",
        '''#if !INTERNAL_UI_TESTS
            form.Shown += async delegate { await form.CheckLocalPackage(); };
#endif
            Application.Run(form);''',
    )
