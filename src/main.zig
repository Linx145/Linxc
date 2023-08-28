const std = @import("std");
const sysdata = @import("reflect-systems.zig");
const structdata = @import("reflect-data.zig");
const translator = @import("translator.zig");
const string = @import("zig-string.zig").String;

pub const Errors = error
{
    InsufficientArguments
};

pub fn ReadFile(path: []const u8, allocator: *const std.mem.Allocator) ![]const u8 {
    var file = try std.fs.openFileAbsolute(path, .{});
    defer file.close();

    const fileStats = try file.stat();
    const endpos: u64 = fileStats.size;

    var result: [] u8 = try allocator.alloc(u8, endpos);
    _ = try file.readAll(result);

    return result;
}

pub fn main() !void {
    var arenaAllocator: std.heap.ArenaAllocator = std.heap.ArenaAllocator.init(std.heap.c_allocator);
    var alloc = arenaAllocator.allocator();
    defer arenaAllocator.deinit();
    //defer arenaAllocator.deinit();

    //const source: []const u8 = try ReadFile("C:/Users/Linus/source/repos/Linxc/Tests/Test.linxc", &alloc);
    //var outputPath: string = try string.init_with_contents(alloc, "C:/Users/Linus/source/repos/Linxc/Tests/Generated/Test");
    //defer outputPath.deinit();

    const args = try std.process.argsAlloc(alloc);
    defer std.process.argsFree(alloc, args);

    if (args.len != 3 and args.len != 2)
    {
        return Errors.InsufficientArguments;
    }

    var sourceData: []const u8 = try ReadFile(args[1], &alloc);
    defer alloc.free(sourceData);
    var destStr: string = undefined;
    if (args.len == 3)
    {
        destStr = try string.init_with_contents(alloc, args[2]);
    }
    else 
    {
        const stem = std.fs.path.stem(args[1]);
        const dirname = std.fs.path.dirname(args[1]).?;

        destStr = try string.init_with_contents(alloc, dirname);
        try destStr.concat("/Generated/");

        std.fs.makeDirAbsolute(destStr.buffer.?[0..destStr.size]) catch
        {
            
        };

        try destStr.concat(stem);
    }
    //const dest = destStr.buffer.?[0..destStr.size];

    //std.debug.print("{s}", .{dest});

    //const source: []const u8 = try ReadFile(args[0]);

    try translator.TranslateFile(alloc, destStr, sourceData);//ParseFile(alloc, source);

    //alloc.free(source);
}