const std = @import("std");
const string = @import("zig-string.zig").String;

pub const FileData = struct
{
    const Self = @This();

    includeDirs: std.ArrayList(string),
    functions: std.ArrayList(FunctionData),
    structs: std.ArrayList(StructData),
    namespace: ?string,
    headerName: ?string,

    pub fn init(allocator: std.mem.Allocator) Self {
        return Self 
        {
            .includeDirs = std.ArrayList(string).init(allocator),
            .functions = std.ArrayList(FunctionData).init(allocator),
            .structs = std.ArrayList(StructData).init(allocator),
            .namespace = null,
            .headerName = null
        };
    }
    pub fn deinit(self: *Self) void {
        

        self.functions.deinit();
        self.structs.deinit();
        self.includeDirs.deinit();
    }

    pub fn OutputTo(self: *Self, allocator: std.mem.Allocator, fileWithoutExtension: string, rootPath: []const u8) !void
    {
        var headerPath = try fileWithoutExtension.clone();
        try headerPath.concat(".h");
        defer headerPath.deinit();

        var filePath = try fileWithoutExtension.clone();
        try filePath.concat(".cpp");
        defer filePath.deinit();

        var headerName = string.init(allocator);
        const relative = try std.fs.path.relative(allocator, rootPath, fileWithoutExtension.str());
        try headerName.concat(relative);
        try headerName.concat(".h");
        self.headerName = headerName;

        //header file
        {
            std.debug.print("Writing to {s}\n", .{headerPath.str()});
            var headerFile: std.fs.File = try std.fs.createFileAbsolute(headerPath.str(), .{.truncate = true});
            const writer = headerFile.writer();
            
            var i: usize = 0;

            _ = try writer.write("#pragma once\n");

            while (i < self.includeDirs.items.len) : (i += 1)
            {
                try writer.print("#include {s}\n", .{self.includeDirs.items[i].str()});
            }

            if (self.namespace != null)
            {
                _ = try writer.write("namespace ");
                _ = try writer.write(self.namespace.?.str());
                _ = try writer.write("\n{\n");
            }

            i = 0;
            _ = try writer.write("\n");
            while (i < self.functions.items.len) : (i += 1)
            {
                const func: *FunctionData = &self.functions.items[i];
                try writer.print("{s} {s}({s});\n", .{func.returnType.str(), func.name.str(), func.paramsText.str()});
            }

            i = 0;
            _ = try writer.write("\n");
            while (i < self.structs.items.len) : (i += 1)
            {
                const structDat: *StructData = &self.structs.items[i];
                try structDat.OutputHeader(allocator, &writer);
            }

            if (self.namespace != null)
            {
                _ = try writer.write("}");
            }

            headerFile.close();
        }
        //cpp file
        {
            std.debug.print("Writing to {s}\n", .{filePath.str()});
            var cFile: std.fs.File = try std.fs.createFileAbsolute(filePath.str(), .{.truncate = true});
            const writer = cFile.writer();
            
            try writer.print("#include <{s}>\n", .{headerName.str()});
            _ = try writer.write("\n");

            if (self.namespace != null)
            {
                _ = try writer.write("namespace ");
                _ = try writer.write(self.namespace.?.str());
                _ = try writer.write("\n{\n");
            }

            var i: usize = 0;
            while (i < self.functions.items.len) : (i += 1)
            {
                const func: *FunctionData = &self.functions.items[i];
                try writer.print("{s} {s}({s})\n", .{func.returnType.str(), func.name.str(), func.paramsText.str()});
                _ = try writer.write("{\n");
                try writer.print("{s}", .{func.bodyText.str()});
                _ = try writer.write("}\n");
            }

            i = 0;
            _ = try writer.write("\n");
            while (i < self.structs.items.len) : (i += 1)
            {
                const structDat: *StructData = &self.structs.items[i];
                try structDat.OutputCpp(allocator, &writer);
            }

            if (self.namespace != null)
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
    name: string,
    args: string,

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

    name: string,
    //do not store type as ID of structdata as not 
    //all fields are guaranteed to have types that are reflected
    type: string,
    tags: ?[]TagData,
    isStatic: bool,

    pub fn Print(self: *Self) void
    {
        var isStatic: []const u8 = "";
        if (self.isStatic)
        {
            isStatic = "static ";
        }
        std.debug.print("{s}variable: {s}\n", .{isStatic, self.name.str()});
        std.debug.print("-type: {s}\n", .{self.type.str()});
    }
    pub fn deinit(self: *Self) void
    {
        self.name.deinit();
        self.type.deinit();
        if (self.tags != null)
        {
            for (self.tags.?) |tag|
            {
                tag.deinit();
            }
        }
    }
};

pub const FunctionData = struct
{
    const Self = @This();

    name: string,
    returnType: string,
    paramsText: string,
    bodyText: string,
    tags: ?[]TagData,
    isStatic: bool,

    pub fn deinit(self: *Self) void
    {
        self.name.deinit();
        self.returnType.deinit();
        self.paramsText.deinit();
        self.bodyText.deinit();

        if (self.tags != null)
        {
            for (self.tags.?) |tag|
            {
                tag.deinit();
            }
        }
    }

    pub inline fn IsConstructor(self: *Self) bool
    {
        return self.returnType.len == 0;
    }

    pub fn Print(self: *Self) void
    {
        if (self.returnType.len == 0)
        {
            std.debug.print("constructor: {s}\n", .{self.name.str()});
        }
        else std.debug.print("function: {s}\n", .{self.name.str()});

        std.debug.print("-TAGS: ", .{});
        var i: usize = 0;
        if (self.tags != null)
        {
            while (i < self.tags.?.len) : (i += 1)
            {
                std.debug.print("{s}, ", .{self.tags.?[i].name.str()});
            }
        }
        std.debug.print("\n", .{});
        std.debug.print("-return type: {s}\n", .{self.returnType.str()});
        std.debug.print("-params text: {s}\n", .{self.paramsText.str()});
        std.debug.print("-body text: {s}\n", .{self.bodyText.str()});
    }
};

pub const StructData = struct
{
    const Self = @This();

    name: string,
    fields: []FieldData,
    methods: []FunctionData,
    ctors: []FunctionData,
    dtor: ?FunctionData,
    tags: ?[]TagData,

    pub fn deinit(self: *Self) void 
    {
        self.name.deinit();
        for (self.fields) |field|
        {
            field.deinit();
        }
        for (self.methods) |method|
        {
            method.deinit();
        }
        for (self.ctors) |ctor|
        {
            ctor.deinit();
        }
        if (self.dtor != null)
        {
            self.dtor.?.deinit();
        }
        if (self.tags != null)
        {
            for (self.tags.?) |tag|
            {
                tag.deinit();
            }
        }
    }

    pub fn OutputCpp(self: *Self, allocator: std.mem.Allocator, writer: *const std.fs.File.Writer) !void
    {
        _ = allocator;
        var i: usize = 0;
        while (i < self.ctors.len) : (i += 1)
        {
            const func: *FunctionData = &self.ctors[i];

            try writer.print("{s}::{s}({s})\n", .{self.name.str(), self.name.str(), func.paramsText.str()});
            _ = try writer.write("{");
            _ = try writer.write(func.bodyText.str());
            _ = try writer.write("\n}\n");
        }
        
        i = 0;
        while (i < self.methods.len) : (i += 1)
        {
            const func: *FunctionData = &self.methods[i];

            try writer.print("{s} {s}::{s}({s})\n", .{func.returnType.str(), self.name.str(), func.name.str(), func.paramsText.str()});
            _ = try writer.write("{");
            _ = try writer.write(func.bodyText.str());
            _ = try writer.write("\n}\n");
        }
        if (self.dtor != null)
        {
            try writer.print("{s}::~{s}()\n", .{self.name.str(), self.name.str()});
            _ = try writer.write("{");
            _ = try writer.write(self.dtor.?.bodyText.str());
            _ = try writer.write("\n}\n");
        }
    }
    pub fn OutputHeader(self: *Self, allocator: std.mem.Allocator, writer: *const std.fs.File.Writer) !void
    {
        try writer.print("struct {s}\n", .{self.name.str()});
        _ = try writer.write("{\n");

        var i: usize = 0;
        while (i < self.fields.len) : (i += 1)
        {
            const field: *FieldData = &self.fields[i];
            try writer.print("   {s} {s};\n", .{field.type.str(), field.name.str()});
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
                .buffer = func.paramsText.str()
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
            const newParamsText = try string.init_with_contents(allocator, func.paramsText.str()[strStart..strEnd]);
            func.paramsText.deinit();
            func.paramsText = newParamsText; //set this so that the cpp outputter will have an easier job lataer
            try writer.print("   {s}({s});\n", .{self.name.str(), func.paramsText.str()});
        }

        i = 0;
        while (i < self.methods.len) : (i += 1)
        {
            const func: *FunctionData = &self.methods[i];
            try writer.print("   {s} {s}({s});\n", .{func.returnType.str(), func.name.str(), func.paramsText.str()});
        }

        if (self.dtor != null)
        {
            try writer.print("   ~{s}();\n", .{self.name.str()});
        }

        _ = try writer.write("};\n");
    }
    pub fn OutputReflection(self: *Self, writer: *std.fs.File.Writer, ID: *usize) !void
    {
        _ = try writer.write("{\n");
        _ = try writer.write("   Reflection::Type type;\n");
        try writer.print("   type.ID = {d};\n", .{ID.*});
        try writer.print("   type.name = \"{s}\";\n", .{self.name.str()});
        for (self.fields) |field|
        {
            if (!field.isStatic)
            {
                try writer.print("   type.variables[\"{s}\"]", .{field.name.str()});
                _ = try writer.write(" = [](void *instance) -> void * { return &((");
                try writer.print("{s}*)instance)->{s};", .{self.name.str(), field.name.str()});
                _ = try writer.write(" };\n");
            }
        }
        try writer.print("   Typeof<{s}>::type = type;\n", .{self.name.str()});
        _ = try writer.write("}\n");

        ID.* += 1;
    }

    pub fn Print(self: *Self) void
    {
        std.debug.print("struct: {s}\n", .{self.name.str()});
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
};