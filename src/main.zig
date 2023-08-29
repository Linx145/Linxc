const std = @import("std");
const sysdata = @import("reflect-systems.zig");
const structdata = @import("reflect-data.zig");
const translator = @import("translator.zig");
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
        // var sourceData: []const u8 = try ReadFile(sourcePath, &std.heap.c_allocator);
        // defer std.heap.c_allocator.free(sourceData);

        // try destDirPath.concat(std.fs.path.stem(args[1]));

        // var result = try translator.TranslateFile(std.heap.c_allocator, sourceData);
        
        // try result.OutputTo(std.heap.c_allocator, destDirPath);

        // result.deinit();
    }
    else
    {
        // var dir = try std.fs.openIterableDirAbsolute(sourcePath, std.fs.Dir.OpenDirOptions{});
        // var walker = try dir.walk(std.heap.c_allocator);
        // defer walker.deinit();
        // while (try walker.next()) |entry| {
        //     const extension: []const u8 = std.fs.path.extension(entry.path);
        //     if (std.mem.eql(u8, extension, ".linxc"))
        //     {
                
        //     }
        //     //std.debug.print("{s}\n", .{entry.path});
        // }

        //dir.close();
    }
}