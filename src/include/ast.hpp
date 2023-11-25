#ifndef linxccast
#define linxccast

#include <string.hpp>
#include <vector.linxc>
#include <hashmap.linxc>

#define ERR_MSG string

typedef struct LinxcType LinxcType;
typedef struct LinxcVar LinxcVar;
typedef struct LinxcFunc LinxcFunc;
typedef struct LinxcNamespace LinxcNamespace;
typedef struct LinxcOperator LinxcOperator;
typedef union LinxcExpressionData LinxcExpressionData;
typedef struct LinxcExpression LinxcExpression;
typedef struct LinxcStatement LinxcStatement;

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

    LinxcType *FindSubtype(string name);
    LinxcFunc *FindFunction(string name);
    LinxcVar *FindVar(string name);

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
    //We only need to store 1 LinxcType as it is a linked list that leads to parent types if any, and the namespace chain.
    LinxcType *lastType;

    collections::Array<LinxcTypeReference> templateArgs;

    u32 pointerCount;

    LinxcTypeReference();
    LinxcTypeReference(LinxcType *type);
};

/// Represents a variable in Linxc, including it's type, name and optionally default value.
struct LinxcVar
{
    LinxcTypeReference type;
    string name;

    LinxcVar();
    LinxcVar(string varName, LinxcTypeReference varType);
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

    collections::vector<ERR_MSG> errors;

    LinxcParsedFile(IAllocator *allocator, string fullPath, string includeName);
};

struct LinxcFunctionCall
{
    LinxcFunc *func;
    collections::Array<LinxcExpression> inputParams;
    collections::Array<LinxcTypeReference> templateSpecializations;
};

struct LinxcOperator
{
    LinxcExpression leftExpr;
    LinxcExpression rightExpr;
    LinxcTokenID operatorType;
};
enum LinxcExpressionID
{
    LinxcExpr_OperatorCall, 
    LinxcExpr_IncrementVar, 
    LinxcExpr_DecrementVar, 
    LinxcExpr_Literal,
    LinxcExpr_Variable,
    LinxcExpr_PointerDeref,
    LinxcExpr_Indexer,
    LinxcExpr_FuncCall,
    LinxcExpr_Sizeof,
    LinxcExpr_Nameof,
    LinxcExpr_Typeof
};
union LinxcExpressionData
{
    LinxcOperator *operatorCall; //Eg: X + Y
    string incrementVariable; //Eg: varName++
    string decrementVariable; //Eg: varName--
    string literal; //Eg: "Hello World"
    string variable; //Eg: varName
    LinxcExpression *pointerDereference; //Eg: *varName
    LinxcExpression *indexerCall; //Eg: varName[expression]
    LinxcFunctionCall functionCall; // Eg: Function(expression1, expression2, ...);
    LinxcTypeReference sizeofCall; //Eg: sizeof(type reference)
    LinxcTypeReference nameofCall; //Eg: nameof(type reference)
    LinxcTypeReference typeofCall; //Eg: typeof(type reference)
};
struct LinxcExpression
{
    LinxcExpressionData data;
    LinxcExpressionID ID;
};

enum LinxcStatementID
{
    LinxcStmt_Include,
    LinxcStmt_Expr,
    LinxcStmt_TypeDecl,
    LinxcStmt_VarDecl,
    LinxcStmt_Namespace
};
union LinxcStatementData
{
    LinxcParsedFile *includeStatement;
    LinxcExpression expression;
    LinxcType *typeDeclaration;
    LinxcVar *varDeclaration;
    LinxcNamespace *namespaceScope;
};
struct LinxcStatement
{
    LinxcStatementData data;
    LinxcStatementID ID;
};
#define LinxcCompoundStmt collections::vector<LinxcStatement>
#endif