const std = @import("std");
const string = @import("zig-string.zig").String;
const lexer = @import("lexer.zig");
const linkedLists = @import("linked-list.zig");
const objPool = @import("object-pool.zig");
const ExprPool = objPool.ObjectPool(linkedLists.LinkedList(ExpressionData));
const io = @import("io.zig");

pub const Errors = error
{
    SyntaxError,
    OutOfMemoryError,
    NotImplemented
};

pub const Parser = struct {
    pub const LinkedList = linkedLists.LinkedList(ExpressionData);
    pub const NodePointerList = std.ArrayList(*linkedLists.LinkedList(ExpressionData).Node);
    const Self = @This();
    allocator: std.mem.Allocator,
    tokenizer: lexer.Tokenizer,
    errorStatements: std.ArrayList(string),

    pub fn init(allocator: std.mem.Allocator, tokenizer: lexer.Tokenizer) !Self 
    {
        return Self
        {
            .allocator = allocator,
            .tokenizer = tokenizer,
            .errorStatements = std.ArrayList(string).init(allocator)
        };
    }
    pub fn deinit(self: *Self) !void
    {
        for (self.errorStatements.items) |*errorStatement|
        {
            errorStatement.deinit();
        }
        self.errorStatements.deinit();
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

    pub fn nextUntilValid(self: *@This()) lexer.Token
    {
        var next = self.tokenizer.next();
        while (next.id == .Nl or next.id == .LineComment or next.id == .MultiLineComment)
        {
            next = self.tokenizer.next();
        }
        return next;
    }
    pub fn peekNext(self: *@This()) lexer.Token
    {
        const currentIndex = self.tokenizer.index;

        const result = self.tokenizer.next();

        self.tokenizer.index = currentIndex;

        return result;
    }
    pub fn peekNextUntilValid(self: *@This()) lexer.Token
    {
        const currentIndex = self.tokenizer.index;

        const result = self.nextUntilValid();

        self.tokenizer.index = currentIndex;

        return result;
    }
    
    pub fn Parse(self: *Self) !CompoundStatementData
    {
        var result = CompoundStatementData.init(self.allocator);

        //var start: ?usize = null;
        //var end: usize = 0;
        var skipThisLine: bool = false;
        var braceCont: i32 = 0;
        var foundIdentifier: ?[]const u8 = null;

        var typeNameStart: ?usize = null;
        var typeNameEnd: usize = 0;

        var nextIsConst: bool = false;

        var nextIsStruct: bool = false;

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
                .MultiLineComment, .LineComment =>
                {
                    continue;
                },
                .Keyword_include =>
                {
                    // if (braceCont > 0)
                    // {
                    //     var err: string = try self.WriteError("Syntax Error: include directive should not be contained within a {}! ");
                    //     try self.errorStatements.append(err);

                    //     return Errors.SyntaxError;
                    // }
                    const token2: lexer.Token = self.tokenizer.next();
                    if (token2.id == .MacroString)
                    {
                        const str = self.SourceTokenSlice(token2);
                        const statement: StatementData = StatementData
                        {
                            .includeStatement = str
                        };
                        try result.append(statement);
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
                        if (braceCont <= 0)
                        {
                            break;
                        }
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
                .Keyword_if =>
                {
                    if (!skipThisLine)
                    {
                        var next = self.nextUntilValid();
                        if (next.id == .LParen)
                        {
                            if (next.id == .Nl)
                            {
                                next = self.nextUntilValid();
                            }
                            var primary = try self.ParseExpression_Primary();
                            var expression = try self.ParseExpression(primary, 0);
                        
                            next = self.tokenizer.next();
                            while (next.id != .LBrace)
                            {
                                next = self.nextUntilValid();
                            }
                            var statement = try self.Parse();

                            try result.append(StatementData
                            {
                                .IfStatement = IfData
                                {
                                    .condition = expression,
                                    .statement = statement
                                }
                            });
                        }
                    }
                },
                .Keyword_const => 
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
                .Keyword_return =>
                {
                    if (!skipThisLine)
                    {
                        var primary = try self.ParseExpression_Primary();
                        var expr = try self.ParseExpression(primary, 0);

                        try result.append(StatementData
                        {
                            .returnStatement = expr
                        });
                    }
                },
                .Keyword_struct =>
                {
                    if (!skipThisLine)
                    {
                        if (nextIsConst)
                        {
                            var err = try self.WriteError("Syntax Error: cannot declare const struct");
                            try self.errorStatements.append(err);
                            return Errors.SyntaxError;
                        }
                        nextIsStruct = true;
                    }
                },
                .Identifier => 
                {
                    if (!skipThisLine)
                    {
                        if (nextIsStruct)
                        {
                            const structName = self.SourceTokenSlice(token);
                            var next = self.nextUntilValid();
                            if (next.id != .LBrace)
                            {
                                var err = try self.WriteError("Syntax Error: Expected { after struct name");
                                try self.errorStatements.append(err);
                                return Errors.SyntaxError;
                            }
                            const structBody = try self.Parse();
                            const structData = StructData
                            {
                                .name = structName,
                                .body = structBody
                            };
                            try result.append(StatementData{.structDeclaration = structData});
                            nextIsStruct = false;
                            typeNameStart = null;
                            typeNameEnd = 0;
                        }
                        else foundIdentifier = self.SourceTokenSlice(token);
                    }
                },
                .LParen =>
                {
                    if (!skipThisLine)
                    {
                        //function
                        if (foundIdentifier != null)
                        {
                            if (typeNameStart != null)
                            {
                                if (nextIsConst)
                                {
                                    var err = try self.WriteError("Syntax Error: Cannot declare function as const");
                                    try self.errorStatements.append(err);
                                    return Errors.SyntaxError;
                                }

                                const functionName = foundIdentifier.?;
                                const functionReturnType = self.SourceSlice(typeNameStart.?, typeNameEnd);

                                //parse arguments
                                var args = try self.ParseArgs();

                                var next: lexer.Token = undefined;
                                while (true)
                                {
                                    next = self.nextUntilValid();
                                    if (next.id == .LBrace)
                                    {
                                        break;
                                    }
                                }
                                const body = try self.Parse();

                                try result.append(StatementData
                                {
                                    .functionDeclaration = FunctionData
                                    {
                                        .name = functionName,
                                        .returnType = functionReturnType,
                                        .args = args,
                                        .statement = body
                                    }
                                });
                            }
                            else
                            {
                                _ = self.nextUntilValid();
                                var inputParams = try self.ParseInputParams();
                                try result.append(StatementData
                                {
                                    .functionInvoke = FunctionCallData
                                    {
                                        .name = foundIdentifier.?,
                                        .inputParams = inputParams
                                    }
                                });
                            }

                            foundIdentifier = null;
                            typeNameStart = null;
                            typeNameEnd = 0;
                        }
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
                            const typeName = self.SourceSlice(typeNameStart.?, typeNameEnd);
                            const varData: VarData = VarData
                            {
                                .name = varName,
                                .isConst = nextIsConst,
                                .typeName = typeName,
                                .defaultValue = null
                            };
                            try result.append(StatementData{.variableDeclaration = varData});
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
                                var primary = try self.ParseExpression_Primary();
                                const expression: ExpressionData = try self.ParseExpression(primary, 0);
                                const varName = foundIdentifier.?;
                                const typeName = self.SourceSlice(typeNameStart.?, typeNameEnd);
                                const varData: VarData = VarData
                                {
                                    .name = varName,
                                    .isConst = nextIsConst,
                                    .typeName = typeName,
                                    .defaultValue = expression
                                };
                                try result.append(StatementData{.variableDeclaration = varData});
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
        return result;
    }

    pub fn ParseInputParams(self: *Self) Errors![]ExpressionData
    {
        var params = std.ArrayList(ExpressionData).init(self.allocator);
        var token = self.nextUntilValid();
        while (true)
        {
            if (token.id == .RParen)
            {
                break;
            }
            else if (token.id == .Eof)
            {
                return Errors.SyntaxError;
            }
            else
            {
                var primary = try self.ParseExpression_Primary();
                var expr = try self.ParseExpression(primary, 0);
                params.append(expr)
                catch
                {
                    return Errors.OutOfMemoryError;
                };
            }
            token = self.nextUntilValid();
        }
        var ownedSlice = params.toOwnedSlice()
        catch
        {
            return Errors.OutOfMemoryError;
        };
        return ownedSlice;
    }
    pub fn ParseArgs(self: *Self) ![]VarData
    {
        var vars = std.ArrayList(VarData).init(self.allocator);

        var nextIsConst = false;
        var variableType: ?[]const u8 = null;
        var variableName: ?[]const u8 = null;

        while (true)
        {
            const token = self.nextUntilValid();
            if (token.id == .RParen)
            {
                if (variableName != null and variableType != null)
                {
                    const variableData = VarData
                    {
                        .name = variableName.?,
                        .typeName = variableType.?,
                        .isConst = nextIsConst,
                        .defaultValue = null
                    };
                    try vars.append(variableData);
                    variableName = null;
                    variableType = null;
                    nextIsConst = false;
                }
                break;
            }
            else if (token.id == .Eof)
            {
                var err: string = try self.WriteError("Syntax Error: End of file reached before arguments declaration end");
                try self.errorStatements.append(err);
                return Errors.SyntaxError;
            }
            else if (token.id == .Keyword_const)
            {
                if (nextIsConst)
                {
                    var err: string = try self.WriteError("Syntax Error: Duplicate const modifier");
                    try self.errorStatements.append(err);
                    return Errors.SyntaxError;
                }
                nextIsConst = true;
            }
            else if (token.id == .Identifier)
            {
                if (variableType == null)
                {
                    variableType = self.SourceTokenSlice(token);
                }
                else if (variableName == null)//variable name
                {
                    variableName = self.SourceTokenSlice(token);
                }
                else
                {
                    var err: string = try self.WriteError("Syntax Error: Duplicate variable name/type");
                    try self.errorStatements.append(err);
                    return Errors.SyntaxError;
                }
            }
            else if (token.id == .Comma)
            {
                if (variableName != null and variableType != null)
                {
                    const variableData = VarData
                    {
                        .name = variableName.?,
                        .typeName = variableType.?,
                        .isConst = nextIsConst,
                        .defaultValue = null
                    };
                    try vars.append(variableData);
                    variableName = null;
                    variableType = null;
                    nextIsConst = false;
                }
                else
                {
                    var err: string = try self.WriteError("Syntax Error: Comma must be after variable type and name");
                    try self.errorStatements.append(err);
                    return Errors.SyntaxError;
                }
            }
            else if (token.id == .Equal)
            {
                if (variableName != null and variableType != null)
                {
                    _ = self.nextUntilValid();
                    var primary = try self.ParseExpression_Primary();
                    var expression = try self.ParseExpression(primary, 0);

                    const variableData = VarData
                    {
                        .name = variableName.?,
                        .typeName = variableType.?,
                        .isConst = nextIsConst,
                        .defaultValue = expression
                    };
                    try vars.append(variableData);
                    variableName = null;
                    variableType = null;
                    nextIsConst = false;
                }
                else
                {
                    var err: string = try self.WriteError("Syntax Error: Comma must be after variable type and name");
                    try self.errorStatements.append(err);
                    return Errors.SyntaxError;
                }
            }
            else //type
            {
                if (variableType == null)
                {
                    variableType = self.SourceTokenSlice(token);
                }
                else
                {
                    var err: string = try self.WriteError("Syntax Error: Duplicate variable type");
                    try self.errorStatements.append(err);
                    return Errors.SyntaxError;
                }
            }
        }

        return vars.toOwnedSlice();
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

    /// parses an identifier in expression, returning either a ExpressionData with
    /// variable or with a function call
    pub fn ParseExpression_Identifier(self: *Self, identifierName: []const u8) Errors!ExpressionData
    {
        var nextToken = self.peekNextUntilValid();
        if (nextToken.id == .LParen)
        {
            var inputParams = try self.ParseInputParams();

            var functionCall = self.allocator.create(FunctionCallData)
            catch
            {
                return Errors.OutOfMemoryError;
            };
            functionCall.name = identifierName;
            functionCall.inputParams = inputParams;
            
            return ExpressionData
            {
                .FunctionCall = functionCall
            };
        }
        else
        {
            return ExpressionData
            {
                .Variable = identifierName
            };
        }
    }
    pub fn ParseExpression_Primary(self: *Self) Errors!ExpressionData
    {
        const token = self.tokenizer.next();
        if (token.id == .LParen)
        {
            //_ = self.tokenizer.next();

            var nextPrimary = try self.ParseExpression_Primary();
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
                    return self.ParseExpression_Identifier(self.tokenizer.buffer[token.start..nextToken.end]);
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
                    return self.ParseExpression_Identifier(self.tokenizer.buffer[token.start..nextToken.end]);
                    // _ = self.tokenizer.next();
                    // return ExpressionData
                    // {
                    //     .Variable = self.tokenizer.buffer[token.start..nextToken.end]
                    // };
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
            return self.ParseExpression_Identifier(self.SourceTokenSlice(token));
            // return ExpressionData
            // {
            //     .Variable = self.SourceTokenSlice(token)
            // };
        }
        else
        {
            std.debug.print("{s}\n", .{self.tokenizer.buffer[token.start..token.end]});
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
            if (op.id == .Eof or op.id == .Semicolon or op.id == .Comma or precedence == -1 or precedence < minPrecedence)
            {
                break;
            }
            _ = self.tokenizer.next();
            var rhs = try self.ParseExpression_Primary();

            while (true)
            {
                var next = self.peekNext();
                var nextPrecedence = GetPrecedence(next.id);
                var nextAssociation = GetAssociation(next.id);
                if (next.id == .Eof or op.id == .Semicolon or op.id == .Comma or nextPrecedence == -1 or nextPrecedence == precedence or nextAssociation == 1)
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
    var primary = try parser.ParseExpression_Primary();
    var expr = parser.ParseExpression(primary, 0) catch |err|
    {
        for (parser.errorStatements.items) |errorStatement|
        {
            std.debug.print("Caught error:\n   {s}\n", .{errorStatement.str()});
        }
        try parser.deinit();
        return err;
    };
    var str = try expr.ToString(alloc);
    defer str.deinit();
    std.debug.print("{s}\n", .{str.str()});

    expr.deinit(alloc);
    
    try parser.deinit();
}

// test "expression parsing"
// {
//     try TestExpressionParsing();
// }

test "file parsing"
{
    var alloc: std.mem.Allocator = std.testing.allocator;

    var buffer: []const u8 = try io.ReadFile("C:/Users/Linus/source/repos/Linxc/Tests/HelloWorld.linxc", alloc);//"#include<stdint.h>";

    var tokenizer: lexer.Tokenizer = lexer.Tokenizer
    {
        .buffer = buffer
    };
    var parser: Parser = try Parser.init(alloc, tokenizer);
    std.debug.print("\n", .{});

    var result = parser.Parse()
    catch |err|
    {
        try parser.deinit();
        alloc.free(buffer);
        return err;
    };

    var str = try CompoundStatementToString(result, alloc);
    std.debug.print("{s}\n", .{str.str()});
    str.deinit();

    for (result.items) |*stmt|
    {
        stmt.deinit(alloc);
    }
    result.deinit();

    try parser.deinit();

    alloc.free(buffer);
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

// NAME(args)
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
    inputParams: []ExpressionData,

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

// returnType name(args) { statement }
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
        try str.concat("}\n");
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
    FunctionCall
};
pub const ExpressionData = union(ExpressionDataTag)
{
    Literal: []const u8,
    Variable: []const u8,
    Op: *OperatorData,
    FunctionCall: *FunctionCallData,

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
            },
            ExpressionDataTag.FunctionCall => |call| 
            {
                str.deinit();
                return call.ToString(allocator);
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

pub const WhileData = struct
{
    condition: ExpressionData,
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
    condition: ExpressionData,
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
    structDeclaration,
    functionInvoke,
    returnStatement,
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
    structDeclaration: StructData,
    functionInvoke: FunctionCallData,
    returnStatement: ExpressionData,
    IfStatement: IfData,
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
            .structDeclaration => |*decl| decl.deinit(allocator),
            .functionInvoke => |*invoke| invoke.deinit(allocator),
            .returnStatement => |*stmt| stmt.deinit(allocator),
            .IfStatement => |*ifData| ifData.deinit(allocator),
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
            .structDeclaration => |*decl| return decl.ToString(allocator),
            .functionInvoke => |*invoke| return invoke.ToString(allocator),
            .returnStatement => |*stmt| return stmt.ToString(allocator),
            .IfStatement => |*ifDat| return ifDat.ToString(allocator),
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