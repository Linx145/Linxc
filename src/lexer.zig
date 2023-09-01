const std = @import("std");

pub const TokenID = union(enum) 
{
    Invalid,
    Eof,
    Nl,
    Identifier,

    /// special case for #include <...>
    MacroString,
    StringLiteral: StrKind,
    CharLiteral: StrKind,
    IntegerLiteral: NumSuffix,
    FloatLiteral: NumSuffix,
    Bang,
    BangEqual,
    Pipe,
    PipePipe,
    PipeEqual,
    Equal,
    EqualEqual,
    LParen,
    RParen,
    LBrace,
    RBrace,
    LBracket,
    RBracket,
    Period,
    Ellipsis,
    Caret,
    CaretEqual,
    Plus,
    PlusPlus,
    PlusEqual,
    Minus,
    MinusMinus,
    MinusEqual,
    Asterisk,
    AsteriskEqual,
    Percent,
    PercentEqual,
    Arrow,
    Colon,
    ColonColon,
    Semicolon,
    Slash,
    SlashEqual,
    Comma,
    Ampersand,
    AmpersandAmpersand,
    AmpersandEqual,
    QuestionMark,
    AngleBracketLeft,
    AngleBracketLeftEqual,
    AngleBracketAngleBracketLeft,
    AngleBracketAngleBracketLeftEqual,
    AngleBracketRight,
    AngleBracketRightEqual,
    AngleBracketAngleBracketRight,
    AngleBracketAngleBracketRightEqual,
    Tilde,
    LineComment,
    MultiLineComment,
    Hash,
    HashHash,

    Keyword_true,
    Keyword_false,
    Keyword_auto,
    Keyword_break,
    Keyword_case,
    Keyword_char,
    Keyword_const,
    Keyword_continue,
    Keyword_default,
    Keyword_do,
    Keyword_double,
    Keyword_else,
    Keyword_enum,
    Keyword_extern,
    Keyword_float,
    Keyword_for,
    Keyword_goto,
    Keyword_if,
    Keyword_int,
    Keyword_long,
    Keyword_register,
    Keyword_return,
    Keyword_short,
    Keyword_signed,
    Keyword_sizeof,
    Keyword_static,
    Keyword_struct,
    Keyword_switch,
    Keyword_typedef,
    Keyword_union,
    Keyword_ushort,
    Keyword_uint,
    Keyword_ulong,
    Keyword_void,
    Keyword_volatile,
    Keyword_while,

    // ISO C99
    Keyword_bool,
    Keyword_complex,
    Keyword_imaginary,
    Keyword_inline,
    Keyword_restrict,

    // ISO C11
    Keyword_alignas,
    Keyword_alignof,
    Keyword_atomic,
    Keyword_noreturn,
    Keyword_thread_local,

    // Preprocessor directives
    Keyword_include,
    Keyword_define,
    Keyword_ifdef,
    Keyword_ifndef,
    Keyword_error,
    Keyword_pragma,

    pub fn symbol(id: TokenID) []const u8 
    {
        return symbolName(id);
    }

    pub fn symbolName(id: std.meta.Tag(TokenID)) []const u8 
    {
        return switch (id) 
        {
            .Invalid => "Invalid",
            .Eof => "Eof",
            .Nl => "NewLine",
            .Identifier => "Identifier",
            .MacroString => "MacroString",
            .StringLiteral => "StringLiteral",
            .CharLiteral => "CharLiteral",
            .IntegerLiteral => "IntegerLiteral",
            .FloatLiteral => "FloatLiteral",
            .LineComment => "LineComment",
            .MultiLineComment => "MultiLineComment",

            .Bang => "!",
            .BangEqual => "!=",
            .Pipe => "|",
            .PipePipe => "||",
            .PipeEqual => "|=",
            .Equal => "=",
            .EqualEqual => "==",
            .LParen => "(",
            .RParen => ")",
            .LBrace => "{",
            .RBrace => "}",
            .LBracket => "[",
            .RBracket => "]",
            .Period => ".",
            .Ellipsis => "...",
            .Caret => "^",
            .CaretEqual => "^=",
            .Plus => "+",
            .PlusPlus => "++",
            .PlusEqual => "+=",
            .Minus => "-",
            .MinusMinus => "--",
            .MinusEqual => "-=",
            .Asterisk => "*",
            .AsteriskEqual => "*=",
            .Percent => "%",
            .PercentEqual => "%=",
            .Arrow => "->",
            .Colon => ":",
            .ColonColon => "::",
            .Semicolon => ";",
            .Slash => "/",
            .SlashEqual => "/=",
            .Comma => ",",
            .Ampersand => "&",
            .AmpersandAmpersand => "&&",
            .AmpersandEqual => "&=",
            .QuestionMark => "?",
            .AngleBracketLeft => "<",
            .AngleBracketLeftEqual => "<=",
            .AngleBracketAngleBracketLeft => "<<",
            .AngleBracketAngleBracketLeftEqual => "<<=",
            .AngleBracketRight => ">",
            .AngleBracketRightEqual => ">=",
            .AngleBracketAngleBracketRight => ">>",
            .AngleBracketAngleBracketRightEqual => ">>=",
            .Tilde => "~",
            .Hash => "#",
            .HashHash => "##",
            .Keyword_true => "true",
            .Keyword_false => "false",
            .Keyword_auto => "auto",
            .Keyword_break => "break",
            .Keyword_case => "case",
            .Keyword_char => "char",
            .Keyword_const => "const",
            .Keyword_continue => "continue",
            .Keyword_default => "default",
            .Keyword_do => "do",
            .Keyword_double => "double",
            .Keyword_else => "else",
            .Keyword_enum => "enum",
            .Keyword_extern => "extern",
            .Keyword_float => "float",
            .Keyword_for => "for",
            .Keyword_goto => "goto",
            .Keyword_if => "if",
            .Keyword_int => "int",
            .Keyword_long => "long",
            .Keyword_register => "register",
            .Keyword_return => "return",
            .Keyword_short => "short",
            .Keyword_signed => "signed",
            .Keyword_sizeof => "sizeof",
            .Keyword_static => "static",
            .Keyword_struct => "struct",
            .Keyword_switch => "switch",
            .Keyword_typedef => "typedef",
            .Keyword_union => "union",
            .Keyword_void => "void",
            .Keyword_volatile => "volatile",
            .Keyword_while => "while",
            .Keyword_bool => "bool",
            .Keyword_complex => "complex",
            .Keyword_imaginary => "imaginary",
            .Keyword_inline => "inline",
            .Keyword_restrict => "restrict",
            .Keyword_alignas => "alignas",
            .Keyword_alignof => "alignof",
            .Keyword_atomic => "atomic",
            .Keyword_noreturn => "noreturn",
            .Keyword_thread_local => "threadlocal",
            .Keyword_include => "include",
            .Keyword_define => "define",
            .Keyword_ifdef => "ifdef",
            .Keyword_ifndef => "ifndef",
            .Keyword_error => "error",
            .Keyword_pragma => "pragma",
            .Keyword_ulong => "ulong",
            .Keyword_ushort => "ushort",
            .Keyword_uint => "uint"
        };
    }

    pub const keywords = std.ComptimeStringMap(TokenID, .{
        .{ "true", .Keyword_true },
        .{ "false", .Keyword_false },
        .{ "auto", .Keyword_auto },
        .{ "break", .Keyword_break },
        .{ "case", .Keyword_case },
        .{ "char", .Keyword_char },
        .{ "const", .Keyword_const },
        .{ "continue", .Keyword_continue },
        .{ "default", .Keyword_default },
        .{ "do", .Keyword_do },
        .{ "double", .Keyword_double },
        .{ "else", .Keyword_else },
        .{ "enum", .Keyword_enum },
        .{ "extern", .Keyword_extern },
        .{ "float", .Keyword_float },
        .{ "for", .Keyword_for },
        .{ "goto", .Keyword_goto },
        .{ "if", .Keyword_if },
        .{ "int", .Keyword_int },
        .{ "long", .Keyword_long },
        .{ "register", .Keyword_register },
        .{ "return", .Keyword_return },
        .{ "short", .Keyword_short },
        .{ "signed", .Keyword_signed },
        .{ "sizeof", .Keyword_sizeof },
        .{ "static", .Keyword_static },
        .{ "struct", .Keyword_struct },
        .{ "switch", .Keyword_switch },
        .{ "typedef", .Keyword_typedef },
        .{ "union", .Keyword_union },
        .{ "ushort", .Keyword_ushort },
        .{ "uint", .Keyword_uint },
        .{ "ulong", .Keyword_ulong },
        .{ "void", .Keyword_void },
        .{ "volatile", .Keyword_volatile },
        .{ "while", .Keyword_while },

        // ISO C99
        .{ "bool", .Keyword_bool },
        .{ "complex", .Keyword_complex },
        .{ "imaginary", .Keyword_imaginary },
        .{ "inline", .Keyword_inline },
        .{ "restrict", .Keyword_restrict },

        // ISO C11
        .{ "alignas", .Keyword_alignas },
        .{ "alignof", .Keyword_alignof },
        .{ "atomic", .Keyword_atomic },
        .{ "noreturn", .Keyword_noreturn },
        .{ "threadlocal", .Keyword_thread_local },

        // Preprocessor directives
        .{ "include", .Keyword_include },
        .{ "define", .Keyword_define },
        .{ "ifdef", .Keyword_ifdef },
        .{ "ifndef", .Keyword_ifndef },
        .{ "error", .Keyword_error },
        .{ "pragma", .Keyword_pragma },
    });

    // TODO do this in the preprocessor
    pub fn getKeyword(bytes: []const u8, pp_directive: bool) ?TokenID {
        if (keywords.get(bytes)) |id| {
            switch (id) {
                .Keyword_include,
                .Keyword_define,
                .Keyword_ifdef,
                .Keyword_ifndef,
                .Keyword_error,
                .Keyword_pragma,
                => if (!pp_directive) return null,
                else => {},
            }
            return id;
        }
        return null;
    }

    pub const NumSuffix = enum 
    {
        none,
        f,
        l,
        u,
        lu,
        ll,
        llu,
    };

    pub const StrKind = enum 
    {
        none,
        wide,
        utf_8,
        utf_16,
        utf_32,
    };
};
pub const Token = struct {
    id: TokenID,
    start: usize,
    end: usize,
};
pub const Tokenizer = struct {
    buffer: []const u8,
    index: usize = 0,
    prev_tok_id: std.meta.Tag(TokenID) = .Invalid,
    pp_directive: bool = false,

    pub fn next(self: *@This()) Token 
    {
        var result = Token
        {
            .id = .Eof,
            .start = self.index,
            .end = undefined,
        };

        var state: enum 
        {
            Start,
            Cr,
            BackSlash,
            BackSlashCr,
            u,
            u8,
            U,
            L,
            StringLiteral,
            CharLiteralStart,
            CharLiteral,
            EscapeSequence,
            CrEscape,
            OctalEscape,
            HexEscape,
            UnicodeEscape,
            Identifier,
            Colon,
            Equal,
            Bang,
            Pipe,
            Percent,
            Asterisk,
            Plus,

            /// special case for #include <...>
            MacroString,
            AngleBracketLeft,
            AngleBracketAngleBracketLeft,
            AngleBracketRight,
            AngleBracketAngleBracketRight,
            Caret,
            Period,
            Period2,
            Minus,
            Slash,
            Ampersand,
            Hash,
            LineComment,
            MultiLineComment,
            MultiLineCommentAsterisk,
            Zero,
            IntegerLiteralOct,
            IntegerLiteralBinary,
            IntegerLiteralBinaryFirst,
            IntegerLiteralHex,
            IntegerLiteralHexFirst,
            IntegerLiteral,
            IntegerSuffix,
            IntegerSuffixU,
            IntegerSuffixL,
            IntegerSuffixLL,
            IntegerSuffixUL,
            FloatFraction,
            FloatFractionHex,
            FloatExponent,
            FloatExponentDigits,
            FloatSuffix,
        } = .Start;
        var string = false;
        var counter: u32 = 0;
        while (self.index < self.buffer.len) : (self.index += 1) 
        {
            const c = self.buffer[self.index];
            switch (state) 
            {
                .Start => switch (c) 
                {
                    '\n' => {
                        self.pp_directive = false;
                        result.id = .Nl;
                        self.index += 1;
                        break;
                    },
                    '\r' => {
                        state = .Cr;
                    },
                    '"' => {
                        result.id = .{ .StringLiteral = .none };
                        state = .StringLiteral;
                    },
                    '\'' => {
                        result.id = .{ .CharLiteral = .none };
                        state = .CharLiteralStart;
                    },
                    'u' => {
                        state = .u;
                    },
                    'U' => {
                        state = .U;
                    },
                    'L' => {
                        state = .L;
                    },
                    'a'...'t', 'v'...'z', 'A'...'K', 'M'...'T', 'V'...'Z', '_', '$' => {
                        state = .Identifier;
                    },
                    '=' => {
                        state = .Equal;
                    },
                    '!' => {
                        state = .Bang;
                    },
                    '|' => {
                        state = .Pipe;
                    },
                    '(' => {
                        result.id = .LParen;
                        self.index += 1;
                        break;
                    },
                    ')' => {
                        result.id = .RParen;
                        self.index += 1;
                        break;
                    },
                    '[' => {
                        result.id = .LBracket;
                        self.index += 1;
                        break;
                    },
                    ']' => {
                        result.id = .RBracket;
                        self.index += 1;
                        break;
                    },
                    ';' => {
                        result.id = .Semicolon;
                        self.index += 1;
                        break;
                    },
                    ',' => {
                        result.id = .Comma;
                        self.index += 1;
                        break;
                    },
                    '?' => {
                        result.id = .QuestionMark;
                        self.index += 1;
                        break;
                    },
                    ':' => {
                        state = .Colon;
                    },
                    '%' => {
                        state = .Percent;
                    },
                    '*' => {
                        state = .Asterisk;
                    },
                    '+' => {
                        state = .Plus;
                    },
                    '<' => {
                        if (self.prev_tok_id == .Keyword_include)
                            state = .MacroString
                        else
                            state = .AngleBracketLeft;
                    },
                    '>' => {
                        state = .AngleBracketRight;
                    },
                    '^' => {
                        state = .Caret;
                    },
                    '{' => {
                        result.id = .LBrace;
                        self.index += 1;
                        break;
                    },
                    '}' => {
                        result.id = .RBrace;
                        self.index += 1;
                        break;
                    },
                    '~' => {
                        result.id = .Tilde;
                        self.index += 1;
                        break;
                    },
                    '.' => {
                        state = .Period;
                    },
                    '-' => {
                        state = .Minus;
                    },
                    '/' => {
                        state = .Slash;
                    },
                    '&' => {
                        state = .Ampersand;
                    },
                    '#' => {
                        state = .Hash;
                    },
                    '0' => {
                        state = .Zero;
                    },
                    '1'...'9' => {
                        state = .IntegerLiteral;
                    },
                    '\\' => {
                        state = .BackSlash;
                    },
                    '\t', '\x0B', '\x0C', ' ' => {
                        result.start = self.index + 1;
                    },
                    else => {
                        // TODO handle invalid bytes better
                        result.id = .Invalid;
                        self.index += 1;
                        break;
                    },
                },
                .Cr => switch (c) {
                    '\n' => {
                        self.pp_directive = false;
                        result.id = .Nl;
                        self.index += 1;
                        break;
                    },
                    else => {
                        result.id = .Invalid;
                        break;
                    },
                },
                .BackSlash => switch (c) {
                    '\n' => {
                        result.start = self.index + 1;
                        state = .Start;
                    },
                    '\r' => {
                        state = .BackSlashCr;
                    },
                    '\t', '\x0B', '\x0C', ' ' => {
                        // TODO warn
                    },
                    else => {
                        result.id = .Invalid;
                        break;
                    },
                },
                .BackSlashCr => switch (c) {
                    '\n' => {
                        result.start = self.index + 1;
                        state = .Start;
                    },
                    else => {
                        result.id = .Invalid;
                        break;
                    },
                },
                .u => switch (c) {
                    '8' => {
                        state = .u8;
                    },
                    '\'' => {
                        result.id = .{ .CharLiteral = .utf_16 };
                        state = .CharLiteralStart;
                    },
                    '\"' => {
                        result.id = .{ .StringLiteral = .utf_16 };
                        state = .StringLiteral;
                    },
                    else => {
                        self.index -= 1;
                        state = .Identifier;
                    },
                },
                .u8 => switch (c) {
                    '\"' => {
                        result.id = .{ .StringLiteral = .utf_8 };
                        state = .StringLiteral;
                    },
                    else => {
                        self.index -= 1;
                        state = .Identifier;
                    },
                },
                .U => switch (c) {
                    '\'' => {
                        result.id = .{ .CharLiteral = .utf_32 };
                        state = .CharLiteralStart;
                    },
                    '\"' => {
                        result.id = .{ .StringLiteral = .utf_32 };
                        state = .StringLiteral;
                    },
                    else => {
                        self.index -= 1;
                        state = .Identifier;
                    },
                },
                .L => switch (c) {
                    '\'' => {
                        result.id = .{ .CharLiteral = .wide };
                        state = .CharLiteralStart;
                    },
                    '\"' => {
                        result.id = .{ .StringLiteral = .wide };
                        state = .StringLiteral;
                    },
                    else => {
                        self.index -= 1;
                        state = .Identifier;
                    },
                },
                .StringLiteral => switch (c) {
                    '\\' => {
                        string = true;
                        state = .EscapeSequence;
                    },
                    '"' => {
                        self.index += 1;
                        break;
                    },
                    '\n', '\r' => {
                        result.id = .Invalid;
                        break;
                    },
                    else => {},
                },
                .CharLiteralStart => switch (c) {
                    '\\' => {
                        string = false;
                        state = .EscapeSequence;
                    },
                    '\'', '\n' => {
                        result.id = .Invalid;
                        break;
                    },
                    else => {
                        state = .CharLiteral;
                    },
                },
                .CharLiteral => switch (c) {
                    '\\' => {
                        string = false;
                        state = .EscapeSequence;
                    },
                    '\'' => {
                        self.index += 1;
                        break;
                    },
                    '\n' => {
                        result.id = .Invalid;
                        break;
                    },
                    else => {},
                },
                .EscapeSequence => switch (c) {
                    '\'', '"', '?', '\\', 'a', 'b', 'f', 'n', 'r', 't', 'v', '\n' => {
                        state = if (string) .StringLiteral else .CharLiteral;
                    },
                    '\r' => {
                        state = .CrEscape;
                    },
                    '0'...'7' => {
                        counter = 1;
                        state = .OctalEscape;
                    },
                    'x' => {
                        state = .HexEscape;
                    },
                    'u' => {
                        counter = 4;
                        state = .OctalEscape;
                    },
                    'U' => {
                        counter = 8;
                        state = .OctalEscape;
                    },
                    else => {
                        result.id = .Invalid;
                        break;
                    },
                },
                .CrEscape => switch (c) {
                    '\n' => {
                        state = if (string) .StringLiteral else .CharLiteral;
                    },
                    else => {
                        result.id = .Invalid;
                        break;
                    },
                },
                .OctalEscape => switch (c) {
                    '0'...'7' => {
                        counter += 1;
                        if (counter == 3) {
                            state = if (string) .StringLiteral else .CharLiteral;
                        }
                    },
                    else => {
                        self.index -= 1;
                        state = if (string) .StringLiteral else .CharLiteral;
                    },
                },
                .HexEscape => switch (c) {
                    '0'...'9', 'a'...'f', 'A'...'F' => {},
                    else => {
                        self.index -= 1;
                        state = if (string) .StringLiteral else .CharLiteral;
                    },
                },
                .UnicodeEscape => switch (c) {
                    '0'...'9', 'a'...'f', 'A'...'F' => {
                        counter -= 1;
                        if (counter == 0) {
                            state = if (string) .StringLiteral else .CharLiteral;
                        }
                    },
                    else => {
                        if (counter != 0) {
                            result.id = .Invalid;
                            break;
                        }
                        self.index -= 1;
                        state = if (string) .StringLiteral else .CharLiteral;
                    },
                },
                .Identifier => switch (c) {
                    'a'...'z', 'A'...'Z', '_', '0'...'9', '$' => {},
                    else => {
                        result.id = TokenID.getKeyword(self.buffer[result.start..self.index], self.prev_tok_id == .Hash and !self.pp_directive) orelse .Identifier;
                        if (self.prev_tok_id == .Hash)
                            self.pp_directive = true;
                        break;
                    },
                },
                .Colon => switch (c) {
                    ':' => {
                        result.id = .ColonColon;
                        self.index += 1;
                        break;
                    },
                    else => {
                        result.id = .Colon;
                        break;
                    }
                },
                .Equal => switch (c) {
                    '=' => {
                        result.id = .EqualEqual;
                        self.index += 1;
                        break;
                    },
                    else => {
                        result.id = .Equal;
                        break;
                    },
                },
                .Bang => switch (c) {
                    '=' => {
                        result.id = .BangEqual;
                        self.index += 1;
                        break;
                    },
                    else => {
                        result.id = .Bang;
                        break;
                    },
                },
                .Pipe => switch (c) {
                    '=' => {
                        result.id = .PipeEqual;
                        self.index += 1;
                        break;
                    },
                    '|' => {
                        result.id = .PipePipe;
                        self.index += 1;
                        break;
                    },
                    else => {
                        result.id = .Pipe;
                        break;
                    },
                },
                .Percent => switch (c) {
                    '=' => {
                        result.id = .PercentEqual;
                        self.index += 1;
                        break;
                    },
                    else => {
                        result.id = .Percent;
                        break;
                    },
                },
                .Asterisk => switch (c) {
                    '=' => {
                        result.id = .AsteriskEqual;
                        self.index += 1;
                        break;
                    },
                    else => {
                        result.id = .Asterisk;
                        break;
                    },
                },
                .Plus => switch (c) {
                    '=' => {
                        result.id = .PlusEqual;
                        self.index += 1;
                        break;
                    },
                    '+' => {
                        result.id = .PlusPlus;
                        self.index += 1;
                        break;
                    },
                    else => {
                        result.id = .Plus;
                        break;
                    },
                },
                .MacroString => switch (c) {
                    '>' => {
                        result.id = .MacroString;
                        self.index += 1;
                        break;
                    },
                    else => {},
                },
                .AngleBracketLeft => switch (c) {
                    '<' => {
                        state = .AngleBracketAngleBracketLeft;
                    },
                    '=' => {
                        result.id = .AngleBracketLeftEqual;
                        self.index += 1;
                        break;
                    },
                    else => {
                        result.id = .AngleBracketLeft;
                        break;
                    },
                },
                .AngleBracketAngleBracketLeft => switch (c) {
                    '=' => {
                        result.id = .AngleBracketAngleBracketLeftEqual;
                        self.index += 1;
                        break;
                    },
                    else => {
                        result.id = .AngleBracketAngleBracketLeft;
                        break;
                    },
                },
                .AngleBracketRight => switch (c) {
                    '>' => {
                        state = .AngleBracketAngleBracketRight;
                    },
                    '=' => {
                        result.id = .AngleBracketRightEqual;
                        self.index += 1;
                        break;
                    },
                    else => {
                        result.id = .AngleBracketRight;
                        break;
                    },
                },
                .AngleBracketAngleBracketRight => switch (c) {
                    '=' => {
                        result.id = .AngleBracketAngleBracketRightEqual;
                        self.index += 1;
                        break;
                    },
                    else => {
                        result.id = .AngleBracketAngleBracketRight;
                        break;
                    },
                },
                .Caret => switch (c) {
                    '=' => {
                        result.id = .CaretEqual;
                        self.index += 1;
                        break;
                    },
                    else => {
                        result.id = .Caret;
                        break;
                    },
                },
                .Period => switch (c) {
                    '.' => {
                        state = .Period2;
                    },
                    '0'...'9' => {
                        state = .FloatFraction;
                    },
                    else => {
                        result.id = .Period;
                        break;
                    },
                },
                .Period2 => switch (c) {
                    '.' => {
                        result.id = .Ellipsis;
                        self.index += 1;
                        break;
                    },
                    else => {
                        result.id = .Period;
                        self.index -= 1;
                        break;
                    },
                },
                .Minus => switch (c) {
                    '>' => {
                        result.id = .Arrow;
                        self.index += 1;
                        break;
                    },
                    '=' => {
                        result.id = .MinusEqual;
                        self.index += 1;
                        break;
                    },
                    '-' => {
                        result.id = .MinusMinus;
                        self.index += 1;
                        break;
                    },
                    else => {
                        result.id = .Minus;
                        break;
                    },
                },
                .Slash => switch (c) {
                    '/' => {
                        state = .LineComment;
                    },
                    '*' => {
                        state = .MultiLineComment;
                    },
                    '=' => {
                        result.id = .SlashEqual;
                        self.index += 1;
                        break;
                    },
                    else => {
                        result.id = .Slash;
                        break;
                    },
                },
                .Ampersand => switch (c) {
                    '&' => {
                        result.id = .AmpersandAmpersand;
                        self.index += 1;
                        break;
                    },
                    '=' => {
                        result.id = .AmpersandEqual;
                        self.index += 1;
                        break;
                    },
                    else => {
                        result.id = .Ampersand;
                        break;
                    },
                },
                .Hash => switch (c) {
                    '#' => {
                        result.id = .HashHash;
                        self.index += 1;
                        break;
                    },
                    else => {
                        result.id = .Hash;
                        break;
                    },
                },
                .LineComment => switch (c) {
                    '\n' => {
                        result.id = .LineComment;
                        break;
                    },
                    else => {},
                },
                .MultiLineComment => switch (c) {
                    '*' => {
                        state = .MultiLineCommentAsterisk;
                    },
                    else => {},
                },
                .MultiLineCommentAsterisk => switch (c) {
                    '/' => {
                        result.id = .MultiLineComment;
                        self.index += 1;
                        break;
                    },
                    else => {
                        state = .MultiLineComment;
                    },
                },
                .Zero => switch (c) {
                    '0'...'9' => {
                        state = .IntegerLiteralOct;
                    },
                    'b', 'B' => {
                        state = .IntegerLiteralBinaryFirst;
                    },
                    'x', 'X' => {
                        state = .IntegerLiteralHexFirst;
                    },
                    '.' => {
                        state = .FloatFraction;
                    },
                    else => {
                        state = .IntegerSuffix;
                        self.index -= 1;
                    },
                },
                .IntegerLiteralOct => switch (c) {
                    '0'...'7' => {},
                    else => {
                        state = .IntegerSuffix;
                        self.index -= 1;
                    },
                },
                .IntegerLiteralBinaryFirst => switch (c) {
                    '0'...'7' => state = .IntegerLiteralBinary,
                    else => {
                        result.id = .Invalid;
                        break;
                    },
                },
                .IntegerLiteralBinary => switch (c) {
                    '0', '1' => {},
                    else => {
                        state = .IntegerSuffix;
                        self.index -= 1;
                    },
                },
                .IntegerLiteralHexFirst => switch (c) {
                    '0'...'9', 'a'...'f', 'A'...'F' => state = .IntegerLiteralHex,
                    '.' => {
                        state = .FloatFractionHex;
                    },
                    'p', 'P' => {
                        state = .FloatExponent;
                    },
                    else => {
                        result.id = .Invalid;
                        break;
                    },
                },
                .IntegerLiteralHex => switch (c) {
                    '0'...'9', 'a'...'f', 'A'...'F' => {},
                    '.' => {
                        state = .FloatFractionHex;
                    },
                    'p', 'P' => {
                        state = .FloatExponent;
                    },
                    else => {
                        state = .IntegerSuffix;
                        self.index -= 1;
                    },
                },
                .IntegerLiteral => switch (c) {
                    '0'...'9' => {},
                    '.' => {
                        state = .FloatFraction;
                    },
                    'e', 'E' => {
                        state = .FloatExponent;
                    },
                    else => {
                        state = .IntegerSuffix;
                        self.index -= 1;
                    },
                },
                .IntegerSuffix => switch (c) {
                    'u', 'U' => {
                        state = .IntegerSuffixU;
                    },
                    'l', 'L' => {
                        state = .IntegerSuffixL;
                    },
                    else => {
                        result.id = .{ .IntegerLiteral = .none };
                        break;
                    },
                },
                .IntegerSuffixU => switch (c) {
                    'l', 'L' => {
                        state = .IntegerSuffixUL;
                    },
                    else => {
                        result.id = .{ .IntegerLiteral = .u };
                        break;
                    },
                },
                .IntegerSuffixL => switch (c) {
                    'l', 'L' => {
                        state = .IntegerSuffixLL;
                    },
                    'u', 'U' => {
                        result.id = .{ .IntegerLiteral = .lu };
                        self.index += 1;
                        break;
                    },
                    else => {
                        result.id = .{ .IntegerLiteral = .l };
                        break;
                    },
                },
                .IntegerSuffixLL => switch (c) {
                    'u', 'U' => {
                        result.id = .{ .IntegerLiteral = .llu };
                        self.index += 1;
                        break;
                    },
                    else => {
                        result.id = .{ .IntegerLiteral = .ll };
                        break;
                    },
                },
                .IntegerSuffixUL => switch (c) {
                    'l', 'L' => {
                        result.id = .{ .IntegerLiteral = .llu };
                        self.index += 1;
                        break;
                    },
                    else => {
                        result.id = .{ .IntegerLiteral = .lu };
                        break;
                    },
                },
                .FloatFraction => switch (c) {
                    '0'...'9' => {},
                    'e', 'E' => {
                        state = .FloatExponent;
                    },
                    else => {
                        self.index -= 1;
                        state = .FloatSuffix;
                    },
                },
                .FloatFractionHex => switch (c) {
                    '0'...'9', 'a'...'f', 'A'...'F' => {},
                    'p', 'P' => {
                        state = .FloatExponent;
                    },
                    else => {
                        result.id = .Invalid;
                        break;
                    },
                },
                .FloatExponent => switch (c) {
                    '+', '-' => {
                        state = .FloatExponentDigits;
                    },
                    else => {
                        self.index -= 1;
                        state = .FloatExponentDigits;
                    },
                },
                .FloatExponentDigits => switch (c) {
                    '0'...'9' => {
                        counter += 1;
                    },
                    else => {
                        if (counter == 0) {
                            result.id = .Invalid;
                            break;
                        }
                        self.index -= 1;
                        state = .FloatSuffix;
                    },
                },
                .FloatSuffix => switch (c) {
                    'l', 'L' => {
                        result.id = .{ .FloatLiteral = .l };
                        self.index += 1;
                        break;
                    },
                    'f', 'F' => {
                        result.id = .{ .FloatLiteral = .f };
                        self.index += 1;
                        break;
                    },
                    else => {
                        result.id = .{ .FloatLiteral = .none };
                        break;
                    },
                },
            }
        } else if (self.index == self.buffer.len) {
            switch (state) {
                .Start => {},
                .u, .u8, .U, .L, .Identifier => {
                    result.id = TokenID.getKeyword(self.buffer[result.start..self.index], self.prev_tok_id == .Hash and !self.pp_directive) orelse .Identifier;
                },

                .Cr,
                .BackSlash,
                .BackSlashCr,
                .Period2,
                .StringLiteral,
                .CharLiteralStart,
                .CharLiteral,
                .EscapeSequence,
                .CrEscape,
                .OctalEscape,
                .HexEscape,
                .UnicodeEscape,
                .MultiLineComment,
                .MultiLineCommentAsterisk,
                .FloatExponent,
                .MacroString,
                .IntegerLiteralBinaryFirst,
                .IntegerLiteralHexFirst,
                => result.id = .Invalid,

                .FloatExponentDigits => result.id = if (counter == 0) .Invalid else .{ .FloatLiteral = .none },

                .FloatFraction,
                .FloatFractionHex,
                => result.id = .{ .FloatLiteral = .none },

                .IntegerLiteralOct,
                .IntegerLiteralBinary,
                .IntegerLiteralHex,
                .IntegerLiteral,
                .IntegerSuffix,
                .Zero,
                => result.id = .{ .IntegerLiteral = .none },
                .IntegerSuffixU => result.id = .{ .IntegerLiteral = .u },
                .IntegerSuffixL => result.id = .{ .IntegerLiteral = .l },
                .IntegerSuffixLL => result.id = .{ .IntegerLiteral = .ll },
                .IntegerSuffixUL => result.id = .{ .IntegerLiteral = .lu },

                .FloatSuffix => result.id = .{ .FloatLiteral = .none },
                .Colon => result.id = .Colon,
                .Equal => result.id = .Equal,
                .Bang => result.id = .Bang,
                .Minus => result.id = .Minus,
                .Slash => result.id = .Slash,
                .Ampersand => result.id = .Ampersand,
                .Hash => result.id = .Hash,
                .Period => result.id = .Period,
                .Pipe => result.id = .Pipe,
                .AngleBracketAngleBracketRight => result.id = .AngleBracketAngleBracketRight,
                .AngleBracketRight => result.id = .AngleBracketRight,
                .AngleBracketAngleBracketLeft => result.id = .AngleBracketAngleBracketLeft,
                .AngleBracketLeft => result.id = .AngleBracketLeft,
                .Plus => result.id = .Plus,
                .Percent => result.id = .Percent,
                .Caret => result.id = .Caret,
                .Asterisk => result.id = .Asterisk,
                .LineComment => result.id = .LineComment,
            }
        }

        self.prev_tok_id = result.id;
        result.end = self.index;
        return result;
    }
};