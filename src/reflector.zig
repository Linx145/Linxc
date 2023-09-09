//the reflector runs on statements produced by the parser and caches the results
//For now, it only runs on statements that are top-level, or within a top-level struct.

const std = @import("std");
const string = @import("zig-string.zig").String;
const Errors = @import("errors.zig").Errors;
const ASTnodes = @import("ASTnodes.zig");
const StatementDataTag = ASTnodes.StatementDataTag;
const StatementData = ASTnodes.StatementData;

pub const ReflectionDatabase = struct
{
    types: std.ArrayList(LinxcType),
    nameToType: std.StringHashMap(usize),
    allocator: std.mem.Allocator,

    pub fn init(allocator: std.mem.Allocator) !ReflectionDatabase
    {
        var result: ReflectionDatabase = ReflectionDatabase {
            .allocator = allocator,
            .nameToType = std.StringHashMap(usize).init(allocator),
            .types = std.ArrayList(LinxcType).init(allocator)
        };
        return result;
    }
    pub fn GetType(self: *@This(), name: []const u8) Errors!*LinxcType
    {
        const index = self.nameToType.get(name);
        if (index != null)
        {
            return &self.types.items[index.?];
        }
        else return Errors.TypeNotFoundError;
    }
    pub fn AddType(self: *@This(), newType: LinxcType) anyerror!void
    {
        try self.nameToType.put(newType.name, self.types.items.len);
        try self.types.append(newType);
    }
    pub inline fn RemoveType(self: *@This(), name: []const u8) anyerror!void
    {
        const index = self.nameToType.get(name).?;
        var currentLastType = self.types.items[self.types.items.len - 1];
        //currentLastType's new index is now index
        try self.nameToType.put(currentLastType.name, index);
        self.types.swapRemove(index);
        self.nameToType.remove(name);
    }
    pub fn PostParseStatement(self: *@This(), statement: StatementData, searchable: bool) anyerror!void
    {
        //is accessible by a reflection system
        if (searchable)
        {
            switch (statement)
            {
                .structDeclaration => |structDecl|
                {
                    const name: string = string.init_with_contents(self.allocator, structDecl.name);
                    const structure = LinxcStruct
                    {
                        .functions = std.ArrayList(LinxcFunc).init(self.allocator),
                        .variables = std.ArrayList(LinxcVariable).init(self.allocator)
                    };
                    const newType: LinxcType = LinxcType
                    {
                        .name = name,
                        .data = LinxcTypeData
                        {
                            .structure = structure
                        }
                    };
                    try self.AddType(newType);
                },
                .variableDeclaration =>
                {

                },
                else =>
                {

                }
            }
        }
    }
};

pub const LinxcVariable = struct
{
    name: string,
    variableType: *LinxcType,

    pub fn deinit(self: *@This()) void
    {
        self.name.deinit();
        //dont deinit type - that is the job of ReflectionDatabase's deinit
    }
};
pub const LinxcFunc = struct
{
    returnType: *LinxcType,
    args: ?[]LinxcVariable,

    pub fn deinit(self: *@This()) void
    {
        if (self.args != null)
        {
            for (self.args.?) |variable|
            {
                variable.deinit();
            }
        }
        //dont deinit type - that is the job of ReflectionDatabase's deinit
    }
};
pub const LinxcStruct = struct
{
    //must be arraylists as they are filled up afterwards as the parser reflects statements
    //that are within the struct
    functions: std.ArrayList(LinxcFunc),
    variables: std.ArrayList(LinxcVariable),

    pub fn deinit(self: *@This()) void
    {
        if (self.functions != null)
        {
            for (self.functions.?.items) |function|
            {
                function.deinit();
            }
            self.functions.?.deinit();
        }
        if (self.variables != null)
        {
            for (self.variables.?.items) |variable|
            {
                variable.deinit();
            }
            self.variables.?.deinit();
        }
    }
};

pub const LinxcTypeDataTag = enum
{
    function,
    structure
};
pub const LinxcTypeData = union(LinxcTypeDataTag)
{
    function: LinxcFunc,
    structure: LinxcStruct
};
pub const LinxcType = struct
{
    name: string,
    data: LinxcTypeData,

    pub fn deinit(self: *@This()) void
    {
        self.name.deinit();
        switch (self.data)
        {
            .function => |*func|
            {
                func.deinit();
            },
            .structure => |*structure|
            {
                structure.deinit();
            }
        }
    }
};