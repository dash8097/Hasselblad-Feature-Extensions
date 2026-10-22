// 仅离线测试的真实 gzip 解码器替身；不随安装包分发。
const std = @import("std");
pub fn main() !void {
    var args = std.process.args();
    _ = args.next();
    if (!std.mem.eql(u8, args.next() orelse "", "gzip") or
        !std.mem.eql(u8, args.next() orelse "", "-dc") or args.next() != null)
        return error.InvalidArguments;
    try std.compress.gzip.decompress(std.io.getStdIn().reader(), std.io.getStdOut().writer());
}
