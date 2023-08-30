const std = @import("std");
const string = @import("zig-string.zig").String;
const lexer = @import("lexer.zig");
const linkedLists = @import("linked-list.zig");

pub const Errors = error
{
    SyntaxError
};

pub const Parser = struct {
    pub const LinkedList = linkedLists.LinkedList(ExpressionData);
    const Self = @This();
    allocator: std.mem.Allocator,
    tokenizer: lexer.Tokenizer,
    result: CompoundStatementData,
    errorStatements: std.ArrayList(string),
    expressionList: LinkedList,

    pub fn init(allocator: std.mem.Allocator, tokenizer: lexer.Tokenizer) Self 
    {
        return Self
        {
            .allocator = allocator,
            .tokenizer = tokenizer,
            .errorStatements = std.ArrayList(string).init(allocator),
            .result = CompoundStatementData.init(allocator),
            .expressionList = LinkedList.init(allocator)
        };
    }
    pub fn deinit(self: *Self) void
    {
        self.errorStatements.deinit();
        self.result.deinit();
        var node = self.expressionList.first;
        while (node != null)
        {
            node.?.data.deinit();
            node = node.?.next;
        }
        self.expressionList.deinit();
    }

    pub fn WriteError(self: *Self, message: []const u8) !string
    {
        var err: string = string.init(self.allocator);
        try err.concat(message);
        try err.concat("\n");
        //var formatted: u8 = try std.fmt.allocPrint(self.allocator, "{d}", .{});
        //defer self.allocator.free(formatted);
        //try err.concat(formatted);

        return err;
    }
    pub inline fn SourceSlice(self: *Self, start: usize, end: usize) []const u8
    {
        return self.tokenizer.buffer[start..end];
    }
    pub inline fn SourceTokenSlice(self: *Self, token: lexer.Token) []const u8
    {
        return self.tokenizer.buffer[token.start..token.end];
    }
    pub inline fn SourceString(self: *Self, start: usize, end: usize) !string
    {
        const result = try string.init_with_contents(self.allocator, self.tokenizer.buffer[start..end]);
        return result;
    }
    pub inline fn SourceTokenString(self: *Self, token: lexer.Token) !string
    {
        const result = try string.init_with_contents(self.allocator, self.tokenizer.buffer[token.start..token.end]);
        return result;
    }
    
    pub fn Parse(self: *Self) !void
    {
        self.result = CompoundStatementData.init(self.allocator);

        //var start: ?usize = null;
        //var end: usize = 0;
        var skipThisLine: bool = false;
        var braceCont: i32 = 0;
        var foundIdentifier: ?[]const u8 = null;

        var typeNameStart: ?usize = null;
        var typeNameEnd: usize = 0;

        var nextIsConst: bool = false;

        while (true)
        {
            const token: lexer.Token = self.tokenizer.next();

            switch (token.id)
            {
                .Eof =>
                {
                    break;
                },
                .Nl =>
                {
                    skipThisLine = false;
                },
                .Hash =>
                {
                    skipThisLine = true;
                },
                .Keyword_include =>
                {
                    if (braceCont > 0)
                    {
                        var err: string = try self.WriteError("Syntax Error: include directive should not be contained within a {}! ");
                        try self.errorStatements.append(err);

                        return Errors.SyntaxError;
                    }
                    const token2: lexer.Token = self.tokenizer.next();
                    if (token2.id == .MacroString)
                    {
                        const str = try self.SourceTokenString(token2);
                        const statement: StatementData = StatementData
                        {
                            .includeStatement = str
                        };
                        try self.result.append(statement);
                    }
                    else
                    {
                        var err: string = try self.WriteError("Syntax Error: Expected enclosed string after #include statement, but found none ");
                        try self.errorStatements.append(err);

                        return Errors.SyntaxError;
                    }
                },
                .LBrace =>
                {
                    if (!skipThisLine)
                    {
                        braceCont += 1;
                    }
                },
                .RBrace =>
                {
                    if (!skipThisLine)
                    {
                        braceCont -= 1;
                    }
                },
                .Keyword_void, .Keyword_int, .Keyword_uint, .Keyword_short, .Keyword_ushort, .Keyword_ulong =>
                {
                    if (!skipThisLine)
                    {
                        if (typeNameStart == null)
                        {
                            typeNameStart = token.start;
                        }
                        typeNameEnd = token.end;
                    }
                },
                .Keyword_const
                {
                    if (!skipThisLine)
                    {
                        if (nextIsConst)
                        {
                            var err = try self.WriteError("Syntax Error: Duplicate const prefix! ");
                            try self.errorStatements.append(err);
                            return Errors.SyntaxError;
                        }
                        else if (typeNameStart != null)
                        {
                            var err = try self.WriteError("Syntax Error: const should be before type");
                            try self.errorStatements.append(err);
                            return Errors.SyntaxError;
                        }
                        nextIsConst = true;
                    }
                },
                .Identifier => 
                {
                    if (!skipThisLine)
                    {
                        if (typeNameStart == null)
                        {
                            typeNameStart = token.start;
                        }
                        else
                        {
                            //reached a declaration of either a variable or function
                            foundIdentifier = self.SourceTokenSlice(token);
                        }
                        typeNameEnd = token.end;
                    }
                },
                .Semicolon =>
                {
                    if (!skipThisLine)
                    {
                        if (foundIdentifier != null)
                        {
                            //is variable
                            const varName = try string.init_with_contents(self.allocator, foundIdentifier.?);
                            const typeName = try string.init_with_contents(self.allocator, self.SourceSlice(typeNameStart, typeNameEnd));
                            const varData: VarData = VarData
                            {
                                .name = varName,
                                .isConst = nextIsConst,
                                .typeName = typeName,
                                .defaultValue = null
                            };
                            try self.result.append(varData);
                            nextIsConst = false;
                            foundIdentifier = null;
                            typeNameStart = null;
                            typeNameEnd = 0;
                        }
                    }
                },
                .Equal =>
                {
                    if (!skipThisLine)
                    {
                        if (foundIdentifier != null)
                        {
                            //is variable
                            if (foundIdentifier != null)
                            {
                                //is variable

                                const expression: ExpressionData = try self.ParseExpression();
                                const varName = try string.init_with_contents(self.allocator, foundIdentifier.?);
                                const typeName = try string.init_with_contents(self.allocator, self.SourceSlice(typeNameStart, typeNameEnd));
                                const varData: VarData = VarData
                                {
                                    .name = varName,
                                    .isConst = nextIsConst,
                                    .typeName = typeName,
                                    .defaultValue = expression
                                };
                                try self.result.append(varData);
                                nextIsConst = false;
                                foundIdentifier = null;
                                typeNameStart = null;
                                typeNameEnd = 0;
                            }
                        }
                    }
                },
                else =>
                {
                    
                }
            }
        }
    }
    pub fn ParseExpression(self: *Self, openParentheses: i32) !void
    {
        var nextNode: ?*LinkedList.Node = self.expressionList.first;
        while (nextNode != null)
        {
            nextNode.?.data.deinit();
            nextNode = nextNode.?.next;
        }
        self.expressionList.emptyIndices.clearRetainingCapacity();
        self.expressionList.unlinkedList.clearRetainingCapacity();

        var openParen: i32 = openParentheses;
        
        while (true)
        {
            const token = self.tokenizer.next();
            switch (token.id)
            {
                .Eof =>
                {
                    var err = try self.WriteError("Syntax Error: Expected expression, but reached end of file!");
                    try self.errorStatements.append(err);
                    return Errors.SyntaxError;
                },
                .Semicolon =>
                {
                    if (openParentheses == 0)
                    {
                        break;
                    }
                    else
                    {
                        var err = try self.WriteError("Syntax Error: Open parenthesis is not closed");
                        try self.errorStatements.append(err);
                        return Errors.SyntaxError;
                    }
                },
                .Asterisk =>
                {
                    //self.tokenizer.peekNext();
                    //if operator is connected to right and right is identifier, token is identifier

                },
                .LParen =>
                {
                    openParen += 1;
                },
                .RParen =>
                {
                    openParen -= 1;
                    if (openParen == openParentheses)
                    {
                        break;
                    }
                },
                .Minus =>
                {

                },
                .StringLiteral, .CharLiteral, .IntegerLiteral, .FloatLiteral, =>
                {
                    const str = try string.init_with_contents(self.allocator, self.SourceTokenSlice(token));
                    _ = try self.expressionList.append(ExpressionData
                    {
                        .Literal = str
                    });
                    //var node: *LinkedList.Node = self.allocator.alloc(comptime T: type, n: usize)
                    //expressionList.append();
                },
                else =>
                {

                }
            }
        }
    }
};

test "expression parsing"
{
    const buffer: []const u8 = "1 + 1;";
    var tokenizer: lexer.Tokenizer = lexer.Tokenizer
    {
        .buffer = buffer
    };
    var parser: Parser = Parser.init(std.testing.allocator, tokenizer);
    std.debug.print("\n", .{});
    try parser.ParseExpression(0);

    var node: ?*Parser.LinkedList.Node = parser.expressionList.first;
    while (node != null)
    {
        var str = try node.?.data.ToString(std.testing.allocator);

        std.debug.print("{s}\n", .{str.str()});
        node = node.?.next;

        str.deinit();
    }
    parser.deinit();
}

// (isConst? const : ) typeName name
//right after varData, can be
//comma(if in function args)
//; (if in compound statement)
pub const VarData = struct
{
    name: string,
    typeName: string,
    isConst: bool,
    defaultValue: ?ExpressionData,

    pub fn deinit(self: *@This()) void
    {
        self.name.deinit();
        self.typeName.deinit();
        if (self.defaultValue != null)
        {
            self.defaultValue.deinit();
        }
    }
};

// NAME(args)
pub const TagData = struct 
{
    name: string,
    args: []VarData,

    pub fn deinit(self: *@This()) void
    {
        self.name.deinit();
        for (self.args) |arg|
        {
            arg.deinit();
        }
    }
};
pub const MacroDefinitionData = struct
{
    name: string,
    args: []string,
    expandsTo: string,

    pub fn deinit(self: *@This()) void
    {
        self.name.deinit();
        for (self.args) |arg|
        {
            arg.deinit();
        }
        self.expandsTo.deinit();
    }
};

// returnType name(args) { statement }
pub const FunctionData = struct
{
    name: string,
    returnType: string,
    args: []VarData,
    statement: CompoundStatementData,

    pub fn deinit(self: *@This()) void
    {
        self.name.deinit();
        self.returnType.deinit();
        for (self.args) |arg|
        {
            arg.deinit();
        }
        for (self.statement) |statement|
        {
            statement.deinit();
        }
        self.statement.deinit();
    }
};

pub const ExpressionDataTag = enum
{
    Literal,
    NotLiteral,
    Variable,
    NotVariable,
    Op
};
pub const ExpressionData = union(ExpressionDataTag)
{
    Literal: string, //true
    NotLiteral: string, // !true, should raise warning
    Variable: string, //variableName
    NotVariable: string, // !variableName
    Op: *OperatorData, //A == 0

    pub fn deinit(self: *@This()) void
    {
        switch (self.*)
        {
            ExpressionDataTag.Literal => |*value| value.deinit(),
            ExpressionDataTag.NotLiteral => |*value| value.deinit(),
            ExpressionDataTag.Variable => |*value| value.deinit(),
            ExpressionDataTag.NotVariable => |*value| value.deinit(),
            ExpressionDataTag.Op => |value| value.deinit()
        }
    }

    pub fn ToString(self: *@This(), allocator: std.mem.Allocator) !string
    {
        var str = string.init(allocator);
        switch (self.*)
        {
            ExpressionDataTag.Literal => |literal| try str.concat(literal.str()),
            ExpressionDataTag.NotLiteral => |literal| try str.concat(literal.str()),
            ExpressionDataTag.Variable => |literal| try str.concat(literal.str()),
            ExpressionDataTag.NotVariable => |literal| try str.concat(literal.str()),
            ExpressionDataTag.Op => |op| 
            {
                str.deinit();
                return op.ToString(allocator);
            }
        }
        return str;
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
};
pub const OperatorData = struct
{
    leftExpression: ExpressionData,
    operator: Operator,
    rightExpression: ExpressionData,

    pub fn deinit(self: *@This()) void
    {
        self.leftExpression.deinit();
        self.rightExpression.deinit();
    }
    pub fn ToString(self: *@This(), allocator: std.mem.Allocator) !string
    {
        var leftString = try self.leftExpression.ToString(allocator);
        defer leftString.deinit();
        var rightString = try self.rightExpression.ToString(allocator);
        defer rightString.deinit();

        var str: string = string.init(allocator);
        try str.concat(leftString.str());
        try str.concat(" ");
        try str.concat(@tagName(self.operator));
        try str.concat(" ");
        try str.concat(rightString.str());

        return str;
    }
};

//while (expression)
pub const WhileData = struct
{
    condition: ExpressionData,
    statement: CompoundStatementData,

    pub fn deinit(self: *@This()) void
    {
        self.condition.deinit();
        for (self.statement) |statement|
        {
            statement.deinit();
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

    pub fn deinit(self: *@This()) void
    {
        for (self.initializer) |statement|
        {
            statement.deinit();
        }
        self.condition.deinit();
        for (self.statement) |statement|
        {
            statement.deinit();
        }
        for (self.shouldStep) |statement|
        {
            statement.deinit();
        }
        self.initializer.deinit();
        self.statement.deinit();
        self.step.deinit();
    }
};
pub const IfData = struct
{
    condition: ExpressionData,
    statement: CompoundStatementData,

    pub fn deinit(self: *@This()) void
    {
        self.condition.deinit();
        for (self.statement) |statement|
        {
            statement.deinit();
        }
        self.statement.deinit();
    }
};

pub const StatementDataTag = enum
{
    functionDeclaration,
    variableDeclaration,
    IfStatement,
    WhileStatement,
    ForStatement,
    Comment,
    includeStatement,
    macroDefinition
};
pub const StatementData = union(StatementDataTag)
{
    functionDeclaration: FunctionData,
    variableDeclaration: VarData,
    IfStatement: IfData,
    WhileStatement: WhileData,
    ForStatement: ForData,
    Comment: string,
    includeStatement: string,
    macroDefinition: MacroDefinitionData
};

pub const CompoundStatementData = std.ArrayList(StatementData);