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
    LinxcParsedFile *ParseFile(collections::Array<string> includeDirs, string fileFullPath, string includeName, string fileContents);
    option<LinxcCompoundStmt> ParseCompoundStmt(LinxcParserState *state);

    void deinit();
    void AddAllFilesFromDirectory(string directoryPath);
    option<string> FullPathFromIncludeName(string includeName);
    //Call after parsing the opening ( of the function declaration, ends after parsing the closing )
    collections::Array<LinxcVar> ParseFunctionArgs(LinxcParserState *state);
    //Parses an identifier or primitive type token. Can return either a LinxcTypeReference, or a variable reference.
    LinxcParseIdentifierResult ParseIdentifier(LinxcParserState *state);
};

#endif