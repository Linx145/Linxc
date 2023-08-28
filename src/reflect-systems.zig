const std = @import("std");

pub fn ParseFile(allocator: std.mem.Allocator, source: []const u8) !void
{
    var tokenizer = std.c.Tokenizer{
        .buffer = source
    };

    var awaitingFunctionStart: bool = false;
    var foundSystem: bool = false;
    var skipNext: bool = false;
    var indents: i32 = 0;
    var systemInteractionMode: u8 = 0;
    var checkComponentInteraction: bool = false;

    var currentSystem: ?*SystemData = null;

    var reflData: ReflectionData = ReflectionData.init(allocator);
    defer reflData.deinit();

    while (true)
    {
        const token: std.c.Token = tokenizer.next();
        // const tokenLength: usize = token.end - token.start;
        // _ = tokenLength;
        
        switch (token.id) {
            .Eof => {
                break;
            },
            .LineComment, .MultiLineComment => continue,
            else => {
                const tokenString: []const u8 = source[token.start..token.end];
                if (tokenString[0] != '\n' and tokenString[0] != '\r')
                {
                    if (indents == 0)
                    {
                        if (foundSystem)
                        {
                            if (std.mem.eql(u8, tokenString, "template"))
                            {
                                skipNext = true;
                            }
                            else if (tokenString[0] == '>')
                            {
                                skipNext = false;
                            }
                            else if (awaitingFunctionStart and tokenString[0] == '{')
                            {
                                indents += 1;
                            }
                            if (token.id == .Identifier and !skipNext and !awaitingFunctionStart)
                            {
                                awaitingFunctionStart = true;
                                //try stdout.print("found system {s}\n", .{tokenString});
                                currentSystem = try reflData.GetSystemData(tokenString);
                            }
                        }
                        else if (std.mem.eql(u8, tokenString, "SYSTEM"))
                        {
                            foundSystem = true;
                        }
                    }
                    else 
                    {
                        //when indents not 0, we are within a system function
                        //thus check for read/write declarations
                        if (checkComponentInteraction)
                        {
                            if (token.id == .Identifier)
                            {
                                const component: *ComponentData = try reflData.GetComponentData(tokenString);
                                //we got the component type, dont need to care about what
                                //the user names the component as
                                switch (systemInteractionMode)
                                {
                                    1 => {
                                        try currentSystem.?.writesComponents.append(component.ID - 1);
                                        //try stdout.print("system writes component {s}\n", .{tokenString});
                                    },
                                    2 => {
                                        try currentSystem.?.readsComponents.append(component.ID - 1);
                                        //try stdout.print("system reads component {s}\n", .{tokenString});
                                    },
                                    else => {}
                                }
                                systemInteractionMode = 0;
                                checkComponentInteraction = false;
                            }
                        }
                        if (std.mem.eql(u8, tokenString, "WRITE"))
                        {
                            checkComponentInteraction = true;
                            systemInteractionMode = 1;
                        }
                        else if (std.mem.eql(u8, tokenString, "READ"))
                        {
                            checkComponentInteraction = true;
                            systemInteractionMode = 2;
                        }

                        if (tokenString[0] == '{')
                        {
                            indents += 1;
                        }
                        else if (tokenString[0] == '}')
                        {
                            indents -= 1;
                        }

                        if (indents == 0)
                        {
                            awaitingFunctionStart = false;
                            foundSystem = false;
                            currentSystem = null;
                        }
                    }
                }
            }
        }
    }

    try reflData.log();
}

pub const ReflectionData = struct 
{
    const Self = @This();
    alloc: std.mem.Allocator,
    systems: std.ArrayList(SystemData),
    nameToSystem: std.StringHashMap(usize),
    components: std.ArrayList(ComponentData),
    nameToComponent: std.StringHashMap(usize),

    pub fn init(allocator: std.mem.Allocator) Self
    {
        return Self
        {
            .alloc = allocator,
            .systems = std.ArrayList(SystemData).init(allocator),
            .components = std.ArrayList(ComponentData).init(allocator),
            .nameToSystem = std.StringHashMap(usize).init(allocator),
            .nameToComponent = std.StringHashMap(usize).init(allocator)
        };
    }
    pub fn GetComponentData(self: *Self, name: []const u8) std.mem.Allocator.Error!*ComponentData 
    {
        if (self.nameToComponent.contains(name))
        {
            return &self.components.items[self.nameToComponent.get(name).?];
        }
        var newData: ComponentData = ComponentData.init();
        newData.name = name;
        try self.components.append(newData);
        try self.nameToComponent.put(name, newData.ID - 1);
        return &self.components.items[newData.ID - 1];
    }
    pub fn GetSystemData(self: *Self, name: []const u8) std.mem.Allocator.Error!*SystemData
    {
        if (self.nameToSystem.contains(name))
        {
            return &self.systems.items[self.nameToSystem.get(name).?];
        }
        var newSystem: SystemData = SystemData.init(name, self.alloc);
        try self.systems.append(newSystem);
        try self.nameToSystem.put(name, newSystem.ID - 1);
        return &self.systems.items[newSystem.ID - 1];
    }
    pub fn deinit(self: *Self) void 
    {
        var i: usize = 0;
        while (i < self.systems.items.len) : (i += 1)
        {
            self.systems.items[i].deinit();
        }
        // for (self.components.items) |component|
        // {
        //     component.deinit();
        // }

        self.nameToSystem.deinit();
        self.nameToComponent.deinit();
        self.systems.deinit();
        self.components.deinit();
    }

    pub fn log(self: *Self) !void
    {
        const stdout_file = std.io.getStdOut().writer();
        var bw = std.io.bufferedWriter(stdout_file);
        const stdout = bw.writer();
        _ = try stdout.write("\n");

        var i: usize = 0;

        while (i < self.systems.items.len)
        {
            const system: *SystemData = &self.systems.items[i];
            try stdout.print("System: {s}\n", .{system.name});

            var j: usize = 0;
            while (j < system.writesComponents.items.len)
            {
                const component: *ComponentData = &self.components.items[system.writesComponents.items[j]];
                try stdout.print("-writes component {s}\n", .{component.name});
                j += 1;
            }

            j = 0;
            while (j < system.readsComponents.items.len)
            {
                const component: *ComponentData = &self.components.items[system.readsComponents.items[j]];
                try stdout.print("-reads component {s}\n", .{component.name});
                j += 1;
            }
            i += 1;
        }

        try bw.flush();
    }
};

pub const ComponentData = struct 
{
    const Self = @This();
    var lastID: usize = 0;

    name: []const u8,
    ID: usize,

    pub fn init() Self {
        lastID += 1;
        return Self 
        {
            .name = "",
            .ID = lastID
        };
    }
};

pub const SystemData = struct
{
    const arraylist = std.ArrayList(usize);
    const Self = @This();
    var lastID: usize = 0;

    name: []const u8,
    ID: usize,
    beforeSystems: arraylist,
    afterSystems: arraylist,
    readsComponents: arraylist,
    writesComponents: arraylist,

    pub fn init(name: []const u8, allocator: std.mem.Allocator) Self {
        lastID += 1;
        return Self
        {
            .name = name,
            .ID = lastID,
            .beforeSystems = arraylist.init(allocator),
            .afterSystems = arraylist.init(allocator),
            .readsComponents = arraylist.init(allocator),
            .writesComponents = arraylist.init(allocator)
        };
    }
    pub fn deinit(self: *Self) void {
        self.afterSystems.deinit();
        self.beforeSystems.deinit();
        self.readsComponents.deinit();
        self.writesComponents.deinit();
    }
};