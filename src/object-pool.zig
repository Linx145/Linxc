const std = @import("std");

pub const Errors = error{
    allocationError
};

pub fn ObjectPool(comptime T: type) type 
{
    return struct 
    {
        const Self = @This();
        pool: std.ArrayList(T),
        generator: *const fn (std.mem.Allocator) Errors!T,

        pub fn init(allocator: std.mem.Allocator, generator: *const fn(std.mem.Allocator) Errors!T) Self
        {
            return Self
            {  
                .pool = std.ArrayList(T).init(allocator),
                .generator = generator
            };
        }
        pub fn deinit(self: *Self) void
        {
            self.pool.deinit();
        }
        pub inline fn Rent(self: *Self) Errors!T
        {
            if (self.pool.items.len > 0)
            {
                return self.pool.pop();
            }
            else {
                return self.generator(self.pool.allocator);
            }
        }
        pub inline fn Return(self: *Self, rented: T) !void
        {
            try self.pool.append(rented);
        }
    };
}