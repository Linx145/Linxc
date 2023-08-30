const std = @import("std");

/// A doubly linked list.
/// Grabbed from github and modified because it appears my version of zig does not have it
pub fn LinkedList(comptime T: type) type {
    return struct {
        const List = std.ArrayList(Node);
        const Lifo = std.ArrayList(usize);
        const Self = @This();

        /// Node inside the linked list pointing to the actual data
        pub const Node = struct {
            data: T,
            index: usize,
            next: ?*Node,
            prev: ?*Node
        };

        unlinkedList: List,
        emptyIndices: Lifo,
        allocator: std.mem.Allocator,
        first: ?*Node = null,
        last: ?*Node = null,
        len: usize = 0,

        pub fn init(allocator: std.mem.Allocator) Self
        {
            return Self
            {
                .unlinkedList = List.init(allocator),
                .emptyIndices = Lifo.init(allocator),
                .allocator = allocator
            };
        }
        pub fn deinit(self: *Self) void
        {
            self.unlinkedList.deinit();
            self.emptyIndices.deinit();
        }

        /// Insert a new node after an existing one.
        ///
        /// Arguments:
        ///     node: Pointer to a node in the list.
        ///     new_node: Pointer to the new node to insert.
        pub fn insertAfter(list: *Self, node: *Node, item: T) !*Node 
        {
            const new_node: *Node = try list.get_free_node(item);

            new_node.prev = node;
            if (node.next) |next_node| {
                // Intermediate node.
                new_node.next = next_node;
                next_node.prev = new_node;
            } else {
                // Last element of the list.
                new_node.next = null;
                list.last = new_node;
            }
            node.next = new_node;

            list.len += 1;

            return new_node;
        }

        /// Insert a new node before an existing one.
        ///
        /// Arguments:
        ///     node: Pointer to a node in the list.
        ///     new_node: Pointer to the new node to insert.
        pub fn insertBefore(list: *Self, node: *Node, item: T) !*Node 
        {
            const new_node: *Node = try list.get_free_node(item);

            new_node.next = node;
            if (node.prev) |prev_node| {
                // Intermediate node.
                new_node.prev = prev_node;
                prev_node.next = new_node;
            } else {
                // First element of the list.
                new_node.prev = null;
                list.first = new_node;
            }
            node.prev = new_node;

            list.len += 1;

            return new_node;
        }

        /// Insert a new node at the end of the list.
        ///
        /// Arguments:
        ///     new_node: Pointer to the new node to insert.
        pub fn append(list: *Self, item: T) !*Node {
            if (list.last) |last| {
                // Insert after last.
                return list.insertAfter(last, item);
            } else {
                // Empty list.
                return list.prepend(item);
            }
        }

        /// Insert a new node at the beginning of the list.
        ///
        /// Arguments:
        ///     new_node: Pointer to the new node to insert.
        pub fn prepend(list: *Self, item: T) !*Node {
            if (list.first) |first| {
                // Insert before first.
                return list.insertBefore(first, item);
            } 
            else 
            {
                const new_node: *Node = try list.get_free_node(item);

                // Empty list.
                list.first = new_node;
                list.last = new_node;
                new_node.prev = null;
                new_node.next = null;

                list.len = 1;

                return new_node;
            }
        }

        pub fn get_free_node(list: *Self, item: T) !*Node
        {
            if (list.emptyIndices.items.len > 0)
            {
                const index: usize = list.emptyIndices.pop();
                list.unlinkedList.items[index].data = item;
                list.unlinkedList.items[index].index = index;
                return &list.unlinkedList.items[index];
            }
            else
            {
                var new_node_instance: Node = Node
                {
                    .data = item,
                    .index = list.unlinkedList.items.len,
                    .prev = null,
                    .next = null
                };
                try list.unlinkedList.append(new_node_instance);
                return &list.unlinkedList.items[list.unlinkedList.items.len - 1];
            }
        }

        /// Remove a node from the list.
        ///
        /// Arguments:
        ///     node: Pointer to the node to be removed.
        pub fn remove(list: *Self, node: *Node) !void 
        {
            if (node.prev) |prev_node| {
                // Intermediate node.
                prev_node.next = node.next;
            } else {
                // First element of the list.
                list.first = node.next;
            }

            if (node.next) |next_node| {
                // Intermediate node.
                next_node.prev = node.prev;
            } else {
                // Last element of the list.
                list.last = node.prev;
            }

            list.len -= 1;

            try list.emptyIndices.append(node.index);
            std.debug.assert(list.len == 0 or (list.first != null and list.last != null));
        }

        /// Remove and return the last node in the list.
        ///
        /// Returns:
        ///     A pointer to the last node in the list.
        pub fn pop(list: *Self) !?T 
        {
            if (list.last == null)
            {
                return null;
            }
            const last = list.last.?.data;
            try list.remove(list.last.?);
            return last;
        }

        /// Remove and return the first node in the list.
        ///
        /// Returns:
        ///     A pointer to the first node in the list.
        pub fn popFirst(list: *Self) !?T {
            if (list.first == null)
            {
                return null;
            }
            const first = list.first.?.data;
            try list.remove(list.first.?);
            return first;
        }
    };
}

// test "basic DoublyLinkedList test" {
//     const L = LinkedList(u32);
//     var list = L.init(std.testing.allocator);

//     var two = try list.append(2); // {2}
//     var five = try list.append(5); // {2, 5}
//     _ = try list.prepend(1); // {1, 2, 5}
//     _ = try list.insertBefore(five, 4); // {1, 2, 4, 5}
//     var three = try list.insertAfter(two, 3); // {1, 2, 3, 4, 5}

//     // Traverse forwards.
//     {
//         var it = list.first;
//         var index: u32 = 1;
//         while (it) |node| : (it = node.next) {
//             try std.testing.expect(node.data == index);
//             index += 1;
//         }
//     }

//     // Traverse backwards.
//     {
//         var it = list.last;
//         var index: u32 = 1;
//         while (it) |node| : (it = node.prev) {
//             try std.testing.expect(node.data == (6 - index));
//             index += 1;
//         }
//     }

//     _ = try list.popFirst(); // {2, 3, 4, 5}
//     _ = try list.pop(); // {2, 3, 4}
//     try list.remove(three); // {2, 4}

//     try std.testing.expect(list.first.?.data == 2);
//     try std.testing.expect(list.last.?.data == 4);
//     try std.testing.expect(list.len == 2);

//     list.deinit();
// }