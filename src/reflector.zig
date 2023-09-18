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
    currentID: usize,

    pub fn init(allocator: std.mem.Allocator) !ReflectionDatabase
    {
        var result: ReflectionDatabase = ReflectionDatabase {
            .allocator = allocator,
            .nameToType = std.StringHashMap(usize).init(allocator),
            .types = std.ArrayList(LinxcType).init(allocator),
            .currentID = 1
        };
        try result.AddPrimitiveType("int");
        try result.AddPrimitiveType("float");
        try result.AddPrimitiveType("bool");
        try result.AddPrimitiveType("uint");
        try result.AddPrimitiveType("long");
        try result.AddPrimitiveType("ulong");
        try result.AddPrimitiveType("short");
        try result.AddPrimitiveType("ushort");
        try result.AddPrimitiveType("char");
        try result.AddPrimitiveType("double");
        return result;
    }
    pub fn deinit(self: *@This()) void
    {
        for (self.types.items) |*linxcType|
        {
            linxcType.deinit();
        }
        self.types.deinit();
        self.nameToType.deinit();
    }
    pub inline fn GetType(self: *@This(), name: []const u8) Errors!*LinxcType
    {
        const index = self.nameToType.get(name);
        if (index != null)
        {
            return &self.types.items[index.?];
        }
        else
        {
            return Errors.TypeNotFoundError;
        }
    }
    pub inline fn AddPrimitiveType(self: *@This(), name: []const u8) anyerror!void
    {
        const nameStr = try string.init_with_contents(self.allocator, name);
        try self.AddType(LinxcType
        {
            .name = nameStr,
            .functions = std.ArrayList(LinxcFunc).init(self.allocator),
            .variables = std.ArrayList(LinxcVariable).init(self.allocator),
            .isPrimitiveType = true
        });
    }
    pub inline fn GetTypeSafe(self: *@This(), name: []const u8) anyerror!*LinxcType
    {
        if (!self.nameToType.contains(name))
        {
            const nameStr = try string.init_with_contents(self.allocator, name);
            try self.AddType(LinxcType
            {
                .name = nameStr,
                .ID = self.currentID,
                .functions = std.ArrayList(LinxcFunc).init(self.allocator),
                .variables = std.ArrayList(LinxcVariable).init(self.allocator),
                .isPrimitiveType = false,
                .isTrait = false,
                .implsTraits = std.ArrayList(*LinxcType).init(self.allocator),
                .implementedBy = std.ArrayList(*LinxcType).init(self.allocator)
            });
            self.currentID += 1;
        }
        return self.GetType(name);
    }
    pub inline fn AddType(self: *@This(), newType: LinxcType) anyerror!void
    {
        try self.nameToType.put(newType.name.str(), self.types.items.len);
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
    pub fn OutputTo(self: *@This(), writer: std.fs.File.Writer) anyerror!void
    {
        _ = try writer.write("#pragma once\n");
        _ = try writer.write("\n");
        _ = try writer.write("namespace Reflection {\n");
        _ = try writer.write("   Database InitReflection() {\n");

        _ = try writer.write("      Type *type;");
        var i: usize = 0;
        while (i < self.types.items.len) : (i += 1)
        {
            var linxcType: *LinxcType = &self.types.items[i];

            if (!linxcType.isPrimitiveType)
            {
                _ = try writer.write("      type = (Type*)malloc(sizeof(Type));");
                _ = try writer.write("      ty");
            }
        }
        _ = try writer.write("   }\n");
        _ = try writer.write("}");
    }
};

pub var globalDatabase: ?ReflectionDatabase = null;

pub fn GetVariable(varTypeName: []const u8, variableName: []const u8) anyerror!LinxcVariable
{
    var variableTypeName: []const u8 = varTypeName;
    var isPointer: bool = false;
    if (variableTypeName[variableTypeName.len - 1] == '*')
    {
        isPointer = true;
        variableTypeName = variableTypeName[0..variableTypeName.len - 1];
    }
    variableTypeName = string.remove_whitespace_ending(variableTypeName);
    var variableType: *LinxcType = try globalDatabase.?.GetTypeSafe(variableTypeName);

    var variableNameStr = try string.init_with_contents(globalDatabase.?.allocator, variableName);

    return LinxcVariable
    {
        .name = variableNameStr,
        .variableType = variableType,
        .isPointer = isPointer
    };
}
pub fn PostParseStatement(statement: StatementData, parent: ?[]const u8) anyerror!void
{
    var self = &globalDatabase.?;
    //is accessible by a reflection system
    switch (statement)
    {
        //this is always called before the addition of the parent struct if any
        .traitDeclaration => |traitDecl|
        {
            var typePtr: *LinxcType = try self.GetTypeSafe(traitDecl.name);
            typePtr.isTrait = true;
        },
        .structDeclaration => |structDecl|
        {
            var typePtr: *LinxcType = try self.GetTypeSafe(structDecl.name);
            var j: usize = 0;
            while (j < structDecl.tags.?.len) : (j += 1)
            {
                var exprData: *ASTnodes.ExpressionData = &structDecl.tags.?[j];
                if (exprData == .FunctionCall)
                {
                    if (std.mem.eql(u8, exprData.FunctionCall.name, "impl_trait"))
                    {
                        var traitName = exprData.FunctionCall.inputParams[0].Variable;
                        var traitType = try self.GetTypeSafe(traitName);
                        try traitType.implementedBy.append(typePtr);
                        traitType.isTrait = true;
                        try typePtr.implsTraits.append(traitType);
                    }
                }
            }
        },
        .variableDeclaration => |varDecl|
        {
            if (parent != null)
            {
                if (parent.?.len > 0)
                {
                    var variable = try GetVariable(varDecl.typeName, varDecl.name);

                    const name = parent.?;

                    var typePtr: *LinxcType = try self.GetTypeSafe(name);
                    
                    try typePtr.variables.append(variable);
                }
            }
            else //is global variable
            {

            }
        },
        .functionDeclaration => |funcDecl|
        {
            if (parent != null)
            {
                if (parent.?.len > 0)
                {
                    var functionTypeName = funcDecl.name;
                    functionTypeName = string.remove_whitespace_ending(functionTypeName);

                    const returnType = try self.GetTypeSafe(funcDecl.returnType);
                    const funcName = try string.init_with_contents(self.allocator, funcDecl.name);
                    var argsList = std.ArrayList(LinxcVariable).init(self.allocator);

                    for (funcDecl.args) |arg|
                    {
                        var argVariable = try GetVariable(arg.typeName, arg.name);
                        try argsList.append(argVariable);
                    }

                    var slice: ?[]LinxcVariable = null;
                    if (argsList.items.len > 0)
                    {
                        slice = try argsList.toOwnedSlice();
                    }

                    var func = LinxcFunc
                    {
                        .name = funcName,
                        .returnType = returnType,
                        .args = slice
                    };
                
                    const name = parent.?;

                    var typePtr: *LinxcType = try self.GetTypeSafe(name);
                    
                    try typePtr.functions.append(func);
                }
            }
            else //is global function
            {
            }
        },
        else =>
        {

        }
    }
}

pub const LinxcVariable = struct
{
    name: string,
    variableType: *LinxcType,
    isPointer: bool,

    pub fn ToString(self: *@This()) anyerror!string
    {
        var result = string.init(self.name.allocator);
        
        if (self.isPointer)
        {
            try result.concat("*");
        }
        try result.concat(self.variableType.name.str());
        try result.concat(" ");
        try result.concat(self.name.str());

        return result;
    }
    pub fn deinit(self: *@This()) void
    {
        self.name.deinit();
        //dont deinit type - that is the job of ReflectionDatabase's deinit
    }
};
pub const LinxcFunc = struct
{
    name: string,
    returnType: *LinxcType,
    args: ?[]LinxcVariable,
    
    pub fn ToString(self: *@This()) anyerror!string
    {
        var result = string.init(self.name.allocator);

        try result.concat(self.returnType.name.str());
        try result.concat(" ");
        try result.concat(self.name.str());
        try result.concat("(");
        if (self.args != null)
        {
            var i: usize = 0;
            while (i < self.args.?.len) : (i += 1)
            {
                var arg: *LinxcVariable = &self.args.?[i];
                try result.concat(arg.variableType.name.str());
                if (i < self.args.?.len - 1)
                {
                    try result.concat(", ");
                }
            }
        }
        try result.concat(")");
        return result;
    }
    pub fn deinit(self: *@This()) void
    {
        self.name.deinit();
        if (self.args != null)
        {
            for (self.args.?) |*variable|
            {
                variable.deinit();
            }
        }
        //dont deinit type - that is the job of ReflectionDatabase's deinit
    }
};

pub const LinxcType = struct
{
    name: string,
    ID: usize,
    functions: std.ArrayList(LinxcFunc),
    variables: std.ArrayList(LinxcVariable),
    isPrimitiveType: bool,
    isTrait: bool,
    implsTraits: std.ArrayList(*LinxcType),
    implementedBy: std.ArrayList(*LinxcType),

    pub fn ToString(self: *@This()) !string
    {
        var result = string.init(self.name.allocator);
        try result.concat("Type ");
        try result.concat(self.name.str());
        try result.concat(": \n");
        for (self.variables.items) |*variable|
        {
            try result.concat("   ");
            var varString = try variable.ToString();
            try result.concat_deinit(&varString);
            try result.concat("\n");
        }
        for (self.functions.items) |*func|
        {
            try result.concat("   ");
            var funcString = try func.ToString();
            try result.concat_deinit(&funcString);
            try result.concat("\n");
        }
        return result;
    }
    pub fn deinit(self: *@This()) void
    {
        self.implsTraits.deinit();
        self.implementedBy.deinit();

        self.name.deinit();
        for (self.functions.items) |*function|
        {
            function.deinit();
        }
        self.functions.deinit();
        for (self.variables.items) |*variable|
        {
            variable.deinit();
        }
        self.variables.deinit();
    }
};

test "reflector"
{
    const io = @import("io.zig");
    const lexer = @import("lexer.zig");
    const parsers = @import("parser.zig");
    const Parser = parsers.Parser;
    const ParseContext = parsers.ParseContext;

    var arena = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena.deinit();
    var alloc: std.mem.Allocator = arena.allocator();

    var buffer: []const u8 = try io.ReadFile("C:/Users/Linus/source/repos/Linxc/Tests/Vector2.linxc", alloc);//"#include<stdint.h>";

    var tokenizer: lexer.Tokenizer = lexer.Tokenizer
    {
        .buffer = buffer
    };
    globalDatabase = try ReflectionDatabase.init(alloc);
    var parser: Parser = try Parser.init(alloc, tokenizer);
    parser.postParseStatement = PostParseStatement;
    std.debug.print("\n", .{});

    var result = parser.Parse(ParseContext.other, "")
    catch
    {
        std.debug.print("ERROR:", .{});
        for (parser.errorStatements.items) |errorStatement|
        {
            std.debug.print("{s}\n", .{errorStatement.str()});
        }

        parser.deinit();
        globalDatabase.?.deinit();
        alloc.free(buffer);
        return;
    };

    // for (globalDatabase.?.types.items) |*linxcType|
    // {
    //     if (!linxcType.isPrimitiveType)
    //     {
    //         var str = try linxcType.ToString();
    //         std.debug.print("{s}\n", .{str.str()});
    //         str.deinit();
    //     }
    // }
    var resultStr = try ASTnodes.CompoundStatementToString(result, alloc);
    std.debug.print("{s}\n", .{resultStr.str()});
    resultStr.deinit();

    globalDatabase.?.deinit();

    ASTnodes.ClearCompoundStatement(result);

    parser.deinit();

    alloc.free(buffer);
}