const std = @import("std");
const string = @import("zig-string.zig").String;
const project = @import("project.zig").project;

pub const Errors = error
{
    InsufficientArguments
};

pub fn PathIsDir(path: []const u8) bool
{
    var dir = std.fs.openDirAbsolute(path, std.fs.Dir.OpenDirOptions{}) catch
    {
        return false;
    };
    dir.close();
    return true;
}

pub fn main() !void {
    const args = try std.process.argsAlloc(std.heap.c_allocator);
    defer std.process.argsFree(std.heap.c_allocator, args);

    if (args.len != 3)// and args.len != 2)
    {
        return Errors.InsufficientArguments;
    }

    var sourcePath: []const u8 = args[1];

    var destDirPath: string = try string.init_with_contents(std.heap.c_allocator, args[2]);
    defer destDirPath.deinit();

    if (PathIsDir(sourcePath)) //parsed folder
    {
        var proj: project = project.init(std.heap.c_allocator, sourcePath);
        try proj.GetFilesToParse();
        try proj.Compile(destDirPath.str());

        proj.deinit();
    }
    else
    {
    }
}