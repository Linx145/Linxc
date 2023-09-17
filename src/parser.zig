//the parser is in charge of turning raw tokens received from the tokenizer(lexer) into an AST.
const ASTnodes = @import("ASTnodes.zig");
const VarData = ASTnodes.VarData;
const TagData = ASTnodes.TagData;
const MacroDefinitionData = ASTnodes.MacroDefinitionData;
const FunctionCallData = ASTnodes.FunctionCallData;
const FunctionData = ASTnodes.FunctionData;
const ExpressionDataTag = ASTnodes.ExpressionDataTag;
const ExpressionData = ASTnodes.ExpressionData;
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
const io = @import("io.zig");
const Errors = @import("errors.zig").Errors;

pub inline fn GetAssociation(ID: lexer.TokenID) i8
{
    switch (ID)
    {
        .Arrow, .Minus, .Plus, .Slash, .Percent, .AmpersandAmpersand, .PipePipe, .EqualEqual, .BangEqual, .AngleBracketLeft, .AngleBracketLeftEqual, .AngleBracketRight, .AngleBracketRightEqual, .Period =>
        {
            return 1; //left to right
        },
        else =>
        {
            return -1; //right to left
        }
    }
}
pub inline fn GetPrecedence(ID: lexer.TokenID) i32
{
    switch (ID)
    {
        .Period, .Arrow =>
        {
            return 5;
        },
        //Reserved for pointer dereference (*), NOT(!), bitwise not(~) and pointer reference (&) =>
        // {
        //     return 4;
        // },
        .Asterisk, .Slash, .Percent =>
        {
            return 3;
        },
        .Plus, .Minus, .Ampersand, .Caret, .Tilde, .Pipe, .AngleBracketLeft, .AngleBracketRight =>
        {
            return 2;
        },
        .PipePipe, .BangEqual, .EqualEqual, .AmpersandAmpersand =>
        {
            return 1;
        },
        .Equal, .PlusEqual, .MinusEqual, .AsteriskEqual, .PercentEqual, .SlashEqual =>
        {
            return 0;
        },
        else =>
        {
            return -1;
        }
    }
}

pub const ParseContext = enum
{
    other,
    traitDeclaration,
    forLoopInitialization,
    forLoopStep
};

pub const Parser = struct {
    const Self = @This();
    allocator: std.mem.Allocator,
    tokenizer: lexer.Tokenizer,
    errorStatements: std.ArrayList(string),
    currentLine: usize,
    charsParsed: usize,
    currentFile: ?[]const u8,
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
            .postParseStatement = null,
            .currentFile = null
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
        var line: []u8 = std.fmt.allocPrint(self.allocator, "at file {s}, line {d}, column {d}\n", .{self.currentFile orelse "Null", self.currentLine + 1, self.tokenizer.index - self.charsParsed})
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

    pub fn AppendToCompoundStatement(self: *Self, result: *CompoundStatementData, statement: StatementData, parentStatementName: ?[]const u8) !void
    {
        try result.append(statement);
        if (self.postParseStatement != null)
        {
            try self.postParseStatement.?(statement, parentStatementName);
        }
    }
    
    //end on semicolon: initializer in for loops
    //commaIsSemicolon: initializer in for loops, step statement(s) in for loops
    //end on rparen: step statement(s) in for loops

    pub fn Parse(self: *Self, parseContext: ParseContext, parent: ?[]const u8) !CompoundStatementData
    {
        const endOnSemicolon = parseContext == ParseContext.forLoopInitialization;
        const commaIsSemicolon = parseContext == ParseContext.forLoopStep or parseContext == ParseContext.forLoopInitialization;
        const endOnRParen = parseContext == ParseContext.forLoopStep;

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
        var nextIsTrait: bool = false;

        var nextTags = std.ArrayList(ExpressionData).init(self.allocator);
        defer nextTags.deinit();

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
                .Keyword_void, .Keyword_bool, .Keyword_float, .Keyword_int, .Keyword_i16, .Keyword_u16, .Keyword_i32, .Keyword_u32, .Keyword_u64, .Keyword_i64 =>
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
                            const statement = try self.Parse(ParseContext.other, null);

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
                        
                        const initializer = try self.Parse(ParseContext.forLoopInitialization, null);

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
                        const shouldStep = try self.Parse(ParseContext.forLoopStep, null);
                        var statement: CompoundStatementData = undefined;

                        if (self.nextUntilValid().id != .LBrace)
                        {
                            self.tokenizer.index = self.tokenizer.prevIndex;
                            statement = try self.Parse(ParseContext.other, null);
                        }
                        else
                        {
                            statement = try self.Parse(ParseContext.other, null);
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
                        const statement = try self.Parse(ParseContext.other, null);
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
                        if (parseContext == ParseContext.forLoopInitialization or parseContext == ParseContext.forLoopStep)
                        {
                            var errStr = try self.WriteError("Syntax Error: Cannot have return statement in for loop initialization or step function");
                            try self.errorStatements.append(errStr);
                            ClearCompoundStatement(result);
                            return Errors.SyntaxError;
                        }

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
                .Keyword_trait =>
                {
                    if (!skipThisLine)
                    {
                        if (parseContext == ParseContext.forLoopInitialization or parseContext == ParseContext.forLoopStep)
                        {
                            var errStr = try self.WriteError("Syntax Error: Cannot declare traits in for loop initialization or step function");
                            try self.errorStatements.append(errStr);
                            ClearCompoundStatement(result);
                            return Errors.SyntaxError;
                        }

                        if (nextIsTrait)
                        {
                            var err = try self.WriteError("Syntax Error: duplicate struct keyword");
                            try self.errorStatements.append(err);
                            ClearCompoundStatement(result);
                            return Errors.SyntaxError;
                        }
                        if (nextIsStruct)
                        {
                            var err = try self.WriteError("Syntax Error: cannot declare struct trait");
                            try self.errorStatements.append(err);
                            ClearCompoundStatement(result);
                            return Errors.SyntaxError;
                        }
                        if (nextIsConst)
                        {
                            var err = try self.WriteError("Syntax Error: cannot declare const trait");
                            try self.errorStatements.append(err);
                            ClearCompoundStatement(result);
                            return Errors.SyntaxError;
                        }
                        nextIsTrait = true;
                    }
                },
                .Keyword_struct =>
                {
                    if (!skipThisLine)
                    {
                        if (parseContext == ParseContext.forLoopInitialization or parseContext == ParseContext.forLoopStep)
                        {
                            var errStr = try self.WriteError("Syntax Error: Cannot declare structs in for loop initialization or step function");
                            try self.errorStatements.append(errStr);
                            ClearCompoundStatement(result);
                            return Errors.SyntaxError;
                        }

                        if (nextIsStruct)
                        {
                            var err = try self.WriteError("Syntax Error: duplicate struct keyword");
                            try self.errorStatements.append(err);
                            ClearCompoundStatement(result);
                            return Errors.SyntaxError;
                        }
                        if (nextIsTrait)
                        {
                            var err = try self.WriteError("Syntax Error: cannot declare struct trait");
                            try self.errorStatements.append(err);
                            ClearCompoundStatement(result);
                            return Errors.SyntaxError;
                        }
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
                        var next = self.peekNextUntilValid();
                        if (nextIsTrait)
                        {
                            const traitName = self.SourceTokenSlice(token);
                            if (next.id != .LBrace)
                            {
                                var err = try self.WriteError("Syntax Error: Unknown token after trait name, expected {");
                                try self.errorStatements.append(err);
                                ClearCompoundStatement(result);
                                return Errors.SyntaxError;
                            }
                            const body = self.Parse(ParseContext.traitDeclaration, traitName)
                            catch |err|
                            {
                                ClearCompoundStatement(result);
                                return err;
                            };

                            var tagsSlice: ?[]ExpressionData = null;
                            if (nextTags.items.len > 0)
                            {
                                tagsSlice = try nextTags.toOwnedSlice();
                            }

                            try self.AppendToCompoundStatement(&result, StatementData
                            {
                                .traitDeclaration = StructData
                                {
                                    .name = traitName,
                                    .tags = tagsSlice,
                                    .body = body
                                }
                            }, parent);
                            nextIsTrait = false;
                            typeNameStart = null;
                            typeNameEnd = 0;

                            expectSemicolon = true;
                        }
                        else if (nextIsStruct)
                        {
                            const structName = self.SourceTokenSlice(token);
                            if (next.id != .LBrace)
                            {
                                var err = try self.WriteError("Syntax Error: Unknown token after struct name, expected {");
                                try self.errorStatements.append(err);
                                ClearCompoundStatement(result);
                                return Errors.SyntaxError;
                            }
                            const body = self.Parse(ParseContext.other, structName)
                            catch |err|
                            {
                                ClearCompoundStatement(result);
                                return err;
                            };

                            var tagsSlice: ?[]ExpressionData = null;
                            if (nextTags.items.len > 0)
                            {
                                tagsSlice = try nextTags.toOwnedSlice();
                            }

                            try self.AppendToCompoundStatement(&result, StatementData
                            {
                                .structDeclaration = StructData
                                {
                                    .name = structName,
                                    .tags = tagsSlice,
                                    .body = body
                                }
                            }, parent);
                            nextIsStruct = false;
                            typeNameStart = null;
                            typeNameEnd = 0;

                            expectSemicolon = true;
                        }
                        else if (typeNameStart == null and (GetPrecedence(next.id) != -1 or next.id == .LParen))
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

                            //if we are outside a function body, function invokes are treated as macro tags
                            if (parent != null and expr == .FunctionCall)
                            {
                                std.debug.print("Tag found: {s}\n", .{expr.FunctionCall.name});
                                //expr.deinit(self.allocator);
                                try nextTags.append(expr);
                            }
                            else
                            {
                                try self.AppendToCompoundStatement(&result, StatementData
                                {
                                    .otherExpression = expr
                                }, parent);
                                expectSemicolon = true;
                            }
                        }
                        else if (typeNameStart == null)
                        {
                            typeNameStart = token.start;
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
                            if (next.id == .ColonColon)
                            {
                                var errStr = try self.WriteError("Syntax Error: Illegal token :: after function name declaration");
                                try self.errorStatements.append(errStr);
                                ClearCompoundStatement(result);
                                return Errors.SyntaxError;
                            }
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
                                        const statement = try self.Parse(ParseContext.other, null);

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
                                    else if (next.id == .Semicolon)
                                    {
                                        if (parseContext == ParseContext.traitDeclaration)
                                        {
                                            try self.AppendToCompoundStatement(&result, StatementData
                                            {
                                                .functionDeclaration = FunctionData
                                                {
                                                    .name = functionName,
                                                    .returnType = functionReturnType,
                                                    .args = args,
                                                    .statement = CompoundStatementData.init(self.allocator)
                                                }
                                            }, parent);
                                        }
                                        else
                                        {
                                            var err = try self.WriteError("Syntax Error: All functions must declare a body unless they are methods in a trait");
                                            try self.errorStatements.append(err);
                                            ClearCompoundStatement(result);
                                            return Errors.SyntaxError;
                                        }
                                    }
                                    else
                                    {
                                        var err = try self.WriteError("Syntax Error: Expected either { or ; after function argument declaration, reached end of file instead");
                                        try self.errorStatements.append(err);
                                        ClearCompoundStatement(result);
                                        return Errors.SyntaxError;
                                    }
                                }
                            }
                            else
                            {

                            }

                            foundIdentifier = null;
                            typeNameStart = null;
                            typeNameEnd = 0;
                        }
                    }
                },
                .Bang, .Asterisk, .Plus, .Minus => 
                {
                    var errStr = try self.WriteError("Syntax Error: Expression must be a modifiable value");
                    try self.errorStatements.append(errStr);
                    ClearCompoundStatement(result);
                    return Errors.SyntaxError;
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
        var defaultValueExpr: ?ExpressionData = null;
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

    pub fn ParseInputParams(self: *Self, comptime endOnRBracket: bool) Errors![]ExpressionData
    {
        var params = std.ArrayList(ExpressionData).init(self.allocator);
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
    /// variable, a function call, or a typecast
    pub fn ParseExpression_Identifier(self: *Self, currentToken: lexer.Token) Errors!ExpressionData
    {
        var typeNameEnd: usize = self.GetFullIdentifier(false) orelse currentToken.end;
        var identifierName = self.SourceSlice(currentToken.start, typeNameEnd);

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

            return ExpressionData
            {
                .FunctionCall = functionCall
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

            return ExpressionData
            {
                .IndexedAccessor = functionCall
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
                var err = try self.WriteError("Syntax Error: Expected )");
                self.errorStatements.append(err)
                catch
                {
                    return Errors.OutOfMemoryError;
                };
                return Errors.SyntaxError;
            }
            if (result == .Op)
            {
                //advance beyond the )
                _ = self.tokenizer.next();
                result.Op.priority = true;
            }
            else if (result == .TypeCast)
            {
                //TODO: check if cast type exists, if not, throw error
                
                var cast = result;
                _ = self.tokenizer.next();
                nextPrimary = try self.ParseExpression_Primary();
                result = try self.ParseExpression(nextPrimary, 0);

                var castOperator: *OperatorData = self.allocator.create(OperatorData)
                catch
                {
                    return Errors.OutOfMemoryError;
                };
                castOperator.operator = .TypeCast;
                castOperator.leftExpression = cast;
                castOperator.rightExpression = result;
                castOperator.priority = false;

                result = ExpressionData
                {
                    .Op = castOperator
                };
            }

            return result;
        }
        else if (token.id == .Asterisk or token.id == .Minus or token.id == .Bang or token.id == .Ampersand or token.id == .Tilde)
        {
            var op = ASTnodes.TokenToOperator.get(@tagName(token.id)).?;
            var nextPrimary = try self.ParseExpression_Primary();
            var result = try self.ParseExpression(nextPrimary, 4);

            var modifiedVarDataPtr: *ASTnodes.ModifiedVariableData = self.allocator.create(ASTnodes.ModifiedVariableData)
            catch
            {
                return Errors.OutOfMemoryError;
            };
            modifiedVarDataPtr.expression = result;
            modifiedVarDataPtr.Op = op;

            return ExpressionData
            {
                .ModifiedVariable = modifiedVarDataPtr
            };
        }
        else if (token.id == .IntegerLiteral or token.id == .StringLiteral or token.id == .FloatLiteral or token.id == .CharLiteral or token.id == .Keyword_true or token.id == .Keyword_false)
        {
            return ExpressionData
            {
                .Literal = self.SourceTokenSlice(token)
            };
        }
        else if (token.id == .Identifier)
        {
            //std.debug.print("Parsing identifier {s}\n", .{self.SourceSlice(token.start, typeNameEnd)});
            return self.ParseExpression_Identifier(token);
        }
        else if (
            token.id == .Keyword_char or
            token.id == .Keyword_i8 or
            token.id == .Keyword_i16 or
            token.id == .Keyword_i32 or
            token.id == .Keyword_i64 or
            token.id == .Keyword_u8 or
            token.id == .Keyword_u16 or
            token.id == .Keyword_u32 or
            token.id == .Keyword_u64 or
            token.id == .Keyword_float or
            token.id == .Keyword_double or
            token.id == .Keyword_bool
            )
        {
            var pointerCount: i32 = 0;
            while(self.peekNextUntilValid().id == .Asterisk)
            {
                _ = self.nextUntilValid();
                pointerCount += 1;
            }
            return ExpressionData
            {
                .TypeCast = ASTnodes.TypeCastData
                {
                    .typeName = self.SourceTokenSlice(token),
                    .pointerCount = pointerCount
                }
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

            const peekNextID = self.peekNext().id;
            if (op.id == .Asterisk and peekNextID == .RParen or peekNextID == .Asterisk)
            {
                if (lhs == .TypeCast)
                {
                    lhs.TypeCast.pointerCount += 1;
                }
                else if (lhs == .Variable)
                {
                    lhs = ExpressionData
                    {
                        .TypeCast = ASTnodes.TypeCastData
                        {
                            .typeName = lhs.Variable,
                            .pointerCount = 1
                        }
                    };
                }
                else
                {
                    var err = try self.WriteError("Syntax Error: Attempting to convert non-type name into a pointer");
                    self.errorStatements.append(err)
                    catch
                    {
                        return Errors.OutOfMemoryError;
                    };
                    return Errors.SyntaxError;
                }
                continue;
            }
            var rhs = try self.ParseExpression_Primary();

            while (true)
            {
                var next = self.peekNext();
                var nextPrecedence = GetPrecedence(next.id);
                var nextAssociation = GetAssociation(next.id);
                if (next.id == .Eof or op.id == .Semicolon or nextPrecedence == -1 or !((nextPrecedence > precedence) or (nextAssociation == 1 and precedence == nextPrecedence)))
                {
                    break;
                }
                var nextFuncPrecedence = precedence;
                if (nextPrecedence > precedence)
                {
                    nextFuncPrecedence += 1;
                }
                rhs = try self.ParseExpression(rhs, nextFuncPrecedence);
            }
            var operator = self.allocator.create(OperatorData) catch
            {
                return Errors.OutOfMemoryError;
            };

            operator.leftExpression = lhs;
            operator.rightExpression = rhs;
            operator.operator = TokenToOperator.get(@tagName(op.id)).?;
            operator.priority = false;

            lhs = ExpressionData
            {
                .Op = operator
            };
        }

        return lhs;
    }
};