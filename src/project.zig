const std = @import("std");
const string = @import("zig-string.zig").String;
const transpiler = @import("transpiler.zig");
//const reflector = @import("reflector.zig");
const ast = @import("ASTnodes.zig");
const lexer = @import("lexer.zig");
const refl = @import("reflector.zig");
const parsers = @import("parser.zig");
const parserStates = @import("parser-state.zig");
const Parser = parsers.Parser;
const ParseContext = parserStates.ParseContext;
const ParserState = parserStates.ParserState;
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
        var database: refl.ReflectionDatabase = try refl.ReflectionDatabase.init(self.allocator);
        var parser: Parser = Parser.init(self.allocator, lexer.Tokenizer
        {
            .buffer = ""
        }, &database);
        //parser.postParseStatement = reflector.PostParseStatement;

        //let rootPath be "C:\Users\Linus\source\repos\Linxc\Tests"
        //let outputPath be "C:\Users\Linus\source\repos\Linxc\linxc-out"
        var i: usize = 0;
        var stopCompilation: bool = false;
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
            parser.tokenizer.currentLine = 0;
            parser.tokenizer.charsParsed = 0;

            const parserStateFilename = try string.init_with_contents(alloc, originalFilePath.str());
            var parserState = ParserState
            {
                .outputC = cppFilePath.str(),
                .outputHeader = hFilePath.str(),
                .filename = parserStateFilename,
                .context = ParseContext.other,
                .namespaces = std.ArrayList([]const u8).init(alloc),
                .structNames = std.ArrayList([]const u8).init(alloc),
                .funcNames = std.ArrayList([]const u8).init(alloc),
                .templateTypenames = std.ArrayList([]const u8).init(alloc),
                .braceCount = 0
            };
            defer parserState.deinit();
            var result = parser.Parse(&parserState)
            catch
            {
                std.debug.print("ERROR:", .{});
                for (parser.errorStatements.items) |errorStatement|
                {
                    std.debug.print("{s}\n", .{errorStatement.str()});
                }
                stopCompilation = true;
                break;
            };


            var cppFile: std.fs.File = try std.fs.createFileAbsolute(cppFilePath.str(), .{.truncate = true});
            var cppWriter = cppFile.writer();
            _ = try cppWriter.write("#include <");
            _ = try cppWriter.write(headerName.str());
            _ = try cppWriter.write(">\n");
            try transpiler.TranspileStatementCpp(alloc, cppWriter, result, true, "", &database, null);
            cppFile.close();



            var hFile: std.fs.File = try std.fs.createFileAbsolute(hFilePath.str(), .{.truncate = true});
            var hWriter = hFile.writer();
            _ = try hWriter.write("#pragma once\n");
            try transpiler.TranspileStatementH(alloc, hWriter, result, &database, null);
            hFile.close();


            ast.ClearCompoundStatement(&result);
        }
        if (!stopCompilation)
        {
            //go through files one more time and append to them the instantiated structs
            for (database.templatedStructs.items) |*templatedStruct|
            {
                var cppFile: std.fs.File = try std.fs.openFileAbsolute(templatedStruct.outputC.str(), std.fs.File.OpenFlags{.mode = std.fs.File.OpenMode.read_write});
                try cppFile.seekFromEnd(0);
                var cppWriter = cppFile.writer();
                
                var hFile: std.fs.File = try std.fs.openFileAbsolute(templatedStruct.outputH.str(), std.fs.File.OpenFlags{.mode = std.fs.File.OpenMode.read_write});
                try hFile.seekFromEnd(0);
                var hWriter = hFile.writer();

                try transpiler.TranspileTemplatedStruct(hWriter, cppWriter, &database, templatedStruct);

                cppFile.close();
                hFile.close();
            }
        }

        parser.deinit();
        //test that memory has been correctly transferred
        // for (database.templatedStructs.items) |*templatedStruct|
        // {
        //     const structData: *ast.StructData = &templatedStruct.structData;
        //     var str = try ast.CompoundStatementToString(&structData.*.body, std.heap.c_allocator);
        //     defer str.deinit();
        //     std.debug.print("{s}\n", .{str.str()});
        // }
        database.deinit();
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