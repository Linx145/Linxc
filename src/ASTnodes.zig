const std = @import("std");
const zstr = @import("zig-string.zig");
const string = zstr.String;
const OptString = zstr.OptString;
const Errors = @import("errors.zig").Errors;

pub const VarData = struct
{
    name: OptString,
    typeName: TypeNameData,
    isConst: bool,
    defaultValue: ?ExpressionData,
    isStatic: bool,

    pub fn ToOwned(self: *@This(), allocator: std.mem.Allocator) anyerror!void
    {
        try self.name.ToOwned(allocator);
        try self.typeName.ToOwned(allocator);
        if (self.defaultValue != null)
        {
            try self.defaultValue.?.ToOwned(allocator);
        }
    }
    pub fn ToString(self: *@This(), allocator: std.mem.Allocator) !string
    {
        var str = string.init(allocator);
        if (self.isStatic)
        {
            try str.concat("static ");
        }
        if (self.isConst)
        {
            try str.concat("const ");
        }
        var typeNameStr = try self.typeName.ToString(allocator);
        try str.concat_deinit(&typeNameStr);
        try str.concat(" ");
        try str.concat(self.name.str());
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
    name: OptString,
    args: []VarData,

    pub inline fn ToOwned(self: *@This(), allocator: std.mem.Allocator) anyerror!void
    {
        try self.name.ToOwned(allocator);
    }
    pub fn deinit(self: *@This(), allocator: std.mem.Allocator) void
    {
        self.name.deinit();
        for (self.args) |arg|
        {
            arg.deinit(allocator);
        }
    }
};
pub const FunctionCallData = struct
{
    name: TypeNameData,
    inputParams: []ExpressionData,

    pub fn ToOwned(self: *@This(), allocator: std.mem.Allocator) anyerror!void
    {
        try self.name.ToOwned(allocator);
        var i: usize = 0;
        while (i < self.inputParams.len) : (i += 1)
        {
            try self.inputParams[i].ToOwned(allocator);
        }
    }
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
    name: OptString,
    returnType: TypeNameData,
    args: []VarData,
    statement: CompoundStatementData,
    isStatic: bool,

    pub fn ToOwned(self: *@This(), allocator: std.mem.Allocator) anyerror!void
    {
        try self.name.ToOwned(allocator);
        try self.returnType.ToOwned(allocator);
        for (self.args) |*arg|
        {
            try arg.ToOwned(allocator);
        }
        try CompoundStatementToOwned(&self.statement, allocator);
    }
    pub fn ToString(self: *@This(), allocator: std.mem.Allocator) anyerror!string
    {
        var compoundStatementString = try CompoundStatementToString(&self.statement, allocator);

        var str = string.init(allocator);
        if (self.isStatic)
        {
            try str.concat("static ");
        }
        var returnTypeStr = try self.returnType.ToString(allocator);
        try str.concat_deinit(&returnTypeStr);
        try str.concat(" ");
        try str.concat(self.name.str());
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
        self.name.deinit();
        self.returnType.deinit();
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

    pub fn ToOwned(self: *@This(), allocator: std.mem.Allocator) anyerror!void
    {
        try self.expression.ToOwned(allocator);
    }
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
    allocator: std.mem.Allocator,
    fullName: OptString,
    name: OptString,
    namespace: OptString,
    templateTypes: ?std.ArrayList(TypeNameData),
    pointerCount: i32,
    next: ?*TypeNameData,

    pub fn ToOwned(self: *@This(), allocator: std.mem.Allocator) anyerror!void
    {
        try self.fullName.ToOwned(allocator);
        try self.name.ToOwned(allocator);
        try self.namespace.ToOwned(allocator);
        if (self.templateTypes != null)
        {
            for (self.templateTypes.?.items) |*templateType|
            {
                try templateType.ToOwned(allocator);
            }
        }
        if (self.next != null)
        {
            try self.next.?.ToOwned(allocator);
        }
    }
    pub fn deinit(self: *@This()) void
    {
        self.fullName.deinit();
        self.name.deinit();
        self.namespace.deinit();
        if (self.templateTypes != null)
        {
            for (self.templateTypes.?.items) |*templateType|
            {
                templateType.deinit();
            }
            self.templateTypes.?.deinit();
        }
        if (self.next != null)
        {
            self.next.?.deinit();
            self.allocator.destroy(self.next.?);
        }
    }
    pub fn ToString(self: *@This(), allocator: std.mem.Allocator) anyerror!string
    {
        var result: string = string.init(allocator);
        try result.concat(self.fullName.str());
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
        if (self.next != null)
        {
            try result.concat("::");
            var nextStr = try self.next.?.ToString(allocator);
            try result.concat_deinit(&nextStr);
        }
        return result;
    }
};
pub const TypeCastData = struct
{
    typeName: TypeNameData,

    pub inline fn ToOwned(self: *@This(), allocator: std.mem.Allocator) anyerror!void
    {
        try self.typeName.ToOwned(allocator);
    }
    pub inline fn ToString(self: *@This(), allocator: std.mem.Allocator) anyerror!string
    {
        return self.typeName.ToString(allocator);
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
    TypeCast,
    sizeOf,
    nameOf,
    typeOf
};
pub const ExpressionData = union(ExpressionDataTag)
{
    Literal: OptString,
    Variable: TypeNameData,
    ModifiedVariable: *ModifiedVariableData,
    Op: *OperatorData,
    FunctionCall: *FunctionCallData,
    IndexedAccessor: *FunctionCallData,
    TypeCast: TypeCastData,
    sizeOf: TypeNameData,
    nameOf: TypeNameData,
    typeOf: TypeNameData,

    pub inline fn ToOwned(self: *@This(), allocator: std.mem.Allocator) anyerror!void
    {
        switch (self.*)
        {
            .Literal => |*literal|
            {
                try literal.ToOwned(allocator);
            },
            .Variable => |*variable|
            {
                try variable.ToOwned(allocator);
            },
            .ModifiedVariable => |*variable|
            {
                try variable.*.ToOwned(allocator);
            },
            .Op => |*operatorData|
            {
                try operatorData.*.ToOwned(allocator);
            },
            .FunctionCall => |*funcCall|
            {
                try funcCall.*.ToOwned(allocator);
            },
            .IndexedAccessor => |*indexedAccessor|
            {
                try indexedAccessor.*.ToOwned(allocator);
            },
            .TypeCast => |*typeCast|
            {
                try typeCast.*.ToOwned(allocator);
            },
            .sizeOf => |*sizeOf|
            {
                try sizeOf.*.ToOwned(allocator);
            },
            .nameOf => |*nameOf|
            {
                try nameOf.*.ToOwned(allocator);
            },
            .typeOf => |*typeOf|
            {
                try typeOf.*.ToOwned(allocator);
            }
        }
    }
    pub fn deinit(self: *@This(), alloc: std.mem.Allocator) void
    {
        switch (self.*)
        {
            .Literal => |*literal|
            {
                literal.deinit();
            },
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
            .sizeOf => |*sizeOf|
            {
                sizeOf.*.deinit();
            },
            .nameOf => |*nameOf|
            {
                nameOf.*.deinit();
            },
            .typeOf => |*typeOf|
            {
                typeOf.*.deinit();
            }
        }
    }

    pub fn ToString(self: *@This(), allocator: std.mem.Allocator) anyerror!string
    {
        switch (self.*)
        {
            .Literal => |literal| 
            {
                var str = string.init(allocator);
                try str.concat(literal.str());
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
            },
            .sizeOf => |*sizeOf|
            {
                try sizeOf.ToString(allocator);
            },
            .nameOf => |*nameOf|
            {
                try nameOf.ToString(allocator);
            },
            .typeOf => |*typeOf|
            {
                try typeOf.ToString(allocator);
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

    pub fn ToOwned(self: *@This(), allocator: std.mem.Allocator) anyerror!void
    {
        try self.leftExpression.ToOwned(allocator);
        try self.rightExpression.ToOwned(allocator);
    }
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

    pub fn ToOwned(self: *@This(), allocator: std.mem.Allocator) anyerror!void
    {
        try CompoundStatementToOwned(&self.statement, allocator);
    }
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

    pub inline fn ToOwned(self: *@This(), allocator: std.mem.Allocator) anyerror!void
    {
        try CompoundStatementToOwned(&self.initializer, allocator);
        try self.condition.ToOwned(allocator);
        try CompoundStatementToOwned(&self.shouldStep, allocator);
        try CompoundStatementToOwned(&self.statement, allocator);
    }
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

    pub fn ToOwned(self: *@This(), allocator: std.mem.Allocator) anyerror!void
    {
        try self.condition.ToOwned(allocator);
        try CompoundStatementToOwned(&self.statement, allocator);
    }
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
    name: OptString,
    tags: ?[]ExpressionData,
    body: CompoundStatementData,
    templateTypes: ?[]OptString,

    pub fn ToOwned(self: *@This(), allocator: std.mem.Allocator) anyerror!void
    {
        try self.name.ToOwned(allocator);
        if (self.tags != null)
        {
            for (self.tags.?) |*tag|
            {
                try tag.ToOwned(allocator);
            }
        }
        try CompoundStatementToOwned(&self.body, allocator);
        if (self.templateTypes != null)
        {
            for (self.templateTypes.?) |*templateType|
            {
                try templateType.ToOwned(allocator);
            }
        }
    }
    pub fn ToString(self: *@This(), allocator: std.mem.Allocator) !string
    {
        var str = string.init(allocator);
        try str.concat("struct ");
        try str.concat(self.name.str());
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
        if (self.templateTypes != null)
        {
            allocator.free(self.templateTypes.?);
        }
        self.body.deinit();
    }
};
pub const NamespaceData = struct
{
    body: CompoundStatementData,
    name: OptString,

    pub inline fn ToOwned(self: *@This(), allocator: std.mem.Allocator) anyerror!void
    {
        try CompoundStatementToOwned(&self.body, allocator);
        try self.name.ToOwned(allocator);
    }
    pub fn deinit(self: *@This()) void
    {
        ClearCompoundStatement(&self.body);
    }
    pub fn ToString(self: *@This(), allocator: std.mem.Allocator) anyerror!string
    {
        var body = try CompoundStatementToString(&self.body, allocator);
        var result = string.init(allocator);

        try result.concat("namespace ");
        try result.concat(self.name.str());
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

    pub fn ToOwned(self: *@This(), allocator: std.mem.Allocator) anyerror!void
    {
        switch (self.*)
        {
            .functionDeclaration => |*decl| try decl.ToOwned(allocator),
            .variableDeclaration => |*decl| try decl.ToOwned(allocator),
            .constructorDeclaration => |*decl| try decl.ToOwned(allocator),
            .destructorDeclaration => |*decl| try decl.ToOwned(allocator),
            .structDeclaration => |*decl| try decl.ToOwned(allocator),
            .traitDeclaration => |*decl| try decl.ToOwned(allocator),
            .returnStatement => |*stmt| try stmt.ToOwned(allocator),
            .otherExpression => |*expr| try expr.ToOwned(allocator),
            .IfStatement => |*stmt| try stmt.ToOwned(allocator),
            .ElseStatement => |*stmt| try CompoundStatementToOwned(stmt, allocator),
            .WhileStatement => |*stmt| try stmt.ToOwned(allocator),
            .ForStatement => |*stmt| try stmt.ToOwned(allocator),
            .NamespaceStatement => |*stmt| try stmt.ToOwned(allocator),
            else =>
            {
                
            }
        }
    }
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
                var stmtStr = try CompoundStatementToString(elseData, allocator);
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
pub inline fn CompoundStatementToOwned(compoundStatement: *CompoundStatementData, allocator: std.mem.Allocator) anyerror!void
{
    for (compoundStatement.*.items) |*stmt|
    {
        try stmt.ToOwned(allocator);
    }
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