//the reflector runs on statements produced by the parser and caches the results
//For now, it only runs on statements that are top-level, or within a top-level struct.

const std = @import("std");
const string = @import("zig-string.zig").String;
const Errors = @import("errors.zig").Errors;
const ast = @import("ASTnodes.zig");
const parsing = @import("parser-state.zig");
const StatementDataTag = ast.StatementDataTag;
const StatementData = ast.StatementData;

pub const TemplatedStruct = struct
{
    allocator: std.mem.Allocator,
    structData: ast.StructData,
    linxcFile: string,
    outputC: string,
    outputH: string,
    namespace: string,

    pub inline fn deinit(self: *@This()) void
    {
        self.outputC.deinit();
        self.outputH.deinit();
        self.structData.deinit(self.allocator);
        self.linxcFile.deinit();
        self.namespace.deinit();
    }
};
pub const ReflectionDatabase = struct
{
    templatedStructs: std.ArrayList(TemplatedStruct),
    types: std.ArrayList(LinxcType),
    nameToType: std.StringHashMap(usize),
    nameToFunc: std.StringHashMap(LinxcFunc),
    namespaceToDefinedTypes: std.StringHashMap(std.AutoHashMap(*LinxcType, void)),
    allocator: std.mem.Allocator,
    currentID: usize,

    pub fn init(allocator: std.mem.Allocator) anyerror!ReflectionDatabase
    {
        var result: ReflectionDatabase = ReflectionDatabase {
            .allocator = allocator,
            .nameToType = std.StringHashMap(usize).init(allocator),
            .templatedStructs = std.ArrayList(TemplatedStruct).init(allocator),
            .types = std.ArrayList(LinxcType).init(allocator),
            .nameToFunc = std.StringHashMap(LinxcFunc).init(allocator),
            .currentID = 1,
            .namespaceToDefinedTypes = std.StringHashMap(std.AutoHashMap(*LinxcType, void)).init(allocator)
        };
        try result.AddPrimitiveType("i8");
        try result.AddPrimitiveType("i16");
        try result.AddPrimitiveType("i32");
        try result.AddPrimitiveType("i64");
        try result.AddPrimitiveType("u8");
        try result.AddPrimitiveType("u16");
        try result.AddPrimitiveType("u32");
        try result.AddPrimitiveType("u64");
        try result.AddPrimitiveType("float");
        try result.AddPrimitiveType("bool");
        try result.AddPrimitiveType("char");
        try result.AddPrimitiveType("double");
        return result;
    }
    pub inline fn deinit(self: *@This()) void
    {
        for (self.types.items) |*linxcType|
        {
            linxcType.deinit();
        }
        self.types.deinit();
        self.nameToType.deinit();
        for (self.templatedStructs.items) |*templatedStruct|
        {
            templatedStruct.deinit();
        }
        self.templatedStructs.deinit();

        {
            var iter = self.nameToFunc.iterator();
            while (true)
            {
                var next = iter.next();
                if (next != null)
                {
                    next.?.value_ptr.*.deinit();
                }
                else break;
            }
            self.nameToFunc.deinit();
        }
        {
            var iter = self.namespaceToDefinedTypes.iterator();
            while (true)
            {
                var next = iter.next();
                if (next != null)
                {
                    next.?.value_ptr.*.deinit();
                }
                else break;
            }
            self.namespaceToDefinedTypes.deinit();
        }
    }
    pub inline fn GetType(self: *@This(), name: []const u8) Errors!*LinxcType
    {
        const index = self.nameToType.get(name);
        if (index != null)
        {
            return &self.types.items[index.?];
        }
        else
        {
            return Errors.TypeNotFoundError;
        }
    }
    pub inline fn AddPrimitiveType(self: *@This(), name: []const u8) anyerror!void
    {
        const nameStr = try string.init_with_contents(self.allocator, name);
        try self.AddType(LinxcType
        {
            .ID = self.currentID,
            .name = nameStr,
            .headerFile = null,
            .nameToFunction = std.StringHashMap(LinxcFunc).init(self.allocator),
            .variables = std.ArrayList(LinxcVariable).init(self.allocator),
            .isPrimitiveType = true,
            .isTrait = false,
            .implsTraits = std.ArrayList(*LinxcType).init(self.allocator),
            .implementedBy = std.ArrayList(*LinxcType).init(self.allocator),
            .genericTypes = std.ArrayList(string).init(self.allocator),
            .pointerCount = 0,
            .templateSpecializations = std.ArrayList(*LinxcType).init(self.allocator)
        });
        self.currentID += 1;
    }
    pub inline fn GetTypeSafe(self: *@This(), name: []const u8) anyerror!*LinxcType
    {
        if (!self.nameToType.contains(name))
        {
            const nameStr = try string.init_with_contents(self.allocator, name);
            try self.AddType(LinxcType
            {
                .name = nameStr,
                .ID = self.currentID,
                .headerFile = null,
                .nameToFunction = std.StringHashMap(LinxcFunc).init(self.allocator),
                .variables = std.ArrayList(LinxcVariable).init(self.allocator),
                .isPrimitiveType = false,
                .isTrait = false,
                .implsTraits = std.ArrayList(*LinxcType).init(self.allocator),
                .implementedBy = std.ArrayList(*LinxcType).init(self.allocator),
                .genericTypes = std.ArrayList(string).init(self.allocator),
                .pointerCount = 0,
                .templateSpecializations = std.ArrayList(*LinxcType).init(self.allocator)
            });
            self.currentID += 1;
        }
        return self.GetType(name);
    }
    pub fn GetTypeFromDataSafe(self: *@This(), typename: *ast.TypeNameData) anyerror!*LinxcType
    {
        var name = string.init(self.allocator);
        defer name.deinit();
        if (typename.namespace.str().len > 0)
        {
            try name.concat(typename.namespace.str());
            try name.concat("::");
        }
        try name.concat(typename.name.str());
        if (typename.templateTypes != null)
        {
            var i: usize = 0;
            while (i < typename.templateTypes.?.items.len) : (i += 1)
            {
                const templateTypeName: *ast.TypeNameData = &typename.templateTypes.?.items[i];
                const templateType = try self.GetTypeFromDataSafe(templateTypeName);
                try name.concat("_");
                var tempStr: []u8 = try std.fmt.allocPrint(self.allocator, "{d}", .{templateType.ID});
                defer self.allocator.free(tempStr);
                try name.concat(tempStr);
            }
        }
        return self.GetTypeSafe(name.str());
    }
    pub inline fn AddType(self: *@This(), newType: LinxcType) anyerror!void
    {
        try self.nameToType.put(newType.name.str(), self.types.items.len);
        try self.types.append(newType);
    }
    pub inline fn RemoveType(self: *@This(), name: []const u8) anyerror!void
    {
        const index = self.nameToType.get(name).?;
        var currentLastType = self.types.items[self.types.items.len - 1];
        //currentLastType's new index is now index
        try self.nameToType.put(currentLastType.name, index);
        self.types.swapRemove(index);
        self.nameToType.remove(name);
    }
    pub fn GetGlobalFuncSafe(self: *@This(), nameAndNamespace: []const u8) anyerror!*LinxcFunc
    {
        var result = self.nameToFunc.getPtr(nameAndNamespace);
        if (result != null)
        {
            return result.?;
        }
        const nameStr = try string.init_with_contents(self.allocator, nameAndNamespace);
        const func = LinxcFunc
        {
            .name = nameStr,
            .returnType = null,
            .args = null,
            .genericTypes = std.ArrayList(string).init(self.allocator)
        };
        try self.nameToFunc.put(nameAndNamespace, func);
        return self.nameToFunc.getPtr(nameAndNamespace).?;
    }
    pub fn GetFuncSafe(self: *@This(), owner: *LinxcType, name: []const u8) anyerror!*LinxcFunc
    {
        var result: ?*LinxcFunc = owner.nameToFunction.getPtr(name);
        if (result != null)
        {
            return result.?;
        }
        const nameStr = try string.init_with_contents(self.allocator, name);
        try owner.nameToFunction.put(name, LinxcFunc
        {
            .name = nameStr,
            .returnType = null,
            .args = null,
            .genericTypes = std.ArrayList(string).init(self.allocator)
        });
        return owner.nameToFunction.getPtr(name).?;
    }
    ///Checks a function call for any potential template specializations
    pub fn CheckFunctionName(self: *@This(), funcName: *ast.TypeNameData) anyerror!void
    {
        if (funcName.templateTypes != null)
        {
            //cannot use fullname here as it would contain the templates
            var baseName: string = string.init(self.allocator);
            defer baseName.deinit();
            if (funcName.namespace.str().len > 0)
            {
                try baseName.concat(funcName.namespace.str());
                try baseName.concat("::");
            }
            try baseName.concat(funcName.name.str());
        }
    }
    ///checks a type name for any potential template specializations
    pub fn CheckTypeName(self: *@This(), typeName: *ast.TypeNameData) anyerror!void
    {
        if (typeName.templateTypes != null)
        {
            //cannot use fullname here as it would contain the templates
            var baseName: string = string.init(self.allocator);
            defer baseName.deinit();
            if (typeName.namespace.str().len > 0)
            {
                try baseName.concat(typeName.namespace.str());
                try baseName.concat("::");
            }
            try baseName.concat(typeName.name.str());

            var baseType = try self.GetTypeSafe(baseName.str());
            var i: usize = 0;
            while (i < typeName.templateTypes.?.items.len) : (i += 1)
            {
                const templateTypeName: *ast.TypeNameData = &typeName.templateTypes.?.items[i];
                //parse child as well, in case it is another templated type
                try self.CheckTypeName(templateTypeName);

                //can use fullname here
                //edit: turns out we can't
                //anyways, we really should be mangling these into non-english
                var templateType = try self.GetTypeFromDataSafe(templateTypeName);
                
                //also need to append the file that templateType is defined in, in case
                //we need to #include it during the specialized template transpilation
                try baseType.templateSpecializations.append(templateType);
            }
            //dont check next as if there exists a next, it must be a static variable
            //namespace::type<spec>::namespace::var
            //or namespace::type<spec>::nestedtype::var does not exist
        }
    }
    pub fn PostParseExpression(self: *@This(), expr: *ast.ExpressionData) anyerror!void
    {
        switch (expr.*)
        {
            .Variable => |*variable|
            {
                try self.CheckTypeName(variable);
            },
            .Literal =>
            {

            },
            .ModifiedVariable => |*modifiedVariable|
            {
                try self.PostParseExpression(&modifiedVariable.*.expression);
            },
            .Op => |*operator|
            {
                try self.PostParseExpression(&operator.*.leftExpression);
                try self.PostParseExpression(&operator.*.rightExpression);
            },
            .FunctionCall => |*funcCall|
            {
                try self.CheckFunctionName(&funcCall.*.name);
                //self.checkFunctionName
            },
            .IndexedAccessor =>// |*indexedAccessor|
            {
                //self.checkFunctionName
            },
            .TypeCast => |*typeCast|
            {
                try self.CheckTypeName(&typeCast.*.typeName);
            },
            .sizeOf => |*sizeOf|
            {
                try self.CheckTypeName(sizeOf);
            },
            .nameOf => |*nameOf|
            {
                try self.CheckTypeName(nameOf);
            },
            .typeOf => |*typeOf|
            {
                try self.CheckTypeName(typeOf);
            }
        }
    }
    pub fn PostParseStatement(self: *@This(), statement: *StatementData, state: *parsing.ParserState) anyerror!void
    {
        switch (statement.*)
        {
            .functionDeclaration => |*funcDeclaration|
            {
                //method is so dang humungous because
                //there is a LOT of things to fill up, unlike a struct, which has just templated types

                var func: *LinxcFunc = undefined;
                if (state.structNames.items.len > 0)
                {
                    var encapsulatingTypeName = string.init(self.allocator);
                    defer encapsulatingTypeName.deinit();
                    try state.ConcatNamespaces(&encapsulatingTypeName);
                    for (state.structNames.items) |structName|
                    {
                        try encapsulatingTypeName.concat("::");
                        try encapsulatingTypeName.concat(structName);
                    }
                    var structType = try self.GetTypeSafe(encapsulatingTypeName.str());
                    func = try self.GetFuncSafe(structType, funcDeclaration.name.str());
                    //std.debug.print("Found function {s} in struct {s}\n", .{funcDeclaration.name.str(), encapsulatingTypeName.str()});
                }
                else
                {
                    var nameAndNamespace = string.init(self.allocator);
                    defer nameAndNamespace.deinit();
                    if (state.namespaces.items.len > 0)
                    {
                        try state.ConcatNamespaces(&nameAndNamespace);
                        try nameAndNamespace.concat("::");
                    }
                    try nameAndNamespace.concat(funcDeclaration.name.str());
                    func = try self.GetGlobalFuncSafe(nameAndNamespace.str());
                    //std.debug.print("Found global function {s}\n", .{nameAndNamespace.str()});
                }
                //parse return type
                var returnType: ?*LinxcType = null;
                if (!std.mem.eql(u8, funcDeclaration.returnType.fullName.str(), "void"))
                {
                    try self.CheckTypeName(&funcDeclaration.*.returnType);
                    returnType = try self.GetTypeFromDataSafe(&funcDeclaration.*.returnType);
                }
                func.returnType = returnType;

                //variables
                var variables = std.ArrayList(LinxcVariable).init(self.allocator);
                for (funcDeclaration.args) |*arg|
                {
                    const argNameStr = try string.init_with_contents(self.allocator, arg.name.str());
                    try self.CheckTypeName(&arg.*.typeName);
                    const argType = try self.GetTypeFromDataSafe(&arg.*.typeName);
                    try variables.append(LinxcVariable
                    {
                        .name = argNameStr,
                        .variableType = argType
                    });
                }
                var variablesSlice: ?[]LinxcVariable = null;
                if (variables.items.len > 0)
                {
                    variablesSlice = try variables.toOwnedSlice();
                }
                func.args = variablesSlice;

                //generic types
                if (funcDeclaration.templateTypes != null)
                {
                    for (funcDeclaration.templateTypes.?) |templateType|
                    {
                        const templateTypeName = try string.init_with_contents(self.allocator, templateType.str());
                        try func.genericTypes.append(templateTypeName);
                    }
                }
            },
            .structDeclaration => |*structDeclaration|
            {
                var namespaceName = string.init(self.allocator);
                defer namespaceName.deinit();
                try state.ConcatNamespaces(&namespaceName);

                var fullName = string.init(self.allocator);
                defer fullName.deinit();
                if (namespaceName.str().len > 0)
                {
                    try fullName.concat(namespaceName.str());
                    try fullName.concat("::");
                }
                try fullName.concat(structDeclaration.name.str());

                var structType = try self.GetTypeSafe(fullName.str());
                structType.headerFile = try string.init_with_contents(self.allocator, state.outputHeaderIncludePath);
                
                if (structDeclaration.templateTypes != null)
                {
                    for (structDeclaration.templateTypes.?) |templateType|
                    {
                        const templateTypeName = try string.init_with_contents(self.allocator, templateType.str());
                        try structType.genericTypes.append(templateTypeName);
                    }
                }

                if (self.namespaceToDefinedTypes.get(namespaceName.str()) == null)
                {
                    try self.namespaceToDefinedTypes.put(namespaceName.str(), std.AutoHashMap(*LinxcType, void).init(self.allocator));
                }
                var hashset = self.namespaceToDefinedTypes.getPtr(namespaceName.str()).?;
                try hashset.put(structDeclaration.name.str(), .{});
            },
            .variableDeclaration => |*varDecl|
            {
                try self.CheckTypeName(&varDecl.typeName);
            },
            .otherExpression => |*otherExpression|
            {
                try self.PostParseExpression(otherExpression);
            },
            else =>
            {
                
            }
        }
    }
    pub fn TypenameToUseableString(db: *@This(), self: *ast.TypeNameData, allocator: std.mem.Allocator, specializeTemplates: bool) anyerror!string
    {
        var result: string = string.init(allocator);
        if (self.namespace.str().len > 0)
        {
            try result.concat(self.namespace.str());
            try result.concat("::");
        }
        try result.concat(self.name.str());
        if (self.templateTypes != null)
        {
            if (specializeTemplates)
            {
                var i: usize = 0;
                while (i < self.templateTypes.?.items.len) : (i += 1)
                {
                    try result.concat("_");
                    const templateType = try db.GetTypeFromDataSafe(&self.templateTypes.?.items[i]);
                    var templateTypeStr = try std.fmt.allocPrint(allocator, "{d}", .{templateType.ID});
                    defer allocator.free(templateTypeStr);
                    //var templateTypeStr = try self.templateTypes.?.items[i].ToUseableString(allocator);
                    try result.concat(templateTypeStr);
                }
            }
            else
            {
                var i: usize = 0;
                try result.concat("<");
                while (i < self.templateTypes.?.items.len) : (i += 1)
                {
                    var templateTypeNameStr = try db.TypenameToUseableString(&self.templateTypes.?.items[i], allocator, specializeTemplates);
                    try result.concat_deinit(&templateTypeNameStr);

                    if (i < self.templateTypes.?.items.len - 1)
                    {
                        try result.concat(", ");
                    }
                }
                try result.concat(">");
            }
            //try result.concat("_");
        }
        var j: i32 = 0;
        while (j < self.pointerCount) : (j += 1)
        {
            try result.concat("*");
        }
        if (self.next != null)
        {
            try result.concat("::");
            var nextStr = try db.TypenameToUseableString(self.next.?, allocator, specializeTemplates);//self.next.?.ToUseableString(allocator);
            try result.concat_deinit(&nextStr);
        }
        return result;
    }
};

pub const LinxcVariable = struct
{
    name: string,
    variableType: *LinxcType,
    //dont care about default value - its useless to the reflector anyways

    pub fn ToString(self: *@This()) anyerror!string
    {
        var result = string.init(self.name.allocator);
        
        if (self.isPointer)
        {
            try result.concat("*");
        }
        try result.concat(self.variableType.name.str());
        try result.concat(" ");
        try result.concat(self.name.str());

        return result;
    }
    pub fn deinit(self: *@This()) void
    {
        self.name.deinit();
        //dont deinit type - that is the job of ReflectionDatabase's deinit
    }
};
pub const LinxcFunc = struct
{
    name: string,
    returnType: ?*LinxcType,
    args: ?[]LinxcVariable,
    genericTypes: std.ArrayList(string),
    
    pub fn ToString(self: *@This()) anyerror!string
    {
        var result = string.init(self.name.allocator);

        if (self.returnType == null)
        {
            try result.concat("void ");
        }
        else
        {
            try result.concat(self.returnType.?.name.str());
            try result.concat(" ");
        }
        try result.concat(self.name.str());
        try result.concat("(");
        if (self.args != null)
        {
            var i: usize = 0;
            while (i < self.args.?.len) : (i += 1)
            {
                var arg: *LinxcVariable = &self.args.?[i];
                try result.concat(arg.variableType.name.str());
                if (i < self.args.?.len - 1)
                {
                    try result.concat(", ");
                }
            }
        }
        try result.concat(")");
        return result;
    }
    pub fn deinit(self: *@This()) void
    {
        self.name.deinit();
        if (self.args != null)
        {
            for (self.args.?) |*variable|
            {
                variable.deinit();
            }
        }
        //dont deinit type - that is the job of ReflectionDatabase's deinit
    }
};

pub const LinxcType = struct
{
    name: string,
    ID: usize,
    headerFile: ?string,
    nameToFunction: std.StringHashMap(LinxcFunc),
    variables: std.ArrayList(LinxcVariable),
    isPrimitiveType: bool,
    isTrait: bool,
    implsTraits: std.ArrayList(*LinxcType),
    implementedBy: std.ArrayList(*LinxcType),
    //eg: <K, V>
    genericTypes: std.ArrayList(string),
    //eg: (std::string, int), (std::string, double) etc (len always multiple of len of genericTypes)
    templateSpecializations: std.ArrayList(*LinxcType),
    pointerCount: i32,

    // pub fn ToString(self: *@This()) !string
    // {
    //     var result = string.init(self.name.allocator);
    //     try result.concat("Type ");
    //     try result.concat(self.name.str());
    //     var i: i32 = 0;
    //     while (i < self.pointerCount) : (i += 1)
    //     {
    //         try result.concat("*");
    //     }
    //     try result.concat(": \n");
    //     for (self.variables.items) |*variable|
    //     {
    //         try result.concat("   ");
    //         var varString = try variable.ToString();
    //         try result.concat_deinit(&varString);
    //         try result.concat("\n");
    //     }
    //     for (self.functions.items) |*func|
    //     {
    //         try result.concat("   ");
    //         var funcString = try func.ToString();
    //         try result.concat_deinit(&funcString);
    //         try result.concat("\n");
    //     }
    //     return result;
    // }
    pub fn deinit(self: *@This()) void
    {
        self.implsTraits.deinit();
        self.implementedBy.deinit();
        if (self.headerFile != null)
            self.headerFile.?.deinit();

        self.name.deinit();
        for (self.variables.items) |*variable|
        {
            variable.deinit();
        }
        self.variables.deinit();
        for (self.genericTypes.items) |*genericType|
        {
            genericType.deinit();
        }
        self.genericTypes.deinit();

        var nameToFunctionIterator = self.nameToFunction.iterator();
        while (true)
        {
            const next = nameToFunctionIterator.next();
            if (next == null)
            {
                break;
            }
            next.?.value_ptr.deinit();
        }
        self.nameToFunction.deinit();
    }
};