#pragma once

#include <Linxc.h>
#include <stdbool.h>
#include <string.hpp>
#include <hashmap.linxc>

typedef struct LinxcToken LinxcToken;
typedef struct LinxcTokenizer LinxcTokenizer;

enum LinxcTokenID
{
    Linxc_Invalid,
    Linxc_Eof,
    Linxc_Nl,
    Linxc_Identifier,

    /// special case for #include <...>
    Linxc_MacroString,
    Linxc_StringLiteral,
    Linxc_CharLiteral,
    Linxc_IntegerLiteral,
    Linxc_FloatLiteral,
    Linxc_Bang,
    Linxc_BangEqual,
    Linxc_Pipe,
    Linxc_PipePipe,
    Linxc_PipeEqual,
    Linxc_Equal,
    Linxc_EqualEqual,
    //(
    Linxc_LParen,
    //)
    Linxc_RParen,
    //{
    Linxc_LBrace,
    //}
    Linxc_RBrace,
    //[
    Linxc_LBracket,
    //]
    Linxc_RBracket,
    Linxc_Period,
    Linxc_Ellipsis,
    Linxc_Caret,
    Linxc_CaretEqual,
    Linxc_Plus,
    Linxc_PlusPlus,
    Linxc_PlusEqual,
    Linxc_Minus,
    Linxc_MinusMinus,
    Linxc_MinusEqual,
    Linxc_Asterisk,
    Linxc_AsteriskEqual,
    Linxc_Percent,
    Linxc_PercentEqual,
    Linxc_Arrow,
    Linxc_Colon,
    Linxc_ColonColon,
    Linxc_Semicolon,
    Linxc_Slash,
    Linxc_SlashEqual,
    Linxc_Comma,
    Linxc_Ampersand,
    Linxc_AmpersandAmpersand,
    Linxc_AmpersandEqual,
    Linxc_QuestionMark,
    Linxc_AngleBracketLeft,
    Linxc_AngleBracketLeftEqual,
    Linxc_AngleBracketAngleBracketLeft,
    Linxc_AngleBracketAngleBracketLeftEqual,
    Linxc_AngleBracketRight,
    Linxc_AngleBracketRightEqual,
    Linxc_AngleBracketAngleBracketRight,
    Linxc_AngleBracketAngleBracketRightEqual,
    Linxc_Tilde,
    Linxc_LineComment,
    Linxc_MultiLineComment,
    Linxc_Hash,
    Linxc_HashHash,

    Linxc_Keyword_true,
    Linxc_Keyword_false,
    Linxc_Keyword_auto,
    Linxc_Keyword_break,
    Linxc_Keyword_case,
    Linxc_Keyword_char,
    Linxc_Keyword_const,
    Linxc_Keyword_continue,
    Linxc_Keyword_default,
    Linxc_Keyword_do,
    Linxc_Keyword_double,
    Linxc_Keyword_else,
    Linxc_Keyword_enum,
    Linxc_Keyword_extern,
    Linxc_Keyword_float,
    Linxc_Keyword_for,
    Linxc_Keyword_goto,
    Linxc_Keyword_if,
    Linxc_Keyword_int,
    Linxc_Keyword_long,
    Linxc_Keyword_register,
    Linxc_Keyword_return,
    Linxc_Keyword_short,
    Linxc_Keyword_signed,
    Linxc_Keyword_static,
    Linxc_Keyword_struct,
    Linxc_Keyword_sizeof,
    Linxc_Keyword_typeof,
    Linxc_Keyword_nameof,

    Linxc_keyword_attribute,
    Linxc_Keyword_trait,

    Linxc_Keyword_switch,
    Linxc_Keyword_typedef,
    Linxc_Keyword_union,
    Linxc_Keyword_template,
    Linxc_Keyword_typename,
    Linxc_Keyword_u8,
    Linxc_Keyword_u16,
    Linxc_Keyword_u32,
    Linxc_Keyword_u64,
    Linxc_Keyword_i8,
    Linxc_Keyword_i16,
    Linxc_Keyword_i32,
    Linxc_Keyword_i64,
    Linxc_Keyword_void,
    Linxc_Keyword_volatile,
    Linxc_Keyword_while,
    Linxc_Keyword_delegate,
    Linxc_Keyword_namespace,

    // ISO C99
    Linxc_Keyword_bool,
    Linxc_Keyword_complex,
    Linxc_Keyword_imaginary,
    Linxc_Keyword_inline,
    Linxc_Keyword_restrict,

    // ISO C11
    Linxc_Keyword_alignas,
    Linxc_Keyword_alignof,
    Linxc_Keyword_atomic,
    Linxc_Keyword_noreturn,
    Linxc_Keyword_thread_local,

    // Preprocessor directives
    Linxc_Keyword_include,
    Linxc_Keyword_define,
    Linxc_Keyword_ifdef,
    Linxc_Keyword_ifndef,
    Linxc_Keyword_error,
    Linxc_Keyword_pragma
};

const char *LinxcTokenIDToString(LinxcTokenID ID);

enum LinxcTokenizerState
{
    Linxc_State_Start,
    Linxc_State_Cr,
    Linxc_State_BackSlash,
    Linxc_State_BackSlashCr,
    Linxc_State_u,
    Linxc_State_u8,
    Linxc_State_U,
    Linxc_State_L,
    Linxc_State_StringLiteral,
    Linxc_State_CharLiteralStart,
    Linxc_State_CharLiteral,
    Linxc_State_EscapeSequence,
    Linxc_State_CrEscape,
    Linxc_State_OctalEscape,
    Linxc_State_HexEscape,
    Linxc_State_UnicodeEscape,
    Linxc_State_Identifier,
    Linxc_State_Colon,
    Linxc_State_Equal,
    Linxc_State_Bang,
    Linxc_State_Pipe,
    Linxc_State_Percent,
    Linxc_State_Asterisk,
    Linxc_State_Plus,

    /// special case for #include <...>
    Linxc_State_MacroString,
    Linxc_State_AngleBracketLeft,
    Linxc_State_AngleBracketAngleBracketLeft,
    Linxc_State_AngleBracketRight,
    Linxc_State_AngleBracketAngleBracketRight,
    Linxc_State_Caret,
    Linxc_State_Period,
    Linxc_State_Period2,
    Linxc_State_Minus,
    Linxc_State_Slash,
    Linxc_State_Ampersand,
    Linxc_State_Hash,
    Linxc_State_LineComment,
    Linxc_State_MultiLineComment,
    Linxc_State_MultiLineCommentAsterisk,
    Linxc_State_Zero,
    Linxc_State_IntegerLiteralOct,
    Linxc_State_IntegerLiteralBinary,
    Linxc_State_IntegerLiteralBinaryFirst,
    Linxc_State_IntegerLiteralHex,
    Linxc_State_IntegerLiteralHexFirst,
    Linxc_State_IntegerLiteral,
    Linxc_State_IntegerSuffix,
    Linxc_State_IntegerSuffixU,
    Linxc_State_IntegerSuffixL,
    Linxc_State_IntegerSuffixLL,
    Linxc_State_IntegerSuffixUL,
    Linxc_State_FloatFraction,
    Linxc_State_FloatFractionHex,
    Linxc_State_FloatExponent,
    Linxc_State_FloatExponentDigits,
    Linxc_State_FloatSuffix,
};

struct LinxcToken
{
    LinxcTokenizer *tokenizer;
    LinxcTokenID ID;
    u32 start;
    u32 end;

    string ToString(IAllocator *allocator);
};

struct LinxcTokenizer
{
    const char *buffer;
    i32 bufferLength;
    usize index;
    usize prevIndex;
    LinxcTokenID prevTokenID;
    bool preprocessorDirective;
    usize currentLine;
    usize lineStartIndex;
    usize currentToken;

    collections::hashmap<string, LinxcTokenID>* nameToToken;
    collections::vector<LinxcToken> tokenStream;

    LinxcTokenizer();
    LinxcTokenizer(const char *buffer, i32 bufferLength, collections::hashmap<string, LinxcTokenID>* nameToTokenRef);

    LinxcToken TokenizeAdvance();
    inline LinxcToken Next()
    {
        return this->tokenStream.ptr[this->currentToken++];
    }
    LinxcToken PeekNext();
    LinxcToken NextUntilValid();
    LinxcToken PeekNextUntilValid();
    inline void Back()
    {
        if (this->currentToken > 0)
            this->currentToken -= 1;
    }
};

bool LinxcIsPrimitiveType(LinxcTokenID ID);

LinxcTokenID LinxcGetKeyword(const char *str, usize strlen, bool isPreprocessorDirective, collections::hashmap<string, LinxcTokenID>* nameToToken);