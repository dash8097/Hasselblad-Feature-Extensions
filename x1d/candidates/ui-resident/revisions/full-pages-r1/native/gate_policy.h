// 同一策略用于 ARM 启动检查与离线故障测试；时间均为单调时钟毫秒。
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
