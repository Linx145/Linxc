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

struct LinxcParsedFile
{
    /// The path of the file name relative to whatever include directories are in the project
    /// (Eg: with a included directory of 'include,' containing 'include/hashset.linxc', includeName would be 'hashset.linxc')
    string includeName;
    /// The full name and path of the file.
    string fullPath;
    /// A list of all defined macros in the file. Does not count macros #undef'd before the end of the file. 
    /// Macros within are owned by LinxcParsedFile instance itself. (Makes no sense for it to be under namespaces)
    collections::vector<LinxcMacro> definedMacros;

    /// A list of all defined or included types in this file. Points to actual type storage location within a namespace.
    collections::vector<LinxcType *> definedTypes;

    /// A list of all defined or included functions in this file. Points to actual function storage location within a namespace.
    collections::vector<LinxcFunc *> definedFuncs;

    /// A list of all defined or included global variables in this file. Points to actual variable storage location within a namespace.
    collections::vector<LinxcVar *> definedVars;
};

struct LinxcParserState
{
    LinxcParser *parser;
    /// We keep a pointer to tokenizer for each parser state in case multiple files are parsed at once or in a call stack
    LinxcTokenizer *tokenizer;
    LinxcNamespace *currentNamespace;
    LinxcType *parentType;
    bool isToplevel;
    LinxcEndOn endOn;

    LinxcParserState(LinxcParser *myParser, LinxcTokenizer *myTokenizer, LinxcEndOn endOn, bool isTopLevel);
};

// A type reference consists of (chain of namespace to parent type to type)<template args, each another typereference> (pointers)
struct LinxcTypeReference
{
    //eg: collections::hashmap<i32, collections::Array<string>>
    string rawText;

    //We only need to store 1 LinxcType as it is a linked list that leads to parent types if any, and the namespace chain.
    LinxcType *lastType;

    collections::Array<LinxcTypeReference> templateArgs;

    u32 pointerCount;
};

struct LinxcParser
{
    IAllocator *allocator;
    /// Maps includeName to parsed file and data.
    collections::hashmap<string, LinxcParsedFile> parsedFiles;
    LinxcNamespace globalNamespace;

    LinxcParser();
    LinxcParser(IAllocator *allocator);
    collections::vector<ERR_MSG> ParseFileH(collections::Array<string> includeDirs, string filePath, string fileContents);
    collections::vector<ERR_MSG> ParseCompoundStmtH(LinxcParserState *state);

    LinxcTypeReference ParseTypeReference(LinxcParserState *state, collections::vector<ERR_MSG>* errors);
};

#endif