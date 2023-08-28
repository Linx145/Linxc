const std = @import("std");
const string = @import("zig-string.zig").String;

pub const FileData = struct
{
    const Self = @This();

    includeDirs: std.ArrayList([]const u8),
    functions: std.ArrayList(FunctionData),
    structs: std.ArrayList(StructData),
    namespace: []const u8,

    pub fn init(allocator: std.mem.Allocator) Self {
        return Self 
        {
            .includeDirs = std.ArrayList([]const u8).init(allocator),
            .functions = std.ArrayList(FunctionData).init(allocator),
            .structs = std.ArrayList(StructData).init(allocator),
            .namespace = ""
        };
    }
    pub fn deinit(self: *Self) void {
        self.functions.deinit();
        self.structs.deinit();
        self.includeDirs.deinit();
    }

    pub fn OutputTo(self: *Self, allocator: std.mem.Allocator, fileWithoutExtension: string) !void
    {
        var headerPath = try fileWithoutExtension.clone();
        try headerPath.concat(".h");
        defer headerPath.deinit();

        var headerName = try string.init_with_contents(allocator, std.fs.path.basename(fileWithoutExtension.buffer.?[0..fileWithoutExtension.size]));
        try headerName.concat(".h");
        defer headerName.deinit();

        var filePath = try fileWithoutExtension.clone();
        try filePath.concat(".cpp");
        defer filePath.deinit();

        //header file
        {
            std.debug.print("Writing to {s}\n", .{headerPath.buffer.?[0..headerPath.size]});
            var headerFile: std.fs.File = try std.fs.createFileAbsolute(headerPath.buffer.?[0..headerPath.size], .{.truncate = true});
            const writer = headerFile.writer();
            
            var i: usize = 0;

            _ = try writer.write("#pragma once\n");

            while (i < self.includeDirs.items.len) : (i += 1)
            {
                try writer.print("#include {s}\n", .{self.includeDirs.items[i]});
            }

            if (self.namespace.len > 0)
            {
                _ = try writer.write("namespace ");
                _ = try writer.write(self.namespace);
                _ = try writer.write("\n{\n");
            }

            i = 0;
            _ = try writer.write("\n");
            while (i < self.functions.items.len) : (i += 1)
            {
                const func: *FunctionData = &self.functions.items[i];
                try writer.print("{s} {s}({s});\n", .{func.returnType, func.name, func.paramsText});
            }

            i = 0;
            _ = try writer.write("\n");
            while (i < self.structs.items.len) : (i += 1)
            {
                const structDat: *StructData = &self.structs.items[i];
                try structDat.OutputHeader(allocator, &writer);
            }

            if (self.namespace.len > 0)
            {
                _ = try writer.write("}");
            }

            headerFile.close();
        }
        //cpp file
        {
            std.debug.print("Writing to {s}\n", .{filePath.buffer.?[0..filePath.size]});
            var cFile: std.fs.File = try std.fs.createFileAbsolute(filePath.buffer.?[0..filePath.size], .{.truncate = true});
            const writer = cFile.writer();
            
            try writer.print("#include <{s}>\n", .{headerName.buffer.?[0..headerName.size]});
            _ = try writer.write("\n");

            if (self.namespace.len > 0)
            {
                _ = try writer.write("namespace ");
                _ = try writer.write(self.namespace);
                _ = try writer.write("\n{\n");
            }

            var i: usize = 0;
            while (i < self.functions.items.len) : (i += 1)
            {
                const func: *FunctionData = &self.functions.items[i];
                try writer.print("{s} {s}({s})\n", .{func.returnType, func.name, func.paramsText});
                _ = try writer.write("{\n");
                try writer.print("{s}", .{func.bodyText});
                _ = try writer.write("}\n");
            }

            i = 0;
            _ = try writer.write("\n");
            while (i < self.structs.items.len) : (i += 1)
            {
                const structDat: *StructData = &self.structs.items[i];
                try structDat.OutputCpp(allocator, &writer);
            }

            if (self.namespace.len > 0)
            {
                _ = try writer.write("}");
            }

            cFile.close();
        }
    }
};

pub const TagData = struct 
{
    const Self = @This();
    name: []const u8,
    args: []const u8,

    pub inline fn GetArgsTokenizer(self: *Self) std.c.Tokenizer
    {
        return std.c.Tokenizer {
            .buffer = self.args
        };
    }
};

pub const FieldData = struct 
{
    const Self = @This();

    name: []const u8,
    //do not store type as ID of structdata as not 
    //all fields are guaranteed to have types that are reflected
    type: []const u8,
    tags: ?[]TagData,
    isStatic: bool,

    pub fn Print(self: *Self) void
    {
        var isStatic: []const u8 = "";
        if (self.isStatic)
        {
            isStatic = "static ";
        }
        std.debug.print("{s}variable: {s}\n", .{isStatic, self.name});
        std.debug.print("-type: {s}\n", .{self.type});
    }
};

pub const FunctionData = struct
{
    const Self = @This();

    name: []const u8,
    returnType: [] const u8,
    paramsText: [] const u8,
    bodyText: [] const u8,
    tags: ?[]TagData,
    isStatic: bool,

    pub inline fn IsConstructor(self: *Self) bool
    {
        return self.returnType.len == 0;
    }

    pub fn Print(self: *Self) void
    {
        if (self.returnType.len == 0)
        {
            std.debug.print("constructor: {s}\n", .{self.name});
        }
        else std.debug.print("function: {s}\n", .{self.name});

        std.debug.print("-TAGS: ", .{});
        var i: usize = 0;
        if (self.tags != null)
        {
            while (i < self.tags.?.len) : (i += 1)
            {
                std.debug.print("{s}, ", .{self.tags.?[i].name});
            }
        }
        std.debug.print("\n", .{});
        std.debug.print("-return type: {s}\n", .{self.returnType});
        std.debug.print("-params text: {s}\n", .{self.paramsText});
        std.debug.print("-body text: {s}\n", .{self.bodyText});
    }
};

pub const StructData = struct
{
    const Self = @This();

    name: []const u8,
    fields: []FieldData,
    methods: []FunctionData,
    ctors: []FunctionData,
    dtor: ?FunctionData,
    tags: ?[]TagData,

    pub fn OutputCpp(self: *Self, allocator: std.mem.Allocator, writer: *const std.fs.File.Writer) !void
    {
        _ = allocator;
        var i: usize = 0;
        while (i < self.ctors.len) : (i += 1)
        {
            const func: *FunctionData = &self.ctors[i];

            try writer.print("{s}::{s}({s})\n", .{self.name, self.name, func.paramsText});
            _ = try writer.write("{");
            _ = try writer.write(func.bodyText);
            _ = try writer.write("\n}\n");
        }
        
        i = 0;
        while (i < self.methods.len) : (i += 1)
        {
            const func: *FunctionData = &self.methods[i];

            //var funcFullName: string = try string.init_with_contents(allocator, self.name);
            //try funcFullName.concat("::");
            //try funcFullName.concat(func.name);

            //var owned: []const u8 = (try funcFullName.toOwned()).?;

            try writer.print("{s} {s}::{s}({s})\n", .{func.returnType, self.name, func.name, func.paramsText});
            _ = try writer.write("{");
            _ = try writer.write(func.bodyText);
            _ = try writer.write("\n}\n");
        }
        if (self.dtor != null)
        {
            try writer.print("{s}::~{s}()\n", .{self.name, self.name});
            _ = try writer.write("{");
            _ = try writer.write(self.dtor.?.bodyText);
            _ = try writer.write("\n}\n");
        }
    }
    pub fn OutputHeader(self: *Self, allocator: std.mem.Allocator, writer: *const std.fs.File.Writer) !void
    {
        _ = allocator;
        try writer.print("struct {s}\n", .{self.name});
        _ = try writer.write("{\n");

        var i: usize = 0;
        while (i < self.fields.len) : (i += 1)
        {
            const field: *FieldData = &self.fields[i];
            try writer.print("   {s} {s};\n", .{field.type, field.name});
        }

        _ = try writer.write("\n");

        i = 0;
        while (i < self.ctors.len) : (i += 1)
        {
            //for constructors, we need translation-time conversion
            //actually we don't, consider moving this to reflection-time
            var strStart: usize = 0;
            var strEnd: usize = 0;
            const func: *FunctionData = &self.ctors[i];
            var firstComma = false;

            var tokenizer: std.c.Tokenizer = std.c.Tokenizer
            {
                .buffer = func.paramsText
            };
            while (true)
            {
                const token = tokenizer.next();
                if (token.id == .Eof)
                {
                    break;
                }
                else
                {
                    //only start concatenating beyond the first comma (EG: After the structname)
                    if (token.id == .Comma)
                    {
                        firstComma = true;
                    }
                    else if (firstComma)
                    {
                        if (strStart == 0)
                        {
                            strStart = token.start;
                        }
                        strEnd = token.end;
                    }
                }
            }
            func.paramsText = func.paramsText[strStart..strEnd]; //set this so that the cpp outputter will have an easier job lataer
            try writer.print("   {s}({s});\n", .{self.name, func.paramsText});
        }

        i = 0;
        while (i < self.methods.len) : (i += 1)
        {
            const func: *FunctionData = &self.methods[i];
            try writer.print("   {s} {s}({s});\n", .{func.returnType, func.name, func.paramsText});
        }

        if (self.dtor != null)
        {
            try writer.print("   ~{s}();\n", .{self.name});
        }

        _ = try writer.write("};\n");
    }

    pub fn Print(self: *Self) void
    {
        std.debug.print("struct: {s}\n", .{self.name});
        var i: usize = 0;
        while (i < self.fields.len) : (i += 1)
        {
            self.fields[i].Print();
        }

        i = 0;
        while (i < self.methods.len) : (i += 1)
        {
            self.methods[i].Print();
        }

        i = 0;
        while (i < self.ctors.len) : (i += 1)
        {
            self.ctors[i].Print();
        }
    }
    // const Self = @This();
    // var lastID: usize = 0;

    // name: []const u8,
    // ID: usize,

    // pub fn init() Self {
    //     lastID += 1;
    //     return Self
    //     {
    //         .name = "",
    //         .ID = lastID
    //     };
    // }
};