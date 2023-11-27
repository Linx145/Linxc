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

enum LinxcParseIdentifierResultID
{
    LinxcParseIdentifierResult_None,
    LinxcParseIdentifierResult_Variable,
    LinxcParseIdentifierResult_Type,
    LinxcParseIdentifierResult_Func,
    LinxcParseIdentifierResult_Namespace
};
union LinxcParseIdentifierResultData
{
    LinxcVar *variableReference;
    LinxcTypeReference typeReference;
    LinxcFunc *functionReference;
    LinxcNamespace *namespaceReference;

    LinxcParseIdentifierResultData();
};
struct LinxcParseIdentifierResult
{
    LinxcParseIdentifierResultData value;
    LinxcParseIdentifierResultID ID;

    LinxcParseIdentifierResult();
};

//can probably make this constexpr, but linxc doesn't support it as C doesn't
inline i8 GetAssociation(LinxcTokenID ID)
{
    switch (ID)
    {
        Linxc_Arrow:
        Linxc_Minus:
        Linxc_Plus:
        Linxc_Slash:
        Linxc_Percent:
        Linxc_AmpersandAmpersand:
        Linxc_PipePipe:
        Linxc_EqualEqual:
        Linxc_BangEqual:
        Linxc_AngleBracketLeft:
        Linxc_AngleBracketLeftEqual:
        Linxc_AngleBracketRight:
        Linxc_AngleBracketRightEqual:
        Linxc_Period:
            return 1; //left to right ->
        default:
            return -1; //<- right to left
    }
};
inline i32 GetPrecedence(LinxcTokenID ID)
{
    switch (ID)
    {
        Linxc_Arrow:
        Linxc_Period:
            return 5;
        //Reserved for pointer dereference (*), NOT(!), bitwise not(~) and pointer reference (&) =>
        //  return 4;
        Linxc_Asterisk:
        Linxc_Slash:
        Linxc_Percent:
            return 3;
        Linxc_Plus:
        Linxc_Minus:
        Linxc_Ampersand:
        Linxc_Caret:
        Linxc_Tilde:
        Linxc_Pipe:
        Linxc_AngleBracketLeft:
        Linxc_AngleBracketRight:
            return 2;
        Linxc_PipePipe:
        Linxc_BangEqual:
        Linxc_EqualEqual:
        Linxc_AmpersandAmpersand:
            return 1;
        Linxc_Equal:
        Linxc_PlusEqual:
        Linxc_MinusEqual:
        Linxc_AsteriskEqual:
        Linxc_PercentEqual:
        Linxc_SlashEqual:
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
    LinxcNamespace *currentNamespace;
    LinxcParsedFile *parsingFile;
    LinxcType *parentType;
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
    void deinit();

    //Call after parsing the opening ( of the function declaration, ends after parsing the closing )
    collections::Array<LinxcVar> ParseFunctionArgs(LinxcParserState *state);
    //Parses an identifier or primitive type token. Can return either a LinxcTypeReference, or a variable reference.
    LinxcParseIdentifierResult ParseIdentifier(LinxcParserState *state);
    //Parses an entire file, parsing include directives under it and their relative files if unparsed thusfar.
    LinxcParsedFile *ParseFile(collections::Array<string> includeDirs, string fileFullPath, string includeName, string fileContents);
    //Parses a compound statement and returns it given the state. Returns invalid if an error is encountered
    option<LinxcCompoundStmt> ParseCompoundStmt(LinxcParserState *state);
    option<LinxcExpression> ParseExpressionPrimary(LinxcParserState *state);
    LinxcExpression ParseExpression(LinxcParserState *state, LinxcExpression primary, i32 startingPrecedence);

    void deinit();
    void AddAllFilesFromDirectory(string directoryPath);
    string FullPathFromIncludeName(string includeName);

};

#endif