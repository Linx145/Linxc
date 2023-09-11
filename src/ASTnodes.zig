const std = @import("std");
const string = @import("zig-string.zig").String;
const Errors = @import("errors.zig").Errors;

pub const VarData = struct
{
    name: []const u8,
    typeName: []const u8,
    isConst: bool,
    defaultValue: ?ExpressionChain,

    pub fn ToString(self: *@This(), allocator: std.mem.Allocator) !string
    {
        var str = string.init(allocator);
        if (self.isConst)
        {
            try str.concat("const ");
        }
        try str.concat(self.typeName);
        try str.concat(" ");
        try str.concat(self.name);
        if (self.defaultValue != null)
        {
            try str.concat(" = ");
            var defaultValueStr = try self.defaultValue.?.ToString(allocator);
            
            try str.concat_deinit(&defaultValueStr);
        }
        return str;
    }
    pub fn deinit(self: *@This(), allocator: std.mem.Allocator) void
    {
        //self.name.deinit();
        //self.typeName.deinit();
        if (self.defaultValue != null)
        {
            self.defaultValue.?.deinit(allocator);
        }
    }
};
pub const TagData = struct 
{
    name: []const u8,
    args: []VarData,

    pub fn deinit(self: *@This(), allocator: std.mem.Allocator) void
    {
        //self.name.deinit();
        for (self.args) |arg|
        {
            arg.deinit(allocator);
        }
    }
};
pub const MacroDefinitionData = struct
{
    name: []const u8,
    args: [][]const u8,
    expandsTo: []const u8
};
pub const FunctionCallData = struct
{
    name: []const u8,
    inputParams: []ExpressionChain,

    pub fn ToString(self: *@This(), allocator: std.mem.Allocator) !string
    {
        var str = string.init(allocator);

        try str.concat(self.name);
        try str.concat("(");
        var i: usize = 0;
        while (i < self.inputParams.len) : (i += 1)
        {
            var exprStr = try self.inputParams[i].ToString(allocator);
            try str.concat_deinit(&exprStr);
            if (i < self.inputParams.len - 1)
            {
                try str.concat(", ");
            }
        }
        try str.concat(")");

        return str;
    }
    pub fn deinit(self: *@This(), allocator: std.mem.Allocator) void
    {
        for (self.inputParams) |*param|
        {
            param.deinit(allocator);
        }
        allocator.free(self.inputParams);
    }
};
pub const FunctionData = struct
{
    name: []const u8,
    returnType: []const u8,
    args: []VarData,
    statement: CompoundStatementData,

    pub fn ToString(self: *@This(), allocator: std.mem.Allocator) anyerror!string
    {
        var compoundStatementString = try CompoundStatementToString(self.statement, allocator);

        var str = string.init(allocator);
        try str.concat(self.returnType);
        try str.concat(" ");
        try str.concat(self.name);
        try str.concat("(");
        var i: usize = 0;
        while (i < self.args.len) : (i += 1)
        {
            const arg = &self.args[i];
            var argsStr = try arg.ToString(allocator);
            try str.concat_deinit(&argsStr);
            if (i < self.args.len - 1)
            {
                try str.concat(", ");
            }
        }
        try str.concat(") {\n");
        try str.concat_deinit(&compoundStatementString);
        try str.concat("}");
        return str;
    }
    pub fn deinit(self: *@This(), allocator: std.mem.Allocator) void
    {
        //self.name.deinit();
        //self.returnType.deinit();
        for (self.args) |*arg|
        {
            arg.deinit(allocator);
        }
        allocator.free(self.args);
        for (self.statement.items) |*statement|
        {
            statement.deinit(allocator);
        }
        self.statement.deinit();
    }
};
pub const ExpressionDataTag = enum
{
    Literal,
    Variable,
    Op,
    FunctionCall,
    IndexedAccessor
};
pub const ExpressionData = union(ExpressionDataTag)
{
    Literal: []const u8,
    Variable: []const u8,
    Op: *OperatorData,
    FunctionCall: *FunctionCallData,
    IndexedAccessor: *FunctionCallData,

    pub fn deinit(self: *@This(), alloc: std.mem.Allocator) void
    {
        switch (self.*)
        {
            ExpressionDataTag.Op => |*value|
            {
                value.*.deinit(alloc);
                alloc.destroy(value.*);
            },
            ExpressionDataTag.FunctionCall => |*value|
            {
                value.*.deinit(alloc);
                alloc.destroy(value.*);
            },
            ExpressionDataTag.IndexedAccessor => |*value|
            {
                value.*.deinit(alloc);
                alloc.destroy(value.*);
            },
            else => {}
        }
    }

    pub fn ToString(self: @This(), allocator: std.mem.Allocator) anyerror!string
    {
        var str = string.init(allocator);
        switch (self)
        {
            ExpressionDataTag.Literal => |literal| 
            {
                try str.concat(literal);
            },
            ExpressionDataTag.Variable => |variable|
            {
                try str.concat(variable);
            },
            ExpressionDataTag.Op => |op| 
            {
                str.deinit();
                return op.ToString(allocator);
            },
            ExpressionDataTag.FunctionCall => |call| 
            {
                str.deinit();
                return call.ToString(allocator);
            },
            ExpressionDataTag.IndexedAccessor => |call|
            {
                str.deinit();
                return call.ToString(allocator);
            }
        }
        return str;
    }
};
pub const ExpressionChain = struct
{
    expression: ExpressionData,
    next: ?*ExpressionChain = null,

    pub fn deinit(self: *@This(), allocator: std.mem.Allocator) void
    {
        self.expression.deinit(allocator);
        if (self.next != null)
        {
            self.next.?.deinit(allocator);
            allocator.destroy(self.next.?);
        }
    }
    pub fn ToString(self: *@This(), allocator: std.mem.Allocator) anyerror!string
    {
        var result: string = string.init(allocator);
        var myExpression: string = try self.expression.ToString(allocator);
        try result.concat_deinit(&myExpression);

        if (self.next != null)
        {
            try result.concat(".");
            var nextExpression: string = try self.next.?.ToString(allocator);
            try result.concat_deinit(&nextExpression);
        }
        return result;
    }
};
pub const Operator = enum
{
    Plus, //+
    Minus, //-
    Divide, // /
    Multiply, //*
    Not, // !
    Equals, //==
    NotEquals, // !=
    LessThan, //<
    LessThanEquals, //<=
    MoreThan, //>
    MoreThanEquals, //>=
    And, //&&
    Or, // ||
    Modulo, //%
    BitwiseAnd, //&
    BitwiseOr, // |
    BitwiseXOr, // ^
    LeftShift, // <<
    RightShift, // >>
    BitwiseNot, // ~
    PlusEqual,
    MinusEqual,
    AsteriskEqual,
    SlashEqual,
    PercentEqual,
    Equal
};
pub const TokenToOperator = std.ComptimeStringMap(Operator, .{
    .{"Plus", Operator.Plus},
    .{"Minus", Operator.Minus},
    .{"Slash", Operator.Divide},
    .{"Asterisk", Operator.Multiply},
    .{"Bang", Operator.Not},
    .{"EqualEqual", Operator.Equals},
    .{"BangEqual", Operator.NotEquals},
    .{"AngleBracketLeft", Operator.LessThan},
    .{"AngleBracketLeftEqual", Operator.LessThanEquals},
    .{"AngleBracketRight", Operator.MoreThan},
    .{"AngleBracketRightEqual", Operator.MoreThanEquals},
    .{"AmpersandAmpersand", Operator.And},
    .{"PipePipe", Operator.Or},
    .{"Percent", Operator.Modulo},
    .{"Ampersand", Operator.BitwiseAnd},
    .{"Pipe", Operator.BitwiseOr},
    .{"Caret", Operator.BitwiseXOr},
    .{"AngleBracketAngleBracketLeft", Operator.LeftShift},
    .{"AngleBracketAngleBracketRight", Operator.RightShift},
    .{"Tilde", Operator.BitwiseNot},
    .{"PlusEqual", Operator.PlusEqual},
    .{"MinusEqual", Operator.MinusEqual},
    .{"AsteriskEqual", Operator.AsteriskEqual},
    .{"SlashEqual", Operator.SlashEqual},
    .{"PercentEqual", Operator.PercentEqual},
    .{"Equal", Operator.Equal}
});
pub const OperatorData = struct
{
    leftExpression: ExpressionChain,
    operator: Operator,
    rightExpression: ExpressionChain,

    pub fn deinit(self: *@This(), alloc: std.mem.Allocator) void
    {
        self.leftExpression.deinit(alloc);
        self.rightExpression.deinit(alloc);
    }
    pub fn ToString(self: *@This(), allocator: std.mem.Allocator) !string
    {
        var leftString = try self.leftExpression.ToString(allocator);
        var rightString = try self.rightExpression.ToString(allocator);

        var str: string = string.init(allocator);
        try str.concat("(");
        try str.concat_deinit(&leftString);
        try str.concat(" ");
        try str.concat(@tagName(self.operator));
        try str.concat(" ");
        try str.concat_deinit(&rightString);
        try str.concat(")");

        return str;
    }
};
pub const WhileData = struct
{
    condition: ExpressionChain,
    statement: CompoundStatementData,

    pub fn ToString(self: *@This(), allocator: std.mem.Allocator) !string
    {
        var str = string.init(allocator);

        var conditionStr = try self.condition.ToString(allocator);
        var statementStr = try CompoundStatementToString(self.statement, allocator);
    
        try str.concat("while (");
        try str.concat_deinit(&conditionStr);
        try str.concat(") {\n");
        try str.concat_deinit(&statementStr);
        try str.concat("}\n");

        return str;
    }
    pub fn deinit(self: *@This(), allocator: std.mem.Allocator) void
    {
        self.condition.deinit(allocator);
        for (self.statement.items) |*statement|
        {
            statement.deinit(allocator);
        }
        self.statement.deinit();
    }
};
pub const ForData = struct
{
    initializer: CompoundStatementData,
    condition: ExpressionChain,
    shouldStep: CompoundStatementData,
    statement: CompoundStatementData,

    pub fn deinit(self: *@This(), allocator: std.mem.Allocator) void
    {
        for (self.initializer.items) |*statement|
        {
            statement.deinit(allocator);
        }
        self.condition.deinit(allocator);
        for (self.statement.items) |*statement|
        {
            statement.deinit(allocator);
        }
        for (self.shouldStep.items) |*statement|
        {
            statement.deinit(allocator);
        }
        self.initializer.deinit();
        self.statement.deinit();
        self.shouldStep.deinit();
    }
    pub fn ToString(self: *@This(), allocator: std.mem.Allocator) !string
    {
        var str = string.init(allocator);

        var initializer = try CompoundStatementToString(self.initializer, allocator);
        var condition = try self.condition.ToString(allocator);
        var shouldStep = try CompoundStatementToString(self.shouldStep, allocator);
        var statement = try CompoundStatementToString(self.statement, allocator);

        try str.concat("for (");
        try str.concat_deinit(&initializer);
        try str.concat("; ");
        try str.concat_deinit(&condition);
        try str.concat("; ");
        try str.concat_deinit(&shouldStep);
        try str.concat(") {\n");
        try str.concat_deinit(&statement);
        try str.concat("}\n");

        return str;
    }
};
pub const IfData = struct
{
    condition: ExpressionChain,
    statement: CompoundStatementData,

    pub fn ToString(self: *@This(), allocator: std.mem.Allocator) !string
    {
        var str = string.init(allocator);

        var conditionStr = try self.condition.ToString(allocator);
        var statementStr = try CompoundStatementToString(self.statement, allocator);
    
        try str.concat("if (");
        try str.concat_deinit(&conditionStr);
        try str.concat(") {\n");
        try str.concat_deinit(&statementStr);
        try str.concat("}");

        return str;
    }
    pub fn deinit(self: *@This(), allocator: std.mem.Allocator) void
    {
        self.condition.deinit(allocator);
        for (self.statement.items) |*statement|
        {
            statement.deinit(allocator);
        }
        self.statement.deinit();
    }
};
pub const StructData = struct
{
    name: []const u8,
    body: CompoundStatementData
,
    pub fn ToString(self: *@This(), allocator: std.mem.Allocator) !string
    {
        var str = string.init(allocator);
        try str.concat("struct ");
        try str.concat(self.name);
        try str.concat(" { \n");

        var bodyStr = try CompoundStatementToString(self.body, allocator);
        try str.concat_deinit(&bodyStr);

        try str.concat("}\n");
        return str;
    }
    pub fn deinit(self: *@This(), allocator: std.mem.Allocator) void
    {
        for (self.body.items) |*item|
        {
            item.deinit(allocator);
        }
        self.body.deinit();
    }
};
pub const StatementDataTag = enum
{
    functionDeclaration,
    variableDeclaration,
    constructorDeclaration,
    destructorDeclaration,
    structDeclaration,
    functionInvoke,
    returnStatement,
    otherExpression,
    IfStatement,
    ElseStatement,
    WhileStatement,
    ForStatement,
    Comment,
    includeStatement,
    //macroDefinition
};
pub const StatementData = union(StatementDataTag)
{
    functionDeclaration: FunctionData,
    variableDeclaration: VarData,
    constructorDeclaration: FunctionData,
    destructorDeclaration: FunctionData,
    structDeclaration: StructData,
    functionInvoke: FunctionCallData,
    returnStatement: ExpressionChain,
    otherExpression: ExpressionChain,
    IfStatement: IfData,
    ElseStatement: CompoundStatementData,
    WhileStatement: WhileData,
    ForStatement: ForData,
    Comment: []const u8,
    includeStatement: []const u8,
    //macroDefinition: MacroDefinitionData

    pub fn deinit(self: *@This(), allocator: std.mem.Allocator) void
    {
        switch (self.*)
        {
            .functionDeclaration => |*decl| decl.deinit(allocator),
            .variableDeclaration => |*decl| decl.deinit(allocator),
            .constructorDeclaration => |*decl| decl.deinit(allocator),
            .destructorDeclaration => |*decl| decl.deinit(allocator),
            .structDeclaration => |*decl| decl.deinit(allocator),
            .functionInvoke => |*invoke| invoke.deinit(allocator),
            .returnStatement => |*stmt| stmt.deinit(allocator),
            .otherExpression => |*stmt| stmt.deinit(allocator),
            .IfStatement => |*ifData| ifData.deinit(allocator),
            .ElseStatement => |*elseData|
            {
                ClearCompoundStatement(elseData.*);
            },
            .WhileStatement => |*whileData| whileData.deinit(allocator),
            .ForStatement => |*forData| forData.deinit(allocator),
            else =>
            {
                //return Errors.NotImplemented;
            }
        }
    }
    pub fn ToString(self: *@This(), allocator: std.mem.Allocator) !string
    {
        switch (self.*)
        {
            .functionDeclaration => |*decl| return decl.ToString(allocator),
            .variableDeclaration => |*decl| return decl.ToString(allocator),
            .constructorDeclaration => |*decl| return decl.ToString(allocator),
            .destructorDeclaration => |*decl| return decl.ToString(allocator),
            .structDeclaration => |*decl| return decl.ToString(allocator),
            .functionInvoke => |*invoke| return invoke.ToString(allocator),
            .returnStatement => |*stmt| 
            {
                var stmtStr = try stmt.ToString(allocator);
                var str = string.init(allocator);
                try str.concat("returns ");
                try str.concat_deinit(&stmtStr);
                return str;
            },
            .otherExpression => |*stmt| return stmt.ToString(allocator),
            .IfStatement => |*ifDat| return ifDat.ToString(allocator),
            .ElseStatement => |*elseDat|
            {
                var stmtStr = try CompoundStatementToString(elseDat.*, allocator);
                var str = string.init(allocator);
                try str.concat("else {\n");
                try str.concat_deinit(&stmtStr);
                try str.concat("}");
                return str;
            },
            .WhileStatement => |*whileDat| return whileDat.ToString(allocator),
            .includeStatement => |include|
            {
                return string.init_with_contents(allocator, include);
            },
            .ForStatement => |*forDat| return forDat.ToString(allocator),
            else =>
            {
                return Errors.NotImplemented;
            }
        }
    }
};
pub const CompoundStatementData = std.ArrayList(StatementData);
pub fn ClearCompoundStatement(compoundStatement: CompoundStatementData) void
{
    for (compoundStatement.items) |*stmt|
    {
        stmt.deinit(compoundStatement.allocator);
    }
    compoundStatement.deinit();
}
pub fn CompoundStatementToString(stmts: CompoundStatementData, allocator: std.mem.Allocator) anyerror!string
{
    var str = string.init(allocator);
    for (stmts.items) |*stmt|
    {
        var stmtStr = try stmt.ToString(allocator);
        try str.concat_deinit(&stmtStr);
        try str.concat("\n");
    }
    return str;
}