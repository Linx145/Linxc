//the transpiler is the final step, and is responsible for taking the result of the
//parser AST and transpile it to c++ and h files.
const std = @import("std");
const ast = @import("ASTnodes.zig");
const io = @import("io.zig");
const refl = @import("reflector.zig");
const string = @import("zig-string.zig").String;

const SpecMap = std.StringHashMap([]const u8);

pub fn TranspileTypeName(allocator: std.mem.Allocator, writer: std.fs.File.Writer, typeName: *ast.TypeNameData, state: *TranspilerState, appendStateScope: bool) anyerror!void
{
    var isTemplateType: bool = false;
    if (state.specializationMap != null)
    {
        var converted = state.specializationMap.?.get(typeName.name.str());
        if (converted != null)
        {
            _ = try writer.write(converted.?);
            var j: usize = 0;
            while (j < typeName.pointerCount) : (j += 1)
            {
                _ = try writer.write("*");
            }
            isTemplateType = true;
        }
    }
    //if not template args type, transpile as per normal
    if (!isTemplateType)
    {
        var typeNameStr: string = try state.database.TypenameToUseableString(typeName, allocator, true);//variableDeclaration.*.typeName.ToString(allocator);
        defer typeNameStr.deinit();
        if (appendStateScope)
        {
            var writtenScope: bool = try state.writeScope(writer);
            if (writtenScope)
            {
                _ = try writer.write("_");
            }
        }
        _ = try writer.write(typeNameStr.str());
    }
}
pub fn TranspileVariableDeclaration(allocator: std.mem.Allocator, writer: std.fs.File.Writer, variableDeclaration: *ast.VarData, state: *TranspilerState, appendStateScope: bool) anyerror!void
{
    // if (variableDeclaration.*.isStatic)
    // {
    //     _ = try writer.write("static ");
    // }
    if (variableDeclaration.*.isConst)
    {
        _ = try writer.write("const ");
    }

    try TranspileTypeName(allocator, writer, &variableDeclaration.*.typeName, state, appendStateScope);
    _ = try writer.write(" ");
    _ = try writer.write(variableDeclaration.*.name.str());

    if (variableDeclaration.*.defaultValue != null)
    {
        _ = try writer.write(" = ");
        try TranspileExpression(allocator, writer, &variableDeclaration.defaultValue.?, state);
    }
    _ = try writer.write(";\n");
}
pub fn TranspileFunctionDeclarationH(allocator: std.mem.Allocator, writer: std.fs.File.Writer, functionDeclaration: *ast.FunctionData, state: *TranspilerState) anyerror!void
{
    try TranspileTypeName(allocator, writer, &functionDeclaration.*.returnType, state, false);
    _ = try writer.write(" ");
    var writtenScope: bool = try state.writeScope(writer);
    if (writtenScope)
    {
        _ = try writer.write("_");
    }
    _ = try writer.write(functionDeclaration.name.str());
    _ = try writer.write("(");
    var j: usize = 0;
    while (j < functionDeclaration.args.len) : (j += 1)
    {
        const arg: *ast.VarData = &functionDeclaration.args[j];
        if (arg.isConst)
        {
            _ = try writer.write("const ");
        }
        var appendStateScope:bool = false;
        try TranspileTypeName(allocator, writer, &arg.*.typeName, state, appendStateScope);
        _ = try writer.write(" ");
        _ = try writer.write(arg.name.str());

        // if (arg.defaultValue != null)
        // {
        //     _ = try writer.write(" = ");
        //     try TranspileExpression(allocator, writer, &arg.defaultValue.?, state);
        // }

        if (j < functionDeclaration.args.len - 1)
        {
            _ = try writer.write(", ");
        }
    }
    _ = try writer.write(");\n");
}
pub fn TranspileStructH(allocator: std.mem.Allocator, writer: std.fs.File.Writer, structDeclaration: *ast.StructData, state: *TranspilerState) anyerror!void
{
    //by this point, state should contain structName
    _ = try writer.write("struct ");
    _ = try state.writeScope(writer);
    _ = try writer.write(" {\n");
    var i: usize = 0;
    //transpile regular variables
    while (i < structDeclaration.body.items.len) : (i += 1)
    {
        var stmt: *ast.StatementData = &structDeclaration.body.items[i];
        if (stmt.* == .variableDeclaration and !stmt.*.variableDeclaration.isStatic)
        {
            try TranspileVariableDeclaration(allocator, writer, &stmt.*.variableDeclaration, state, false);
        }
    }
    _ = try writer.write("};\n");

    //transpile static variables, methods
    i = 0;
    while (i < structDeclaration.body.items.len) : (i += 1)
    {
        
        var stmt: *ast.StatementData = &structDeclaration.body.items[i];
        if (stmt.* == .variableDeclaration and stmt.*.variableDeclaration.isStatic)
        {
            try TranspileVariableDeclaration(allocator, writer, &stmt.*.variableDeclaration, state, true);
        }
        else if (stmt.* == .functionDeclaration)
        {
            
            if (!stmt.*.functionDeclaration.isStatic)
            {
                try TranspileFunctionDeclarationH(allocator, writer, &stmt.*.functionDeclaration, state);
            }
        }
    }
}
pub fn TranspileStatementH(allocator: std.mem.Allocator, writer: std.fs.File.Writer, cstmt: ast.CompoundStatementData, state: *TranspilerState) anyerror!void
{
    var i: usize = 0;
    while (i < cstmt.items.len) : (i += 1)
    {
        var stmt: *ast.StatementData = &cstmt.items[i];
        switch (stmt.*)
        {
            .includeStatement => |includeStatement|
            {
                var withoutAngledBrackets = includeStatement[1..includeStatement.len - 1];
                var withoutExtension = io.WithoutExtension(withoutAngledBrackets); //in case we have a file with a linxc extension
                _ = try writer.write("#include <");
                _ = try writer.write(withoutExtension);
                _ = try writer.write(".h>\n");
            },
            .NamespaceStatement => |namespaceStatement|
            {
                var newState = try state.clone();
                var namespaceNameStr = try string.init_with_contents(allocator, namespaceStatement.name.str());
                try newState.namespaces.append(namespaceNameStr);
                defer newState.deinit();

                _ = try writer.write("\n");
                try TranspileStatementH(allocator, writer, namespaceStatement.body, &newState);
                // _ = try writer.write("namespace ");
                // _ = try writer.write(namespaceStatement.name.str());
                // _ = try writer.write(" {\n");

                //try TranspileStatementH(allocator, writer, namespaceStatement.body, database, specializationMap);
                //_ = try writer.write("}\n");
            },
            .structDeclaration => |*structDeclaration|
            {
                if (structDeclaration.templateTypes == null)
                {
                    var newState = try state.clone();
                    var structNameStr = try string.init_with_contents(allocator, structDeclaration.name.str());
                    try newState.structNames.append(structNameStr);
                    defer newState.deinit();

                    try TranspileStructH(allocator, writer, structDeclaration, &newState);
                }
            },
            .traitDeclaration =>// |traitDeclaration|
            {
                // _ = try writer.write("struct ");
                // _ = try writer.write(traitDeclaration.name.str());
                // _ = try writer.write(" {\n");
                // try TranspileStatementH(allocator, writer, traitDeclaration.body, state);
                // _ = try writer.write("};\n");
            },
            .functionDeclaration => |*functionDeclaration| //global functions
            {
                //same state
                try TranspileFunctionDeclarationH(allocator, writer, functionDeclaration, state);
                //ignore function body
            },
            .variableDeclaration => |*variableDeclaration| //global variables
            {
                try TranspileVariableDeclaration(allocator, writer, variableDeclaration, state, true);
            },
            else =>
            {

            }
        }
    }
}
pub inline fn AppendEndOfLine(writer: std.fs.File.Writer, semicolon: bool) !void
{
    if (semicolon)
    {
        _ = try writer.write(";\n");
    }
    else _ = try writer.write(",\n");
}
pub fn TranspileExpression(allocator: std.mem.Allocator, writer: std.fs.File.Writer, expr: *ast.ExpressionData, state: *TranspilerState) anyerror!void
{
    switch (expr.*)
    {
        .Literal => |literal|
        {
            _ = try writer.write(literal.str());
        },
        .Variable => |*variable|
        {
            try TranspileTypeName(allocator, writer, variable, state, false);
        },
        .ModifiedVariable => |variable|
        {
            switch (variable.Op)
            {
                .Not => _ = try writer.write("!"),
                .Multiply => _ = try writer.write("*"),
                .BitwiseNot => _ = try writer.write("~"),
                .Minus => _ = try writer.write("-"),
                .ToPointer => _ = try writer.write("&"),
                else => {}
            }
            try TranspileExpression(allocator, writer, &variable.expression, state);
        },
        .sizeOf => |*argType|
        {
            _ = try writer.write("sizeof(");
            try TranspileTypeName(allocator, writer, argType, state, false);
            _ = try writer.write(")");
        },
        .nameOf => |*argType|
        {
            _ = try writer.write("nameof(");
            try TranspileTypeName(allocator, writer, argType, state, false);
            _ = try writer.write(")");
        },
        .typeOf =>// |argType|
        {
        },
        .FunctionCall => |FunctionCall|
        {
            //TODO: change?
            var funcNameStr: string = try state.database.TypenameToUseableString(&FunctionCall.name, allocator, false);
            _ = try writer.write(funcNameStr.str());
            funcNameStr.deinit();

            //try TranspileTypeName(allocator, writer, &FunctionCall.name, database, specializationMap);
            _ = try writer.write("(");
            var j: usize = 0;
            while (j < FunctionCall.inputParams.len) : (j += 1)
            {
                try TranspileExpression(allocator, writer, &FunctionCall.inputParams[j], state);
                if (j < FunctionCall.inputParams.len - 1)
                {
                    _ = try writer.write(", ");
                }
            }
            _ = try writer.write(")");
        },
        .IndexedAccessor => |IndexedAccessor|
        {
            try TranspileTypeName(allocator, writer, &IndexedAccessor.name, state, false);

            _ = try writer.write("[");
            var j: usize = 0;
            while (j < IndexedAccessor.inputParams.len) : (j += 1)
            {
                try TranspileExpression(allocator, writer, &IndexedAccessor.inputParams[j], state);
                if (j < IndexedAccessor.inputParams.len - 1)
                {
                    _ = try writer.write(", ");
                }
            }
            _ = try writer.write("]");
        },
        .Op => |Op|
        {
            if (Op.priority)
            {
                _ = try writer.write("(");
            }
            try TranspileExpression(allocator, writer, &Op.leftExpression, state);
            if (Op.operator != .Arrow and Op.operator != .Period and Op.operator != .TypeCast)
            {
                _ = try writer.write(" ");
            }
            switch (Op.operator)
            {
                .Plus => _ = try writer.write("+"),
                .Minus => _ = try writer.write("-"),
                .Divide => _ = try writer.write("/"),
                .Multiply => _ = try writer.write("*"),
                .Not => _ = try writer.write("!"),
                .Equals => _ = try writer.write("=="),
                .NotEquals => _ = try writer.write("!="),
                .LessThan => _ = try writer.write("<"),
                .LessThanEquals => _ = try writer.write("<="),
                .MoreThan => _ = try writer.write(">"),
                .MoreThanEquals => _ = try writer.write(">="),
                .And => _ = try writer.write("&&"),
                .Or => _ = try writer.write("||"),
                .Modulo => _ = try writer.write("%"),
                .BitwiseAnd => _ = try writer.write("&"),
                .BitwiseNot => _ = try writer.write("~"),
                .BitwiseOr => _ = try writer.write("|"),
                .LeftShift => _ = try writer.write("<<"),
                .RightShift => _ = try writer.write(">>"),
                .BitwiseXOr => _ = try writer.write("^"),
                .PlusEqual => _ = try writer.write("+="),
                .MinusEqual => _ = try writer.write("-="),
                .AsteriskEqual => _ = try writer.write("*="),
                .SlashEqual => _ = try writer.write("/="),
                .PercentEqual => _ = try writer.write("%="),
                .Arrow => _ = try writer.write("->"),
                .Equal => _ = try writer.write("="),
                .Period => _ = try writer.write("."),
                .ToPointer => _ = try writer.write("&"),
                else =>
                {
                    
                }
            }
            if (Op.operator != .Arrow and Op.operator != .Period and Op.operator != .TypeCast)
            {
                _ = try writer.write(" ");
            }
            else if (Op.leftExpression == .Variable)
            {
                
            }
            try TranspileExpression(allocator, writer, &Op.rightExpression, state);
            if (Op.priority)
            {
                _ = try writer.write(")");
            }
        },
        .TypeCast => |*TypeCast|
        {
            _ = try writer.write("(");
            try TranspileTypeName(allocator, writer, &TypeCast.*.typeName, state, false);
            _ = try writer.write(")");
        }
        
    }
}
//parent is not null if we are in reflectable compound statement
// pub fn TranspileStatementCpp(allocator: std.mem.Allocator, writer: std.fs.File.Writer, cstmt: ast.CompoundStatementData, eolIsSemicolon: bool, parent: ?[]const u8, database: *refl.ReflectionDatabase, specializationMap: ?SpecMap) anyerror!void
// {
//     var i: usize = 0;
//     while (i < cstmt.items.len) : (i += 1)
//     {
//         var stmt: *ast.StatementData = &cstmt.items[i];
//         switch (stmt.*)
//         {
//             .structDeclaration => |*structDeclaration|
//             {
//                 if (structDeclaration.templateTypes == null)
//                     try TranspileStatementCpp(allocator, writer, structDeclaration.body, true, structDeclaration.name.str(), database, specializationMap);
//             },
//             .NamespaceStatement => |namespaceStatement|
//             {
//                 _ = try writer.write("namespace ");
//                 _ = try writer.write(namespaceStatement.name.str());
//                 _ = try writer.write(" {\n");
//                 try TranspileStatementCpp(allocator, writer, namespaceStatement.body, false, null, database, specializationMap);
//                 _ = try writer.write("}\n");
//             },
//             .traitDeclaration =>// |*traitDeclaration|
//             {
//             },
//             .variableDeclaration => |*variableDeclaration|
//             {
//                 //dont declare variable again, that's the header file's job
//                 if (parent == null)
//                 {
//                     if (variableDeclaration.isConst)
//                     {
//                         _ = try writer.write("const ");
//                     }
//                     if (variableDeclaration.isStatic)
//                     {
//                          _ = try writer.write("static ");
//                     }
//                     try TranspileTypeName(allocator, writer, &variableDeclaration.*.typeName, database, specializationMap);
//                     _ = try writer.write(" ");
//                     _ = try writer.write(variableDeclaration.name.str());
//                     if (variableDeclaration.defaultValue != null)
//                     {
//                         _ = try writer.write(" = ");
//                         try TranspileExpression(allocator, writer, &variableDeclaration.defaultValue.?, database, specializationMap);
//                     }
//                     try AppendEndOfLine(writer, eolIsSemicolon);
//                 }
//             },
//             .functionDeclaration => |*functionDeclaration|
//             {
//                 try TranspileTypeName(allocator, writer, &functionDeclaration.*.returnType, database, specializationMap);
//                 _ = try writer.write(" ");
//                 if (parent != null and parent.?.len > 0)
//                 {
//                     _ = try writer.write(parent.?);
//                     _ = try writer.write("::");
//                 }
//                 _ = try writer.write(functionDeclaration.name.str());
//                 _ = try writer.write("(");
//                 var j: usize = 0;
//                 while (j < functionDeclaration.args.len) : (j += 1)
//                 {
//                     const arg: *ast.VarData = &functionDeclaration.args[j];
//                     if (arg.isConst)
//                     {
//                         _ = try writer.write("const ");
//                     }

//                     if (specializationMap != null)
//                     {

//                     }
//                     try TranspileTypeName(allocator, writer, &arg.*.typeName, database, specializationMap);
//                     _ = try writer.write(" ");
//                     _ = try writer.write(arg.name.str());

//                     //dont need to handle default value here as it's already handled for us in the header file

//                     if (j < functionDeclaration.args.len - 1)
//                     {
//                         _ = try writer.write(", ");
//                     }
//                 }
//                 _ = try writer.write(") {\n");
//                 try TranspileStatementCpp(allocator, writer, functionDeclaration.statement, true, null, database, specializationMap);
//                 _ = try writer.write("}\n");
//             },
//             .returnStatement => |*returnStatement|
//             {
//                 _ = try writer.write("return ");
//                 try TranspileExpression(allocator, writer, returnStatement, database, specializationMap);
//                 try AppendEndOfLine(writer, eolIsSemicolon);
//             },
//             .otherExpression => |*otherExpression|
//             {
//                 try TranspileExpression(allocator, writer, otherExpression, database, specializationMap);
//                 try AppendEndOfLine(writer, eolIsSemicolon);
//             },
//             .IfStatement => |*IfStatement|
//             {
//                 _ = try writer.write("if (");
//                 try TranspileExpression(allocator, writer, &IfStatement.condition, database, specializationMap);
//                 _ = try writer.write(") {\n");
//                 try TranspileStatementCpp(allocator, writer, IfStatement.statement, true, null, database, specializationMap);
//                 _ = try writer.write("}\n");
//             },
//             .ElseStatement => |*ElseStatement|
//             {
//                 _ = try writer.write("else ");
//                 if (ElseStatement.items.len > 1)
//                 {
//                     _ = try writer.write(" {\n");
//                     _ = try writer.write("}\n");
//                 }
//                 else
//                 {
//                     try TranspileStatementCpp(allocator, writer, ElseStatement.*, true, null, database, specializationMap);
//                 }
//             },
//             .WhileStatement => |*WhileStatement|
//             {
//                 _ = try writer.write("while (");
//                 try TranspileExpression(allocator, writer, &WhileStatement.condition, database, specializationMap);
//                 _ = try writer.write(") {\n");
//                 try TranspileStatementCpp(allocator, writer, WhileStatement.statement, true, null, database, specializationMap);
//                 _ = try writer.write("}\n");
//             },
//             .ForStatement => |*ForStatement|
//             {
//                 _ = try writer.write("for (");
//                 try TranspileStatementCpp(allocator, writer, ForStatement.initializer, false, null, database, specializationMap);
//                 _ = try writer.write("; ");
//                 try TranspileExpression(allocator, writer, &ForStatement.condition, database, specializationMap);
//                 _ = try writer.write("; ");
//                 try TranspileStatementCpp(allocator, writer, ForStatement.shouldStep, false, null, database, specializationMap);
//                 _ = try writer.write(") {\n");
//                 try TranspileStatementCpp(allocator, writer, ForStatement.statement, true, null, database, specializationMap);
//                 _ = try writer.write("}\n");
//             },
//             else =>
//             {

//             }
//         }
//     }
// }

// pub fn TranspileTemplatedStruct(hwriter: std.fs.File.Writer, cwriter: std.fs.File.Writer, database: *refl.ReflectionDatabase, templatedStruct: *refl.TemplatedStruct) anyerror!void
// {
//     const allocator = templatedStruct.allocator;

//     var genericTypeName: string = string.init(allocator);
//     defer genericTypeName.deinit();
//     if (templatedStruct.namespace.str().len > 0)
//     {
//         try genericTypeName.concat(templatedStruct.namespace.str());
//         try genericTypeName.concat("::");
//     }
//     try genericTypeName.concat(templatedStruct.structData.name.str());

//     var genericTypeIndex = database.nameToType.get(genericTypeName.str());
//     if (genericTypeIndex != null)
//     {
//         var genericType: *refl.LinxcType = &database.types.items[genericTypeIndex.?];

//         var genericMap = SpecMap.init(allocator);

//         var j: usize = 0;
//         var alreadyIncludedHeaders = std.StringHashMap(void).init(allocator);
//         if (genericType.headerFile != null)
//         {
//             //std.debug.print("generic type {s} implemented in header {s}\n", .{genericTypeName.str(), genericType.headerFile.?.str()});
//             try alreadyIncludedHeaders.put(genericType.headerFile.?.str(), {});
//         }
//         while (j < genericType.templateSpecializations.items.len) : (j += 1)
//         {
//             if (genericType.templateSpecializations.items[j].headerFile != null and 
//             !alreadyIncludedHeaders.contains(genericType.templateSpecializations.items[j].headerFile.?.str()))
//             {
//                 const headerFileStr = genericType.templateSpecializations.items[j].headerFile.?.str();
//                 _ = try hwriter.write("#include <");
//                 _ = try hwriter.write(headerFileStr);
//                 _ = try hwriter.write(">\n");
//                 try alreadyIncludedHeaders.put(headerFileStr, {});
//             }
//         }

//         alreadyIncludedHeaders.deinit();

//         j = 0;
//         const genericTypeArgsCount: usize = templatedStruct.structData.templateTypes.?.len;
//         //collect (generic type arguments count) specializations from genericType.templateSpecializations
//         while (j < genericType.templateSpecializations.items.len)
//         {
//             if (templatedStruct.namespace.str().len > 0)
//             {
//                 _ = try hwriter.write(templatedStruct.namespace.str());
//                 _ = try hwriter.write(" {\n");
//             }
//             _ = try hwriter.write("struct ");
//             _ = try hwriter.write(templatedStruct.structData.name.str());

//             //append to specialization map, at the same time transpile full name
//             var i: usize = 0;
//             while (i < genericTypeArgsCount) : (i += 1)
//             {
//                 const specializationStr = genericType.templateSpecializations.items[j + i].name.str();
//                 try genericMap.put(templatedStruct.structData.templateTypes.?[i].str(), specializationStr);
//                 _ = try hwriter.write("_");
//                 try hwriter.print("{d}", .{genericType.templateSpecializations.items[j + i].ID});
//             }

//             _ = try hwriter.write(" {\n");
//             try TranspileStatementH(allocator, hwriter, templatedStruct.structData.body, database, genericMap);
//             _ = try hwriter.write("};\n");

//             if (templatedStruct.namespace.str().len > 0)
//             {
//                 _ = try hwriter.write("}\n");
//             }

//             for (templatedStruct.structData.body.items) |*structBodyStmt|
//             {
//                 if (structBodyStmt.* == .variableDeclaration)
//                 {
//                     const variableDeclaration: *ast.VarData = &structBodyStmt.*.variableDeclaration;
//                     if (variableDeclaration.*.isStatic and !variableDeclaration.*.isConst)
//                     {
//                         try TranspileTypeName(allocator, cwriter, &variableDeclaration.*.typeName, database, genericMap);
//                         // var typeNameStr: string = try variableDeclaration.*.typeName.ToString(allocator);
//                         // defer typeNameStr.deinit();
//                         // _ = try cwriter.write(typeNameStr.str());
//                         _ = try cwriter.write(" ");
//                         _ = try cwriter.write(templatedStruct.structData.name.str());
//                         i = 0;
//                         while (i < genericTypeArgsCount) : (i += 1)
//                         {
//                             _ = try cwriter.write("_");
//                             try cwriter.print("{d}", .{genericType.templateSpecializations.items[j + i].ID});
//                         }
//                         _ = try cwriter.write("::");
//                         _ = try cwriter.write(variableDeclaration.*.name.str());
//                         if (variableDeclaration.*.defaultValue != null)
//                         {
//                             _ = try cwriter.write(" = ");
//                             try TranspileExpression(allocator, cwriter, &variableDeclaration.defaultValue.?, database, genericMap);
//                         }
//                         _ = try cwriter.write(";\n");
//                     }
//                 }
//             }

//             //cpp file
//             var structName = string.init(allocator);
//             defer structName.deinit();
//             try structName.concat(templatedStruct.structData.name.str());
//             i = 0;
//             while (i < genericTypeArgsCount) : (i += 1)
//             {
//                 try structName.concat("_");
//                 var typeIDStr = try std.fmt.allocPrint(allocator, "{d}", .{genericType.templateSpecializations.items[j + i].ID});
//                 try structName.concat(typeIDStr);
//                 allocator.free(typeIDStr);
//             }

//             try TranspileStatementCpp(allocator, cwriter, templatedStruct.structData.body, false, structName.str(), database, genericMap);

//             genericMap.clearRetainingCapacity();
//             j += genericTypeArgsCount;
//         }
//     }
// }

pub const TranspilerState = struct
{
    allocator: std.mem.Allocator,
    database: *refl.ReflectionDatabase, 
    specializationMap: ?SpecMap,
    namespaces: std.ArrayList(string),
    structNames: std.ArrayList(string),
    pub inline fn clone(self: *@This()) anyerror!TranspilerState
    {
        var namespaces = std.ArrayList(string).init(self.allocator);
        var structNames = std.ArrayList(string).init(self.allocator);
        for (self.namespaces.items) |namespace|
        {
            var namespaceStr = try string.init_with_contents(self.allocator, namespace.str());
            try namespaces.append(namespaceStr);
        }
        for (self.structNames.items) |structName|
        {
            var structNameStr = try string.init_with_contents(self.allocator, structName.str());
            try structNames.append(structNameStr);
        }
        return TranspilerState
        {
            .allocator = self.allocator,
            .database = self.database,
            .specializationMap = self.specializationMap,
            .namespaces = namespaces,
            .structNames = structNames
        };
    }
    pub inline fn deinit(self: *@This()) void
    {
        for (self.namespaces.items) |*namespace|
        {
            namespace.deinit();
        }
        for (self.structNames.items) |*structName|
        {
            structName.deinit();
        }
        self.namespaces.deinit();
        self.structNames.deinit();
    }
    pub fn writeScope(self: *@This(), writer: std.fs.File.Writer) anyerror!bool
    {
        if (self.namespaces.items.len > 0)
        {
            var i: usize = 0;
            while (i < self.namespaces.items.len) : (i += 1)
            {
                _ = try writer.write(self.namespaces.items[i].str());
                if (i < self.namespaces.items.len - 1 or self.structNames.items.len > 0)
                {
                    _ = try writer.write("_");
                }
            }
        }
        if (self.structNames.items.len > 0)
        {
            var i: usize = 0;
            while (i < self.structNames.items.len) : (i += 1)
            {
                _ = try writer.write(self.structNames.items[i].str());
                if (i < self.structNames.items.len - 1)
                {
                    _ = try writer.write("_");
                }
            }
        }
        if (self.structNames.items.len > 0 or self.namespaces.items.len > 0)
        {
            return true;
        }
        else return false;
    }
};