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
    currentLine: usize,
    charsParsed: usize,

    pub fn init(allocator: std.mem.Allocator, tokenizer: lexer.Tokenizer) !Self 
    {
        return Self
        {
            .allocator = allocator,
            .tokenizer = tokenizer,
            .errorStatements = std.ArrayList(string).init(allocator),
            .currentLine = 0,
            .charsParsed = 0
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

    pub fn WriteError(self: *Self, message: []const u8) Errors!string
    {
        var err: string = string.init(self.allocator);
        err.concat(message)
        catch
        {
            return Errors.OutOfMemoryError;
        };
        err.concat("\n")
        catch
        {
            return Errors.OutOfMemoryError;
        };
        var line: []u8 = std.fmt.allocPrint(self.allocator, "at line {d}, column {d}\n", .{self.currentLine, self.tokenizer.index - self.charsParsed})
        catch
        {
            return Errors.OutOfMemoryError;
        };
        err.concat(line)
        catch
        {
            return Errors.OutOfMemoryError;
        };

        self.allocator.free(line);
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
            if (next.id == .Nl)
            {
                self.currentLine += 1;
                self.charsParsed = self.tokenizer.index;
            }
            next = self.tokenizer.next();
        }
        return next;
    }
    pub fn peekNext(self: *@This()) lexer.Token
    {
        const prevIndex = self.tokenizer.prevIndex;
        const currentIndex = self.tokenizer.index;

        const result = self.tokenizer.next();

        self.tokenizer.index = currentIndex;
        self.tokenizer.prevIndex = prevIndex;

        return result;
    }
    pub fn peekNextUntilValid(self: *@This()) lexer.Token
    {
        const prevIndex = self.tokenizer.prevIndex;
        const currentIndex = self.tokenizer.index;

        const result = self.nextUntilValid();

        self.tokenizer.index = currentIndex;
        self.tokenizer.prevIndex = prevIndex;

        return result;
    }
    pub fn peekNextUntilFoundOperator(self: *@This()) lexer.Token
    {
        const prevIndex = self.tokenizer.prevIndex;
        const currentIndex = self.tokenizer.index;

        //dont do any validation checking here
        var result = self.nextUntilValid();
        while (result.id == .Period)
        {
            _ = self.nextUntilValid();
            result = self.nextUntilValid();
        }
        self.tokenizer.index = currentIndex;
        self.tokenizer.prevIndex = prevIndex;

        return result;
    }
    
    pub fn Parse(self: *Self, endOnSemicolon: bool, commaIsSemicolon: bool, endOnRParen: bool) !CompoundStatementData
    {
        var result = CompoundStatementData.init(self.allocator);

        //var start: ?usize = null;
        //var end: usize = 0;
        var skipThisLine: bool = false;
        var braceCont: i32 = 0;
        var foundIdentifier: ?[]const u8 = null;
        var expectSemicolon: bool = false;

        var typeNameStart: ?usize = null;
        var typeNameEnd: usize = 0;

        var nextIsConst: bool = false;

        var nextIsStruct: bool = false;

        while (true)
        {
            const token: lexer.Token = self.tokenizer.next();

            //there are too many ways that a statement may just be an expression,
            //which in itself is valid too. Thus, we just perform a check if the expression is statement. If so,
            //we simply return
            if (expectSemicolon)
            {
                if (token.id != .Nl)
                {
                    if (token.id == .RParen and endOnRParen)
                    {

                    }
                    else if (token.id != .Semicolon and (!commaIsSemicolon or token.id != .Comma))
                    {
                        var err: string = try self.WriteError("Syntax Error: Missing semicolon");
                        try self.errorStatements.append(err);
                        ClearCompoundStatement(result, self.allocator);
                        return Errors.SyntaxError;
                    }
                    expectSemicolon = false;
                }
            }

            switch (token.id)
            {
                .Eof =>
                {
                    break;
                },
                .Nl =>
                {
                    self.currentLine += 1;
                    self.charsParsed = self.tokenizer.index;
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
                        ClearCompoundStatement(result, self.allocator);
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
                .Keyword_void, .Keyword_bool, .Keyword_float, .Keyword_int, .Keyword_uint, .Keyword_short, .Keyword_ushort, .Keyword_ulong =>
                {
                    if (!skipThisLine)
                    {
                        if (typeNameStart == null)
                        {
                            typeNameStart = token.start;
                        }
                        else
                        {
                            if (foundIdentifier != null)
                            {
                                var err: string = try self.WriteError("Syntax Error: Missing semicolon");
                                try self.errorStatements.append(err);
                                ClearCompoundStatement(result, self.allocator);
                                return Errors.SyntaxError;
                            }
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
                                self.currentLine += 1;
                                self.charsParsed = self.tokenizer.index;
                                next = self.nextUntilValid();
                            }
                            var primary = try self.ParseExpression_Primary();
                            var expression = try self.ParseExpression(primary, 0);
                        
                            next = self.nextUntilValid();
                            while (next.id != .LBrace)
                            {
                                next = self.nextUntilValid();
                            }
                            var statement = try self.Parse(endOnSemicolon, commaIsSemicolon, endOnRParen);

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
                .Keyword_else =>
                {
                    if (!skipThisLine)
                    {
                        if (self.peekNextUntilValid().id == .LBrace)
                        {
                            var nextStatement = try self.Parse(endOnSemicolon, commaIsSemicolon, endOnRParen);
                            try result.append(StatementData
                            {
                                .ElseStatement = nextStatement
                            });
                        }
                        else
                        {
                            //compound statement parse until semicolon
                            var nextStatement = try self.Parse(true, commaIsSemicolon, endOnRParen);
                            try result.append(StatementData
                            {
                                .ElseStatement = nextStatement
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
                            ClearCompoundStatement(result, self.allocator);
                            return Errors.SyntaxError;
                        }
                        else if (typeNameStart != null)
                        {
                            var err = try self.WriteError("Syntax Error: const should be before type");
                            try self.errorStatements.append(err);
                            ClearCompoundStatement(result, self.allocator);
                            return Errors.SyntaxError;
                        }
                        nextIsConst = true;
                    }
                },
                .Keyword_for =>
                {
                    if (!skipThisLine)
                    {
                        if (nextIsConst)
                        {
                            var err = try self.WriteError("Syntax Error: For statement cannot be const");
                            try self.errorStatements.append(err);
                            ClearCompoundStatement(result, self.allocator);
                            return Errors.SyntaxError;
                        }
                        if (self.nextUntilValid().id != .LParen)
                        {
                            var err = try self.WriteError("Syntax Error: Expected ( after for statement");
                            try self.errorStatements.append(err);
                            ClearCompoundStatement(result, self.allocator);
                            return Errors.SyntaxError;
                        }
                        //parse initializer statement
                        
                        const initializerStatements = try self.Parse(true, true, endOnRParen);

                        const conditionPrimary = try self.ParseExpression_Primary();
                        const condition = try self.ParseExpression(conditionPrimary, 0);
                        _ = self.tokenizer.next(); //skip the ;
                        if (self.tokenizer.prev_tok_id != .Semicolon)
                        {
                            var err: string = try self.WriteError("Syntax Error: Expected semicolon at the end of the condition expression in a for loop");
                            try self.errorStatements.append(err);
                            ClearCompoundStatement(result, self.allocator);
                            return Errors.SyntaxError;
                        }
                        const onStep = try self.Parse(true, commaIsSemicolon, true);

                        var bodyStatement: CompoundStatementData = undefined;
                        if (self.nextUntilValid().id != .LBrace)
                        {
                            self.tokenizer.index = self.tokenizer.prevIndex;
                            bodyStatement = try self.Parse(true, commaIsSemicolon, endOnRParen);
                        }
                        else
                        {
                            bodyStatement = try self.Parse(endOnSemicolon, commaIsSemicolon, endOnRParen);
                        }

                        try result.append(StatementData
                        {
                            .ForStatement = ForData
                            {
                                .initializer = initializerStatements,
                                .condition = condition,
                                .shouldStep = onStep,
                                .statement = bodyStatement
                            }
                        });
                    }
                },
                .Keyword_while =>
                {
                    if (!skipThisLine)
                    {
                        if (nextIsConst)
                        {
                            var err = try self.WriteError("Syntax Error: Duplicate const prefix! ");
                            try self.errorStatements.append(err);
                            ClearCompoundStatement(result, self.allocator);
                            return Errors.SyntaxError;
                        }
                        if (self.nextUntilValid().id != .LParen)
                        {
                            var err = try self.WriteError("Syntax Error: Expected ( after while statement");
                            try self.errorStatements.append(err);
                            ClearCompoundStatement(result, self.allocator);
                            return Errors.SyntaxError;
                        }
                        var primary = try self.ParseExpression_Primary();
                        var expr = try self.ParseExpression(primary, 0);
                        var next = self.nextUntilValid();
                        while (next.id != .LBrace)
                        {
                            next = self.nextUntilValid();
                        }
                        var statement = try self.Parse(endOnSemicolon, commaIsSemicolon, endOnRParen);
                        try result.append(StatementData
                        {
                            .WhileStatement = WhileData
                            {
                                .condition = expr,
                                .statement = statement
                            }
                        });
                    }
                },
                .Keyword_return =>
                {
                    if (!skipThisLine)
                    {
                        var primary = self.ParseExpression_Primary()
                        catch |err|
                        {
                            var errStr = try self.WriteError("Syntax Error: Issue parsing primary expression on return statement");
                            try self.errorStatements.append(errStr);
                            ClearCompoundStatement(result, self.allocator);
                            return err;
                        };
                        var expr = self.ParseExpression(primary, 0)
                        catch |err|
                        {
                            var errStr = try self.WriteError("Syntax Error: Issue parsing expression on return statement");
                            try self.errorStatements.append(errStr);
                            ClearCompoundStatement(result, self.allocator);
                            return err;
                        };

                        try result.append(StatementData
                        {
                            .returnStatement = expr
                        });

                        expectSemicolon = true;
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
                            ClearCompoundStatement(result, self.allocator);
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
                                var err = try self.WriteError("Syntax Error: Linxc requires struct name to be right after the struct keyword");
                                try self.errorStatements.append(err);
                                ClearCompoundStatement(result, self.allocator);
                                return Errors.SyntaxError;
                            }
                            const structBody = self.Parse(endOnSemicolon, commaIsSemicolon, endOnRParen)
                            catch |err|
                            {
                                ClearCompoundStatement(result, self.allocator);
                                return err;
                            };
                            const structData = StructData
                            {
                                .name = structName,
                                .body = structBody
                            };
                            try result.append(StatementData{.structDeclaration = structData});
                            nextIsStruct = false;
                            typeNameStart = null;
                            typeNameEnd = 0;

                            expectSemicolon = true;
                        }
                        else if (GetPrecedence(self.peekNextUntilFoundOperator().id) != -1)
                        {
                            //is some kind of expression
                            //go back
                            self.tokenizer.index = self.tokenizer.prevIndex;
                            //parse expression as per normal
                            var primary = self.ParseExpression_Primary()
                            catch |err|
                            {
                                var errStr = try self.WriteError("Syntax Error: Issue parsing primary expression");
                                try self.errorStatements.append(errStr);
                                ClearCompoundStatement(result, self.allocator);
                                return err;
                            };
                            var expr = self.ParseExpression(primary, 0)
                            catch |err|
                            {
                                var errStr = try self.WriteError("Syntax Error: Issue parsing expression");
                                try self.errorStatements.append(errStr);
                                ClearCompoundStatement(result, self.allocator);
                                return err;
                            };

                            try result.append(StatementData
                            {
                                .otherExpression = expr
                            });
                            expectSemicolon = true;
                        }
                        else if (typeNameStart == null)
                        {
                            typeNameStart = token.start;
                            var next = self.peekNextUntilValid();
                            if (next.id == .ColonColon)
                            {
                                while (true)
                                {
                                    next = self.nextUntilValid();
                                    if (next.id == .ColonColon or next.id == .Identifier)
                                    {
                                        typeNameEnd = next.end;
                                    }
                                    else break;
                                }
                            }
                            else typeNameEnd = token.end;

                            //handle standalone function invocation
                            if (next.id == .LParen)
                            {
                                foundIdentifier = self.SourceSlice(typeNameStart.?, typeNameEnd);
                                typeNameStart = null;
                                typeNameEnd = 0;
                            }
                        }
                        else 
                        {
                            if (foundIdentifier != null)
                            {
                                var errStr = try self.WriteError("Syntax Error: Duplicate identifier");
                                try self.errorStatements.append(errStr);
                                ClearCompoundStatement(result, self.allocator);
                                return Errors.SyntaxError;
                            }
                            foundIdentifier = self.SourceTokenSlice(token);
                        }
                    }
                },
                .RParen =>
                {
                    if (endOnRParen)
                    {
                        break;
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
                                    ClearCompoundStatement(result, self.allocator);
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
                                const body = try self.Parse(endOnSemicolon, commaIsSemicolon, endOnRParen);

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
                                //just use parseidentifier here, if rando variable, throw it
                                //we need ParseExpression_Identifier to detect the beginning LParen so we move back 1 step
                                self.tokenizer.index = self.tokenizer.prevIndex;
                                var expr: ExpressionChain = try self.ParseExpression_Identifier(foundIdentifier.?);
                                switch (expr.expression)
                                {
                                    .FunctionCall => |funcCall|
                                    {
                                        try result.append(StatementData
                                        {
                                            .functionInvoke = FunctionCallData
                                            {
                                                .name = funcCall.name,
                                                .inputParams = funcCall.inputParams
                                            }
                                        }
                                        );
                                        //deinit original funcCall item
                                    },
                                    else =>
                                    {
                                        expr.deinit(self.allocator);
                                        var err = try self.WriteError("Syntax Error: Random literal/variable name");
                                        try self.errorStatements.append(err);
                                        ClearCompoundStatement(result, self.allocator);
                                        return Errors.SyntaxError;
                                    }
                                }
                                expectSemicolon = true;
                            }

                            foundIdentifier = null;
                            typeNameStart = null;
                            typeNameEnd = 0;
                        }
                    }
                },
                .Minus, .Asterisk, .Bang, .Tilde =>
                {
                    if (typeNameStart == null)
                    {
                        const peekForwardID = self.peekNext().id;
                        if ((token.id == .Asterisk and peekForwardID == .Identifier) or (token.id != .Asterisk and (peekForwardID == .Identifier or peekForwardID == .IntegerLiteral or peekForwardID == .FloatLiteral)))
                        {
                            //is some kind of expression
                            //go back
                            self.tokenizer.index = self.tokenizer.prevIndex;
                            //parse expression as per normal
                            var primary = self.ParseExpression_Primary()
                            catch |err|
                            {
                                var errStr = try self.WriteError("Syntax Error: Issue parsing expression");
                                try self.errorStatements.append(errStr);
                                ClearCompoundStatement(result, self.allocator);
                                return err;
                            };
                            var expr = self.ParseExpression(primary, 0)
                            catch |err|
                            {
                                var errStr = try self.WriteError("Syntax Error: Issue parsing expression");
                                try self.errorStatements.append(errStr);
                                ClearCompoundStatement(result, self.allocator);
                                return err;
                            };

                            try result.append(StatementData
                            {
                                .otherExpression = expr
                            });
                        }
                    }
                    else
                    {
                        if (token.id == .Asterisk)
                        {
                            typeNameEnd = token.end;
                        }
                        else
                        {
                            ClearCompoundStatement(result, self.allocator);
                            return Errors.SyntaxError;
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
                        else if (nextIsStruct)
                        {
                            var errStr = try self.WriteError("Syntax Error: Expected identifier after struct declaration");
                            try self.errorStatements.append(errStr);
                            ClearCompoundStatement(result, self.allocator);
                            return Errors.SyntaxError;
                        }
                        else if (nextIsConst)
                        {
                            var errStr = try self.WriteError("Syntax Error: Expected identifier and type after const declaration");
                            try self.errorStatements.append(errStr);
                            ClearCompoundStatement(result, self.allocator);
                            return Errors.SyntaxError;
                        }
                        else if (typeNameStart != null)
                        {
                            var errStr = try self.WriteError("Syntax Error: Expected identifier after variable/function type");
                            try self.errorStatements.append(errStr);
                            ClearCompoundStatement(result, self.allocator);
                            return Errors.SyntaxError;
                        }
                        if (endOnSemicolon)
                        {
                            break;
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
                            var primary = try self.ParseExpression_Primary();
                            const expression: ExpressionChain = try self.ParseExpression(primary, 0);
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

                            expectSemicolon = true;
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
    ///Gets the full identifier of a variable type by compiling :: namespaces
    pub fn GetFullIdentifier(self: *Self, comptime untilValid: bool) ?usize
    {
        var typeNameEnd: ?usize = null;
        if (untilValid)
        {
            var next = self.peekNextUntilValid();
            if (next.id == .ColonColon)
            {
                while (true)
                {
                    var initial: usize = self.tokenizer.index;
                    next = self.nextUntilValid();
                    if (next.id == .ColonColon or next.id == .Identifier)
                    {
                        typeNameEnd = next.end;
                    }
                    else
                    {
                        self.tokenizer.index = initial;
                        break;
                    }
                }
            }
        }
        else
        {
            var next = self.peekNext();
            if (next.id == .ColonColon)
            {
                while (true)
                {
                    var initial: usize = self.tokenizer.index;
                    next = self.tokenizer.next();
                    if (next.id == .ColonColon or next.id == .Identifier)
                    {
                        typeNameEnd = next.end;
                    }
                    else
                    {
                        self.tokenizer.index = initial;
                        break;
                    }
                }
            }
        }
        return typeNameEnd;
    }
    pub fn ParseArgs(self: *Self) ![]VarData
    {
        var vars = std.ArrayList(VarData).init(self.allocator);

        var nextIsConst = false;
        var variableType: ?[]const u8 = null;
        var variableName: ?[]const u8 = null;
        var defaultValueExpr: ?ExpressionChain = null;
        var encounteredFirstDefaultValue: bool = false;

        while (true)
        {
            const token = self.nextUntilValid();
            if (token.id == .Semicolon or token.id == .Period)
            {
                var err: string = try self.WriteError("Syntax Error: Unidentified/disallowed character token in arguments declaration");
                try self.errorStatements.append(err);
                return Errors.SyntaxError;
            }
            if (token.id == .RParen)
            {
                if (variableName != null and variableType != null)
                {
                    if (encounteredFirstDefaultValue and defaultValueExpr == null)
                    {
                        var err: string = try self.WriteError("Syntax Error: All arguments with default values must be declared only after arguments without");
                        try self.errorStatements.append(err);
                        return Errors.SyntaxError;
                    }
                    const variableData = VarData
                    {
                        .name = variableName.?,
                        .typeName = variableType.?,
                        .isConst = nextIsConst,
                        .defaultValue = defaultValueExpr
                    };
                    try vars.append(variableData);
                    variableName = null;
                    variableType = null;
                    defaultValueExpr = null;
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
                //todo: handle accessor chaining properly, with array indexing with expressions,
                //function chaining
                if (variableType == null)
                {
                    //detect variable type
                    var typeNameEnd: usize = self.GetFullIdentifier(true) orelse token.end;
                    variableType = self.SourceSlice(token.start, typeNameEnd);
                }
                else if (variableName == null)//variable name
                {
                    variableName = self.SourceTokenSlice(token);
                }
                else
                {
                    var err: string = try self.WriteError("Syntax Error: Duplicate variable type or name in arguments declaration");
                    try self.errorStatements.append(err);
                    return Errors.SyntaxError;
                }
            }
            else if (token.id == .Equal)
            {
                if (variableType == null)
                {
                    var err: string = try self.WriteError("Syntax Error: Expected argument type declaration before using = sign to declare default value");
                    try self.errorStatements.append(err);
                    return Errors.SyntaxError;
                }
                if (variableName == null)
                {
                    var err: string = try self.WriteError("Syntax Error: Expected argument name declaration before using = sign to declare default value");
                    try self.errorStatements.append(err);
                    return Errors.SyntaxError;
                }
                var primary = try self.ParseExpression_Primary();
                defaultValueExpr = try self.ParseExpression(primary, 0);
                encounteredFirstDefaultValue = true;
            }
            else if (token.id == .Comma)
            {
                if (variableName != null and variableType != null)
                {
                    if (encounteredFirstDefaultValue and defaultValueExpr == null)
                    {
                        var err: string = try self.WriteError("Syntax Error: All arguments with default values must be declared only after arguments without");
                        try self.errorStatements.append(err);
                        return Errors.SyntaxError;
                    }
                    const variableData = VarData
                    {
                        .name = variableName.?,
                        .typeName = variableType.?,
                        .isConst = nextIsConst,
                        .defaultValue = defaultValueExpr
                    };
                    try vars.append(variableData);
                    variableName = null;
                    variableType = null;
                    defaultValueExpr = null;
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
                    var err: string = try self.WriteError("Syntax Error: Duplicate variable type in arguments declaration");
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
            .Minus, .Plus, .AmpersandAmpersand, .PipePipe =>
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
            .PlusEqual, .MinusEqual, .AsteriskEqual, .PercentEqual, .SlashEqual =>
            {
                return 4;
            },
            .Asterisk, .Slash, .Percent =>
            {
                return 3;
            },
            .Plus, .Minus, .Ampersand, .Caret, .Tilde, .Pipe, .AngleBracketLeft, .AngleBracketRight =>
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

    pub fn ParseInputParams(self: *Self, comptime endOnRBracket: bool) Errors![]ExpressionChain
    {
        var params = std.ArrayList(ExpressionChain).init(self.allocator);
        var token = self.nextUntilValid();
        while (true)
        {
            if (token.id == .RParen)
            {
                if (!endOnRBracket)
                {
                    break;
                }
                else
                {
                    var err = try self.WriteError("Syntax Error: Expecting function arguments to be closed on a ), not ]");
                    self.errorStatements.append(err)
                    catch
                    {
                        return Errors.OutOfMemoryError;
                    };
                    return Errors.SyntaxError;
                }
            }
            else if (token.id == .RBracket)
            {
                if (endOnRBracket)
                {
                    break;
                }
                else
                {
                    var err = try self.WriteError("Syntax Error: Expecting indexer to be closed on a ], not )");
                    self.errorStatements.append(err)
                    catch
                    {
                        return Errors.OutOfMemoryError;
                    };
                    return Errors.SyntaxError;
                }
            }
            else if (token.id == .Eof or token.id == .Semicolon)
            {
                var err = try self.WriteError("Syntax Error: Reached end of line while expecting )");
                self.errorStatements.append(err)
                catch
                {
                    return Errors.OutOfMemoryError;
                };
                return Errors.SyntaxError;
            }
            else if (token.id == .Comma)
            {
                
            }
            else
            {
                self.tokenizer.index = self.tokenizer.prevIndex;
                
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

    /// parses an identifier in expression, returning either a ExpressionData with
    /// variable or with a function call
    pub fn ParseExpression_Identifier(self: *Self, identifierName: []const u8) Errors!ExpressionChain
    {
        var result: ExpressionChain = undefined;
        var token = self.peekNextUntilValid();
        if (token.id == .LParen)
        {
            _ = self.nextUntilValid(); //advance beyond the (
            var inputParams = try self.ParseInputParams(false);

            var functionCall = self.allocator.create(FunctionCallData)
            catch
            {
                return Errors.OutOfMemoryError;
            };
            functionCall.name = identifierName;
            functionCall.inputParams = inputParams;

            result = ExpressionChain
            {
                .expression = ExpressionData
                {
                    .FunctionCall = functionCall
                }
            };
        }
        else if (token.id == .LBracket) //array indexer
        {
            _ = self.nextUntilValid(); //advance beyond the [
            var indexParams = try self.ParseInputParams(true);

            var functionCall = self.allocator.create(FunctionCallData)
            catch
            {
                return Errors.OutOfMemoryError;
            };
            functionCall.name = identifierName;
            functionCall.inputParams = indexParams;

            result = ExpressionChain
            {
                .expression = ExpressionData
                {
                    .IndexedAccessor = functionCall
                }
            };
        }
        else
        {
            result = ExpressionChain
            {
                .expression = ExpressionData
                {
                    .Variable = identifierName
                }
            };
        }

        var nextIdentifierPtr: ?*ExpressionChain = null;

        //function call changing/retrieval
        //eg: glm::vec2(1, 2).add(2, 3).x
        var peekNextID = self.peekNextUntilValid().id;
        if (peekNextID == .Period)
        {
            _ = self.nextUntilValid();
            var next = self.nextUntilValid();//peekNextUntilValid();
            if (next.id != .Identifier)
            {
                var err = try self.WriteError("Syntax Error: Identifier expected after period");
                self.errorStatements.append(err)
                catch
                {
                    return Errors.OutOfMemoryError;
                };
            }
            else
            {
                var nextIdentifierName = self.SourceTokenSlice(next);
                const nextIdentifier = try self.ParseExpression_Identifier(nextIdentifierName);

                nextIdentifierPtr = self.allocator.create(ExpressionChain)
                catch
                {
                    return Errors.OutOfMemoryError;
                };
                nextIdentifierPtr.?.expression = nextIdentifier.expression;
                nextIdentifierPtr.?.next = nextIdentifier.next;
            }
        }

        result.next = nextIdentifierPtr;
        return result;
    }
    pub fn ParseExpression_Primary(self: *Self) Errors!ExpressionChain
    {
        const token = self.tokenizer.next();
        if (token.id == .LParen)
        {
            //_ = self.tokenizer.next();

            var nextPrimary = try self.ParseExpression_Primary();
            var result = try self.ParseExpression(nextPrimary, 0);

            if (self.tokenizer.buffer[self.tokenizer.index] != ')')
            {
                var err = try self.WriteError("Syntax Error: Expected )");
                self.errorStatements.append(err)
                catch
                {
                    return Errors.OutOfMemoryError;
                };
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
                    return ExpressionChain
                    {
                        .expression = ExpressionData
                        {
                            .Literal = self.tokenizer.buffer[token.start..nextToken.end]
                        }
                    };
                }
                else if (nextToken.id == .Identifier)
                {
                    _ = self.tokenizer.next();
                    var typeNameEnd: usize = self.GetFullIdentifier(false) orelse token.end;
                    return self.ParseExpression_Identifier(self.tokenizer.buffer[token.start..typeNameEnd]);
                }
                else
                {
                    var err = try self.WriteError("Syntax Error: Attempting to place negative sign on unrecognised literal");
                    self.errorStatements.append(err)
                    catch
                    {
                        return Errors.OutOfMemoryError;
                    };
                    return Errors.SyntaxError;
                }
            }
            else
            {
                var err = try self.WriteError("Syntax Error: Spacing is not allowed between the negative sign and whatever comes next");
                self.errorStatements.append(err)
                catch
                {
                    return Errors.OutOfMemoryError;
                };
                return Errors.SyntaxError;
            }
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
                    return ExpressionChain
                    {
                        .expression = ExpressionData
                        {
                            .Variable = self.tokenizer.buffer[token.start..nextToken.end]
                        }
                    };
                }
                else
                {
                    var err = try self.WriteError("Syntax Error: Expected identifier after pointer dereference asterisk");
                    self.errorStatements.append(err)
                    catch
                    {
                        return Errors.OutOfMemoryError;
                    };
                    return Errors.SyntaxError;
                }
            }
            else 
            {
                var err = try self.WriteError("Syntax Error: Spacing not allowed after pointer dereference asterisk!");
                self.errorStatements.append(err)
                catch
                {
                    return Errors.OutOfMemoryError;
                };
                return Errors.SyntaxError;
            }
        }
        else if (token.id == .Bang)
        {
            if (self.tokenizer.buffer[token.end] != ' ')
            {
                const nextToken = self.peekNext();
                if (nextToken.id == .Identifier)
                {
                    _ = self.tokenizer.next();
                    var typeNameEnd: usize = self.GetFullIdentifier(false) orelse token.end;
                    return self.ParseExpression_Identifier(self.tokenizer.buffer[token.start..typeNameEnd]);
                }
                else if (nextToken.id == .Keyword_true or nextToken.id == .Keyword_false)
                {
                    var err = try self.WriteError("Syntax Error: !true and !false are not allowed for the sake of clarity");
                    self.errorStatements.append(err)
                    catch
                    {
                        return Errors.OutOfMemoryError;
                    };
                    return Errors.SyntaxError;
                }
                else
                {
                    var err = try self.WriteError("Syntax Error: Cannot put ! in front of non identifier!");
                    self.errorStatements.append(err)
                    catch
                    {
                        return Errors.OutOfMemoryError;
                    };
                    return Errors.SyntaxError;
                }
            }
            else return Errors.SyntaxError;
        }
        else if (token.id == .IntegerLiteral or token.id == .StringLiteral or token.id == .FloatLiteral or token.id == .CharLiteral or token.id == .Keyword_true or token.id == .Keyword_false)
        {
            return ExpressionChain
            {
                .expression = ExpressionData
                {
                    .Literal = self.SourceTokenSlice(token)
                }
            };
        }
        else if (token.id == .Identifier)
        {
            var typeNameEnd: usize = self.GetFullIdentifier(false) orelse token.end;
            return self.ParseExpression_Identifier(self.SourceSlice(token.start, typeNameEnd));
        }
        else
        {
            return Errors.SyntaxError;
        }
    }
    pub fn ParseExpression(self: *Self, initial: ExpressionChain, minPrecedence: i32) Errors!ExpressionChain
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

            lhs.expression = ExpressionData
            {
                .Op = operator
            };
        }

        var resultStr = lhs.ToString(self.allocator)
        catch
        {
            return Errors.NotImplemented;
        };
        resultStr.deinit();

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
    var arena = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena.deinit();
    var alloc: std.mem.Allocator = arena.allocator();

    var buffer: []const u8 = try io.ReadFile("C:/Users/Linus/source/repos/Linxc/Tests/HelloWorld.linxc", alloc);//"#include<stdint.h>";

    var tokenizer: lexer.Tokenizer = lexer.Tokenizer
    {
        .buffer = buffer
    };
    var parser: Parser = try Parser.init(alloc, tokenizer);
    std.debug.print("\n", .{});

    var result = parser.Parse(false, false, false)
    catch
    {
        for (parser.errorStatements.items) |errorStatement|
        {
            std.debug.print("{s}\n", .{errorStatement.str()});
        }

        try parser.deinit();
        alloc.free(buffer);
        return;
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
    PercentEqual
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
    .{"PercentEqual", Operator.PercentEqual}
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
        try str.concat("{");
        try str.concat_deinit(&leftString);
        try str.concat(" ");
        try str.concat(@tagName(self.operator));
        try str.concat(" ");
        try str.concat_deinit(&rightString);
        try str.concat("}");

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
            .structDeclaration => |*decl| decl.deinit(allocator),
            .functionInvoke => |*invoke| invoke.deinit(allocator),
            .returnStatement => |*stmt| stmt.deinit(allocator),
            .otherExpression => |*stmt| stmt.deinit(allocator),
            .IfStatement => |*ifData| ifData.deinit(allocator),
            .ElseStatement => |*elseData|
            {
                ClearCompoundStatement(elseData.*, allocator);
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

pub fn ClearCompoundStatement(compoundStatement: CompoundStatementData, allocator: std.mem.Allocator) void
{
    for (compoundStatement.items) |*stmt|
    {
        stmt.deinit(allocator);
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