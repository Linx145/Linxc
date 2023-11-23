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
    LinxcNamespace *typeNamespace;
    LinxcType *parentType;
    string name;
    collections::vector<LinxcVar> variables;
    collections::vector<LinxcFunc> functions;
    collections::vector<LinxcType> subTypes;
    collections::vector<string> templateArgs;

    LinxcType();
    LinxcType(IAllocator *allocator, string name, LinxcNamespace *myNamespace, LinxcType *myParent);

    string GetFullName(IAllocator *allocator);
};

/// Represents a function in Linxc, including the character in it's file where it starts and ends.
struct LinxcFunc
{
    LinxcNamespace *funcNamespace;
    LinxcType *methodOf;
    usize startIndex;
    usize endIndex;
    string name;
    LinxcTypeReference returnType;
    collections::Array<LinxcVar> arguments;
    collections::Array<string> templateArgs;

    LinxcFunc(string name, LinxcTypeReference returnType);
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

/// Represents a variable in Linxc, including it's type, name and optionally default value.
struct LinxcVar
{
    LinxcTypeReference type;
    string name;

    LinxcVar(string varName, LinxcTypeReference varType);
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
    LinxcNamespace *parentNamespace;
    string name;
    collections::hashmap<string, LinxcVar> variables;
    collections::hashmap<string, LinxcFunc> functions;
    collections::hashmap<string, LinxcType> types;
    collections::hashmap<string, LinxcNamespace> subNamespaces; //dont need pointer here as internal is pointer already

    LinxcNamespace();
    LinxcNamespace(IAllocator *allocator, string name);
};

#endif