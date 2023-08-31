const std = @import("std");
const string = @import("zig-string.zig").String;
const lexer = @import("lexer.zig");
const linkedLists = @import("linked-list.zig");
const objPool = @import("object-pool.zig");
const exprPool = objPool.ObjectPool(linkedLists.LinkedList(ExpressionData));

pub const Errors = error
{
    SyntaxError
};

pub fn generateNewExpressionList(allocator: std.mem.Allocator) objPool.Errors!linkedLists.LinkedList(ExpressionData)
{
    var result = linkedLists.LinkedList(ExpressionData).init(allocator);
    return result;
}

pub const Parser = struct {
    pub const LinkedList = linkedLists.LinkedList(ExpressionData);
    const Self = @This();
    allocator: std.mem.Allocator,
    tokenizer: lexer.Tokenizer,
    result: CompoundStatementData,
    errorStatements: std.ArrayList(string),
    expressionListPool: exprPool,

    pub fn init(allocator: std.mem.Allocator, tokenizer: lexer.Tokenizer) !Self 
    {
        var expressionListPool = exprPool.init(allocator, &generateNewExpressionList);
        return Self
        {
            .allocator = allocator,
            .tokenizer = tokenizer,
            .errorStatements = std.ArrayList(string).init(allocator),
            .result = CompoundStatementData.init(allocator),
            .expressionListPool = expressionListPool
        };
    }
    pub fn deinit(self: *Self) void
    {
        for (self.errorStatements.items) |*errorStatement|
        {
            errorStatement.deinit();
        }
        self.errorStatements.deinit();
        self.result.deinit();
        for (self.expressionListPool.pool.items) |*expressionList|
        {
            for (expressionList.unlinkedList.items) |*node|
            {
                node.data.deinit(self.allocator);
            }
            // var node = expressionList.first;
            // while (node != null)
            // {
            //     node.?.data.deinit(self.allocator);
            //     node = node.?.next;
            // }
            expressionList.deinit();
        }
        self.expressionListPool.deinit();
    }

    pub fn WriteError(self: *Self, message: []const u8) !string
    {
        var err: string = try self.stringPool.Rent();//(self.allocator);
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

                                const expression: ExpressionData = try self.ParseExpression(null);
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
    pub fn peekNext(self: *@This()) lexer.Token
    {
        const currentIndex = self.tokenizer.index;

        const result = self.tokenizer.next();

        self.tokenizer.index = currentIndex;

        return result;
    }
    pub fn CreateOperatorFrom(self: *Self, node: *LinkedList.Node, expressionList: *LinkedList) !*LinkedList.Node
    {
        if (node.prev == null or node.next == null)
        {
            const errorStr = try self.WriteError("Syntax Error: Operator is missing symbol either to the left or right ");
            try self.errorStatements.append(errorStr);
            return Errors.SyntaxError;
        }
        const leftValue = node.prev.?.data;
        const rightValue = node.next.?.data;

        var opDataPtr = try self.allocator.create(OperatorData);
        opDataPtr.leftExpression = leftValue;
        opDataPtr.rightExpression = rightValue;
        opDataPtr.operator = node.data.IncompleteOp;
            // {
            //     .leftExpression = leftValue,
            //     .rightExpression = rightValue,
            //     .operator = node.data.IncompleteOp
            // };

        const expr = ExpressionData
        {
            .Op = opDataPtr
        };

        const newNode = try expressionList.insertBefore(node.prev.?, expr);
        try expressionList.remove(node.prev.?);
        try expressionList.remove(node.next.?);
        try expressionList.remove(node);

        return newNode;
    }
    pub inline fn ReturnExpressionList(self: *Self, expressionList: *LinkedList) !void
    {
        for (expressionList.unlinkedList.items) |*item|
        {
            item.data.deinit(self.allocator);
        }
        expressionList.clear();
        try self.expressionListPool.Return(expressionList.*);
    }
    pub fn ParseExpression(self: *Self, openParentheses: i32) !ExpressionData
    {
        var expressionList = try self.expressionListPool.Rent();
        var nextNode: ?*LinkedList.Node = null;

        var openParen: i32 = openParentheses;
        if (openParentheses != 0)
        {
            openParen += 1;
        }

        //these can be used to return a syntax error early
        var numIdentifiersAndLiterals: i32 = 0;
        var numOperators: i32 = 0;
        
        while (true)
        {
            const token = self.tokenizer.next();
            switch (token.id)
            {
                .Eof =>
                {
                    var err = try self.WriteError("Syntax Error: Expected expression, but reached end of file!");
                    try self.ReturnExpressionList(&expressionList);
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
                        try self.ReturnExpressionList(&expressionList);
                        try self.errorStatements.append(err);
                        return Errors.SyntaxError;
                    }
                },
                .Minus =>
                {
                    //if operator is connected to right and not left and right is identifier or literal, token is identifier/literal
                    const next = self.peekNext();

                    if ((token.start == 0 or self.tokenizer.buffer[token.start - 1] == ' ') and (token.end != self.tokenizer.buffer.len and self.tokenizer.buffer[token.end] != ' '))
                    {
                        if (next.id == .Identifier)
                        {
                            _ = self.tokenizer.next();
                            var str = try string.init_with_contents(self.allocator, "-");
                            try str.concat(self.SourceTokenSlice(next));
                            _ = try expressionList.append(ExpressionData
                            {
                                .Variable = str
                            });
                            numIdentifiersAndLiterals += 1;
                        }
                        else if (next.id == .IntegerLiteral or next.id == .FloatLiteral)
                        {
                            _ = self.tokenizer.next();
                            var str = try string.init_with_contents(self.allocator, "-");
                            try str.concat(self.SourceTokenSlice(next));
                            _ = try expressionList.append(ExpressionData
                            {
                                .Literal = str
                            });
                            numIdentifiersAndLiterals += 1;
                        }
                        else 
                        {
                            _ = try expressionList.append(ExpressionData
                            {
                                .IncompleteOp = Operator.Minus
                            });
                            numOperators += 1;
                        }
                    }
                    else 
                    {
                        _ = try expressionList.append(ExpressionData
                        {
                            .IncompleteOp = Operator.Minus
                        });
                        numOperators += 1;
                    }
                },
                .Asterisk =>
                {
                    //if operator is connected to right and not left and right is identifier, token is identifier

                    const next = self.peekNext();

                    if ((token.start == 0 or self.tokenizer.buffer[token.start - 1] == ' ') and next.id == .Identifier and (token.end != self.tokenizer.buffer.len and self.tokenizer.buffer[token.end] != ' '))
                    {
                        //ignore next token as we are processing it right now
                        _ = self.tokenizer.next();
                        var str = try string.init_with_contents(self.allocator, "*");
                        try str.concat(self.SourceTokenSlice(next));
                        _ = try expressionList.append(ExpressionData
                        {
                            .Variable = str
                        });
                        numIdentifiersAndLiterals += 1;
                    }
                    else 
                    {
                        _ = try expressionList.append(ExpressionData
                        {
                            .IncompleteOp = Operator.Multiply
                        });
                        numOperators += 1;
                    }
                },
                .Identifier =>
                {
                    var str = try string.init_with_contents(self.allocator, self.SourceTokenSlice(token));
                    _ = try expressionList.append(ExpressionData
                    {
                        .Variable = str
                    });
                    numIdentifiersAndLiterals += 1;
                },
                .LParen =>
                {
                    openParen += 1;
                    const expr = try self.ParseExpression(openParen);

                    // var debugStr = try expr.ToString(self.allocator);
                    // defer debugStr.deinit();
                    // std.debug.print("{s}\n", .{debugStr.str()});

                    _ = try expressionList.append(expr);

                    numIdentifiersAndLiterals += 1;
                },
                .RParen =>
                {
                    openParen -= 1;
                    if (openParen == openParentheses)
                    {
                        break;
                    }
                },
                .StringLiteral, .CharLiteral, .IntegerLiteral, .FloatLiteral, =>
                {
                    const str = try string.init_with_contents(self.allocator, self.SourceTokenSlice(token));
                    _ = try expressionList.append(ExpressionData
                    {
                        .Literal = str
                    });
                    numIdentifiersAndLiterals += 1;
                    //var node: *LinkedList.Node = self.allocator.alloc(comptime T: type, n: usize)
                    //expressionList.append();
                },
                else =>
                {
                    const operator: ?Operator = TokenToOperator.get(@tagName(token.id));
                    if (operator != null)
                    {
                        _ = try expressionList.append(ExpressionData
                        {
                            .IncompleteOp = operator.?
                        });
                        numOperators += 1;
                    }
                }
            }
        }
        if (numOperators == 0)
        {
            //return
            if (expressionList.first != null)
            {
                const result = expressionList.first.?.data;
                expressionList.clear();
                return result;
            }
            else
            {
                const errorStr = try self.WriteError("Syntax Error: invalid expression ");
                try self.ReturnExpressionList(&expressionList);
                try self.errorStatements.append(errorStr);
                return Errors.SyntaxError;
            }
        }
        if (numIdentifiersAndLiterals != numOperators + 1)
        {
            const str = try self.WriteError("Syntax Error: Stray operator ");
            try self.ReturnExpressionList(&expressionList);
            try self.errorStatements.append(str);
            return Errors.SyntaxError;
        }

        nextNode = expressionList.first;
        while (nextNode != null)
        {
            switch (nextNode.?.data)
            {
                ExpressionDataTag.IncompleteOp => |op|
                {
                    if (op == Operator.Divide
                    or op == Operator.Multiply
                    or op == Operator.Modulo
                    or op == Operator.NotEquals //all boolean operators have equal precedence
                    or op == Operator.Equals
                        or op == Operator.And
                        or op == Operator.LessThan
                        or op == Operator.LessThanEquals
                        or op == Operator.MoreThan
                            or op == Operator.MoreThanEquals)
                    {
                        nextNode = try self.CreateOperatorFrom(nextNode.?, &expressionList);
                    }
                },
                else => {}
            }
            nextNode = nextNode.?.next;
        }
        if (expressionList.len == 1)
        {
            const result = expressionList.first.?.data;
            expressionList.clear();
            try self.expressionListPool.Return(expressionList);
            return result;
        }

        nextNode = expressionList.first;
        while (nextNode != null)
        {
            switch (nextNode.?.data)
            {
                ExpressionDataTag.IncompleteOp => |op|
                {
                    if (op == Operator.Minus or op == Operator.Plus)
                    {
                        nextNode = try self.CreateOperatorFrom(nextNode.?, &expressionList);
                    }
                },
                else => {}
            }
            nextNode = nextNode.?.next;
        }
        if (expressionList.len == 1)
        {
            const result = expressionList.first.?.data;
            expressionList.clear();
            try self.expressionListPool.Return(expressionList);
            return result;
        }
        else
        {
            const errorStr = try self.WriteError("Syntax Error: invalid expression ");
            try self.errorStatements.append(errorStr);
            try self.ReturnExpressionList(&expressionList);
            return Errors.SyntaxError;
        }
    }
};

pub fn TestExpressionParsing() !void
{
    const buffer: []const u8 = "a;";//*b-c/d;";
    var arenaAllocator = std.heap.ArenaAllocator.init(std.heap.c_allocator);
    defer arenaAllocator.deinit();
    var alloc = arenaAllocator.allocator();

    var tokenizer: lexer.Tokenizer = lexer.Tokenizer
    {
        .buffer = buffer
    };
    var parser: Parser = try Parser.init(alloc, tokenizer);
    std.debug.print("\n", .{});

    var timer = try std.time.Timer.start();
    
    var i: usize = 0;
    while (i < 100000) : (i += 1)
    {
        parser.tokenizer.index = 0;
        _ = parser.ParseExpression(0) catch
        {
            for (parser.errorStatements.items) |errorStatement|
            {
                std.debug.print("Caught error:\n   {s}\n", .{errorStatement.str()});
            }
            parser.deinit();
            return;
        };
    }

    var ms: f32 = @intToFloat(f32, timer.read()) / 1000000.0;
    std.debug.print("{d}\n", .{ms});
    //var str = try expr.ToString(alloc);
    //std.debug.print("{s}\n", .{str.str()});
    //str.deinit();
    //expr.deinit(alloc);

    parser.deinit();
}

test "expression parsing"
{
    try TestExpressionParsing();
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
    Op,
    IncompleteOp
};
pub const ExpressionData = union(ExpressionDataTag)
{
    Literal: string, //true
    NotLiteral: string, // !true, should raise warning
    Variable: string, //variableName
    NotVariable: string, // !variableName
    Op: *OperatorData, //A == 0
    IncompleteOp: Operator,

    pub fn deinit(self: *@This(), alloc: std.mem.Allocator) void
    {
        switch (self.*)
        {
            ExpressionDataTag.Literal => |*value| value.deinit(),
            ExpressionDataTag.NotLiteral => |*value| value.deinit(),
            ExpressionDataTag.Variable => |*value| value.deinit(),
            ExpressionDataTag.NotVariable => |*value| value.deinit(),
            ExpressionDataTag.Op => |*value|
            {
                
                value.*.deinit(alloc);
                alloc.destroy(value.*);
            },
            ExpressionDataTag.IncompleteOp => {}
        }
    }

    pub fn ToString(self: @This(), allocator: std.mem.Allocator) !string
    {
        var str = string.init(allocator);
        switch (self)
        {
            ExpressionDataTag.Literal => |literal| 
            {
                try str.concat(literal.str());
            },
            ExpressionDataTag.NotLiteral => |literal|
            {
                try str.concat(literal.str());
            },
            ExpressionDataTag.Variable => |literal|
            {
                try str.concat(literal.str());
            },
            ExpressionDataTag.NotVariable => |literal|
            {
                try str.concat(literal.str());
            },
            ExpressionDataTag.Op => |op| 
            {
                str.deinit();
                return op.ToString(allocator);
            },
            ExpressionDataTag.IncompleteOp => |op| try str.concat(@tagName(op))
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
});
pub const OperatorData = struct
{
    leftExpression: ExpressionData,
    operator: Operator,
    rightExpression: ExpressionData,

    pub fn deinit(self: *@This(), alloc: std.mem.Allocator) void
    {
        self.leftExpression.deinit(alloc);
        self.rightExpression.deinit(alloc);
    }
    pub fn ToString(self: *@This(), allocator: std.mem.Allocator) !string
    {
        var leftString = try self.leftExpression.ToString(allocator);
        defer leftString.deinit();
        var rightString = try self.rightExpression.ToString(allocator);
        defer rightString.deinit();

        var str: string = string.init(allocator);
        try str.concat("{");
        try str.concat(leftString.str());
        try str.concat(" ");
        try str.concat(@tagName(self.operator));
        try str.concat(" ");
        try str.concat(rightString.str());
        try str.concat("}");

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