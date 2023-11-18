#ifndef linxccparser
#define linxccparser

#include <Linxc.h>
#include <ast.hpp>
#include <vector.linxc>
#include <string.hpp>

enum LinxcEndOn
{
    LinxcEndOn_Semicolon,
    LinxcEndOn_RBrace,
    LinxcEndOn_Endif
};

struct LinxcParserState
{
    collections::vector<string> namespaces;
    bool isToplevel;
    LinxcEndOn endOn;

    LinxcParserState();
};

struct LinxcParserResult
{
    collections::vector<LinxcType> types;
    collections::vector<LinxcVar> variables;
    collections::vector<LinxcVar> functions;

    LinxcParserResult();
};

struct LinxcParser
{
    string currentFilePath;
    LinxcNamespace globalNamespace;

};

#endif