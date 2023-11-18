#ifndef linxccast
#define linxccast

#include <string.hpp>
#include <vector.linxc>
#include <hashmap.linxc>

typedef struct LinxcType LinxcType;
typedef struct LinxcVar LinxcVar;
typedef struct LinxcFunc LinxcFunc;
typedef struct LinxcNamespace LinxcNamespace;

/// Represents a type (struct) in Linxc.
struct LinxcType
{
    string namespaces;
    string name;
    collections::vector<LinxcVar> variables;
    collections::vector<LinxcFunc> functions;
    collections::vector<LinxcType*> subTypes;
    collections::vector<string> templateArgs;
};

/// Represents a function in Linxc, including the character in it's file where it starts and ends.
struct LinxcFunc
{
    usize startIndex;
    usize endIndex;
    string name;
    LinxcType *returnType;
    collections::vector<LinxcVar> arguments;
    collections::vector<string> templateArgs;
};

/// Represents a variable in Linxc, including it's type, name and optionally default value.
struct LinxcVar
{
    LinxcType *type;
    string name;
    // LinxcExpression defaultValue;
};

struct LinxcMacro
{
    string name;
    collections::vector<string> arguments;
    string body;
};

struct LinxcNamespace
{
    string name;
    collections::hashmap<string, LinxcVar> variables;
    collections::hashmap<string, LinxcFunc> functions;
    collections::hashmap<string, LinxcType> types;
    collections::hashmap<string, LinxcNamespace> subNamespaces; //dont need pointer here as internal is pointer already

    LinxcNamespace(string name);
};

#endif