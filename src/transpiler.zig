//the transpiler is the final step, and is responsible for taking the result of the
//parser AST and transpile it to c++ and h files.
const std = @import("std");
const ast = @import("ASTnodes.zig");
const io = @import("io.zig");
const refl = @import("reflector.zig");
const string = @import("zig-string.zig").String;

pub fn TranspileStatementH(allocator: std.mem.Allocator, writer: std.fs.File.Writer, cstmt: ast.CompoundStatementData, database: *refl.ReflectionDatabase, specializationMap: ?std.StringHashMap([]const u8)) anyerror!void
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
                _ = try writer.write("namespace ");
                _ = try writer.write(namespaceStatement.name.str());
                _ = try writer.write(" {\n");

                // var nextNamespace: string = string.init(allocator);
                // if (namespaces.len > 0)
                // {
                //     try nextNamespace.concat(namespaces);
                //     try nextNamespace.concat("::");
                // }
                // try nextNamespace.concat(namespaceStatement.name.str());

                try TranspileStatementH(allocator, writer, namespaceStatement.body, database, specializationMap);
                _ = try writer.write("}\n");
            },
            .structDeclaration => |structDeclaration|
            {
                if (structDeclaration.templateTypes == null)
                {
                    _ = try writer.write("struct ");
                    _ = try writer.write(structDeclaration.name.str());
                    _ = try writer.write(" {\n");
                    try TranspileStatementH(allocator, writer, structDeclaration.body, database, specializationMap);
                    _ = try writer.write("};\n");
                }
            },
            .traitDeclaration => |traitDeclaration|
            {
                _ = try writer.write("struct ");
                _ = try writer.write(traitDeclaration.name.str());
                _ = try writer.write(" {\n");
                try TranspileStatementH(allocator, writer, traitDeclaration.body, database, specializationMap);
                _ = try writer.write("};\n");
            },
            .functionDeclaration => |*functionDeclaration|
            {
                var returnTypeStr = try functionDeclaration.*.returnType.ToUseableString(allocator);
                defer returnTypeStr.deinit();
                _ = try writer.write(returnTypeStr.str());
                _ = try writer.write(" ");
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
                    var typeNameStr: string = try arg.typeName.ToString(allocator);
                    defer typeNameStr.deinit();
                    _ = try writer.write(typeNameStr.str());
                    _ = try writer.write(" ");
                    _ = try writer.write(arg.name.str());

                    if (arg.defaultValue != null)
                    {
                        _ = try writer.write(" = ");
                        try TranspileExpression(writer, &arg.defaultValue.?, allocator);
                    }

                    if (j < functionDeclaration.args.len - 1)
                    {
                        _ = try writer.write(", ");
                    }
                }
                _ = try writer.write(");\n");

                //ignore function body
            },
            .variableDeclaration => |*variableDeclaration|
            {
                if (variableDeclaration.*.isStatic)
                {
                    _ = try writer.write("static ");
                }
                if (variableDeclaration.*.isConst)
                {
                    _ = try writer.write("const ");
                }
                var typeNameStr: string = try variableDeclaration.*.typeName.ToString(allocator);
                defer typeNameStr.deinit();
                _ = try writer.write(typeNameStr.str());
                _ = try writer.write(" ");
                _ = try writer.write(variableDeclaration.*.name.str());
                if (variableDeclaration.*.defaultValue != null)
                {
                    _ = try writer.write(" = ");
                    try TranspileExpression(writer, &variableDeclaration.defaultValue.?, allocator);
                }
                _ = try writer.write(";\n");
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
pub fn TranspileExpression(writer: std.fs.File.Writer, expr: *ast.ExpressionData, allocator: std.mem.Allocator) anyerror!void
{
    switch (expr.*)
    {
        .Literal => |literal|
        {
            _ = try writer.write(literal.str());
        },
        .Variable => |*variable|
        {
            var variableStr = try variable.ToUseableString(allocator);
            _ = try writer.write(variableStr.str());
            variableStr.deinit();
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
            try TranspileExpression(writer, &variable.expression, allocator);
        },
        .FunctionCall => |FunctionCall|
        {
            //TODO: change?
            var functionCallName = try FunctionCall.name.ToString(allocator);
            defer functionCallName.deinit();
            _ = try writer.write(functionCallName.str());
            _ = try writer.write("(");
            var j: usize = 0;
            while (j < FunctionCall.inputParams.len) : (j += 1)
            {
                try TranspileExpression(writer, &FunctionCall.inputParams[j], allocator);
                if (j < FunctionCall.inputParams.len - 1)
                {
                    _ = try writer.write(", ");
                }
            }
            _ = try writer.write(")");
        },
        .IndexedAccessor => |IndexedAccessor|
        {
            _ = try writer.write("[");
            var j: usize = 0;
            while (j < IndexedAccessor.inputParams.len) : (j += 1)
            {
                try TranspileExpression(writer, &IndexedAccessor.inputParams[j], allocator);
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
            try TranspileExpression(writer, &Op.leftExpression, allocator);
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
            try TranspileExpression(writer, &Op.rightExpression, allocator);
            if (Op.priority)
            {
                _ = try writer.write(")");
            }
        },
        .TypeCast => |*TypeCast|
        {
            var typeNameStr: string = try TypeCast.*.typeName.ToUseableString(allocator);
            defer typeNameStr.deinit();
            _ = try writer.write(")");
        }
    }
}
//parent is not null if we are in reflectable compound statement
pub fn TranspileStatementCpp(allocator: std.mem.Allocator, writer: std.fs.File.Writer, cstmt: ast.CompoundStatementData, eolIsSemicolon: bool, parent: ?[]const u8) anyerror!void
{
    var i: usize = 0;
    while (i < cstmt.items.len) : (i += 1)
    {
        var stmt: *ast.StatementData = &cstmt.items[i];
        switch (stmt.*)
        {
            .structDeclaration => |*structDeclaration|
            {
                if (structDeclaration.templateTypes == null)
                    try TranspileStatementCpp(allocator, writer, structDeclaration.body, true, structDeclaration.name.str());
            },
            .NamespaceStatement => |namespaceStatement|
            {
                _ = try writer.write("namespace ");
                _ = try writer.write(namespaceStatement.name.str());
                _ = try writer.write(" {\n");
                try TranspileStatementCpp(allocator, writer, namespaceStatement.body, false, null);
                _ = try writer.write("}\n");
            },
            .traitDeclaration =>// |*traitDeclaration|
            {
                // for (traitDeclaration.*.body.items) |*traitStmt|
                // {
                //     if (traitStmt.* == .functionDeclaration)
                //     {
                //         var returnTypeStr = try traitStmt.*.functionDeclaration.returnType.ToUseableString(allocator);
                //         defer returnTypeStr.deinit();
                //         _ = try writer.write(returnTypeStr.str());
                //         _ = try writer.write(" ");
                //         if (parent != null and parent.?.len > 0)
                //         {
                //             _ = try writer.write(parent.?);
                //             _ = try writer.write("::");
                //         }
                //         _ = try writer.write(traitStmt.functionDeclaration.name.str());
                //         _ = try writer.write("(");
                //         var j: usize = 0;
                //         while (j < traitStmt.functionDeclaration.args.len) : (j += 1)
                //         {
                //             const arg: *ast.VarData = &traitStmt.functionDeclaration.args[j];
                //             if (arg.isConst)
                //             {
                //                 _ = try writer.write("const ");
                //             }
                //             var typeNameStr: string = try arg.typeName.ToString(allocator);
                //             defer typeNameStr.deinit();
                //             _ = try writer.write(typeNameStr.str());
                //             _ = try writer.write(" ");
                //             _ = try writer.write(arg.name.str());

                //             //dont need to handle default value here as it's already handled for us in the header file

                //             if (j < traitStmt.functionDeclaration.args.len - 1)
                //             {
                //                 _ = try writer.write(", ");
                //             }
                //         }
                //         _ = try writer.write(") {\n");

                //         _ = try writer.write("   switch (_SELF.type->ID) {\n");
                        
                //         j = 0;
                //         var traitType: *refl.LinxcType = try refl.globalDatabase.?.GetTypeSafe(traitDeclaration.name.str());
                //         while (j < traitType.implementedBy.items.len) : (j += 1)
                //         {
                //             try writer.print("      case {d}:\n", .{traitType.implementedBy.items[j].ID});
                //             try writer.print("         (({s}*)_SELF.ptr).{s}(", .{traitType.implementedBy.items[j].name.str(), traitStmt.functionDeclaration.name});
                //             var c: usize = 0;

                //             while (c < traitStmt.functionDeclaration.args.len) : (c += 1)
                //             {
                //                 const arg: *ast.VarData = &traitStmt.functionDeclaration.args[j];
                //                 _ = try writer.write(arg.name.str());
                //                 if (c < traitStmt.functionDeclaration.args.len - 1)
                //                 {
                //                     _ = try writer.write(", ");
                //                 }
                //             }
                            
                //             _ = try writer.write(");\n");
                //             _ = try writer.write("         break;\n");
                //         }
                //         _ = try writer.write("   }\n");

                //         _ = try writer.write("}\n");
                //     }
                // }
            },
            .variableDeclaration => |*variableDeclaration|
            {
                if (parent == null)
                {
                    if (variableDeclaration.isConst)
                    {
                        _ = try writer.write("const ");
                    }
                    var typeNameStr: string = try variableDeclaration.typeName.ToString(allocator);
                    defer typeNameStr.deinit();
                    _ = try writer.write(typeNameStr.str());
                    _ = try writer.write(" ");
                    _ = try writer.write(variableDeclaration.name.str());
                    if (variableDeclaration.defaultValue != null)
                    {
                        _ = try writer.write(" = ");
                        try TranspileExpression(writer, &variableDeclaration.defaultValue.?, allocator);
                    }
                    try AppendEndOfLine(writer, eolIsSemicolon);
                }
            },
            .functionDeclaration => |*functionDeclaration|
            {
                var returnTypeStr = try functionDeclaration.returnType.ToUseableString(allocator);
                defer returnTypeStr.deinit();
                _ = try writer.write(returnTypeStr.str());
                _ = try writer.write(" ");
                if (parent != null and parent.?.len > 0)
                {
                    _ = try writer.write(parent.?);
                    _ = try writer.write("::");
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
                    var typeNameStr: string = try arg.typeName.ToString(allocator);
                    defer typeNameStr.deinit();
                    _ = try writer.write(typeNameStr.str());
                    _ = try writer.write(" ");
                    _ = try writer.write(arg.name.str());

                    //dont need to handle default value here as it's already handled for us in the header file

                    if (j < functionDeclaration.args.len - 1)
                    {
                        _ = try writer.write(", ");
                    }
                }
                _ = try writer.write(") {\n");
                try TranspileStatementCpp(allocator, writer, functionDeclaration.statement, true, null);
                _ = try writer.write("}\n");
            },
            .returnStatement => |*returnStatement|
            {
                _ = try writer.write("return ");
                try TranspileExpression(writer, returnStatement, allocator);
                try AppendEndOfLine(writer, eolIsSemicolon);
            },
            .otherExpression => |*otherExpression|
            {
                try TranspileExpression(writer, otherExpression, allocator);
                try AppendEndOfLine(writer, eolIsSemicolon);
            },
            .IfStatement => |*IfStatement|
            {
                _ = try writer.write("if (");
                try TranspileExpression(writer, &IfStatement.condition, allocator);
                _ = try writer.write(") {\n");
                try TranspileStatementCpp(allocator, writer, IfStatement.statement, true, null);
                _ = try writer.write("}\n");
            },
            .ElseStatement => |*ElseStatement|
            {
                _ = try writer.write("else ");
                if (ElseStatement.items.len > 1)
                {
                    _ = try writer.write(" {\n");
                    _ = try writer.write("}\n");
                }
                else
                {
                    try TranspileStatementCpp(allocator, writer, ElseStatement.*, true, null);
                }
            },
            .WhileStatement => |*WhileStatement|
            {
                _ = try writer.write("while (");
                try TranspileExpression(writer, &WhileStatement.condition, allocator);
                _ = try writer.write(") {\n");
                try TranspileStatementCpp(allocator, writer, WhileStatement.statement, true, null);
                _ = try writer.write("}\n");
            },
            .ForStatement => |*ForStatement|
            {
                _ = try writer.write("for (");
                try TranspileStatementCpp(allocator, writer, ForStatement.initializer, false, null);
                _ = try writer.write("; ");
                try TranspileExpression(writer, &ForStatement.condition, allocator);
                _ = try writer.write("; ");
                try TranspileStatementCpp(allocator, writer, ForStatement.shouldStep, false, null);
                _ = try writer.write(") {\n");
                try TranspileStatementCpp(allocator, writer, ForStatement.statement, true, null);
                _ = try writer.write("}\n");
            },
            else =>
            {

            }
        }
    }
}

pub fn TranspileTemplatedStruct(hwriter: std.fs.File.Writer, cwriter: std.fs.File.Writer, database: *refl.ReflectionDatabase, templatedStruct: *refl.TemplatedStruct) anyerror!void
{
    _ = cwriter;
    const allocator = templatedStruct.allocator;

    var genericTypeName: string = string.init(allocator);
    defer genericTypeName.deinit();
    if (templatedStruct.namespace.str().len > 0)
    {
        try genericTypeName.concat(templatedStruct.namespace.str());
        try genericTypeName.concat("::");
    }
    try genericTypeName.concat(templatedStruct.structData.name.str());

    var genericType = try database.GetTypeSafe(genericTypeName.str());

    var genericMap = std.StringHashMap([]const u8).init(allocator);

    var j: usize = 0;
    const genericTypeArgsCount: usize = templatedStruct.structData.templateTypes.?.len;
    //collect (generic type arguments count) specializations from genericType.templateSpecializations
    while (j < genericType.templateSpecializations.items.len)
    {
        if (templatedStruct.namespace.str().len > 0)
        {
            _ = try hwriter.write(templatedStruct.namespace.str());
            _ = try hwriter.write(" {\n");
        }
        _ = try hwriter.write("struct ");
        _ = try hwriter.write(templatedStruct.structData.name.str());

        //append to specialization map, at the same time transpile fill name
        var i: usize = 0;
        while (i < genericTypeArgsCount) : (i += 1)
        {
            const specializationStr = genericType.templateSpecializations.items[j + i].str();
            try genericMap.put(templatedStruct.structData.templateTypes.?[i].str(), specializationStr);
            _ = try hwriter.write("_");
            _ = try hwriter.write(specializationStr);
        }

        _ = try hwriter.write(" {\n");
        try TranspileStatementH(allocator, hwriter, templatedStruct.structData.body, database, genericMap);
        _ = try hwriter.write("};\n");
        if (templatedStruct.namespace.str().len > 0)
        {
            _ = try hwriter.write("}\n");
        }

        genericMap.clearRetainingCapacity();
        j += genericTypeArgsCount;
    }
}