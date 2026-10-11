// 与 r1 相同的就绪策略；r2 只增强对象收集和诊断。
#pragma once
namespace ResidentGate {
enum Result { Pending, Ready, Failed, TimedOut };
struct Policy {
    int stable = 0;
    Result result = Pending;
    Result sample(bool complete, bool error, long long elapsed) {
        if (result != Pending) return result;
        if (error) return result = Failed;
        if (elapsed >= 30000) return result = TimedOut;
        stable = complete ? stable + 1 : 0;
        if (stable >= 3) result = Ready;
        return result;
    }
};
}
