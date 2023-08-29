const std = @import("std");
const data = @import("reflect-data.zig");
const string = @import("zig-string.zig").String;

//const ActionStack = std.ArrayList(fn(tokenizer: std.c.Tokenizer) void);

pub const ParsingTypeID = enum {
    Searching,
    DetectedDeclaration,
    FunctionParams,
    FunctionBody,
    Structure
};
pub const Errors = error {
    SyntaxError,
    RepeatIdentifierError,
    IllegalParamsError
};
pub fn HasTag(tags: std.ArrayList(data.TagData), tag: []const u8) bool
{
    var i: usize = 0;
    while (i < tags.items.len) : (i += 1)
    {
        if (std.mem.eql(u8, tags.items[i], tag))
        {
            return true;
        }
    }
    return false;
}

pub fn ParseStruct(allocator: std.mem.Allocator, structName: []const u8, fileContents: []const u8, tokenizer: *std.c.tokenizer.Tokenizer) !data.StructData
{
    var fields = std.ArrayList(data.FieldData).init(allocator);
    var methods = std.ArrayList(data.FunctionData).init(allocator);
    var ctors = std.ArrayList(data.FunctionData).init(allocator);
    var destructor: ?data.FunctionData = null;
    var tags = std.ArrayList(data.TagData).init(allocator);

    defer fields.deinit();
    defer methods.deinit();
    defer tags.deinit();

    var indent: i32 = 0;
    var skipUntilLineBreak: bool = false;
    var recordingStart: ?usize = null;
    var recordingEnd: usize = 0;
    var foundIdentifier: ?[]const u8 = null;
    var foundTag: ?[] const u8 = null;
    var nextIsStatic: bool = false;

    while (true)
    {
        const token: std.c.Token = tokenizer.next();

        switch (token.id) {
            .Eof => 
            {
                break;
            },
            .LineComment, .MultiLineComment => continue,
            .LParen => 
            {
                if (foundIdentifier != null) //reached a function
                {
                    var functionData = try ParseFunction(allocator, token.end, fileContents[recordingStart.?..recordingEnd], foundIdentifier.?, fileContents, tokenizer);
                    if (tags.items.len > 0)
                    {
                        var ownedSlice = try tags.toOwnedSlice();
                        functionData.tags = ownedSlice;
                    }
                    functionData.isStatic = nextIsStatic;
                    try methods.append(functionData);
                    nextIsStatic = false;
                    foundIdentifier = null;
                    recordingStart = null;
                    recordingEnd = 0;
                }
                else if (foundTag != null)
                {
                    recordingStart = token.end;
                    recordingEnd = token.end;
                }
            },
            .RParen =>
            {
                if (!skipUntilLineBreak)
                {
                    if (foundTag != null and recordingStart != null)
                    {
                        const tagArgs = try string.init_with_contents(allocator, fileContents[recordingStart.?..recordingEnd]);
                        const tagName = try string.init_with_contents(allocator, foundTag.?);
                        try tags.append(data.TagData{
                            .args = tagArgs,
                            .name = tagName
                        });
                        foundTag = null;
                        recordingStart = null;
                        recordingEnd = 0;
                    }
                }
            },
            .LBrace => 
            {
                if (!skipUntilLineBreak)
                {
                    indent += 1;
                }
            },
            .RBrace => 
            {
                if (!skipUntilLineBreak)
                {
                    indent -= 1;
                    if (indent <= 0)
                    {
                        break;
                    }
                }
            },
            .Hash => 
            {
                skipUntilLineBreak = true;
            },
            .Nl => {
                if (skipUntilLineBreak)
                {
                    skipUntilLineBreak = false;
                }
            },
            .Keyword_static =>
            {
                nextIsStatic = true;
            },
            .Identifier => {
                if (!skipUntilLineBreak)
                {
                    //'bool' is not a keyword in C, _Bool is (Because C is dumb and ancient)
                    //it is an identifier, henceforth we need to make it a keyword in linxc
                    if (std.mem.eql(u8, fileContents[token.start..token.end], "bool"))
                    {
                        if (recordingStart == null)
                        {
                            recordingStart = token.start;
                        }
                        recordingEnd = token.end;
                    }
                    if (recordingStart != null)
                    {
                        foundIdentifier = fileContents[token.start..token.end];

                        if (std.mem.eql(u8, foundIdentifier.?, "bool"))
                        {
                            foundIdentifier = null; //if we hit the 'bool' identifier, we did not indeed find an identifier
                        }
                        else if (std.mem.eql(u8, fileContents[recordingStart.?..recordingEnd], "struct"))
                        {
                            //nested struct
                            const structData = try ParseStruct(allocator, foundIdentifier.?, fileContents, tokenizer);
                            _ = structData;
                            //todo: See how we should implement nested structs (Probably by translation into _'d functions in C)
                            nextIsStatic = false;
                            foundIdentifier = null;
                            recordingStart = null;
                            recordingEnd = 0;
                        }
                        //else need more data to determine what our identifier is, could be variable or function
                    }
                    else if (std.mem.eql(u8, fileContents[token.start..token.end], "CONSTRUCTOR"))
                    {
                        while (true)
                        {
                            const token2 = tokenizer.next();
                            if (token2.id == .LParen)
                            {
                                var startparams = token2.end;
                                var ctor = try ParseFunction(allocator, startparams, "", fileContents[token.start..token.end], fileContents, tokenizer);
                                if (tags.items.len > 0)
                                {
                                    var ownedSlice = try tags.toOwnedSlice();
                                    ctor.tags = ownedSlice;
                                }
                                try ctors.append(ctor);
                                break;
                            }
                            else if (token2.id == .Eof)
                            {
                                return Errors.SyntaxError;
                            }
                        }
                    }
                    else if (std.mem.eql(u8, fileContents[token.start..token.end], "DESTRUCTOR"))
                    {
                        if (destructor != null)
                        {
                            return Errors.RepeatIdentifierError;
                        }
                        while (true)
                        {
                            const token2 = tokenizer.next();
                            if (token2.id == .LParen)
                            {
                                var startparams = token2.end;
                                destructor = try ParseFunction(allocator, startparams, "", fileContents[token.start..token.end], fileContents, tokenizer);
                                if (tags.items.len > 0)
                                {
                                    var ownedSlice = try tags.toOwnedSlice();
                                    destructor.?.tags = ownedSlice;
                                }
                                
                                break;
                            }
                            else if (token2.id == .Eof)
                            {
                                return Errors.SyntaxError;
                            }
                        }
                    }
                    else if (foundTag == null) //tag
                    {
                        foundTag = fileContents[token.start..token.end];
                    }
                }
            },
            else => {
                
                if (token.id == .Semicolon) //todo: support variable default values?
                {
                    if (foundIdentifier != null) //declaring a variable
                    {
                        const nameStr: string = try string.init_with_contents(allocator, foundIdentifier.?);
                        const typeStr: string = try string.init_with_contents(allocator, fileContents[recordingStart.?..recordingEnd]);
                        var fieldData = data.FieldData
                        {
                            .isStatic = nextIsStatic,
                            .name = nameStr,
                            .type = typeStr,
                            .tags = null
                        };
                        if (tags.items.len > 0)
                        {
                            var ownedSlice = try tags.toOwnedSlice();
                            fieldData.tags = ownedSlice;
                        }
                        try fields.append(fieldData);
                        nextIsStatic = false;
                        foundIdentifier = null;
                        recordingStart = null;
                        recordingEnd = 0;
                    }
                }
                else if (!skipUntilLineBreak)
                {
                    if (recordingStart == null)
                    {
                        recordingStart = token.start;
                    }
                    recordingEnd = token.end;
                }
            }
        }
    }

    const fieldsOwnedSlice = try fields.toOwnedSlice();
    const methodsOwnedSlice = try methods.toOwnedSlice();
    const ctorsOwnedSlice = try ctors.toOwnedSlice();
    const name = try string.init_with_contents(allocator, structName);
    
    return data.StructData
    {
        .name = name,
        .fields = fieldsOwnedSlice,
        .methods = methodsOwnedSlice,
        .ctors = ctorsOwnedSlice,
        .dtor = destructor,
        .tags = null
    };
    //try filedata.structs.append(structData);
}
pub fn ParseFunction(alloc: std.mem.Allocator, startparams: usize, functionReturnType: []const u8, functionName: []const u8, fileContents: []const u8, tokenizer: *std.c.tokenizer.Tokenizer) !data.FunctionData
{
    var indents: i32 = 0;
    //by this point, we would have already parsed the starting parenthesis (
    var parenthesis: i32 = 1;

    var endparams: usize = 0;
    var startfunction: usize = 0;
    var endfunction: usize = 0;
    while (true)
    {
        const token: std.c.Token = tokenizer.next();
        switch (token.id) {
            .Eof => 
            {
                break;
            },
            .LineComment, .MultiLineComment => continue,
            .LParen =>
            {
                if (indents == 0 and parenthesis > 0) //ensure this doesnt get called when encountering parenthesis in the body of the function
                {
                    parenthesis += 1;
                }
            },
            .RParen =>
            {
                if (parenthesis > 0)
                {
                    parenthesis -= 1;
                    if (parenthesis == 0)
                    {
                        endparams = token.start;
                    }
                }
                //else try paramsText.concat(fileContents[token.start..token.end]);
            },
            .LBrace =>
            {
                //dont append the first {
                if (indents == 0)
                {
                    startfunction = token.end;
                    //try bodyText.concat(fileContents[token.start..token.end]);
                }
                indents += 1;
            },
            .RBrace =>
            {
                indents -= 1;
                if (indents <= 0)
                {
                    endfunction = token.start;
                    break;
                }
                else //dont append the last }
                {
                    //try bodyText.concat(fileContents[token.start..token.end]);
                }
            },
            else => 
            {
            }
        }
    }

    const bodyText = try string.init_with_contents(alloc, fileContents[startfunction..endfunction]);
    const paramsText = try string.init_with_contents(alloc, fileContents[startparams..endparams]);
    const returnType = try string.init_with_contents(alloc, functionReturnType);
    const funcName = try string.init_with_contents(alloc, functionName);

    return data.FunctionData
    {
        .bodyText = bodyText,
        .paramsText = paramsText,
        .name = funcName,
        .returnType = returnType,
        .isStatic = false,
        .tags = null
    };
}
pub fn ParseRoot(allocator: std.mem.Allocator, filedata: *data.FileData, fileContents: []const u8, tokenizer: *std.c.tokenizer.Tokenizer) !void
{
    var tags = std.ArrayList(data.TagData).init(allocator);
    defer tags.deinit();
    var recordingStart: ?usize = null;
    var recordingEnd: usize = 0;

    var foundIdentifier: ?[]const u8 = null;
    var foundTag: ?[]const u8 = null;
    var skipUntilLineBreak: bool = false;

    while (true)
    {
        const token: std.c.Token = tokenizer.next();
        switch (token.id) {
            .Eof => 
            {
                break;
            },
            .Nl =>
            {
                if (skipUntilLineBreak)
                {
                    //std.debug.print("skip end\n", .{});
                    skipUntilLineBreak = false;
                }
            },
            .LineComment, .MultiLineComment => continue,
            .Hash, .Keyword_pragma, .Keyword_include =>
            {
                //std.debug.print("skip begin\n", .{});
                skipUntilLineBreak = true;
            },
            .MacroString => 
            {
                const tokenString: string = try string.init_with_contents(allocator, fileContents[token.start..token.end]);
                //std.debug.print("includes {s}\n", .{tokenString});
                try filedata.includeDirs.append(tokenString);
            },
            .Identifier =>
            {
                //to identify a function, it is <recordingType> <identifier> <(>
                //to identify a struct, it is <recordingType = struct> <identifier>
                //to identify a variable, it is <recordingType> <identifier> <;/=>
                //to identify a tag, it is <recordingType invalid> <identifier>
                if (!skipUntilLineBreak)
                {
                    if (recordingStart != null)
                    {
                        if (foundTag != null) //inside arguments in a tag
                        {
                            if (recordingStart == null)
                            {
                                recordingStart = token.start;
                            }
                            recordingEnd = token.end;
                        }
                        else
                        {
                            foundIdentifier = fileContents[token.start..token.end];
                            
                            if (std.mem.eql(u8, fileContents[recordingStart.?..recordingEnd], "struct"))
                            {
                                var structData = try ParseStruct(allocator, foundIdentifier.?, fileContents, tokenizer);
                                if (tags.items.len > 0)
                                {
                                    var ownedSlice = try tags.toOwnedSlice();
                                    structData.tags = ownedSlice;
                                }
                                try filedata.structs.append(structData);

                                foundIdentifier = null;
                                recordingStart = null;
                                recordingEnd = 0;
                            }
                        }
                    }
                    else if (std.mem.eql(u8, fileContents[token.start..token.end], "NAMESPACE"))
                    {
                        //set namespace
                        while (true)
                        {
                            const token2 = tokenizer.next();

                            if (token2.id == .LParen)
                            {
                                recordingStart = token2.end;
                            }
                            else if (token2.id == .RParen)
                            {
                                recordingEnd = token2.start;
                                const namespaceString: string = try string.init_with_contents(allocator, fileContents[recordingStart.?..recordingEnd]);
                                filedata.namespace = namespaceString;

                                recordingStart = null;
                                recordingEnd = 0;

                                break;
                            }
                            else if (token2.id == .Eof)
                            {
                                return Errors.SyntaxError;
                            }
                        }
                    }
                    else if (foundTag == null)
                    {
                        foundTag = fileContents[token.start..token.end];
                    }
                }
            },
            .LParen =>
            {
                if (!skipUntilLineBreak)
                {
                    if (foundIdentifier != null) //reached a function
                    {
                        var functionData = try ParseFunction(allocator, token.end, fileContents[recordingStart.?..recordingEnd], foundIdentifier.?, fileContents, tokenizer);
                        if (tags.items.len > 0)
                        {
                            var ownedSlice = try tags.toOwnedSlice();
                            functionData.tags = ownedSlice;
                        }
                        try filedata.functions.append(functionData);
                        foundIdentifier = null;
                        recordingStart = null;
                        recordingEnd = 0;
                    }
                    else if (foundTag != null)//entering tag data (if any)
                    {
                        recordingStart = token.end;
                        recordingEnd = token.end;
                    }
                }
            },
            .RParen =>
            {
                if (!skipUntilLineBreak)
                {
                    if (foundTag != null and recordingStart != null)
                    {
                        const tagArgs = try string.init_with_contents(allocator, fileContents[recordingStart.?..recordingEnd]);
                        const tagName = try string.init_with_contents(allocator, foundTag.?);
                        try tags.append(data.TagData{
                            .args = tagArgs,
                            .name = tagName
                        });
                        foundTag = null;
                        recordingStart = null;
                        recordingEnd = 0;
                    }
                }
            },
            //things that come first: tag, type
            else =>
            {
                if (!skipUntilLineBreak)
                {
                    if (recordingStart == null)
                    {
                        recordingStart = token.start;
                    }
                    recordingEnd = token.end;
                }
            }
        }
    }
}
pub fn TranslateFile(allocator: std.mem.Allocator, fileContents: []const u8) !data.FileData
{
    var tokenizer = std.c.Tokenizer {
        .buffer = fileContents
    };

    var filedata = data.FileData.init(allocator);

    try ParseRoot(allocator, &filedata, fileContents, &tokenizer);

    return filedata;
    // var i: usize = 0;

    // while (i < filedata.includeDirs.items.len) : (i += 1)
    // {
    //     std.debug.print("Includes {s}\n", .{filedata.includeDirs.items[i]});
    // }

    // i = 0;
    // while (i < filedata.functions.items.len) : (i += 1)
    // {
    //     const functionData: *data.FunctionData = &filedata.functions.items[i];

    //     functionData.Print();
    // }
    // i = 0;
    // while (i < filedata.structs.items.len) : (i += 1)
    // {
    //     const structData: *data.StructData = &filedata.structs.items[i];

    //     structData.Print();
    // }

    //std.debug.print("WRITING TO DISK\n", .{});

    //try filedata.OutputTo(allocator, outputPath);
}