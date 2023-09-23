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
const TypeNameData = ASTnodes.TypeNameData;

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
    elseWithoutBraces,
    traitDeclaration,
    forLoopInitialization,
    forLoopStep
};

pub const ParserState = struct
{
    context: ParseContext,
    namespaces: std.ArrayList([]const u8),
    structNames: std.ArrayList([]const u8),
    funcNames: std.ArrayList([]const u8),
    braceCount: i32,

    pub fn deinit(self: *@This()) void
    {
        self.namespaces.deinit();
        self.structNames.deinit();
        self.funcNames.deinit();
    }
    pub fn clone(self: *@This()) !ParserState
    {
        var namespaces = std.ArrayList([]const u8).init(self.namespaces.allocator);
        var structNames = std.ArrayList([]const u8).init(self.structNames.allocator);
        var funcNames = std.ArrayList([]const u8).init(self.funcNames.allocator);

        for (self.namespaces.items) |namespace|
        {
            try namespaces.append(namespace);
        }
        for (self.structNames.items) |structName|
        {
            try structNames.append(structName);
        }
        for (self.funcNames.items) |funcName|
        {
            try funcNames.append(funcName);
        }

        return ParserState
        {
            .context = self.context,
            .namespaces = namespaces,
            .structNames = structNames,
            .funcNames = funcNames,
            .braceCount = self.braceCount
        };
    }
};

pub const Parser = struct {
    const Self = @This();
    allocator: std.mem.Allocator,
    tokenizer: lexer.Tokenizer,
    errorStatements: std.ArrayList(string),
    currentFile: ?[]const u8,
    postParseStatement: ?*const fn(statement: StatementData, parentStatement: *ParserState) anyerror!void,

    pub fn init(allocator: std.mem.Allocator, tokenizer: lexer.Tokenizer) !Self 
    {
        return Self
        {
            .allocator = allocator,
            .tokenizer = tokenizer,
            .errorStatements = std.ArrayList(string).init(allocator),
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

    pub fn WriteError(self: *Self, message: []const u8) anyerror!void
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
        var line: []u8 = std.fmt.allocPrint(self.allocator, "at file {s}, line {d}, column {d}\n", .{self.currentFile orelse "Null", self.tokenizer.currentLine, self.tokenizer.index - self.tokenizer.charsParsed})
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
        try self.errorStatements.append(err);

        return Errors.SyntaxError;
    }
    pub inline fn SourceSlice(self: *Self, start: usize, end: usize) []const u8
    {
        return self.tokenizer.buffer[start..end];
    }
    pub inline fn SourceTokenSlice(self: *Self, token: lexer.Token) []const u8
    {
        return self.tokenizer.buffer[token.start..token.end];
    }

    pub fn AppendToCompoundStatement(self: *Self, result: *CompoundStatementData, statement: StatementData, state: *ParserState) anyerror!void
    {
        try result.append(statement);
        if (self.postParseStatement != null)
        {
            try self.postParseStatement.?(statement, state);
        }
    }
    
    //end on semicolon: initializer in for loops
    //commaIsSemicolon: initializer in for loops, step statement(s) in for loops
    //end on rparen: step statement(s) in for loops

    pub fn Parse(self: *Self, state: *ParserState) anyerror!CompoundStatementData
    {
        var braceCount: i32 = state.braceCount;
        var skipThisLine: bool = false;
        var result = CompoundStatementData.init(self.allocator);
        var nextCanBeElseStatement: bool = false;
        var expectSemicolon: bool = false;

        var nextTags = std.ArrayList(ExpressionData).init(self.allocator);
        defer nextTags.deinit();

        while (true)
        {
            var token = self.tokenizer.next();

            if (expectSemicolon)
            {
                if (token.id != .Nl and token.id != .LineComment and token.id != .MultiLineComment)
                {
                    if (token.id != .Semicolon)
                    {
                        try self.WriteError("Expected semicolon ;");
                    }
                    else expectSemicolon = false;
                }
            }

            switch (token.id)
            {
                .Nl =>
                {
                    self.tokenizer.currentLine += 1;
                    self.tokenizer.charsParsed = self.tokenizer.index;
                    skipThisLine = false;
                },
                .MultiLineComment, .LineComment =>
                {
                    continue;
                },
                .Hash =>
                {
                    skipThisLine = true;
                },
                .Semicolon =>
                {
                    continue;
                },
                .Keyword_if =>
                {
                    if (skipThisLine)
                    {
                        continue;
                    }
                    if (state.funcNames.items.len == 0)
                    {
                        try self.WriteError("If statement must be within the body of a function");
                    }
                    var next = self.tokenizer.nextUntilValid();
                    if (next.id == .LParen)
                    {
                        var primary = try self.ParseExpression_Primary();
                        var expression = try self.ParseExpression(primary, 0);

                        if (self.tokenizer.nextUntilValid().id != .RParen)
                        {
                            try self.WriteError("Expected ) after expression in if statement");
                        }
                        if (self.tokenizer.nextUntilValid().id != .LBrace)
                        {
                            try self.WriteError("Expected { after ) in if statement");
                        }
                        var newState = try state.clone();
                        newState.braceCount += 1;
                        defer newState.deinit();

                        var body = try self.Parse(&newState);
                        try self.AppendToCompoundStatement(&result, StatementData
                        {
                            .IfStatement = ASTnodes.IfData
                            {
                                .condition = expression,
                                .statement = body
                            }
                        }, state);
                        nextCanBeElseStatement = true;
                    }
                    else
                    {
                        try self.WriteError("If statement must be followed up with a ()");
                    }
                },
                .Keyword_else =>
                {
                    if (skipThisLine)
                    {
                        continue;
                    }
                    if (!nextCanBeElseStatement)
                    {
                        try self.WriteError("Expected if statement before else statement");
                    }
                    if (state.funcNames.items.len == 0)
                    {
                        try self.WriteError("else statement must be within a function body");
                    }
                    var newState = try state.*.clone();
                    defer newState.deinit();
                    var peek = self.tokenizer.peekNextUntilValid();
                    if (peek.id == .LBrace)
                    {
                        newState.braceCount += 1;
                    }
                    else
                    {
                        newState.context = ParseContext.elseWithoutBraces;
                    }

                    const elseBody = try self.Parse(&newState);
                    try self.AppendToCompoundStatement(&result, StatementData
                    {
                        .ElseStatement = elseBody
                    }, state);
                },
                .Keyword_return =>
                {
                    if (skipThisLine)
                    {
                        continue;
                    }
                    if (state.funcNames.items.len == 0)
                    {
                        try self.WriteError("return statement must be within a function body");
                    }
                    var primary = try self.ParseExpression_Primary();
                    var expr = try self.ParseExpression(primary, 0);

                    try self.AppendToCompoundStatement(&result, StatementData
                    {
                        .returnStatement = expr
                    }, state);
                    expectSemicolon = true;
                },
                .Keyword_struct =>
                {
                    if (skipThisLine)
                    {
                        continue;
                    }
                    var structNameToken = self.tokenizer.nextUntilValid();
                    if (structNameToken.id != .Identifier)
                    {
                        try self.WriteError("Linxc expects struct name to be directly after struct keyword");
                    }
                    const structName = self.SourceTokenSlice(structNameToken);
                    if (self.tokenizer.nextUntilValid().id != .LBrace)
                    {
                        try self.WriteError("Expected struct body after struct name");
                    }
                    var newState = try state.*.clone();
                    defer newState.deinit();
                    try newState.structNames.append(structName);
                    newState.braceCount += 1;

                    const body = try self.Parse(&newState);

                    const structData = ASTnodes.StructData
                    {
                        .name = structName,
                        .tags = null,
                        .body = body
                    };
                    try self.AppendToCompoundStatement(&result, ASTnodes.StatementData
                    {
                        .structDeclaration = structData
                    }, state);
                    expectSemicolon = true;
                },
                .Bang, .Tilde =>
                {
                    if (skipThisLine)
                    {
                        continue;
                    }
                    try self.WriteError("Expression cannot start with !");
                },
                .Asterisk =>
                {
                    if (skipThisLine)
                    {
                        continue;
                    }
                    if (state.funcNames.items.len == 0)
                    {
                        try self.WriteError("Expression must be within another function's body");
                    }
                    self.tokenizer.index = token.start;
                    var primary = try self.ParseExpression_Primary();
                    var expr = try self.ParseExpression(primary, 0);

                    try self.AppendToCompoundStatement(&result, StatementData
                    {
                        .otherExpression = expr
                    }, state);
                    expectSemicolon = true;
                },
                .Identifier, .Keyword_void, .Keyword_bool, .Keyword_i8, .Keyword_i16, .Keyword_i32, .Keyword_i64, .Keyword_u8, .Keyword_u16, .Keyword_u32, .Keyword_u64, .Keyword_float, .Keyword_double, .Keyword_char =>
                {
                    if (skipThisLine)
                    {
                        continue;
                    }
                    self.tokenizer.index = token.start;
                    var typeName: ASTnodes.TypeNameData = try self.ParseTypeName(state.funcNames.items.len == 0);

                    var next = self.tokenizer.peekNextUntilValid();

                    if (next.id == .LParen) //call function, optionally do stuff with result
                    {
                        self.tokenizer.index = next.end;
                        if (typeName.pointerCount > 0)
                        {
                            try self.WriteError("Invalid function call syntax, cannot have * between function name and () arguments");
                        }
                        if (state.funcNames.items.len == 0)
                        {
                            try self.WriteError("Function call must be within another function's body");
                        }
                        if (lexer.IsPrimitiveType(token.id))
                        {
                            try self.WriteError("Cannot invoke function with a primitive type");
                        }
                        const inputParams = try self.ParseInputParams(false);
                        var functionCall = try self.allocator.create(FunctionCallData);
                        functionCall.name = typeName;
                        functionCall.inputParams = inputParams;

                        var primary = ExpressionData
                        {
                            .FunctionCall = functionCall
                        };
                        var expression = try self.ParseExpression(primary, 0);

                        try self.AppendToCompoundStatement(&result, StatementData
                        {
                            .otherExpression = expression
                        }, state);
                        expectSemicolon = true;
                    }
                    else if (next.id == .LBracket) //index into variable, optionally do stuff with result
                    {
                        self.tokenizer.index = next.end;
                        if (typeName.pointerCount > 0)
                        {
                            try self.WriteError("Invalid variable indexing syntax, cannot have * between variable name and [] indexer arguments");
                        }
                        if (state.funcNames.items.len == 0)
                        {
                            try self.WriteError("Array indexer call must be within another function's body");
                        }
                        if (lexer.IsPrimitiveType(token.id))
                        {
                            try self.WriteError("Cannot index into a primitive type");
                        }
                        const inputParams = try self.ParseInputParams(true);
                        var indexedAccessor = try self.allocator.create(FunctionCallData);
                        indexedAccessor.name = typeName;
                        indexedAccessor.inputParams = inputParams;

                        var primary = ExpressionData
                        {
                            .IndexedAccessor = indexedAccessor
                        };
                        var expression = try self.ParseExpression(primary, 0);

                        try self.AppendToCompoundStatement(&result, StatementData
                        {
                            .otherExpression = expression
                        }, state);
                        expectSemicolon = true;
                    }
                    else if (next.id == .Identifier) //function declaration or variable declaration
                    {
                        //advance beyond the identifier
                        self.tokenizer.index = next.end;
                        var name = self.SourceTokenSlice(next);

                        var tokenAfterName = self.tokenizer.nextUntilValid();

                        if (tokenAfterName.id == .LParen) //function declaration
                        {
                            var args = try self.ParseArgs();
                            if (self.tokenizer.nextUntilValid().id != .LBrace)
                            {
                                try self.WriteError("Expected { after function arguments list () unless it is in a trait");
                            }
                            var newState = try state.*.clone();
                            defer newState.deinit();
                            try newState.funcNames.append(name);
                            newState.braceCount += 1;
                            const body = try self.Parse(&newState);

                            const functionDeclaration = ASTnodes.FunctionData
                            {
                                .name = name,
                                .args = args,
                                .returnType = typeName,
                                .statement = body
                            };
                            try self.AppendToCompoundStatement(&result, StatementData
                            {
                                .functionDeclaration = functionDeclaration
                            }, state);
                        }
                        else if (tokenAfterName.id == .Equal) //variable creation and assignment
                        {
                            var primary = try self.ParseExpression_Primary();
                            var expr = try self.ParseExpression(primary, 0);

                            try self.AppendToCompoundStatement(&result, StatementData
                            {
                                .variableDeclaration = ASTnodes.VarData
                                {
                                    .defaultValue = expr,
                                    .name = name,
                                    .isConst = false,
                                    .typeName = typeName
                                }
                            }, state);
                            expectSemicolon = true;
                        }
                        else if (tokenAfterName.id == .Semicolon) //variable creation
                        {
                            try self.AppendToCompoundStatement(&result, StatementData
                            {
                                .variableDeclaration = ASTnodes.VarData
                                {
                                    .defaultValue = null,
                                    .name = name,
                                    .isConst = false,
                                    .typeName = typeName
                                }
                            }, state);
                        }
                    }
                    else
                    {
                        //is some kind of expression
                        //eg: vector2.x = 1.0f;

                        //what differentiates
                        //vector2 *create()
                        //and
                        //a * b()

                        //1: vector2 *create() MUST NOT be present within a function body, a * b() MUST be within a function body
                        //2: vector2 is a type, a is a variable (Requires variable tracking, reserved for the future)

                        var primary = ExpressionData
                        {
                            .Variable = typeName
                        };
                        var expr = try self.ParseExpression(primary, 0);

                        try self.AppendToCompoundStatement(&result, StatementData
                        {
                            .otherExpression = expr
                        }, state);
                        expectSemicolon = true;
                    }
                },
                .Keyword_namespace =>
                {
                    if (skipThisLine)
                    {
                        continue;
                    }
                    if (state.structNames.items.len > 0 or state.funcNames.items.len > 0)
                    {
                        try self.WriteError("namespace statement must not be within the body of a struct or function");
                    }
                    var foundIdentifier: bool = false;
                    var newState: ParserState = try state.*.clone();
                    defer newState.deinit();
                    var namespaceNameStart: ?usize = null;
                    var namespaceNameEnd: usize = 0;
                    while (true)
                    {
                        var next = self.tokenizer.next();
                        if (next.id == .Identifier)
                        {
                            if (foundIdentifier)
                            {
                                try self.WriteError("Expected { or :: after namespace name");
                            }
                            foundIdentifier = true;
                            try newState.namespaces.append(self.SourceTokenSlice(next));
                            if (namespaceNameStart == null)
                            {
                                namespaceNameStart = next.start;
                            }
                            namespaceNameEnd = next.end;
                        }
                        else if (next.id == .ColonColon)
                        {
                            if (foundIdentifier)
                            {
                                foundIdentifier = false;
                            }
                            else
                            {
                                try self.WriteError(":: only permissible after identifier in namespace name");
                            }
                        }
                        else if (next.id == .LBrace)
                        {
                            if (foundIdentifier)
                            {
                                newState.braceCount += 1;
                                var body = try self.Parse(&newState);
                                try self.AppendToCompoundStatement(&result, StatementData
                                {
                                    .NamespaceStatement = ASTnodes.NamespaceData
                                    {
                                        .body = body,
                                        .name = self.SourceSlice(namespaceNameStart.?, namespaceNameEnd)
                                    }
                                }, state);
                                break;
                            }
                            else
                            {
                                try self.WriteError("Expected namespace name before contents");
                            }
                        }
                        else
                        {
                            try self.WriteError("Invalid namespace name");
                            break;
                        }
                    }
                },
                .Eof =>
                {
                    break;
                },
                .RBrace => //this means that we will advance past the final } in a body compound statement
                {
                    if (skipThisLine)
                    {
                        continue;
                    }
                    braceCount -= 1;
                    if (braceCount <= state.braceCount)
                    {
                        break;
                    }
                },
                .MacroString =>
                {
                    try self.AppendToCompoundStatement(&result, StatementData
                    {
                       .includeStatement = self.SourceTokenSlice(token) 
                    }, state);
                },
                else =>
                {
                    continue;
                }
            }
            nextCanBeElseStatement = false;
        }

        return result;
    }
    ///Gets the full identifier of a variable type by compiling :: namespaces
    pub fn GetFullIdentifier(self: *Self, comptime untilValid: bool) ?usize
    {
        var typeNameEnd: ?usize = null;
        if (untilValid)
        {
            var next = self.tokenizer.peekNextUntilValid();
            if (next.id == .ColonColon)
            {
                while (true)
                {
                    var initial: usize = self.tokenizer.index;
                    next = self.tokenizer.nextUntilValid();
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
            var next = self.tokenizer.peekNext();
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
    pub fn ParseArgs(self: *Self) anyerror![]VarData
    {
        var vars = std.ArrayList(VarData).init(self.allocator);

        var nextIsConst = false;
        var variableType: ?TypeNameData = null;
        var variableName: ?[]const u8 = null;
        var defaultValueExpr: ?ExpressionData = null;
        var encounteredFirstDefaultValue: bool = false;

        while (true)
        {
            const token = self.tokenizer.nextUntilValid();
            if (token.id == .Semicolon or token.id == .Period)
            {
                try self.WriteError("Syntax Error: Unidentified/disallowed character token in arguments declaration");

                return Errors.SyntaxError;
            }
            if (token.id == .RParen)
            {
                if (variableName != null and variableType != null)
                {
                    if (encounteredFirstDefaultValue and defaultValueExpr == null)
                    {
                        try self.WriteError("Syntax Error: All arguments with default values must be declared only after arguments without");
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
                else if (variableType != null)
                {
                    try self.WriteError("Missing variable name");
                }
                break;
            }
            else if (token.id == .Eof)
            {
                try self.WriteError("Syntax Error: End of file reached before arguments declaration end");
                
                return Errors.SyntaxError;
            }
            else if (token.id == .Keyword_const)
            {
                if (nextIsConst)
                {
                    try self.WriteError("Syntax Error: Duplicate const modifier");
                    
                    return Errors.SyntaxError;
                }
                nextIsConst = true;
            }
            else if (token.id == .Identifier)
            {
                self.tokenizer.index = token.start;
                if (variableType == null)
                {
                    //detect variable type
                    variableType = try self.ParseTypeName(true);
                    //var typeNameEnd: usize = self.GetFullIdentifier(true) orelse token.end;
                    //variableType = self.SourceSlice(token.start, typeNameEnd);
                }
                else if (variableName == null)//variable name
                {
                    variableName = self.SourceTokenSlice(token);
                    self.tokenizer.index = token.end;
                }
                else
                {
                    try self.WriteError("Syntax Error: Duplicate variable type or name in arguments declaration");
                    
                    return Errors.SyntaxError;
                }
            }
            else if (token.id == .Equal)
            {
                if (variableType == null)
                {
                    try self.WriteError("Syntax Error: Expected argument type declaration before using = sign to declare default value");
                    
                    return Errors.SyntaxError;
                }
                if (variableName == null)
                {
                    try self.WriteError("Syntax Error: Expected argument name declaration before using = sign to declare default value");
                    
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
                        try self.WriteError("Syntax Error: All arguments with default values must be declared only after arguments without");
                        
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
                    try self.WriteError("Syntax Error: Comma must be after variable type and name");
                    
                    return Errors.SyntaxError;
                }
            }
            else if (token.id == .Equal)
            {
                if (variableName != null and variableType != null)
                {
                    _ = self.tokenizer.nextUntilValid();
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
                    try self.WriteError("Syntax Error: Comma must be after variable type and name");
                    
                    return Errors.SyntaxError;
                }
            }
            else if (lexer.IsPrimitiveType(token.id))
            {
                if (variableType == null)
                {
                    variableType = try self.ParseTypeName(true);
                }
                else
                {
                    try self.WriteError("Syntax Error: Duplicate variable type in arguments declaration");
                    
                    return Errors.SyntaxError;
                }
            }
            else
            {
                try self.WriteError("Issue parsing function input args");
            }
        }

        return vars.toOwnedSlice();
    }

    pub fn ParseInputParams(self: *Self, comptime endOnRBracket: bool) anyerror![]ExpressionData
    {
        var params = std.ArrayList(ExpressionData).init(self.allocator);
        var token = self.tokenizer.nextUntilValid();
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
                    try self.WriteError("Syntax Error: Expecting function arguments to be closed on a ), not ]");

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
                    try self.WriteError("Syntax Error: Expecting indexer to be closed on a ], not )");

                    return Errors.SyntaxError;
                }
            }
            else if (token.id == .Eof or token.id == .Semicolon)
            {
                try self.WriteError("Syntax Error: Reached end of line while expecting closing character");

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
            token = self.tokenizer.nextUntilValid();
        }
        var ownedSlice = params.toOwnedSlice()
        catch
        {
            return Errors.OutOfMemoryError;
        };
        return ownedSlice;
    }

    pub fn ParseTypeName(self: *Self, parsePointer: bool) anyerror!TypeNameData
    {
        var result: TypeNameData = TypeNameData
        {
            .name = "",
            .fullName = "",
            .namespace = "",
            .templateTypes = null,
            .pointerCount = 0
        };
        var typeNameStart: ?usize = null;
        var typeNameEnd: usize = 0;
        //TODO: namespace data
        var foundAsterisk: bool = false;
        var pointerCount: i32 = 0;
        var expectConnectorNext: bool = false;
        while (true)
        {
            var token: lexer.Token = self.tokenizer.peekNextUntilValid();
            switch (token.id)
            {
                .Asterisk =>
                {
                    if (!parsePointer)
                    {
                        break;
                    }
                    foundAsterisk = true;
                    pointerCount += 1;
                    typeNameEnd = token.end;
                    expectConnectorNext = false;
                },
                .Identifier, .ColonColon, .Keyword_float, .Keyword_void, .Keyword_i8, .Keyword_i16, .Keyword_i32, .Keyword_i64, .Keyword_u8, .Keyword_u16, .Keyword_u32, .Keyword_u64, .Keyword_double, .Keyword_char, .Keyword_bool =>
                {
                    if (expectConnectorNext and token.id != .ColonColon)
                    {
                        break;
                    }
                    expectConnectorNext = false;
                    if (foundAsterisk)
                    {
                        break;
                        //cant do the following as the next identifier may actually be the variable name
                        // var err = try self.WriteError("Syntax Error: No type identifier after pointer declaration * allowed");
                        // 
                        // return Errors.SyntaxError;
                    }

                    if (typeNameStart == null)
                    {
                        if (token.id == .ColonColon)
                        {
                            try self.WriteError("Syntax Error: Cannot start type name declaration off with ::");
                            
                            return Errors.SyntaxError;
                        }
                        typeNameStart = token.start;
                    }
                    typeNameEnd = token.end;

                    if (token.id != .ColonColon)
                    {
                        expectConnectorNext = true;
                        result.name = self.SourceTokenSlice(token);
                    }
                    
                },
                .AngleBracketLeft =>
                {
                    self.tokenizer.index = token.end; //advance beyond the <
                    result.templateTypes = std.ArrayList(TypeNameData).init(self.allocator);

                    while (true)
                    {
                        var templateType = try self.ParseTypeName(true);
                        if (templateType.name.len > 0)
                        {
                            //chance that the user declares typename<>, to deal with how this is done later
                            //legal in languages like c# for generic reflection, might not have a use in linxc
                            try result.templateTypes.?.append(templateType);
                        }
                        var next = self.tokenizer.nextUntilValid();
                        if (next.id == .AngleBracketRight or next.id == .Comma)
                        {
                            break;
                        }
                        else
                        {
                            try self.WriteError("Syntax Error: Expected comma or closing angle bracket > after type name in template");
                            
                            return Errors.SyntaxError;
                        }
                    }
                },
                else =>
                {
                    break;
                }
            }
            self.tokenizer.index = token.end;
        }
        result.pointerCount = pointerCount;
        result.fullName = self.SourceSlice(typeNameStart.?, typeNameEnd);
        return result;
    }

    /// parses an identifier in expression, returning either a ExpressionData with
    /// variable, a function call
    pub fn ParseExpression_Identifier(self: *Self) anyerror!ExpressionData
    {
        //under no circumstance can type name contain a pointer here, even if it may contain template stuff
        //see:
        //type_name*(): invalid
        //type_name*[]: invalid
        //type_name variable = type_name*: invalid
        var typeName = try self.ParseTypeName(false);
        //var typeName = try self.ParseTypeName();
        //var identifierName = self.GetFullIdentifier(false);
        // var typeNameEnd: usize = self.GetFullIdentifier(false) orelse self.tokenizer.index;
        // var identifierName = self.SourceSlice(currentToken.start, typeNameEnd);

        var token = self.tokenizer.peekNextUntilValid();
        if (token.id == .LParen)
        {
            _ = self.tokenizer.nextUntilValid(); //advance beyond the (
            var inputParams = try self.ParseInputParams(false);

            var functionCall = self.allocator.create(FunctionCallData)
            catch
            {
                return Errors.OutOfMemoryError;
            };
            functionCall.name = typeName;
            functionCall.inputParams = inputParams;

            return ExpressionData
            {
                .FunctionCall = functionCall
            };
        }
        else if (token.id == .LBracket) //array indexer
        {
            _ = self.tokenizer.nextUntilValid(); //advance beyond the [
            var indexParams = try self.ParseInputParams(true);

            var functionCall = self.allocator.create(FunctionCallData)
            catch
            {
                return Errors.OutOfMemoryError;
            };
            functionCall.name = typeName;
            functionCall.inputParams = indexParams;

            return ExpressionData
            {
                .IndexedAccessor = functionCall
            };
        }
        else
        {
            const result = ExpressionData
            {
                .Variable = typeName
            };
            return result;
        }
    }
    pub fn ParseExpression_Primary(self: *Self) anyerror!ExpressionData
    {
        const token = self.tokenizer.next();
        if (token.id == .LParen)
        {
            //_ = self.tokenizer.next();

            var nextPrimary = try self.ParseExpression_Primary();
            var result = try self.ParseExpression(nextPrimary, 0);

            if (self.tokenizer.buffer[self.tokenizer.index] != ')')
            {
                try self.WriteError("Syntax Error: Expected )");
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
            self.tokenizer.index = self.tokenizer.prevIndex;
            return self.ParseExpression_Identifier();
        }
        else if (lexer.IsPrimitiveType(token.id))
        {
            self.tokenizer.index = token.start;
            var nextTypeName = try self.ParseTypeName(true);
            return ExpressionData
            {
                .TypeCast = ASTnodes.TypeCastData
                {
                    .typeName = nextTypeName,
                    .pointerCount = 0
                }
            };
        }
        else
        {
            return Errors.SyntaxError;
        }
    }
    pub fn ParseExpression(self: *Self, initial: ExpressionData, minPrecedence: i32) anyerror!ExpressionData
    {
        var lhs = initial;

        while (true)
        {
            var op = self.tokenizer.peekNext();
            var precedence = GetPrecedence(op.id);
            if (op.id == .Eof or op.id == .Semicolon or precedence == -1 or precedence < minPrecedence)
            {
                break;
            }
            _ = self.tokenizer.next();

            const peekNextID = self.tokenizer.peekNext().id;
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
                    try self.WriteError("Syntax Error: Attempting to convert non-type name into a pointer");
                    return Errors.SyntaxError;
                }
                continue;
            }
            var rhs = try self.ParseExpression_Primary();

            while (true)
            {
                var next = self.tokenizer.peekNext();
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