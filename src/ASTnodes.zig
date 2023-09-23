const std = @import("std");
const string = @import("zig-string.zig").String;
const Errors = @import("errors.zig").Errors;

pub const VarData = struct
{
    name: []const u8,
    typeName: TypeNameData,
    isConst: bool,
    defaultValue: ?ExpressionData,

    pub fn ToString(self: *@This(), allocator: std.mem.Allocator) !string
    {
        var str = string.init(allocator);
        if (self.isConst)
        {
            try str.concat("const ");
        }
        var typeNameStr = try self.typeName.ToString(allocator);
        try str.concat_deinit(&typeNameStr);
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
    name: TypeNameData,
    inputParams: []ExpressionData,

    pub fn ToString(self: *@This(), allocator: std.mem.Allocator) !string
    {
        var str = string.init(allocator);

        var nameStr = try self.name.ToString(allocator);
        try str.concat_deinit(&nameStr);
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
        self.name.deinit();
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
    returnType: TypeNameData,
    args: []VarData,
    statement: CompoundStatementData,

    pub fn ToString(self: *@This(), allocator: std.mem.Allocator) anyerror!string
    {
        var compoundStatementString = try CompoundStatementToString(&self.statement, allocator);

        var str = string.init(allocator);
        var returnTypeStr = self.returnType.ToString(allocator);
        try str.concat_deinit(&returnTypeStr);
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
pub const ModifiedVariableData = struct
{
    expression: ExpressionData,
    Op: Operator,

    pub inline fn deinit(self: *@This(), allocator: std.mem.Allocator) void
    {
        self.expression.deinit(allocator);
    }
    pub fn ToString(self: *@This(), allocator: std.mem.Allocator) anyerror!string
    {
        var result: string = string.init(allocator);
        if (self.Op == .Multiply)
        {
            try result.concat("*");
        }
        else if (self.Op == .Not)
        {
            try result.concat("!");
        }

        var exprStr = try self.expression.ToString(allocator);
        try result.concat_deinit(&exprStr);

        return result;
    }
};
pub const TypeNameData = struct
{
    fullName: []const u8,
    name: []const u8,
    namespace: []const u8,
    templateTypes: ?std.ArrayList(TypeNameData),
    pointerCount: i32,

    pub inline fn deinit(self: *@This()) void
    {
        if (self.templateTypes != null)
        {
            self.templateTypes.?.deinit();
        }
    }
    pub fn ToUseableString(self: *@This(), allocator: std.mem.Allocator) anyerror!string
    {
        var result: string = string.init(allocator);
        try result.concat(self.fullName);
        if (self.templateTypes != null)
        {
            try result.concat("_");
            var i: usize = 0;
            while (i < self.templateTypes.?.items.len) : (i += 1)
            {
                var templateTypeStr = try self.templateTypes.?.items[i].ToUseableString(allocator);
                try result.concat_deinit(&templateTypeStr);
                if (i < self.templateTypes.?.items.len - 1)
                {
                    try result.concat("_");
                }
            }
            //try result.concat("_");
        }
        var j: i32 = 0;
        while (j < self.pointerCount) : (j += 1)
        {
            try result.concat("*");
        }
        return result;
    }
    pub fn ToString(self: *@This(), allocator: std.mem.Allocator) anyerror!string
    {
        var result: string = string.init(allocator);
        try result.concat(self.fullName);
        if (self.templateTypes != null)
        {
            try result.concat("<");
            var i: usize = 0;
            while (i < self.templateTypes.?.items.len) : (i += 1)
            {
                var templateTypeStr = try self.templateTypes.?.items[i].ToString(allocator);
                try result.concat_deinit(&templateTypeStr);
                if (i < self.templateTypes.?.items.len - 1)
                {
                    try result.concat(", ");
                }
            }
            try result.concat(">");
        }
        var j: i32 = 0;
        while (j < self.pointerCount) : (j += 1)
        {
            try result.concat("*");
        }
        return result;
    }
};
pub const TypeCastData = struct
{
    typeName: TypeNameData,
    pointerCount: i32,

    pub fn ToString(self: *@This(), allocator: std.mem.Allocator) anyerror!string
    {
        var result: string = string.init(allocator);
        var i: i32 = 0;
        while (i < self.pointerCount) : (i += 1)
        {
            try result.concat("*");
        }
        var typeNameStr = try self.typeName.ToString(allocator);
        try result.concat_deinit(&typeNameStr);

        return result;
    }
    pub inline fn deinit(self: *@This()) void
    {
        self.typeName.deinit();
    }
};
pub const ExpressionDataTag = enum
{
    Literal,
    Variable,
    ModifiedVariable,
    Op,
    FunctionCall,
    IndexedAccessor,
    TypeCast
};
pub const ExpressionData = union(ExpressionDataTag)
{
    Literal: []const u8,
    Variable: TypeNameData,
    ModifiedVariable: *ModifiedVariableData,
    Op: *OperatorData,
    FunctionCall: *FunctionCallData,
    IndexedAccessor: *FunctionCallData,
    TypeCast: TypeCastData,

    pub fn deinit(self: *@This(), alloc: std.mem.Allocator) void
    {
        switch (self.*)
        {
            .Variable => |*value|
            {
                value.deinit();
            },
            .ModifiedVariable => |*value|
            {
                value.*.deinit(alloc);
                alloc.destroy(value.*);
            },
            .Op => |*value|
            {
                value.*.deinit(alloc);
                alloc.destroy(value.*);
            },
            .FunctionCall => |*value|
            {
                value.*.deinit(alloc);
                alloc.destroy(value.*);
            },
            .IndexedAccessor => |*value|
            {
                value.*.deinit(alloc);
                alloc.destroy(value.*);
            },
            .TypeCast => |*typecast|
            {
                typecast.*.deinit();
            },
            else => {}
        }
    }

    pub fn ToString(self: *@This(), allocator: std.mem.Allocator) anyerror!string
    {
        switch (self.*)
        {
            .Literal => |literal| 
            {
                var str = string.init(allocator);
                try str.concat(literal);
                return str;
            },
            .Variable => |*variable|
            {
                return variable.ToString(allocator);
            },
            .ModifiedVariable => |variable|
            {
                return variable.ToString(allocator);
            },
            .Op => |op| 
            {
                return op.ToString(allocator);
            },
            .FunctionCall => |call| 
            {
                return call.ToString(allocator);
            },
            .IndexedAccessor => |call|
            {
                return call.ToString(allocator);
            },
            .TypeCast => |*typeCast|
            {
                return typeCast.ToString(allocator);
            }
        }
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
    ToPointer, //&
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
    Equal,
    Period,
    Arrow,
    TypeCast
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
    .{"Equal", Operator.Equal},
    .{"Period", Operator.Period},
    .{"Arrow", Operator.Arrow},
    .{"Ampersand", Operator.ToPointer},
    .{"TypeCast", Operator.TypeCast}
});
pub const OperatorData = struct
{
    leftExpression: ExpressionData,
    operator: Operator,
    rightExpression: ExpressionData,
    priority: bool,

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
        if (self.priority)
        {
            try str.concat("(");
        }
        try str.concat_deinit(&leftString);
        try str.concat(" ");
        try str.concat(@tagName(self.operator));
        try str.concat(" ");
        try str.concat_deinit(&rightString);
        if (self.priority)
        {
            try str.concat(")");
        }

        return str;
    }
};
pub const WhileData = struct
{
    condition: ExpressionData,
    statement: CompoundStatementData,

    pub fn ToString(self: *@This(), allocator: std.mem.Allocator) !string
    {
        var str = string.init(allocator);

        var conditionStr = try self.condition.ToString(allocator);
        var statementStr = try CompoundStatementToString(&self.statement, allocator);
    
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
    condition: ExpressionData,
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

        var initializer = try CompoundStatementToString(&self.initializer, allocator);
        var condition = try self.condition.ToString(allocator);
        var shouldStep = try CompoundStatementToString(&self.shouldStep, allocator);
        var statement = try CompoundStatementToString(&self.statement, allocator);

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
    condition: ExpressionData,
    statement: CompoundStatementData,

    pub fn ToString(self: *@This(), allocator: std.mem.Allocator) !string
    {
        var str = string.init(allocator);

        var conditionStr = try self.condition.ToString(allocator);
        var statementStr = try CompoundStatementToString(&self.statement, allocator);
    
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
    tags: ?[]ExpressionData,
    body: CompoundStatementData
,
    pub fn ToString(self: *@This(), allocator: std.mem.Allocator) !string
    {
        var str = string.init(allocator);
        try str.concat("struct ");
        try str.concat(self.name);
        try str.concat(" { \n");

        var bodyStr = try CompoundStatementToString(&self.body, allocator);
        try str.concat_deinit(&bodyStr);

        try str.concat("}\n");
        return str;
    }
    pub fn deinit(self: *@This(), allocator: std.mem.Allocator) void
    {
        if (self.tags != null)
        {
            for (self.tags.?) |*tag|
            {
                tag.deinit(allocator);
            }
            allocator.free(self.tags.?);
        }
        for (self.body.items) |*item|
        {
            item.deinit(allocator);
        }
        self.body.deinit();
    }
};
pub const NamespaceData = struct
{
    body: CompoundStatementData,
    name: []const u8,

    pub fn deinit(self: *@This()) void
    {
        ClearCompoundStatement(&self.body);
    }
    pub fn ToString(self: *@This(), allocator: std.mem.Allocator) anyerror!string
    {
        var body = try CompoundStatementToString(&self.body, allocator);
        var result = string.init(allocator);

        try result.concat("namespace ");
        try result.concat(self.name);
        try result.concat(" {\n");
        try result.concat_deinit(&body);
        try result.concat("}\n");

        return result;
    }
};
pub const StatementDataTag = enum
{
    functionDeclaration,
    variableDeclaration,
    constructorDeclaration,
    destructorDeclaration,
    structDeclaration,
    traitDeclaration,
    functionInvoke,
    returnStatement,
    otherExpression,
    IfStatement,
    ElseStatement,
    WhileStatement,
    ForStatement,
    NamespaceStatement,
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
    traitDeclaration: StructData,
    functionInvoke: FunctionCallData,
    returnStatement: ExpressionData,
    otherExpression: ExpressionData,
    IfStatement: IfData,
    ElseStatement: CompoundStatementData,
    WhileStatement: WhileData,
    ForStatement: ForData,
    NamespaceStatement: NamespaceData,
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
            .traitDeclaration => |*decl| decl.deinit(allocator),
            .functionInvoke => |*invoke| invoke.deinit(allocator),
            .returnStatement => |*stmt| stmt.deinit(allocator),
            .otherExpression => |*stmt| stmt.deinit(allocator),
            .IfStatement => |*ifData| ifData.deinit(allocator),
            .ElseStatement => |*elseData|
            {
                ClearCompoundStatement(&elseData.*);
            },
            .NamespaceStatement => |*namespaceData|
            {
                ClearCompoundStatement(&namespaceData.body);
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
            .traitDeclaration => |*decl| return decl.ToString(allocator),
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
            .ElseStatement => |*elseData|
            {
                var stmtStr = try CompoundStatementToString(&elseData, allocator);
                var str = string.init(allocator);
                try str.concat("else {\n");
                try str.concat_deinit(&stmtStr);
                try str.concat("}");
                return str;
            },
            .NamespaceStatement => |*namespaceData|
            {
                return namespaceData.*.ToString(allocator);
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
pub fn ClearCompoundStatement(compoundStatement: *CompoundStatementData) void
{
    for (compoundStatement.*.items) |*stmt|
    {
        stmt.deinit(compoundStatement.allocator);
    }
    compoundStatement.deinit();
}
pub fn CompoundStatementToString(stmts: *CompoundStatementData, allocator: std.mem.Allocator) anyerror!string
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