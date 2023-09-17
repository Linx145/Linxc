const std = @import("std");
const string = @import("zig-string.zig").String;
const transpiler = @import("transpiler.zig");
const reflector = @import("reflector.zig");
const ast = @import("ASTnodes.zig");
const lexer = @import("lexer.zig");
const parsers = @import("parser.zig");
const Parser = parsers.Parser;
const ParseContext = parsers.ParseContext;
const io = @import("io.zig");

const StringList = std.ArrayList(string);

pub const project = struct {
    const Self = @This();
    linxcFiles: StringList,
    allocator: std.mem.Allocator,
    outputPath: ?[]const u8,
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
            .outputPath = null,
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
        reflector.globalDatabase = try reflector.ReflectionDatabase.init(self.allocator);
        var parser: Parser = try Parser.init(self.allocator, lexer.Tokenizer
        {
            .buffer = ""
        });
        parser.postParseStatement = reflector.PostParseStatement;
        //let rootPath be "C:\Users\Linus\source\repos\Linxc\Tests"
        //let outputPath be "C:\Users\Linus\source\repos\Linxc\linxc-out"
        var i: usize = 0;
        while (i < self.linxcFiles.items.len) : (i += 1)
        {
            var arena = std.heap.ArenaAllocator.init(self.allocator);
            defer arena.deinit();
            var alloc = arena.allocator();
            //"C:\Users\Linus\source\repos\Linxc\Tests\Test.linxc"
            var originalFilePath: *string = &self.linxcFiles.items[i];

            //"Test.linxc"
            var relativePath = try std.fs.path.relative(alloc, self.rootPath, originalFilePath.str());
            defer alloc.free(relativePath);

            //C:\Users\Linus\source\repos\Linxc\linxc-out
            var newFilePath = try string.init_with_contents(alloc, outputPath);
            defer newFilePath.deinit();
            
            //ensure new file path directory exists
            std.fs.makeDirAbsolute(newFilePath.str()) catch
            {
                
            };

            //C:\Users\Linus\source\repos\Linxc\linxc-out\
            try newFilePath.concat("/");
            //C:\Users\Linus\source\repos\Linxc\linxc-out\Test
            //no extension so we can generate .cpp and .h files properly
            try newFilePath.concat(io.WithoutExtension(relativePath));

            var cppFilePath = try string.init_with_contents(alloc, newFilePath.str());
            try cppFilePath.concat(".cpp");
            defer cppFilePath.deinit();

            var hFilePath = try string.init_with_contents(alloc, newFilePath.str());
            try hFilePath.concat(".h");
            defer hFilePath.deinit();

            var headerName = string.init(alloc);
            try headerName.concat(io.WithoutExtension(relativePath));
            try headerName.concat(".h");
            defer headerName.deinit();

            var fileContents: []const u8 = try io.ReadFile(originalFilePath.str(), alloc);
            defer alloc.free(fileContents);
            parser.tokenizer = lexer.Tokenizer
            {
                .buffer = fileContents
            };
            parser.currentFile = self.linxcFiles.items[i].str();
            parser.currentLine = 0;
            parser.charsParsed = 0;
            var result = parser.Parse(ParseContext.other, "")
            catch
            {
                std.debug.print("ERROR:", .{});
                for (parser.errorStatements.items) |errorStatement|
                {
                    std.debug.print("{s}\n", .{errorStatement.str()});
                }
                break;
            };


            var cppFile: std.fs.File = try std.fs.createFileAbsolute(cppFilePath.str(), .{.truncate = true});
            var cppWriter = cppFile.writer();
            _ = try cppWriter.write("#include <");
            _ = try cppWriter.write(headerName.str());
            _ = try cppWriter.write(">\n");
            try transpiler.TranspileStatementCpp(cppWriter, result, true, "");
            cppFile.close();



            var hFile: std.fs.File = try std.fs.createFileAbsolute(hFilePath.str(), .{.truncate = true});
            var hWriter = hFile.writer();
            _ = try hWriter.write("#pragma once\n");
            try transpiler.TranspileStatementH(hWriter, result);
            hFile.close();


            ast.ClearCompoundStatement(result);
        }
        parser.deinit();
        reflector.globalDatabase.?.deinit();
    }
    // pub fn Reflect(self: *Self, outputFile: []const u8) !void
    // {
    //     var i: usize = 0;
    //     std.debug.print("Writing to {s}\n", .{outputFile});
    //     var lastTypeID: usize = 0;

    //     var cppFile: std.fs.File = try std.fs.createFileAbsolute(outputFile, .{.truncate = true});
    //     var writer = cppFile.writer();

    //     _ = try writer.write("#include <Reflection.h>\n");

    //     while (i < self.compiledFiles.items.len) : (i += 1)
    //     {
    //         const compiled: *data.FileData = &self.compiledFiles.items[i];
    //         try writer.print("#include <{s}>\n", .{compiled.headerName.?.str()});
    //     }

    //     _ = try writer.write("\n");
    //     _ = try writer.write("namespace Reflection\n");
    //     _ = try writer.write("{\n\n");
    //     _ = try writer.write("void initReflection()\n");
    //     _ = try writer.write("{\n");

    //     i = 0;
    //     while (i < self.compiledFiles.items.len) : (i += 1)
    //     {
    //         const compiled: *data.FileData = &self.compiledFiles.items[i];

    //         var j: usize = 0;
    //         while (j < compiled.structs.items.len) : (j += 1)
    //         {
    //             const structData: *data.StructData = &compiled.structs.items[j];
    //             try structData.OutputReflection(&writer, &lastTypeID);
    //         }
    //     }

    //     _ = try writer.write("}\n");
    //     _ = try writer.write("}\n");
    //     cppFile.close();
    // }
};