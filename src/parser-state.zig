const std = @import("std");
const string = @import("zig-string.zig").String;

pub const ParseContext = enum
{
    other,
    elseWithoutBraces,
    traitDeclaration,
    forLoopInitialization,
    forLoopStep
};

pub const ParserState = struct
{
    filename: string,
    outputHeader: []const u8,
    outputHeaderIncludePath: []const u8,
    outputC: []const u8,
    context: ParseContext,
    namespaces: std.ArrayList([]const u8),
    structNames: std.ArrayList([]const u8),
    funcNames: std.ArrayList([]const u8),
    templateTypenames: std.ArrayList([]const u8),
    braceCount: i32,

    pub fn deinit(self: *@This()) void
    {
        self.filename.deinit();
        self.namespaces.deinit();
        self.structNames.deinit();
        self.funcNames.deinit();
        self.templateTypenames.deinit();
    }
    pub inline fn ConcatNamespaces(self: *@This(), str: *string) anyerror!void
    {
        var i: usize = 0;
        while (i < self.namespaces.items.len) : (i += 1)
        {
            try str.concat(self.namespaces.items[i]);
            if (i < self.namespaces.items.len - 1)
            {
                try str.concat("::");
            }
        }
    }
    pub fn clone_with(self: *@This(), allocator: std.mem.Allocator) anyerror!ParserState
    {
        var namespaces = std.ArrayList([]const u8).init(allocator);
        var structNames = std.ArrayList([]const u8).init(allocator);
        var funcNames = std.ArrayList([]const u8).init(allocator);
        var templateTypenames = std.ArrayList([]const u8).init(allocator);
        var filename = try string.init_with_contents(allocator, self.filename.str());

        for (self.namespaces.items) |namespace|
        {
            try namespaces.append(namespace);
        }
        for (self.structNames.items) |structName|
        {
            try structNames.append(structName);
        }
        for (self.funcNames.items) |funcName|
        {
            try funcNames.append(funcName);
        }
        for (self.templateTypenames.items) |templateTypename|
        {
            try templateTypenames.append(templateTypename);
        }

        return ParserState
        {
            .outputHeaderIncludePath = self.outputHeaderIncludePath,
            .outputHeader = self.outputHeader,
            .outputC = self.outputC,
            .filename = filename,
            .context = self.context,
            .namespaces = namespaces,
            .structNames = structNames,
            .funcNames = funcNames,
            .braceCount = self.braceCount,
            .templateTypenames = templateTypenames
        };
    }
    pub inline fn clone(self: *@This()) anyerror!ParserState
    {
        return self.clone_with(self.structNames.allocator);
    }
};