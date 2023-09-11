//the transpiler is the final step, and is responsible for taking the result of the
//parser AST and transpile it to c++ and h files.
const std = @import("std");
const ast = @import("ASTnodes.zig");

pub fn TranspileStatementH(writer: std.fs.File.Writer, cstmt: ast.CompoundStatementData) anyerror!void
{
    var i: usize = 0;
    while (i < cstmt.items.len) : (i += 1)
    {
        var stmt: *ast.StatementData = &cstmt.items[i];
        switch (stmt.*)
        {
            .includeStatement => |includeStatement|
            {
                try writer.print("#include {s}\n", .{includeStatement});
            },
            .structDeclaration => |structDeclaration|
            {
                _ = try writer.write("struct ");
                _ = try writer.write(structDeclaration.name);
                _ = try writer.write(" {\n");
                try TranspileStatementH(writer, structDeclaration.body);
                _ = try writer.write("};\n");
            },
            .functionDeclaration => |functionDeclaration|
            {
                _ = try writer.write(functionDeclaration.returnType);
                _ = try writer.write(" ");
                _ = try writer.write(functionDeclaration.name);
                _ = try writer.write("(");
                var j: usize = 0;
                while (j < functionDeclaration.args.len) : (j += 1)
                {
                    const arg: *ast.VarData = &functionDeclaration.args[j];
                    if (arg.isConst)
                    {
                        _ = try writer.write("const ");
                    }
                    _ = try writer.write(arg.typeName);
                    _ = try writer.write(" ");
                    _ = try writer.write(arg.name);

                    if (arg.defaultValue != null)
                    {
                        _ = try writer.write(" = ");
                        try TranspileExpression(writer, &arg.defaultValue.?);
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
                if (variableDeclaration.*.isConst)
                {
                    _ = try writer.write("const ");
                }
                _ = try writer.write(variableDeclaration.*.typeName);
                _ = try writer.write(" ");
                _ = try writer.write(variableDeclaration.*.name);
                if (variableDeclaration.*.defaultValue != null)
                {
                    _ = try writer.write(" = ");
                    try TranspileExpression(writer, &variableDeclaration.defaultValue.?);
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
pub fn TranspileExpression(writer: std.fs.File.Writer, expr: *ast.ExpressionChain) anyerror!void
{
    switch (expr.expression)
    {
        .Literal => |literal|
        {
            _ = try writer.write(literal);
        },
        .Variable => |variable|
        {
            _ = try writer.write(variable);
        },
        .FunctionCall => |FunctionCall|
        {
            _ = try writer.write(FunctionCall.name);
            _ = try writer.write("(");
            var j: usize = 0;
            while (j < FunctionCall.inputParams.len) : (j += 1)
            {
                try TranspileExpression(writer, &FunctionCall.inputParams[j]);
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
                try TranspileExpression(writer, &IndexedAccessor.inputParams[j]);
                if (j < IndexedAccessor.inputParams.len - 1)
                {
                    _ = try writer.write(", ");
                }
            }
            _ = try writer.write("]");
        },
        .Op => |Op|
        {
            _ = try writer.write("(");
            try TranspileExpression(writer, &Op.leftExpression);
            _ = try writer.write(" ");
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
                .PercentEqual => _ = try writer.write("%=")
            }
            _ = try writer.write(" ");
            try TranspileExpression(writer, &Op.rightExpression);
            _ = try writer.write(")");
        }
    }
    if (expr.next != null)
    {
        if (expr.next.?.expression != .IndexedAccessor)
        {
            _ = try writer.write(".");
        }
        try TranspileExpression(writer, expr.next.?);
    }
}
//parent is not null if we are in reflectable compound statement
pub fn TranspileStatementCpp(writer: std.fs.File.Writer, cstmt: ast.CompoundStatementData, eolIsSemicolon: bool, parent: ?[]const u8) anyerror!void
{
    var i: usize = 0;
    while (i < cstmt.items.len) : (i += 1)
    {
        var stmt: *ast.StatementData = &cstmt.items[i];
        switch (stmt.*)
        {
            .structDeclaration => |*structDeclaration|
            {
                try TranspileStatementCpp(writer, structDeclaration.body, true, structDeclaration.name);
            },
            .variableDeclaration => |*variableDeclaration|
            {
                if (parent == null)
                {
                    if (variableDeclaration.isConst)
                    {
                        _ = try writer.write("const ");
                    }
                    _ = try writer.write(variableDeclaration.typeName);
                    _ = try writer.write(" ");
                    _ = try writer.write(variableDeclaration.name);
                    if (variableDeclaration.defaultValue != null)
                    {
                        _ = try writer.write(" = ");
                        try TranspileExpression(writer, &variableDeclaration.defaultValue.?);
                    }
                    try AppendEndOfLine(writer, eolIsSemicolon);
                }
            },
            .functionDeclaration => |*functionDeclaration|
            {
                _ = try writer.write(functionDeclaration.returnType);
                _ = try writer.write(" ");
                if (parent != null)
                {
                    _ = try writer.write(parent.?);
                    _ = try writer.write("::");
                }
                _ = try writer.write(functionDeclaration.name);
                _ = try writer.write("(");
                var j: usize = 0;
                while (j < functionDeclaration.args.len) : (j += 1)
                {
                    const arg: *ast.VarData = &functionDeclaration.args[j];
                    if (arg.isConst)
                    {
                        _ = try writer.write("const ");
                    }
                    _ = try writer.write(arg.typeName);
                    _ = try writer.write(" ");
                    _ = try writer.write(arg.name);

                    if (j < functionDeclaration.args.len - 1)
                    {
                        _ = try writer.write(", ");
                    }
                }
                _ = try writer.write(") {\n");
                try TranspileStatementCpp(writer, functionDeclaration.statement, true, null);
                _ = try writer.write("}\n");
            },
            .functionInvoke => |*functionInvoke|
            {
                _ = try writer.write(functionInvoke.name);
                var j: usize = 0;
                while (j < functionInvoke.inputParams.len) : (j += 1)
                {
                    try TranspileExpression(writer, &functionInvoke.inputParams[j]);
                    if (j <= functionInvoke.inputParams.len - 1)
                    {
                        _ = try writer.write(", ");
                    }
                }
                _ = try writer.write(")");
                try AppendEndOfLine(writer, eolIsSemicolon);
            },
            .returnStatement => |*returnStatement|
            {
                _ = try writer.write("return ");
                try TranspileExpression(writer, returnStatement);
                try AppendEndOfLine(writer, eolIsSemicolon);
            },
            .otherExpression => |*otherExpression|
            {
                try TranspileExpression(writer, otherExpression);
                try AppendEndOfLine(writer, eolIsSemicolon);
            },
            .IfStatement => |*IfStatement|
            {
                _ = try writer.write("if (");
                try TranspileExpression(writer, &IfStatement.condition);
                _ = try writer.write(") {\n");
                try TranspileStatementCpp(writer, IfStatement.statement, true, null);
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
                    try TranspileStatementCpp(writer, ElseStatement.*, true, null);
                }
            },
            .WhileStatement => |*WhileStatement|
            {
                _ = try writer.write("while (");
                try TranspileExpression(writer, &WhileStatement.condition);
                _ = try writer.write(") {\n");
                try TranspileStatementCpp(writer, WhileStatement.statement, true, null);
                _ = try writer.write("}\n");
            },
            .ForStatement => |*ForStatement|
            {
                _ = try writer.write("for (");
                try TranspileStatementCpp(writer, ForStatement.initializer, false, null);
                _ = try writer.write("; ");
                try TranspileExpression(writer, &ForStatement.condition);
                _ = try writer.write("; ");
                try TranspileStatementCpp(writer, ForStatement.shouldStep, false, null);
                _ = try writer.write(") {\n");
                try TranspileStatementCpp(writer, ForStatement.statement, true, null);
                _ = try writer.write("}\n");
            },
            else =>
            {

            }
        }
    }
}