//the reflector runs on statements produced by the parser and caches the results
//For now, it only runs on statements that are top-level, or within a top-level struct.

const std = @import("std");
const string = @import("zig-string.zig").String;
const Errors = @import("errors.zig").Errors;
const ast = @import("ASTnodes.zig");
const parsing = @import("parser-state.zig");
const StatementDataTag = ast.StatementDataTag;
const StatementData = ast.StatementData;

pub const ReflectionDatabase = struct
{
    types: std.ArrayList(LinxcType),
    nameToType: std.StringHashMap(usize),
    allocator: std.mem.Allocator,
    currentID: usize,

    pub fn init(allocator: std.mem.Allocator) anyerror!ReflectionDatabase
    {
        var result: ReflectionDatabase = ReflectionDatabase {
            .allocator = allocator,
            .nameToType = std.StringHashMap(usize).init(allocator),
            .types = std.ArrayList(LinxcType).init(allocator),
            .currentID = 1
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
    }
    pub inline fn GetType(self: *@This(), name: ast.TypeNameData) Errors!*LinxcType
    {
        const index = self.nameToType.get(name.fullName);
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
            .functions = std.ArrayList(LinxcFunc).init(self.allocator),
            .variables = std.ArrayList(LinxcVariable).init(self.allocator),
            .isPrimitiveType = true,
            .isTrait = false,
            .implsTraits = std.ArrayList(*LinxcType).init(self.allocator),
            .implementedBy = std.ArrayList(*LinxcType).init(self.allocator),
            .genericTypes = std.ArrayList(string).init(self.allocator),
            .pointerCount = 0,
            .templateSpecializations = std.ArrayList(string).init(self.allocator)
        });
        self.currentID += 1;
    }
    pub inline fn GetTypeSafe(self: *@This(), name: []const u8) anyerror!*LinxcType
    {
        if (!self.nameToType.contains(name.fullName))
        {
            const nameStr = try string.init_with_contents(self.allocator, name);
            try self.AddType(LinxcType
            {
                .name = nameStr,
                .ID = self.currentID,
                .functions = std.ArrayList(LinxcFunc).init(self.allocator),
                .variables = std.ArrayList(LinxcVariable).init(self.allocator),
                .isPrimitiveType = false,
                .isTrait = false,
                .implsTraits = std.ArrayList(*LinxcType).init(self.allocator),
                .implementedBy = std.ArrayList(*LinxcType).init(self.allocator),
                .genericTypes = std.ArrayList(string).init(self.allocator),
                .pointerCount = 0,
                .templateSpecializations = std.ArrayList(string).init(self.allocator)
            });
            self.currentID += 1;
        }
        return self.GetType(name);
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
    pub inline fn GetVariable(self: *@This(), variableTypeName: ast.TypeNameData, variableName: []const u8) anyerror!LinxcVariable
    {
        const variableType: *LinxcType = try self.GetTypeSafe(variableTypeName.fullName);

        const variableNameStr = try string.init_with_contents(self.allocator, variableName);

        return LinxcVariable
        {
            .name = variableNameStr,
            .variableType = variableType
        };
    }
    ///checks a type name for any potential template specializations
    pub fn CheckTypeName(self: *@This(), typeName: *ast.TypeNameData) anyerror!void
    {
        if (typeName.templateTypes != null)
        {
            //cannot use fullname here as it would contain the templates
            var baseName: string = string.init(self.allocator);
            defer baseName.deinit();
            try baseName.append(typeName.namespace);
            try baseName.append("::");
            try baseName.append(typeName.name);

            var linxcType = try self.GetTypeSafe(baseName.str());
            var i: usize = 0;
            while (i < typeName.templateTypes.?.items.len) : (i += 1)
            {
                const templateType: *ast.TypeNameData = &typeName.templateTypes.?.items[i];
                //parse child as well, in case it is another templated type
                self.CheckTypeName(templateType);
                //can use fullname here
                const templateTypeNameStr = try string.init_with_contents(self.allocator, templateType.fullName);
                try linxcType.templateSpecializations.append(templateTypeNameStr);
            }
        }
    }
    pub fn PostParseStatement(self: *@This(), statement: *StatementData, state: *parsing.ParserState) anyerror!void
    {
        switch (statement)
        {
            .structDeclaration => |*structDeclaration|
            {
                var namespaceName = string.init(self.allocator);
                defer namespaceName.deinit();
                try state.ConcatNamespaces(&namespaceName);

                var fullName = string.init(self.allocator);
                defer fullName.deinit();
                try fullName.concat(namespaceName.str());
                try fullName.concat("::");
                try fullName.concat(structDeclaration.name);

                var structType = try self.GetTypeSafe(fullName);
                for (structDeclaration.templateTypes.?) |templateType|
                {
                    const templateTypeName = try string.init_with_contents(self.allocator, templateType);
                    try structType.genericTypes.append(templateTypeName);
                }
            },
            .variableDeclaration => |*varDecl|
            {
                try self.CheckTypeName(&varDecl.typeName);
            },
            else =>
            {
                
            }
        }
    }
};

pub const LinxcVariable = struct
{
    name: string,
    variableType: *LinxcType,
    isPointer: bool,

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
    returnType: *LinxcType,
    args: ?[]LinxcVariable,
    
    pub fn ToString(self: *@This()) anyerror!string
    {
        var result = string.init(self.name.allocator);

        try result.concat(self.returnType.name.str());
        try result.concat(" ");
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
    functions: std.ArrayList(LinxcFunc),
    variables: std.ArrayList(LinxcVariable),
    isPrimitiveType: bool,
    isTrait: bool,
    implsTraits: std.ArrayList(*LinxcType),
    implementedBy: std.ArrayList(*LinxcType),
    //eg: <K, V>
    genericTypes: std.ArrayList(string),
    //eg: (std::string, int), (std::string, double) etc (len always multiple of len of genericTypes)
    templateSpecializations: std.ArrayList(string),
    pointerCount: i32,

    pub fn ToString(self: *@This()) !string
    {
        var result = string.init(self.name.allocator);
        try result.concat("Type ");
        try result.concat(self.name.str());
        var i: i32 = 0;
        while (i < self.pointerCount) : (i += 1)
        {
            try result.concat("*");
        }
        try result.concat(": \n");
        for (self.variables.items) |*variable|
        {
            try result.concat("   ");
            var varString = try variable.ToString();
            try result.concat_deinit(&varString);
            try result.concat("\n");
        }
        for (self.functions.items) |*func|
        {
            try result.concat("   ");
            var funcString = try func.ToString();
            try result.concat_deinit(&funcString);
            try result.concat("\n");
        }
        return result;
    }
    pub fn deinit(self: *@This()) void
    {
        self.implsTraits.deinit();
        self.implementedBy.deinit();

        self.name.deinit();
        for (self.functions.items) |*function|
        {
            function.deinit();
        }
        self.functions.deinit();
        for (self.variables.items) |*variable|
        {
            variable.deinit();
        }
        self.variables.deinit();
        for (self.genericTypes.items) |genericType|
        {
            genericType.deinit();
        }
        self.genericTypes.deinit();
    }
};