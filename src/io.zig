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
pub fn WithoutExtension(input: []const u8) []const u8 {
    var ending: usize = input.len;
    var i: usize = ending - 1;
    while (true)
    {
        if (input[i] == '/' or input[i] == '\\')
        {
            break;
        }
        if (input[i] == '.')
        {
            ending = i;
        }
        if (i == 0)
        {
            break;
        }
        i -= 1;
    }
    return input[0..ending];
}