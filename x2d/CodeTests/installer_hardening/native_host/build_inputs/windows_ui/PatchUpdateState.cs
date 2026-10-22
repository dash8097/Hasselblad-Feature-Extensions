using System;
using System.Collections.Generic;
using System.Text.RegularExpressions;

// 客户端只在子进程成功结束后记住重启续接状态；不自行连接或重发操作。
internal sealed class PatchUpdateState {
    const string Prefix = "PATCH_UPDATE_RESTART_PENDING=";
    string pending = "", candidate = "";
    bool malformed;
    bool waiting;
    public bool Pending { get { return pending.Length != 0; } }
    public string PreviousBootFingerprint { get { return pending; } }
    public string Action { get { return Pending ? "ResumeUpdate" : "Install"; } }
    public string ButtonText { get { return "检测并安装"; } }
    public void BeginAttempt() { candidate = ""; malformed = false; waiting = false; }
    public bool Observe(string line) {
        if(line == "PATCH_UPDATE_RESTART_STILL_PENDING") { waiting=true; return true; }
        if(!line.StartsWith(Prefix, StringComparison.Ordinal)) return false;
        string value = line.Substring(Prefix.Length);
        if(!Regex.IsMatch(value, "\\A[a-f0-9]{64}\\z") || (candidate.Length != 0 && candidate != value)) malformed = true;
        else candidate = value;
        return true; // 开机摘要仅用于续接，不写入日志或诊断。
    }
    public void Complete(string action, int exitCode) {
        if(exitCode != 0) { candidate = ""; return; }
        if(waiting) {
            if(action != "ResumeUpdate" || !Pending || candidate.Length != 0 || malformed) throw new InvalidOperationException("等待重启回执无效。");
            return;
        }
        if(malformed || (candidate.Length != 0 && action != "Install")) throw new InvalidOperationException("更新续接回复无效，未自动继续安装。");
        if(candidate.Length != 0) pending = candidate;
        else if(action == "ResumeUpdate" || action == "Install" || action == "Uninstall") pending = "";
        candidate = "";
    }
    public static string[] SelfTest() {
        var cases = new List<string>(); var state = new PatchUpdateState();
        if(state.Pending || state.Action != "Install") throw new Exception("Unexpected initial update state"); cases.Add("initial-install");
        state.BeginAttempt(); state.Observe(Prefix+new string('a',64));
        if(state.Pending) throw new Exception("Pending before process success"); cases.Add("no-early-commit");
        state.Complete("Install",1); if(state.Pending) throw new Exception("Committed failed operation"); cases.Add("failed-uninstall-no-resume");
        state.BeginAttempt(); if(!state.Observe(Prefix+new string('a',64))) throw new Exception("Private token not consumed");
        state.Complete("Install",0); if(!state.Pending || state.Action != "ResumeUpdate" || state.ButtonText != "检测并安装") throw new Exception("Resume action missing"); cases.Add("same-button-resume");
        state.BeginAttempt(); state.Observe("PATCH_UPDATE_RESTART_STILL_PENDING"); state.Complete("ResumeUpdate",0);
        if(!state.Pending) throw new Exception("Premature retry cleared reboot state"); cases.Add("same-boot-wait-keeps-state");
        state.BeginAttempt(); state.Complete("ResumeUpdate",1); if(!state.Pending) throw new Exception("Lost pending after failed resume"); cases.Add("failed-resume-keeps-state");
        state.BeginAttempt(); state.Complete("Check",0); if(!state.Pending) throw new Exception("Check discarded update"); cases.Add("check-keeps-state");
        state.BeginAttempt(); state.Complete("ResumeUpdate",0); if(state.Pending) throw new Exception("Completed resume retained"); cases.Add("successful-resume-clears");
        foreach(string token in new[]{"", "bad", new string('A',64), new string('a',64)+"\n"}) {
            state.BeginAttempt(); state.Observe(Prefix+token); bool rejected=false;
            try { state.Complete("Install",0); } catch(InvalidOperationException) { rejected=true; }
            if(!rejected || state.Pending) throw new Exception("Malformed update marker accepted");
        } cases.Add("malformed-markers-rejected");
        state.BeginAttempt(); state.Observe(Prefix+new string('a',64)); state.Observe(Prefix+new string('b',64));
        bool conflict=false; try { state.Complete("Install",0); } catch(InvalidOperationException) { conflict=true; }
        if(!conflict) throw new Exception("Conflicting update markers accepted"); cases.Add("conflicting-markers-rejected");
        state.BeginAttempt(); state.Observe(Prefix+new string('a',64)); state.Complete("Install",0);
        state.BeginAttempt(); state.Complete("Uninstall",0); if(state.Pending) throw new Exception("Removal retained update"); cases.Add("standalone-removal-clears");
        if(state.Observe("PATCH_MANUAL_REBOOT_REQUIRED")) throw new Exception("Ordinary output consumed"); cases.Add("ordinary-output-retained");
        return cases.ToArray();
    }
}
