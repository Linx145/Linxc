#include <lexer.hpp>
#include <ctype.h>
#include <stdio.h>

LinxcToken LinxcTokenizer::PeekNextUntilValid()
{
    usize prevToken = this->currentToken;
    LinxcToken result = NextUntilValid();
    this->currentToken = prevToken;

    return result;
}

LinxcToken LinxcTokenizer::NextUntilValid()
{
    while (true)
    {
        LinxcToken nextToken = Next();

        if (nextToken.ID != Linxc_Nl)
        {
            return nextToken;
        }
    }
}

LinxcToken LinxcTokenizer::PeekNext()
{
    usize prevToken = this->currentToken;
    LinxcToken result = this->Next();
    this->currentToken = prevToken;

    return result;
}

LinxcToken LinxcTokenizer::TokenizeAdvance()
{
    LinxcTokenizer *self = this;

    prevIndex = index;

    LinxcToken result;
    result.tokenizer = self;
    result.end = 0;
    result.start = self->index;
    result.ID = Linxc_Eof;
    LinxcTokenizerState state = Linxc_State_Start;

    bool isString = false;
    i32 counter = 0;

    for (; self->index < self->bufferLength; self->index++)
    {
        bool toBreak = false;
        const char c = self->buffer[self->index];

        switch (state)
        {
            case Linxc_State_Start:
                {
                    switch (c)
                    {
                        case '\n':
                            self->preprocessorDirective = false;
                            result.ID = Linxc_Nl;
                            self->index += 1;
                            toBreak = true;
                            break;
                        case '\r':
                            state = Linxc_State_Cr;
                            break;
                        case '"':
                            result.ID = Linxc_StringLiteral;
                            state = Linxc_State_StringLiteral;
                            break;
                        case '\'':
                            result.ID = Linxc_CharLiteral;
                            state = Linxc_State_CharLiteralStart;
                            break;
                        case 'u':
                            state = Linxc_State_u;
                            break;
                        case 'U':
                            state = Linxc_State_U;
                            break;
                        case 'L':
                            state = Linxc_State_L;
                            break;
                        case '=':
                            state = Linxc_State_Equal;
                            break;
                        case '!':
                            state = Linxc_State_Bang;
                            break;
                        case '|':
                            state = Linxc_State_Pipe;
                            break;
                        case '(':
                            result.ID = Linxc_LParen;
                            self->index += 1;
                            toBreak = true;
                            break;
                        case ')':
                            result.ID = Linxc_RParen;
                            self->index += 1;
                            toBreak = true;
                            break;
                        case '[':
                            result.ID = Linxc_LBracket;
                            self->index += 1;
                            toBreak = true;
                            break;
                        case ']':
                            result.ID = Linxc_RBracket;
                            self->index += 1;
                            toBreak = true;
                            break;
                        case ';':
                            result.ID = Linxc_Semicolon;
                            self->index += 1;
                            toBreak = true;
                            break;
                        case ',':
                            result.ID = Linxc_Comma;
                            self->index += 1;
                            toBreak = true;
                            break;
                        case '?':
                            result.ID = Linxc_QuestionMark;
                            self->index += 1;
                            toBreak = true;
                            break;
                        case ':':
                            state = Linxc_State_Colon;
                            break;
                        case '%':
                            state = Linxc_State_Percent;
                            break;
                        case '*':
                            state = Linxc_State_Asterisk;
                            break;
                        case '+':
                            state = Linxc_State_Plus;
                            break;
                        case '<':
                            if (self->prevTokenID == Linxc_Keyword_include)
                            {
                                state = Linxc_State_MacroString;
                            }
                            else state = Linxc_State_AngleBracketLeft;
                            break;
                        case '>':
                            state = Linxc_State_AngleBracketRight;
                            break;
                        case '^':
                            state = Linxc_State_Caret;
                            break;
                        case '{':
                            result.ID = Linxc_LBrace;
                            self->index += 1;
                            toBreak = true;
                            break;
                        case '}':
                            result.ID = Linxc_RBrace;
                            self->index += 1;
                            toBreak = true;
                            break;
                        case '~':
                            result.ID = Linxc_Tilde;
                            self->index += 1;
                            toBreak = true;
                            break;
                        case '.':
                            state = Linxc_State_Period;
                            break;
                        case '-':
                            state = Linxc_State_Minus;
                            break;
                        case '/':
                            state = Linxc_State_Slash;
                            break;
                        case '&':
                            state = Linxc_State_Ampersand;
                            break;
                        case '#':
                            state = Linxc_State_Hash;
                            break;
                        case '0':
                            state = Linxc_State_Zero;
                            break;
                        case '\\':
                            state = Linxc_State_BackSlash;
                            break;
                        case '\t':
                        case '\x0B':
                        case '\x0C':
                        case ' ':
                            //self->index += 1;
                            result.start = self->index + 1;
                            break;
                        case '\0':
                            toBreak = true;
                            break;
                        default:
                            if (c >= '1' && c <= '9')
                            {
                                state = Linxc_State_IntegerLiteral;
                            }
                            else if (isalpha(c) != 0)
                            {
                                state = Linxc_State_Identifier;
                            }
                            else
                            {
                                result.ID = Linxc_Invalid;
                                self->index += 1;
                                toBreak = true;
                            }
                            break;
                    }
                }
                break;
            case Linxc_State_Cr:
                if (c == '\n')
                {
                    self->preprocessorDirective = false;
                    result.ID = Linxc_Nl;
                    self->index += 1;
                    toBreak = true;
                }
                else
                {
                    result.ID = Linxc_Invalid;
                    toBreak = true;
                }
                break;
            case Linxc_State_BackSlash:
                switch (c)
                {
                    case '\n':
                    result.start = self->index + 1;
                    state = Linxc_State_Start;
                    break;

                    case '\r':
                    state = Linxc_State_BackSlashCr;
                    break;

                    case '\t':
                    case '\x0B':
                    case '\x0C':
                    case ' ':
                    break;

                    default:
                    result.ID = Linxc_Invalid;
                    toBreak = true;
                    break;
                }
                break;
            case Linxc_State_BackSlashCr:
                if (c == '\n')
                {
                    result.start = self->index + 1;
                    state = Linxc_State_Start;
                }
                else
                {
                    result.ID = Linxc_Invalid;
                    toBreak = true;
                }
                break;
            case Linxc_State_u:
                switch (c)
                {
                    case '8':
                    state = Linxc_State_u8;
                    break;

                    case '\'':
                    result.ID = Linxc_CharLiteral;
                    state = Linxc_State_CharLiteralStart;
                    break;

                    case '\"':
                    result.ID = Linxc_StringLiteral;
                    state = Linxc_State_StringLiteral;
                    break;

                    default:
                    self->index -= 1;
                    state = Linxc_State_Identifier;
                    break;
                }
                break;
            case Linxc_State_u8:
                if (c == '\"')
                {
                    result.ID = Linxc_StringLiteral;
                    state = Linxc_State_StringLiteral;
                }
                else
                {
                    self->index -= 1;
                    state = Linxc_State_Identifier;
                }
                break;
            case Linxc_State_L:
            case Linxc_State_U:
                switch (c)
                {
                    case '\'':
                    result.ID = Linxc_CharLiteral;
                    state = Linxc_State_CharLiteralStart;
                    break;

                    case '\"':
                    result.ID = Linxc_StringLiteral;
                    state = Linxc_State_StringLiteral;

                    default:
                    self->index -= 1;
                    state = Linxc_State_Identifier;
                    break;
                }
                break;
            case Linxc_State_StringLiteral:
                switch (c)
                {
                    case '\\':
                    isString = true;
                    state = Linxc_State_EscapeSequence;
                    break;

                    case '"':
                    self->index += 1;
                    toBreak = true;
                    break;

                    case '\n':
                    case '\r':
                    result.ID = Linxc_Invalid;
                    toBreak = true;
                    break;
                }
                break;
            case Linxc_State_CharLiteralStart:
                switch (c)
                {
                    case '\\':
                    isString = false;
                    state = Linxc_State_EscapeSequence;
                    break;

                    case '\'':
                    case '\n':
                    result.ID = Linxc_Invalid;
                    toBreak = true;
                    break;

                    default:
                    state = Linxc_State_CharLiteralStart;
                    break;
                }
                break;
            case Linxc_State_CharLiteral:
                switch (c)
                {
                    case '\\':
                    isString = false;
                    state = Linxc_State_EscapeSequence;
                    break;

                    case '\'':
                    self->index += 1;
                    toBreak = true;
                    break;

                    case '\n':
                    result.ID = Linxc_Invalid;
                    toBreak = true;
                    break;

                    default:
                    break;
                }
                break;
            case Linxc_State_EscapeSequence:
                switch (c)
                {
                    case '\'':
                    case '"':
                    case '?':
                    case '\\':
                    case 'a':
                    case 'b':
                    case 'f':
                    case 'n':
                    case 'r':
                    case 't':
                    case 'v':
                    case '\n':
                    state = isString ? Linxc_State_StringLiteral : Linxc_State_CharLiteral;
                    break;

                    case '\r':
                    state = Linxc_State_CrEscape;
                    break;

                    case 'x':
                    state = Linxc_State_HexEscape;
                    break;

                    case 'u':
                    counter = 4;
                    state = Linxc_State_OctalEscape;
                    break;

                    case 'U':
                    counter = 8;
                    state = Linxc_State_OctalEscape;
                    break;

                    default:
                    if ('0' <= c && c <= '7')
                    {
                        counter = 1;
                        state = Linxc_State_OctalEscape;
                    }
                    else
                    {
                        result.ID = Linxc_Invalid;
                        toBreak = true;
                    }
                    break;
                }
                break;
            case Linxc_State_CrEscape:
                if (c == '\n')
                {
                    state = isString ? Linxc_State_StringLiteral : Linxc_State_CharLiteral;
                }
                else
                {
                    result.ID = Linxc_Invalid;
                    toBreak = true;
                }
                break;
            case Linxc_State_OctalEscape:
                if ('0' <= c && c <= '7')
                {
                    counter += 1;
                    if (counter == 3)
                    {
                        state = isString ? Linxc_State_StringLiteral : Linxc_State_CharLiteral;
                    }
                }
                else
                {
                    self->index -= 1;
                    state = isString ? Linxc_State_StringLiteral : Linxc_State_CharLiteral;
                }
                break;
            case Linxc_State_HexEscape:
                if (('0' <= c && c <= '9') || ('a' <= c && c <= 'f') || ('A' <= c && c <= 'F'))
                {

                }
                else
                {
                    self->index -= 1;
                    state = isString ? Linxc_State_StringLiteral : Linxc_State_CharLiteral;
                }
                break;
            case Linxc_State_UnicodeEscape:
                if (('0' <= c && c <= '9') || ('a' <= c && c <= 'f') || ('A' <= c && c <= 'F'))
                {
                    counter -= 1;
                    if (counter == 0) {
                        state = isString ? Linxc_State_StringLiteral : Linxc_State_CharLiteral;
                    }
                }
                else
                {
                    if (counter != 0) {
                        result.ID = Linxc_Invalid;
                        toBreak = true;
                    }
                    self->index -= 1;
                    state = isString ? Linxc_State_StringLiteral : Linxc_State_CharLiteral;
                }
                break;
            case Linxc_State_Identifier:
                if (c == '_' || c == '$' || isalnum(c))
                {

                }
                else
                {
                    result.ID = LinxcGetKeyword(self->buffer + result.start, self->index - result.start, self->prevTokenID == Linxc_Hash && !self->preprocessorDirective, self->nameToToken);
                    if (result.ID == Linxc_Invalid)
                    {
                        result.ID = Linxc_Identifier;
                    }
                    if (self->prevTokenID == Linxc_Hash)
                    {
                        self->preprocessorDirective = true;
                    }
                    // result.ID = TokenID.getKeyword(self.buffer[result.start..self.index], self.prev_tok_id == .Hash and !self.pp_directive) orelse .Identifier;
                    // if (self.prev_tok_id == .Hash)
                    //     self.pp_directive = true;
                    toBreak = true;
                }
                break;
            case Linxc_State_Colon:
                if (c == ':')
                {
                    result.ID = Linxc_ColonColon;
                    self->index += 1;
                    toBreak = true;
                }
                else
                {
                    result.ID = Linxc_Colon;
                    toBreak = true;
                }
                break;
            case Linxc_State_Equal:
                if (c == '=')
                {
                    result.ID = Linxc_EqualEqual;
                    self->index += 1;
                    toBreak = true;
                }
                else
                {
                    result.ID = Linxc_Equal;
                    toBreak = true;
                }
                break;
            case Linxc_State_Bang:
                if (c == '=')
                {
                    result.ID = Linxc_BangEqual;
                    self->index += 1;
                    toBreak = true;
                }
                else
                {
                    result.ID = Linxc_Bang;
                    toBreak = true;
                }
                break;
            case Linxc_State_Pipe:
                if (c == '=')
                {
                    result.ID = Linxc_PipeEqual;
                    self->index += 1;
                    toBreak = true;
                }
                else if (c == '|')
                {
                    result.ID = Linxc_PipePipe;
                    self->index += 1;
                    toBreak = true;
                }
                else
                {
                    result.ID = Linxc_Bang;
                    toBreak = true;
                }
                break;
            case Linxc_State_Percent:
                if (c == '=')
                {
                    result.ID = Linxc_PercentEqual;
                    self->index += 1;
                    toBreak = true;
                }
                else
                {
                    result.ID = Linxc_Percent;
                    toBreak = true;
                }
                break;
            case Linxc_State_Plus:
                if (c == '=')
                {
                    result.ID = Linxc_PlusEqual;
                    self->index += 1;
                    toBreak = true;
                }
                else if (c == '+')
                {
                    result.ID = Linxc_PlusPlus;
                    self->index += 1;
                    toBreak = true;
                }
                else
                {
                    result.ID = Linxc_Plus;
                    toBreak = true;
                }
                break;
            case Linxc_State_Asterisk:
                if (c == '=')
                {
                    result.ID = Linxc_AsteriskEqual;
                    self->index += 1;
                }
                else
                {
                    result.ID = Linxc_Asterisk;
                }
                toBreak = true;
                break;
            case Linxc_State_MacroString:
                if (c == '>')
                {
                    result.ID = Linxc_MacroString;
                    self->index += 1;
                    toBreak = true;
                }
                break;
            case Linxc_State_AngleBracketLeft:
                if (c == '<')
                {
                    state = Linxc_State_AngleBracketAngleBracketLeft;
                }
                else if (c == '=')
                {
                    result.ID = Linxc_AngleBracketLeftEqual;
                    self->index += 1;
                    toBreak = true;
                }
                else
                {
                    result.ID = Linxc_AngleBracketLeft;
                    toBreak = true;
                }
                break;
            case Linxc_State_AngleBracketAngleBracketLeft:
                if (c == '=')
                {
                    result.ID = Linxc_AngleBracketAngleBracketLeftEqual;
                    self->index += 1;
                    toBreak = true;
                }
                else
                {
                    result.ID = Linxc_AngleBracketAngleBracketLeft;
                    toBreak = true;
                }
                break;
            case Linxc_State_AngleBracketRight:
                /*if (c == '>')
                {
                    state = Linxc_State_AngleBracketAngleBracketRight;
                }
                else */if (c == '=')
                {
                    result.ID = Linxc_AngleBracketRightEqual;
                    self->index += 1;
                    toBreak = true;
                }
                else
                {
                    result.ID = Linxc_AngleBracketRight;
                    toBreak = true;
                }
                break;
            case Linxc_State_AngleBracketAngleBracketRight:
                if (c == '=')
                {
                    result.ID = Linxc_AngleBracketAngleBracketRightEqual;
                    self->index += 1;
                    toBreak = true;
                }
                else
                {
                    result.ID = Linxc_AngleBracketAngleBracketRight;
                    toBreak = true;
                }
                break;
            case Linxc_State_Caret:
                if (c == '=')
                {
                    result.ID = Linxc_CaretEqual;
                    self->index += 1;
                    toBreak = true;
                }
                else
                {
                    result.ID = Linxc_Caret;
                    toBreak = true;
                }
                break;
            case Linxc_State_Period:
                if (c == '.')
                {
                    state = Linxc_State_Period2;
                }
                else if ('0' <= c && c <= '9')
                {
                    state = Linxc_State_FloatFraction;
                }
                else
                {
                    result.ID = Linxc_Period;
                    toBreak = true;
                }
                break;
            case Linxc_State_Period2:
                if (c == '.')
                {
                    result.ID = Linxc_Ellipsis;
                    self->index += 1;
                    toBreak = true;
                }
                else
                {
                    result.ID = Linxc_Period;
                    self->index -= 1;
                    toBreak = true;
                }
                break;
            case Linxc_State_Minus:
                switch (c)
                {
                    case '>':
                    result.ID = Linxc_Arrow;
                    self->index += 1;
                    toBreak = true;
                    break;

                    case '=':
                    result.ID = Linxc_MinusEqual;
                    self->index += 1;
                    toBreak = true;
                    break;

                    case '-':
                    result.ID = Linxc_MinusMinus;
                    self->index += 1;
                    toBreak = true;
                    break;

                    default:
                    result.ID = Linxc_Minus;
                    toBreak = true;
                    break;
                }
                break;
            case Linxc_State_Slash:
                switch (c) 
                {
                    case '/':
                    state = Linxc_State_LineComment;
                    break;

                    case '*':
                    state = Linxc_State_MultiLineComment;
                    break;

                    case '=':
                    result.ID = Linxc_SlashEqual;
                    self->index += 1;
                    toBreak = true;
                    break;

                    default:
                    result.ID = Linxc_Slash;
                    toBreak = true;
                    break;
                }
                break;
            case Linxc_State_Ampersand:
                if (c == '&')
                {
                    result.ID = Linxc_AmpersandAmpersand;
                    self->index += 1;
                    toBreak = true;
                }
                else if (c == '=')
                {
                    result.ID = Linxc_AmpersandEqual;
                    self->index += 1;
                    toBreak = true;
                }
                else
                {
                    result.ID = Linxc_Ampersand;
                    toBreak = true;
                }
                break;
            case Linxc_State_Hash:
                if (c == '#')
                {
                    result.ID = Linxc_HashHash;
                    self->index += 1;
                    toBreak = true;
                }
                else
                {
                    result.ID = Linxc_Hash;
                    toBreak = true;
                }
                break;
            case Linxc_State_LineComment:
                if (c == '\n')
                {
                    result.ID = Linxc_LineComment;
                    toBreak = true;
                }
                break;
            case Linxc_State_MultiLineComment:
                if (c == '*')
                {
                    state = Linxc_State_MultiLineCommentAsterisk;
                }
                break;
            case Linxc_State_MultiLineCommentAsterisk:
                if (c == '/')
                {
                    result.ID = Linxc_MultiLineComment;
                    self->index += 1;
                    toBreak = true;
                }
                else
                {
                    state = Linxc_State_MultiLineComment;
                }
                break;
            case Linxc_State_Zero:
                if ('0' <= c && c <= '9')
                {
                    state = Linxc_State_IntegerLiteralOct;
                }
                else if (c == 'b' || c == 'B')
                {
                    state = Linxc_State_IntegerLiteralBinaryFirst;
                }
                else if (c == 'x' || c == 'X')
                {
                    state = Linxc_State_IntegerLiteralHexFirst;
                }
                else if (c == '.')
                {
                    state = Linxc_State_FloatFraction;
                }
                else
                {
                    state = Linxc_State_IntegerSuffix;
                    self->index -= 1;
                }
                break;
            case Linxc_State_IntegerLiteralOct:
                if ('0' <= c && c <= '7')
                {

                }
                else
                {
                    state = Linxc_State_IntegerSuffix;
                    self->index -= 1;
                }
                break;
            case Linxc_State_IntegerLiteralBinaryFirst:
                if ('0' <= c && c <= '7')
                {
                    state = Linxc_State_IntegerLiteralBinary;
                }
                else
                {
                    result.ID = Linxc_Invalid;
                    toBreak = true;
                }
                break;
            case Linxc_State_IntegerLiteralBinary:
                if (c == '0' || c == '1')
                {

                }
                else
                {
                    state = Linxc_State_IntegerSuffix;
                    self->index -= 1;
                }
                break;
            case Linxc_State_IntegerLiteralHexFirst:
                if (('0' <= c && c <= '9') || ('a' <= c && c <= 'f') || ('A' <= c && c <= 'F'))
                {
                    state = Linxc_State_IntegerLiteralHex;
                }
                else if (c == '.')
                {
                    state = Linxc_State_FloatFractionHex;
                }
                else if (c == 'p' || c == 'P')
                {
                    state = Linxc_State_FloatExponent;
                }
                else
                {
                    result.ID = Linxc_Invalid;
                    toBreak = true;
                }
                break;
            case Linxc_State_IntegerLiteralHex:
                if (('0' <= c && c <= '9') || ('a' <= c && c <= 'f') || ('A' <= c && c <= 'F'))
                {
                }
                else if (c == '.')
                {
                    state = Linxc_State_FloatFractionHex;
                }
                else if (c == 'p' || c == 'P')
                {
                    state = Linxc_State_FloatExponent;
                }
                else
                {
                    state = Linxc_State_IntegerSuffix;
                    self->index -= 1;
                }
                break;
            case Linxc_State_IntegerLiteral:
                if ('0' <= c && c <= '9')
                {

                }
                else if (c == '.')
                {
                    state = Linxc_State_FloatFraction;
                }
                else if (c == 'e' || c == 'E')
                {
                    state = Linxc_State_FloatExponent;
                }
                else
                {
                    state = Linxc_State_IntegerSuffix;
                    self->index -= 1;
                }
                break;
            case Linxc_State_IntegerSuffix:
                switch (c)
                {
                    case 'u':
                    case 'U':
                    state = Linxc_State_IntegerSuffix;
                    break;

                    case 'l':
                    case 'L':
                    state = Linxc_State_IntegerSuffix;
                    break;

                    default:
                    result.ID = Linxc_IntegerLiteral;
                    toBreak = true;
                    break;
                }
                break;
            //todo: Linxc_State_IntegerSuffixU, Linxc_State_IntegerSuffixL, etc?
            case Linxc_State_FloatFraction:
                if ('0' <= c && c <= '9')
                {

                }
                else if (c == 'e' || c == 'E')
                {
                    state = Linxc_State_FloatExponent;
                }
                else
                {
                    state = Linxc_State_FloatSuffix;
                    self->index -= 1;
                }
                break;
            case Linxc_State_FloatFractionHex:
                if (('0' <= c && c <= '9') || ('a' <= c && c <= 'f') || ('A' <= c && c <= 'F'))
                {

                }
                else if (c == 'p' || c == 'P')
                {
                    state = Linxc_State_FloatExponent;
                }
                else
                {
                    result.ID = Linxc_Invalid;
                    toBreak = true;
                }
                break;
            case Linxc_State_FloatExponent:
                if (c != '+' && c != '-')
                {
                    self->index -= 1;
                }
                state = Linxc_State_FloatExponentDigits;
                break;
            case Linxc_State_FloatExponentDigits:
                if ('0' <= c && c <= '9')
                {
                    counter += 1;
                }
                else
                {
                    if (counter == 0)
                    {
                        result.ID = Linxc_Invalid;
                        toBreak = true;
                        break;
                    }
                    self->index -= 1;
                    state = Linxc_State_FloatSuffix;
                }
                break;
            case Linxc_State_FloatSuffix:
                switch (c)
                {
                    case 'f':
                    case 'F':
                    result.ID = Linxc_FloatLiteral; //f
                    self->index += 1;
                    break;

                    case 'l':
                    case 'L':
                    result.ID = Linxc_FloatLiteral; //l
                    self->index += 1;
                    break;

                    default:
                    result.ID = Linxc_FloatLiteral; //none
                    break;
                }
                toBreak = true;
                break;
            default:
                toBreak = true;
                break;
        }
        if (toBreak)
        {
            break;
        }
    }
    if (self->index == self->bufferLength)
    {
        switch (state)
        {
            case Linxc_State_Start:
                break;

            case Linxc_State_U:
            case Linxc_State_u:
            case Linxc_State_u8:
            case Linxc_State_L:
            case Linxc_State_Identifier:
                //Get identifier orelse identifier
                break;
            case Linxc_State_Cr:
            case Linxc_State_BackSlash:
            case Linxc_State_BackSlashCr:
            case Linxc_State_Period2:
            case Linxc_State_StringLiteral:
            case Linxc_State_CharLiteralStart:
            case Linxc_State_CharLiteral:
            case Linxc_State_EscapeSequence:
            case Linxc_State_CrEscape:
            case Linxc_State_OctalEscape:
            case Linxc_State_HexEscape:
            case Linxc_State_UnicodeEscape:
            case Linxc_State_MultiLineComment:
            case Linxc_State_MultiLineCommentAsterisk:
            case Linxc_State_FloatExponent:
            case Linxc_State_MacroString:
            case Linxc_State_IntegerLiteralBinaryFirst:
            case Linxc_State_IntegerLiteralHexFirst:
                result.ID = Linxc_Invalid;
                break;
            case Linxc_State_FloatExponentDigits:
                result.ID = counter == 0 ? Linxc_Invalid : Linxc_FloatLiteral;
                break;
            case Linxc_State_FloatFraction:
            case Linxc_State_FloatFractionHex:
                result.ID = Linxc_FloatLiteral;
                break;
            case Linxc_State_IntegerLiteralOct:
            case Linxc_State_IntegerLiteralBinary:
            case Linxc_State_IntegerLiteralHex:
            case Linxc_State_IntegerLiteral:
            case Linxc_State_IntegerSuffix:
            case Linxc_State_Zero:
                result.ID = Linxc_IntegerLiteral;
                break;
            case Linxc_State_IntegerSuffixU:
            case Linxc_State_IntegerSuffixL:
            case Linxc_State_IntegerSuffixLL:
            case Linxc_State_IntegerSuffixUL:
                result.ID = Linxc_IntegerLiteral;
                break;
            case Linxc_State_FloatSuffix:
                result.ID = Linxc_FloatLiteral;
                break;
            case Linxc_State_Colon: result.ID = Linxc_Colon; break;
            case Linxc_State_Equal: result.ID = Linxc_Equal; break;
            case Linxc_State_Bang: result.ID = Linxc_Bang; break;
            case Linxc_State_Minus: result.ID = Linxc_Minus; break;
            case Linxc_State_Slash: result.ID = Linxc_Slash; break;
            case Linxc_State_Ampersand: result.ID = Linxc_Ampersand; break;
            case Linxc_State_Hash: result.ID = Linxc_Hash; break;
            case Linxc_State_Period: result.ID = Linxc_Period; break;
            case Linxc_State_Pipe: result.ID = Linxc_Pipe; break;
            case Linxc_State_AngleBracketAngleBracketRight: result.ID = Linxc_AngleBracketAngleBracketRight; break;
            case Linxc_State_AngleBracketRight: result.ID = Linxc_AngleBracketRight; break;
            case Linxc_State_AngleBracketAngleBracketLeft: result.ID = Linxc_AngleBracketAngleBracketLeft; break;
            case Linxc_State_AngleBracketLeft: result.ID = Linxc_AngleBracketLeft; break;
            case Linxc_State_Plus: result.ID = Linxc_Plus; break;
            case Linxc_State_Percent: result.ID = Linxc_Percent; break;
            case Linxc_State_Caret: result.ID = Linxc_Caret; break;
            case Linxc_State_Asterisk: result.ID = Linxc_Asterisk; break;
            case Linxc_State_LineComment: result.ID = Linxc_LineComment; break;
        }
    }
    //printf("%u\n", result.start);
    self->prevTokenID = result.ID;
    result.end = self->index;
    return result;
};

string LinxcToken::ToString(IAllocator *allocator)
{
    return string(allocator, this->tokenizer->buffer + this->start, this->end - this->start);
}
CharSlice LinxcToken::ToCharSlice()
{
    return CharSlice(this->tokenizer->buffer + this->start, this->end - this->start);
}

LinxcTokenizer::LinxcTokenizer()
{
    this->buffer = NULL;
    this->bufferLength = 0;
    this->lineStartIndex = 0;
    this->currentLine = 0;
    this->index = 0;
    this->preprocessorDirective = false;
    this->prevIndex = 0;
    this->prevTokenID = Linxc_Invalid;
    this->nameToToken = NULL;
    this->currentToken = 0;
    this->tokenStream = collections::vector<LinxcToken>();
}

LinxcTokenizer::LinxcTokenizer(const char *buffer, usize bufferLength, collections::hashmap<string, LinxcTokenID>* nameToTokenRef)
{
    this->buffer = buffer;
    this->bufferLength = bufferLength;
    this->lineStartIndex = 0;
    this->currentLine = 0;
    this->index = 0;
    this->preprocessorDirective = false;
    this->prevIndex = 0;
    this->prevTokenID = Linxc_Invalid;
    this->nameToToken = nameToTokenRef;
    this->currentToken = 0;
    this->tokenStream = collections::vector<LinxcToken>();
};

LinxcTokenID LinxcGetKeyword(const char *chars, usize strlen, bool isPreprocessorDirective, collections::hashmap<string, LinxcTokenID> *nameToToken)
{
    IAllocator defaultAllocator = GetCAllocator();
    string str = string(&defaultAllocator, chars, strlen); // string(chars, strlen);

    LinxcTokenID *tokenIDPtr = nameToToken->Get(str);

    str.deinit();

    if (tokenIDPtr != NULL)
    {
        LinxcTokenID tokenID = *tokenIDPtr;
        if (tokenID == Linxc_Keyword_endif || tokenID == Linxc_Keyword_include || tokenID == Linxc_Keyword_define || tokenID == Linxc_Keyword_ifdef || tokenID == Linxc_Keyword_ifndef || tokenID == Linxc_Keyword_error || tokenID == Linxc_Keyword_pragma
        || tokenID == Acsl_Keyword_Fragment || tokenID == Acsl_Keyword_Vertex || tokenID == Acsl_Keyword_Compute || tokenID == Acsl_Keyword_Buffer || tokenID == Acsl_Keyword_End)
        {
            if (!isPreprocessorDirective)
            {
                return Linxc_Invalid;
            }
        }
        return tokenID;
    }
    return Linxc_Invalid;
};