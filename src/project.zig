const std = @import("std");
const string = @import("zig-string.zig").String;
const structdata = @import("reflect-data.zig");
const translator = @import("translator.zig");
const io = @import("io.zig");

const StringList = std.ArrayList(string);

pub const project = struct {
    const Self = @This();
    linxcFiles: StringList,
    allocator: std.mem.Allocator,
    rootPath: []const u8,

    pub fn init(allocator: std.mem.Allocator, pathToProjFolder: []const u8) Self
    {
        var rootPath = pathToProjFolder;
        if (rootPath[rootPath.len - 1] == '\\' or rootPath[rootPath.len - 1] == '/')
        {
            rootPath = pathToProjFolder[0..(rootPath.len - 1)];
        }

        return Self
        {
            .rootPath = rootPath,
            .allocator = allocator,
            .linxcFiles = StringList.init(allocator)
        };
    }
    pub fn deinit(self: *Self) void
    {
        var i: usize = 0;
        while (i < self.linxcFiles.items.len) : (i += 1)
        {
            self.linxcFiles.items[i].deinit();
        }
        self.linxcFiles.deinit();
    }

    pub fn GetFilesToParse(self: *Self) !void
    {
        var dir = try std.fs.openIterableDirAbsolute(self.rootPath, std.fs.Dir.OpenDirOptions{});
        var walker = try dir.walk(self.allocator);
        defer walker.deinit();
        while (try walker.next()) |entry| {
            const extension: []const u8 = std.fs.path.extension(entry.path);
            if (std.mem.eql(u8, extension, ".linxc"))
            {
                var fullPath = try entry.dir.realpathAlloc(self.allocator, entry.basename);
                defer self.allocator.free(fullPath);
                //fullpath converted into str, can now discard fullpath
                var fullPathStr = try string.init_with_contents(self.allocator, fullPath);

                try self.linxcFiles.append(fullPathStr);
            }
        }

        dir.close();
    }

    pub fn Compile(self: *Self, outputPath: []const u8) !void
    {

        //let rootPath be "C:\Users\Linus\source\repos\Linxc\Tests"
        //let outputPath be "C:\Users\Linus\source\repos\Linxc\linxc-out"
        var i: usize = 0;
        while (i < self.linxcFiles.items.len) : (i += 1)
        {
            //"C:\Users\Linus\source\repos\Linxc\Tests\Test.linxc"
            var originalFilePath: *string = &self.linxcFiles.items[i];

            //"Test.linxc"
            var relativePath = try std.fs.path.relative(self.allocator, self.rootPath, originalFilePath.buffer.?[0..originalFilePath.size]);
            defer self.allocator.free(relativePath);

            //C:\Users\Linus\source\repos\Linxc\linxc-out
            var newFilePath = try string.init_with_contents(self.allocator, outputPath);
            defer newFilePath.deinit();
            
            //ensure new file path directory exists
            std.fs.makeDirAbsolute(newFilePath.buffer.?[0..newFilePath.size]) catch
            {
                
            };

            //C:\Users\Linus\source\repos\Linxc\linxc-out\
            try newFilePath.concat("/");
            //C:\Users\Linus\source\repos\Linxc\linxc-out\Test
            //no extension so we can generate .cpp and .h files properly
            try newFilePath.concat(std.fs.path.stem(relativePath));
            
            var fileContents: []const u8 = try io.ReadFile(originalFilePath.str(), self.allocator);
            defer self.allocator.free(fileContents);
            
            var fileData = try translator.TranslateFile(self.allocator, fileContents);
        
            try fileData.OutputTo(self.allocator, newFilePath);

            fileData.deinit();
        }
    }
};