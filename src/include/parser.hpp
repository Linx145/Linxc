#ifndef linxccparser
#define linxccparser

#include <Linxc.h>
#include <ast.hpp>
#include <vector.linxc>
#include <hashset.linxc>
#include <string.hpp>
#include <lexer.hpp>
#include <array.linxc>
#include <io.hpp>

typedef struct LinxcParserState LinxcParserState;
typedef struct LinxcParser LinxcParser;

enum LinxcEndOn
{
    LinxcEndOn_Semicolon,
    LinxcEndOn_RBrace,
    LinxcEndOn_Endif,
    LinxcEndOn_Eof
};

enum LinxcParseTypeState
{
    LinxcParseType_NextOrEnd,
    LinxcParseType_ExpectIdentifier,
    LinxcParseType_ExpectOnlyPointer
};

// enum LinxcParseIdentifierResultID
// {
//     LinxcParseIdentifierResult_None,
//     LinxcParseIdentifierResult_Variable,
//     LinxcParseIdentifierResult_Type,
//     LinxcParseIdentifierResult_Func,
//     LinxcParseIdentifierResult_Namespace
// };
// union LinxcParseIdentifierResultData
// {
//     LinxcVar *variableReference;
//     LinxcType *typeReference;
//     LinxcFunc *functionReference;
//     LinxcNamespace *namespaceReference;

//     LinxcParseIdentifierResultData();
// };
// struct LinxcParseIdentifierResult
// {
//     LinxcParseIdentifierResultData value;
//     LinxcParseIdentifierResultID ID;

//     LinxcParseIdentifierResult();
// };

//can probably make this constexpr, but linxc doesn't support it as C doesn't
inline i8 GetAssociation(LinxcTokenID ID)
{
    switch (ID)
    {
        case Linxc_Arrow:
        case Linxc_Minus:
        case Linxc_Plus:
        case Linxc_Slash:
        case Linxc_Percent:
        case Linxc_AmpersandAmpersand:
        case Linxc_PipePipe:
        case Linxc_EqualEqual:
        case Linxc_BangEqual:
        case Linxc_AngleBracketLeft:
        case Linxc_AngleBracketLeftEqual:
        case Linxc_AngleBracketRight:
        case Linxc_AngleBracketRightEqual:
        case Linxc_Period:
        case Linxc_ColonColon:
            return 1; //left to right ->
        default:
            return -1; //<- right to left
    }
};
inline i32 GetPrecedence(LinxcTokenID ID)
{
    switch (ID)
    {
        case Linxc_ColonColon:
            return 6;
        case Linxc_Arrow:
        case Linxc_Period:
            return 5;
        //Reserved for pointer dereference (*), NOT(!), bitwise not(~) and pointer reference (&) =>
        //  return 4;
        case Linxc_Asterisk:
        case Linxc_Slash:
        case Linxc_Percent:
            return 3;
        case Linxc_Plus:
        case Linxc_Minus:
        case Linxc_Ampersand:
        case Linxc_Caret:
        case Linxc_Tilde:
        case Linxc_Pipe:
        case Linxc_AngleBracketLeft:
        case Linxc_AngleBracketRight:
            return 2;
        case Linxc_PipePipe:
        case Linxc_BangEqual:
        case Linxc_EqualEqual:
        case Linxc_AmpersandAmpersand:
            return 1;
        case Linxc_Equal:
        case Linxc_PlusEqual:
        case Linxc_MinusEqual:
        case Linxc_AsteriskEqual:
        case Linxc_PercentEqual:
        case Linxc_SlashEqual:
            return 0;
        default:
            return -1;
    }
};

struct LinxcParserState
{
    LinxcParser *parser;
    /// We keep a pointer to a tokenizer for each parser state in case multiple files are parsed at once or in a call stack
    LinxcTokenizer *tokenizer;
    LinxcParsedFile *parsingFile;
    LinxcNamespace *currentNamespace;
    LinxcType *parentType;
    LinxcFunc *currentFunction;
    bool isToplevel;
    LinxcEndOn endOn;
    collections::hashmap<string, LinxcVar *> varsInScope;

    void deinit();
    LinxcParserState(LinxcParser *myParser, LinxcParsedFile *currentFile, LinxcTokenizer *myTokenizer, LinxcEndOn endOn, bool isTopLevel);
};

struct LinxcParser
{
    IAllocator *allocator;
    /// The root directories for #include statements. 
    ///In pure-linxc projects, normally is your project's
    ///src folder. May consist of include folders for C .h files as well
    ///or .linxch static libraries
    collections::vector<string> includeDirectories;
    ///The actual .linxc files to be included in the compilation.
    collections::vector<string> includedFiles;
    /// Maps includeName to parsed file and data.
    collections::hashset<string> parsingFiles;
    collections::hashmap<string, LinxcParsedFile> parsedFiles;
    collections::hashmap<string, LinxcType *> fullNameToType;
    LinxcNamespace globalNamespace;

    LinxcParser(IAllocator *allocator);

    //Call after parsing the opening ( of the function declaration, ends after parsing the closing )
    collections::Array<LinxcVar> ParseFunctionArgs(LinxcParserState *state);
    //Parses an entire file, parsing include directives under it and their relative files if unparsed thusfar.
    LinxcParsedFile *ParseFile(string fileFullPath, string includeName, string fileContents);
    //Parses a compound statement and returns it given the state. Returns invalid if an error is encountered
    option<collections::vector<LinxcStatement>> ParseCompoundStmt(LinxcParserState *state);
    //Parses a single, non-operator expression
    option<LinxcExpression> ParseExpressionPrimary(LinxcParserState *state);
    //Given a primary expression, parse following expressions and join them with operators in appropriate order
    LinxcExpression ParseExpression(LinxcParserState *state, LinxcExpression primary, i32 startingPrecedence);
    // parses a single identifier and returns either a func reference, type reference or variable reference. searches for references within the provided parentScopeOverride if any, if not, takes the values from all current namespace scopes in state and using namespace; declarations as well.
    LinxcExpression ParseIdentifier(LinxcParserState *state, option<LinxcExpression> parentScopeOverride);

    void deinit();
    void AddAllFilesFromDirectory(string directoryPath);
    string FullPathFromIncludeName(string includeName);

};

#endif