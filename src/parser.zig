//the parser is in charge of turning raw tokens received from the tokenizer(lexer) into an AST.
const ASTnodes = @import("ASTnodes.zig");
const VarData = ASTnodes.VarData;
const TagData = ASTnodes.TagData;
const MacroDefinitionData = ASTnodes.MacroDefinitionData;
const FunctionCallData = ASTnodes.FunctionCallData;
const FunctionData = ASTnodes.FunctionData;
const ExpressionDataTag = ASTnodes.ExpressionDataTag;
const ExpressionData = ASTnodes.ExpressionData;
const ExpressionChain = ASTnodes.ExpressionChain;
const Operator = ASTnodes.Operator;
const TokenToOperator = ASTnodes.TokenToOperator;
const OperatorData = ASTnodes.OperatorData;
const WhileData = ASTnodes.WhileData;
const ForData = ASTnodes.ForData;
const IfData = ASTnodes.IfData;
const StructData = ASTnodes.StructData;
const StatementDataTag = ASTnodes.StatementDataTag;
const StatementData = ASTnodes.StatementData;
const CompoundStatementData = ASTnodes.CompoundStatementData;
const ClearCompoundStatement = ASTnodes.ClearCompoundStatement;
const CompoundStatementToString = ASTnodes.CompoundStatementToString;

const std = @import("std");
const string = @import("zig-string.zig").String;
const lexer = @import("lexer.zig");
const linkedLists = @import("linked-list.zig");
const objPool = @import("object-pool.zig");
const ExprPool = objPool.ObjectPool(linkedLists.LinkedList(ExpressionData));
const io = @import("io.zig");
const Errors = @import("errors.zig").Errors;

pub const Parser = struct {
    pub const LinkedList = linkedLists.LinkedList(ExpressionData);
    pub const NodePointerList = std.ArrayList(*linkedLists.LinkedList(ExpressionData).Node);
    const Self = @This();
    allocator: std.mem.Allocator,
    tokenizer: lexer.Tokenizer,
    errorStatements: std.ArrayList(string),
    currentLine: usize,
    charsParsed: usize,
    postParseStatement: ?*const fn(statement: StatementData, parentStatement: ?[]const u8) anyerror!void,

    pub fn init(allocator: std.mem.Allocator, tokenizer: lexer.Tokenizer) !Self 
    {
        return Self
        {
            .allocator = allocator,
            .tokenizer = tokenizer,
            .errorStatements = std.ArrayList(string).init(allocator),
            .currentLine = 0,
            .charsParsed = 0,
            .postParseStatement = null
        };
    }
    pub fn deinit(self: *Self) void
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

    pub fn AppendToCompoundStatement(self: *Self, result: *CompoundStatementData, statement: StatementData, parentStatementName: ?[]const u8) !void
    {
        try result.append(statement);
        if (self.postParseStatement != null)
        {
            try self.postParseStatement.?(statement, parentStatementName);
        }
    }
    
    pub fn Parse(self: *Self, endOnSemicolon: bool, commaIsSemicolon: bool, endOnRParen: bool, parent: ?[]const u8) !CompoundStatementData
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
                        ClearCompoundStatement(result);
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
                .Keyword_delegate =>
                {

                    while (true)
                    {
                        var next = self.tokenizer.next();
                        if (next.id == .Semicolon)
                        {
                            break;
                        }
                        if (next.id == .LParen) //todo: delegate declaration statement
                        {

                        }
                        else if (next.id == .RParen)
                        {

                        }
                        else if (next.id == .Nl or next.id == .Eof)
                        {
                            var err: string = try self.WriteError("Syntax Error: Expected ; after delegate() declaration");
                            try self.errorStatements.append(err);
                            ClearCompoundStatement(result);
                            return Errors.SyntaxError;
                        }
                    }
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
                        try self.AppendToCompoundStatement(&result, statement, parent);
                    }
                    else
                    {
                        var err: string = try self.WriteError("Syntax Error: Expected enclosed string after #include statement, but found none ");
                        try self.errorStatements.append(err);
                        ClearCompoundStatement(result);
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
                                ClearCompoundStatement(result);
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
                            const statement = try self.Parse(endOnSemicolon, commaIsSemicolon, endOnRParen, null);

                            try self.AppendToCompoundStatement(&result, StatementData
                            {
                                .IfStatement = IfData
                                {
                                    .condition = expression,
                                    .statement = statement
                                }
                            }, parent);
                        }
                    }
                },
                .Keyword_else =>
                {
                    if (!skipThisLine)
                    {
                        var elseStatement: CompoundStatementData = undefined;
                        if (self.peekNextUntilValid().id == .LBrace)
                        {
                            elseStatement = try self.Parse(endOnSemicolon, commaIsSemicolon, endOnRParen, null);
                        }
                        else
                        {
                            //compound statement parse until semicolon
                            elseStatement = try self.Parse(true, commaIsSemicolon, endOnRParen, null);
                        }
                        try self.AppendToCompoundStatement(&result, StatementData{.ElseStatement=elseStatement}, parent);
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
                            ClearCompoundStatement(result);
                            return Errors.SyntaxError;
                        }
                        else if (typeNameStart != null)
                        {
                            var err = try self.WriteError("Syntax Error: const should be before type");
                            try self.errorStatements.append(err);
                            ClearCompoundStatement(result);
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
                            ClearCompoundStatement(result);
                            return Errors.SyntaxError;
                        }
                        if (self.nextUntilValid().id != .LParen)
                        {
                            var err = try self.WriteError("Syntax Error: Expected ( after for statement");
                            try self.errorStatements.append(err);
                            ClearCompoundStatement(result);
                            return Errors.SyntaxError;
                        }
                        //parse initializer statement
                        
                        const initializer = try self.Parse(true, true, endOnRParen, null);

                        const conditionPrimary = try self.ParseExpression_Primary();
                        const condition = try self.ParseExpression(conditionPrimary, 0);
                        _ = self.tokenizer.next(); //skip the ;
                        if (self.tokenizer.prev_tok_id != .Semicolon)
                        {
                            var err: string = try self.WriteError("Syntax Error: Expected semicolon at the end of the condition expression in a for loop");
                            try self.errorStatements.append(err);
                            ClearCompoundStatement(result);
                            return Errors.SyntaxError;
                        }
                        const shouldStep = try self.Parse(true, commaIsSemicolon, true, null);
                        var statement: CompoundStatementData = undefined;

                        if (self.nextUntilValid().id != .LBrace)
                        {
                            self.tokenizer.index = self.tokenizer.prevIndex;
                            statement = try self.Parse(true, commaIsSemicolon, endOnRParen, null);
                        }
                        else
                        {
                            statement = try self.Parse(endOnSemicolon, commaIsSemicolon, endOnRParen, null);
                        }

                        try self.AppendToCompoundStatement(&result, StatementData
                        {
                            .ForStatement = ForData
                            {
                                .initializer = initializer,
                                .condition = condition,
                                .shouldStep = shouldStep,
                                .statement = statement
                            }
                        }, parent);
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
                            ClearCompoundStatement(result);
                            return Errors.SyntaxError;
                        }
                        if (self.nextUntilValid().id != .LParen)
                        {
                            var err = try self.WriteError("Syntax Error: Expected ( after while statement");
                            try self.errorStatements.append(err);
                            ClearCompoundStatement(result);
                            return Errors.SyntaxError;
                        }
                        var primary = try self.ParseExpression_Primary();
                        const condition = try self.ParseExpression(primary, 0);
                        var next = self.nextUntilValid();
                        while (next.id != .LBrace)
                        {
                            next = self.nextUntilValid();
                        }
                        const statement = try self.Parse(endOnSemicolon, commaIsSemicolon, endOnRParen, null);
                        try self.AppendToCompoundStatement(&result, StatementData
                        {
                            .WhileStatement = WhileData
                            {
                                .condition = condition,
                                .statement = statement
                            }
                        }, parent);
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
                            ClearCompoundStatement(result);
                            return err;
                        };
                        var expr = self.ParseExpression(primary, 0)
                        catch |err|
                        {
                            var errStr = try self.WriteError("Syntax Error: Issue parsing expression on return statement");
                            try self.errorStatements.append(errStr);
                            ClearCompoundStatement(result);
                            return err;
                        };

                        try self.AppendToCompoundStatement(&result, StatementData
                        {
                            .returnStatement = expr
                        }, parent);

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
                            ClearCompoundStatement(result);
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
                                ClearCompoundStatement(result);
                                return Errors.SyntaxError;
                            }
                            const body = self.Parse(endOnSemicolon, commaIsSemicolon, endOnRParen, structName)
                            catch |err|
                            {
                                ClearCompoundStatement(result);
                                return err;
                            };

                            try self.AppendToCompoundStatement(&result, StatementData
                            {
                                .structDeclaration = StructData
                                {
                                    .name = structName,
                                    .body = body
                                }
                            }, parent);
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
                                ClearCompoundStatement(result);
                                return err;
                            };
                            var expr = self.ParseExpression(primary, 0)
                            catch |err|
                            {
                                var errStr = try self.WriteError("Syntax Error: Issue parsing expression");
                                try self.errorStatements.append(errStr);
                                ClearCompoundStatement(result);
                                return err;
                            };

                            try self.AppendToCompoundStatement(&result, StatementData
                            {
                                .otherExpression = expr
                            }, parent);
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
                        }
                        else 
                        {
                            if (foundIdentifier != null)
                            {
                                var errStr = try self.WriteError("Syntax Error: Duplicate identifier");
                                try self.errorStatements.append(errStr);
                                ClearCompoundStatement(result);
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
                        if (foundIdentifier == null and typeNameStart != null)
                        {
                            foundIdentifier = self.SourceSlice(typeNameStart.?, typeNameEnd);
                            typeNameStart = null;
                            typeNameEnd = 0;
                        
                            var next = self.peekNextUntilValid();
                            if (next.id == .Asterisk)
                            {
                                var err = try self.WriteError("Syntax Error: found C style function pointer, use delegate() macro from Linxc.h to typedef function pointer types and then use them instead");
                                try self.errorStatements.append(err);
                                ClearCompoundStatement(result);
                                return Errors.SyntaxError;
                            }
                        }
                        //declare function
                        if (foundIdentifier != null)
                        {
                            if (typeNameStart != null)
                            {
                                if (nextIsConst)
                                {
                                    var err = try self.WriteError("Syntax Error: Cannot declare function as const");
                                    try self.errorStatements.append(err);
                                    ClearCompoundStatement(result);
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
                                const statement = try self.Parse(endOnSemicolon, commaIsSemicolon, endOnRParen, null);

                                try self.AppendToCompoundStatement(&result, StatementData
                                {
                                    .functionDeclaration = FunctionData
                                    {
                                        .name = functionName,
                                        .returnType = functionReturnType,
                                        .args = args,
                                        .statement = statement
                                    }
                                }, parent);
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
                                        try self.AppendToCompoundStatement(&result, StatementData
                                        {
                                            .functionInvoke = FunctionCallData
                                            {
                                                .name = funcCall.name,
                                                .inputParams = funcCall.inputParams
                                            }
                                        }, parent);
                                        //deinit original funcCall item
                                    },
                                    else =>
                                    {
                                        expr.deinit(self.allocator);
                                        var err = try self.WriteError("Syntax Error: Random literal/variable name");
                                        try self.errorStatements.append(err);
                                        ClearCompoundStatement(result);
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
                                ClearCompoundStatement(result);
                                return err;
                            };
                            var expr = self.ParseExpression(primary, 0)
                            catch |err|
                            {
                                var errStr = try self.WriteError("Syntax Error: Issue parsing expression");
                                try self.errorStatements.append(errStr);
                                ClearCompoundStatement(result);
                                return err;
                            };

                            try self.AppendToCompoundStatement(&result, StatementData
                            {
                                .otherExpression = expr
                            }, parent);
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
                            ClearCompoundStatement(result);
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
                            try self.AppendToCompoundStatement(&result, StatementData{.variableDeclaration = varData}, parent);
                            nextIsConst = false;
                            foundIdentifier = null;
                            typeNameStart = null;
                            typeNameEnd = 0;
                        }
                        else if (nextIsStruct)
                        {
                            var errStr = try self.WriteError("Syntax Error: Expected identifier after struct declaration");
                            try self.errorStatements.append(errStr);
                            ClearCompoundStatement(result);
                            return Errors.SyntaxError;
                        }
                        else if (nextIsConst)
                        {
                            var errStr = try self.WriteError("Syntax Error: Expected identifier and type after const declaration");
                            try self.errorStatements.append(errStr);
                            ClearCompoundStatement(result);
                            return Errors.SyntaxError;
                        }
                        else if (typeNameStart != null)
                        {
                            var errStr = try self.WriteError("Syntax Error: Expected identifier after variable/function type");
                            try self.errorStatements.append(errStr);
                            ClearCompoundStatement(result);
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
                            try self.AppendToCompoundStatement(&result, StatementData{.variableDeclaration = varData}, parent);
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
                var err = try self.WriteError("Syntax Error: Reached end of line while expecting closing character");
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
                    var typeNameEnd: usize = self.GetFullIdentifier(false) orelse token.end;
                    return self.ParseExpression_Identifier(self.tokenizer.buffer[token.start..typeNameEnd]);
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