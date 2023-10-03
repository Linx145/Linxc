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
                .outputHeaderIncludePath = headerName.str(),
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


            // var cppFile: std.fs.File = try std.fs.createFileAbsolute(cppFilePath.str(), .{.truncate = true});
            // var cppWriter = cppFile.writer();
            // _ = try cppWriter.write("#include <");
            // _ = try cppWriter.write(headerName.str());
            // _ = try cppWriter.write(">\n");
            // try transpiler.TranspileStatementCpp(alloc, cppWriter, result, true, "", &database, null);
            // cppFile.close();



            var hFile: std.fs.File = try std.fs.createFileAbsolute(hFilePath.str(), .{.truncate = true});
            var hWriter = hFile.writer();
            _ = try hWriter.write("#pragma once\n");
            var state = transpiler.TranspilerState
            {
                .allocator = alloc,
                .database = &database,
                .specializationMap = null,
                .namespaces = std.ArrayList(string).init(alloc),
                .structNames = std.ArrayList(string).init(alloc)
            };
            defer state.deinit();
            try transpiler.TranspileStatementH(alloc, hWriter, result, &state);
            hFile.close();


            ast.ClearCompoundStatement(&result);
        }
        // if (!stopCompilation)
        // {
        //     //go through files one more time and append to them the instantiated structs
        //     for (database.templatedStructs.items) |*templatedStruct|
        //     {
        //         var cppFile: std.fs.File = try std.fs.openFileAbsolute(templatedStruct.outputC.str(), std.fs.File.OpenFlags{.mode = std.fs.File.OpenMode.read_write});
        //         try cppFile.seekFromEnd(0);
        //         var cppWriter = cppFile.writer();
                
        //         //issue: header definitions needs to be appended at the start of the file instead of at the back
        //         //in case there is a mention of the templated struct in the original linxc file,
        //         //it will depend on the template specializations to be in front of it
        //         //however, we also can't just append to the start of the file as we will require the original file's
        //         //include directories in case those mentions of the specialized templates depend on the headers.
                
        //         var oldHFile = try io.ReadFile(templatedStruct.outputH.str(), self.allocator);
        //         defer self.allocator.free(oldHFile);

        //         var miniLexer = lexer.Tokenizer
        //         {
        //             .buffer = oldHFile
        //         };
        //         while (true)
        //         {
        //             var next = miniLexer.peekNextUntilValid();
        //             if (next.id == .Hash)
        //             {
        //                 while (true)
        //                 {
        //                     next = miniLexer.next();
        //                     miniLexer.index = next.end;
        //                     if (next.id == .Nl or next.id == .Eof)
        //                     {
        //                         break;
        //                     }
        //                 }
        //             }
        //             else
        //             {
        //                 break;
        //             }
        //         }
        //         const includes = oldHFile[0..miniLexer.index];
        //         const restOfFile = oldHFile[miniLexer.index..oldHFile.len];
        //         //var hFile: std.fs.File = try std.fs.openFileAbsolute(templatedStruct.outputH.str(), std.fs.File.OpenFlags{.mode = std.fs.File.OpenMode.read_write});
                
        //         //var hWriter = hFile.writer();
        //         var hFile: std.fs.File = try std.fs.createFileAbsolute(templatedStruct.outputH.str(), .{.truncate = true});
        //         var hWriter = hFile.writer();
        //         _ = try hWriter.write(includes);
        //         _ = try hWriter.write("\n");
        //         try transpiler.TranspileTemplatedStruct(hWriter, cppWriter, &database, templatedStruct);
        //         _ = try hWriter.write("\n");
        //         _ = try hWriter.write(restOfFile);
        //         cppFile.close();
        //         hFile.close();
        //     }
        // }

        parser.deinit();
        database.deinit();
    }
};