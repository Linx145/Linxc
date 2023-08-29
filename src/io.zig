const std = @import("std");

pub fn ReadFile(path: []const u8, allocator: std.mem.Allocator) ![]const u8 {
    var file = try std.fs.openFileAbsolute(path, .{});
    defer file.close();

    const fileStats = try file.stat();
    const endpos: u64 = fileStats.size;

    var result: [] u8 = try allocator.alloc(u8, endpos);
    _ = try file.readAll(result);

    return result;
}