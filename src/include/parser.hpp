#ifndef linxccparser
#define linxccparser

#include <Linxc.h>
#include <ast.hpp>
#include <vector.linxc>
#include <hashset.linxc>
#include <string.hpp>
#include <lexer.hpp>
#include <array.linxc>

typedef struct LinxcParserState LinxcParserState;
typedef struct LinxcParser LinxcParser;

#define ERR_MSG string

enum LinxcEndOn
{
    LinxcEndOn_Semicolon,
    LinxcEndOn_RBrace,
    LinxcEndOn_Endif,
    LinxcEndOn_Eof
};

struct LinxcParserState
{
    LinxcParser *parser;
    /// We keep a pointer to tokenizer for each parser state in case multiple files are parsed at once or in a call stack
    LinxcTokenizer *tokenizer;
    collections::vector<LinxcNamespace*> namespaces;
    bool isToplevel;
    LinxcEndOn endOn;

    LinxcNamespace *GetCurrentNamespace();
    LinxcParserState(LinxcParser *myParser, LinxcTokenizer *myTokenizer, LinxcEndOn endOn, bool isTopLevel);
};

struct LinxcParser
{
    IAllocator *allocator;
    collections::hashset<string> parsedFiles;
    LinxcNamespace globalNamespace;

    LinxcParser();
    LinxcParser(IAllocator *allocator);
    collections::vector<ERR_MSG> ParseFileH(collections::Array<string> includeDirs, string filePath, string fileContents);
    collections::vector<ERR_MSG> ParseCompoundStmtH(LinxcParserState *state);
};

#endif