const std = @import("std");
const string = @import("zig-string.zig").String;
const lexer = @import("lexer.zig");
const linkedLists = @import("linked-list.zig");
const objPool = @import("object-pool.zig");
const ExprPool = objPool.ObjectPool(linkedLists.LinkedList(ExpressionData));

pub const Errors = error
{
    SyntaxError,
    OutOfMemoryError
};

pub const Parser = struct {
    pub const LinkedList = linkedLists.LinkedList(ExpressionData);
    pub const NodePointerList = std.ArrayList(*linkedLists.LinkedList(ExpressionData).Node);
    const Self = @This();
    allocator: std.mem.Allocator,
    tokenizer: lexer.Tokenizer,
    result: CompoundStatementData,
    errorStatements: std.ArrayList(string),

    timer0: f32,
    timer1: f32,

    pub fn init(allocator: std.mem.Allocator, tokenizer: lexer.Tokenizer) !Self 
    {
        return Self
        {
            .timer0 = 0,
            .timer1 = 0,
            .allocator = allocator,
            .tokenizer = tokenizer,
            .errorStatements = std.ArrayList(string).init(allocator),
            .result = CompoundStatementData.init(allocator)
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
                        const str = try self.SourceTokenSlice(token2);
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
                            const varName = foundIdentifier.?;
                            const typeName = self.SourceSlice(typeNameStart, typeNameEnd);
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
                                const varName = foundIdentifier.?;
                                const typeName = self.SourceSlice(typeNameStart, typeNameEnd);
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
    pub inline fn GetAssociation(ID: lexer.TokenID) i8
    {
        switch (ID)
        {
            .Minus, .Plus =>
            {
                return 1;
            },
            else =>
            {
                return -1;
            }
        }
    }
    pub inline fn GetPrecedence(ID: lexer.TokenID) i32
    {
        switch (ID)
        {
            .Asterisk, .Slash, .Percent =>
            {
                return 3;
            },
            .Plus, .Minus, .Ampersand, .Caret, .Tilde, .Pipe, .AngleBracketAngleBracketLeft, .AngleBracketAngleBracketRight =>
            {
                return 2;
            },
            .AmpersandAmpersand =>
            {
                return 1;
            },
            .PipePipe, .BangEqual, .EqualEqual =>
            {
                return 0;
            },
            else =>
            {
                return -1;
            }
        }
    }
    pub fn ParsePrimary(self: *Self) Errors!ExpressionData
    {
        const token = self.tokenizer.next();
        if (token.id == .LParen)
        {
            //_ = self.tokenizer.next();

            var nextPrimary = try self.ParsePrimary();
            var result = try self.ParseExpression(nextPrimary, 0);

            if (self.tokenizer.buffer[self.tokenizer.index] != ')')
            {
                return Errors.SyntaxError;
            }

            _ = self.tokenizer.next();
            return result;
        }
        else if (token.id == .Minus)
        {
            if (self.tokenizer.buffer[token.end] != ' ')
            {
                const nextToken = self.peekNext();
                if (nextToken.id == .IntegerLiteral or nextToken.id == .FloatLiteral)
                {
                    _ = self.tokenizer.next();
                    return ExpressionData
                    {
                        .Literal = self.tokenizer.buffer[token.start..nextToken.end]
                    };
                }
                else if (nextToken.id == .Identifier)
                {
                    _ = self.tokenizer.next();
                    return ExpressionData
                    {
                        .Variable = self.tokenizer.buffer[token.start..nextToken.end]
                    };
                }
                else
                {
                    return Errors.SyntaxError;
                }
            }
            else return Errors.SyntaxError;
        }
        else if (token.id == .Asterisk)
        {
            if (self.tokenizer.buffer[token.end] != ' ')
            {
                const nextToken = self.peekNext();
                //if (nextToken.id == .IntegerLiteral or nextToken.id == .StringLiteral or nextToken.id == .FloatLiteral or nextToken.id == .CharLiteral)
                if (nextToken.id == .Identifier)
                {
                    _ = self.tokenizer.next();
                    return ExpressionData
                    {
                        .Variable = self.tokenizer.buffer[token.start..nextToken.end]
                    };
                }
                else
                {
                    return Errors.SyntaxError;
                }
            }
            else return Errors.SyntaxError;
        }
        else if (token.id == .Bang)
        {
            if (self.tokenizer.buffer[token.end] != ' ')
            {
                const nextToken = self.peekNext();
                if (nextToken.id == .Keyword_true or nextToken.id == .Keyword_false)
                {
                    _ = self.tokenizer.next();
                    return ExpressionData
                    {
                        .Literal = self.tokenizer.buffer[token.start..nextToken.end]
                    };
                }
                else if (nextToken.id == .Identifier)
                {
                    _ = self.tokenizer.next();
                    return ExpressionData
                    {
                        .Variable = self.tokenizer.buffer[token.start..nextToken.end]
                    };
                }
                else
                {
                    return Errors.SyntaxError;
                }
            }
            else return Errors.SyntaxError;
        }
        else if (token.id == .IntegerLiteral or token.id == .StringLiteral or token.id == .FloatLiteral or token.id == .CharLiteral)
        {
            return ExpressionData
            {
                .Literal = self.SourceTokenSlice(token)
            };
        }
        else if (token.id == .Identifier)
        {
            return ExpressionData
            {
                .Variable = self.SourceTokenSlice(token)
            };
        }
        else
        {
            return Errors.SyntaxError;
        }
    }
    pub fn ParseExpression(self: *Self, initial: ExpressionData, minPrecedence: i32) Errors!ExpressionData
    {
        var lhs = initial;

        while (true)
        {
            var op = self.peekNext();
            var precedence = GetPrecedence(op.id);
            if (op.id == .Eof or op.id == .Semicolon or precedence == -1 or precedence < minPrecedence)
            {
                break;
            }
            _ = self.tokenizer.next();
            var rhs = try self.ParsePrimary();

            while (true)
            {
                var next = self.peekNext();
                var nextPrecedence = GetPrecedence(next.id);
                var nextAssociation = GetAssociation(next.id);
                if (next.id == .Eof or op.id == .Semicolon or nextPrecedence == -1 or nextPrecedence == precedence or nextAssociation == 1)
                {
                    break;
                }
                rhs = try self.ParseExpression(rhs, nextPrecedence);
            }
            var operator = self.allocator.create(OperatorData) catch
            {
                return Errors.OutOfMemoryError;
            };

            operator.leftExpression = lhs;
            operator.rightExpression = rhs;
            operator.operator = TokenToOperator.get(@tagName(op.id)).?;

            lhs = ExpressionData
            {
                .Op = operator
            };
        }

        return lhs;
    }
};

pub fn TestExpressionParsing() !void
{
    const buffer: []const u8 = "a || (!c && b) || !true;";//"a*((b-*c)/d);";
    //var arenaAllocator = std.heap.ArenaAllocator.init(std.heap.c_allocator);
    //defer arenaAllocator.deinit();
    var alloc = std.testing.allocator;//alloc = arenaAllocator.allocator();

    var tokenizer: lexer.Tokenizer = lexer.Tokenizer
    {
        .buffer = buffer
    };
    var parser: Parser = try Parser.init(alloc, tokenizer);
    std.debug.print("\n", .{});

    // var timer = try std.time.Timer.start();
    
    // var i: usize = 0;
    // while (i < 1000000) : (i += 1)
    // {
    parser.tokenizer.index = 0;
    var primary = try parser.ParsePrimary();
    var expr = parser.ParseExpression(primary, 0) catch |err|
    {
        for (parser.errorStatements.items) |errorStatement|
        {
            std.debug.print("Caught error:\n   {s}\n", .{errorStatement.str()});
        }
        parser.deinit();
        return err;
    };
    //primary.deinit(alloc);
    //expr.deinit(alloc);
    //}
    //std.debug.print("{d}\n", .{@intToFloat(f32, timer.read()) / 1000000.0});
    var str = try expr.ToString(alloc);
    defer str.deinit();
    std.debug.print("{s}\n", .{str.str()});

    expr.deinit(alloc);
    //}

    //var ms: f32 = @intToFloat(f32, timer.read()) / 1000000.0;
    //parser.timer0 += ms;
    //std.debug.print("{d}, {d}\n", .{parser.timer0, parser.timer1});
    
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
    name: []const u8,
    typeName: []const u8,
    isConst: bool,
    defaultValue: ?ExpressionData,

    pub fn deinit(self: *@This()) void
    {
        //self.name.deinit();
        //self.typeName.deinit();
        if (self.defaultValue != null)
        {
            self.defaultValue.deinit();
        }
    }
};

// NAME(args)
pub const TagData = struct 
{
    name: []const u8,
    args: []VarData,

    pub fn deinit(self: *@This()) void
    {
        //self.name.deinit();
        for (self.args) |arg|
        {
            arg.deinit();
        }
    }
};
// pub const MacroDefinitionData = struct
// {
//     name: []const u8,
//     args: [][]const u8,
//     expandsTo: []const u8,

//     pub fn deinit(self: *@This()) void
//     {
//         self.name.deinit();
//         for (self.args) |arg|
//         {
//             arg.deinit();
//         }
//         self.expandsTo.deinit();
//     }
// };

// returnType name(args) { statement }
pub const FunctionData = struct
{
    name: []const u8,
    returnType: []const u8,
    args: []VarData,
    statement: CompoundStatementData,

    pub fn deinit(self: *@This()) void
    {
        //self.name.deinit();
        //self.returnType.deinit();
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
    Variable,
    Op
};
pub const ExpressionData = union(ExpressionDataTag)
{
    Literal: []const u8,
    Variable: []const u8,
    Op: *OperatorData,

    pub fn deinit(self: *@This(), alloc: std.mem.Allocator) void
    {
        switch (self.*)
        {
            ExpressionDataTag.Op => |*value|
            {
                value.*.deinit(alloc);
                alloc.destroy(value.*);
            },
            else => {}
        }
    }

    pub fn ToString(self: @This(), allocator: std.mem.Allocator) !string
    {
        var str = string.init(allocator);
        switch (self)
        {
            ExpressionDataTag.Literal => |literal| 
            {
                try str.concat(literal);
            },
            ExpressionDataTag.Variable => |literal|
            {
                try str.concat(literal);
            },
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
    //macroDefinition
};
pub const StatementData = union(StatementDataTag)
{
    functionDeclaration: FunctionData,
    variableDeclaration: VarData,
    IfStatement: IfData,
    WhileStatement: WhileData,
    ForStatement: ForData,
    Comment: []const u8,
    includeStatement: []const u8,
    //macroDefinition: MacroDefinitionData
};

pub const CompoundStatementData = std.ArrayList(StatementData);