#ifndef linxccast
#define linxccast

#include <string.hpp>
#include <vector.linxc>
#include <hashmap.linxc>
#include <lexer.hpp>

#define ERR_MSG string

typedef struct LinxcType LinxcType;
typedef struct LinxcVar LinxcVar;
typedef struct LinxcFunc LinxcFunc;
typedef struct LinxcNamespace LinxcNamespace;
typedef struct LinxcOperator LinxcOperator;
typedef union LinxcExpressionData LinxcExpressionData;
typedef struct LinxcExpression LinxcExpression;
typedef struct LinxcStatement LinxcStatement;
typedef struct LinxcModifiedExpression LinxcModifiedExpression;
typedef struct LinxcTypeReference LinxcTypeReference;
typedef struct LinxcOperatorImpl LinxcOperatorImpl;
typedef struct LinxcOperatorFunc LinxcOperatorFunc;
typedef struct LinxcTypeCast LinxcTypeCast;

struct LinxcFunctionCall
{
    LinxcFunc *func;
    collections::Array<LinxcExpression> inputParams;
    collections::Array<LinxcTypeReference> templateSpecializations;

    string ToString(IAllocator *allocator);
};

/// Represents a type (struct) in Linxc.
struct LinxcType
{
    collections::vector<LinxcStatement> body;
    LinxcNamespace *typeNamespace;
    LinxcType *parentType;
    string name;
    collections::vector<LinxcVar> variables;
    collections::vector<LinxcFunc> functions;
    collections::vector<LinxcType> subTypes;
    collections::vector<string> templateArgs;
    collections::hashmap<LinxcOperatorImpl, LinxcOperatorFunc> operatorOverloads;

    LinxcType();
    LinxcType(IAllocator *allocator, string name, LinxcNamespace *myNamespace, LinxcType *myParent);

    LinxcType *FindSubtype(string name);
    LinxcFunc *FindFunction(string name);
    LinxcVar *FindVar(string name);

    string GetFullName(IAllocator *allocator);
    string GetCName(IAllocator* allocator);

    LinxcExpression AsExpression();
};

// A type reference consists of (chain of namespace to parent type to type)<template args, each another typereference> (pointers)
struct LinxcTypeReference
{
    //We only need to store 1 LinxcType as it is a linked list that leads to parent types if any, and the namespace chain.
    LinxcType *lastType;

    collections::Array<LinxcTypeReference> templateArgs;

    u32 pointerCount;

    bool isConst;

    LinxcTypeReference();
    LinxcTypeReference(LinxcType *type);
    string ToString(IAllocator *allocator);
    string GetCName(IAllocator* allocator);

    bool CanCastTo(LinxcTypeReference type, bool implicitly);
    //dont need to check const as only const u8* is a special type
    //we parse that within EvaluatePossible
    bool operator==(LinxcTypeReference B)
    {
        bool templatesEqual = true;
        if (this->templateArgs.length != B.templateArgs.length)
        {
            templatesEqual = false;
        }
        if (templatesEqual)
        {
            for (usize i = 0; i < this->templateArgs.length; i++)
            {
                templatesEqual = this->templateArgs.data[i] == B.templateArgs.data[i];
            }
        }
        return this->lastType == B.lastType && templatesEqual && this->pointerCount == B.pointerCount;
    }
    inline bool operator!=(LinxcTypeReference B)
    {
        return !(*this==B);
    }
};
u32 LinxcTypeReferenceHash(LinxcTypeReference A);

enum LinxcExpressionID
{
    LinxcExpr_None,
    LinxcExpr_OperatorCall, 
    LinxcExpr_IncrementVar, 
    LinxcExpr_DecrementVar, 
    LinxcExpr_Literal,
    LinxcExpr_Variable,
    LinxcExpr_FunctionRef,
    LinxcExpr_TypeRef,
    LinxcExpr_NamespaceRef,
    LinxcExpr_TypeCast,
    LinxcExpr_Modified,
    LinxcExpr_Indexer,
    LinxcExpr_FuncCall,
    LinxcExpr_Sizeof,
    LinxcExpr_Nameof,
    LinxcExpr_Typeof
};
union LinxcExpressionData
{
    LinxcOperator *operatorCall; //Eg: X + Y
    LinxcVar *incrementVariable; //Eg: varName++
    LinxcVar *decrementVariable; //Eg: varName--
    string literal; //Eg: "Hello World"
    LinxcVar *variable; //Eg: varName
    LinxcFunc *functionRef; //Eg: funcName <- is incomplete
    LinxcTypeReference typeRef; //Eg: typeName <- is incomplete
    LinxcNamespace *namespaceRef; //Eg: namespaceName <- is incomplete
    LinxcTypeCast *typeCast; //Eg: (typeName)
    LinxcModifiedExpression *modifiedExpression; //Eg: *varName
    LinxcExpression *indexerCall; //Eg: varName[expression]
    LinxcFunctionCall functionCall; // Eg: Function(expression1, expression2, ...);
    LinxcTypeReference sizeofCall; //Eg: sizeof(type reference)
    LinxcTypeReference nameofCall; //Eg: nameof(type reference)
    LinxcTypeReference typeofCall; //Eg: typeof(type reference)

    LinxcExpressionData();
};
struct LinxcExpression
{
    LinxcExpressionData data;
    LinxcExpressionID ID;
    //what this expression returns. Can be any primitive type, void, or NULL (which is what variable type name expressions resolve to)
    LinxcTypeReference resolvesTo;

    string ToString(IAllocator *allocator);
    //In the case that ID is a LinxcTypeReference or operator that eventually resolves to one,
    //resolvesTo.type will be NULL, as this expression itself *is* a type name.
    //However, because LinxcTypeReference may not be immediately accessible,
    //call this function to parse the potential operator tree and retrieve the final type.
    option<LinxcTypeReference> AsTypeReference();
    LinxcExpression *ToHeap(IAllocator *allocator);
};
struct LinxcTypeCast
{
    LinxcExpression castToType;
    LinxcExpression expressionToCast;
};

/// Represents a function in Linxc, including the character in it's file where it starts and ends.
struct LinxcFunc
{
    collections::vector<LinxcStatement> body;
    LinxcNamespace *funcNamespace;
    LinxcType *methodOf;
    string name;
    LinxcExpression returnType;
    collections::Array<LinxcVar> arguments;
    u16 necessaryArguments;
    collections::Array<string> templateArgs;

    LinxcFunc();
    LinxcFunc(string name, LinxcExpression returnType);
    string GetCName(IAllocator* allocator);
};

/// Represents a variable in Linxc, including it's type, name and optionally default value.
struct LinxcVar
{
    //must resolve to a LinxcTypeReference
    LinxcExpression type;
    string name;
    option<LinxcExpression> defaultValue;
    bool isConst;

    LinxcVar();
    LinxcVar(string varName, LinxcExpression varType, option<LinxcExpression> defaultVal);

    string ToString(IAllocator *allocator);
};

struct LinxcMacro
{
    string name;
    bool isFunctionMacro;
    collections::Array<LinxcToken> arguments;
    collections::vector<LinxcToken> body;
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
struct LinxcNamespaceScope
{
    LinxcNamespace* referencedNamespace;
    collections::vector<LinxcStatement> body;

    LinxcNamespaceScope();
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

    collections::vector<LinxcMacro> definedAttributes;

    /// A list of all defined or included types in this file. Points to actual type storage location within a namespace.
    collections::vector<LinxcType *> definedTypes;

    /// A list of all defined or included functions in this file. Points to actual function storage location within a namespace.
    collections::vector<LinxcFunc *> definedFuncs;

    /// A list of all defined or included global variables in this file. Points to actual variable storage location within a namespace.
    collections::vector<LinxcVar *> definedVars;

    collections::vector<ERR_MSG> errors;

    collections::vector<LinxcStatement> ast;

    LinxcParsedFile();
    LinxcParsedFile(IAllocator *allocator, string fullPath, string includeName);
};

//A modified expression is an expression that is either a dereferenced pointer, a type to pointer conversion or an inverted/NOT/negative expression
struct LinxcModifiedExpression
{
    LinxcExpression expression;
    //either *, -, !, ~, &
    LinxcTokenID modification;
};
//An operator is an expression containing two sub-expressions connected with a token, normally an operator
struct LinxcOperator
{
    LinxcExpression leftExpr;
    LinxcExpression rightExpr;
    LinxcTokenID operatorType;

    option<LinxcTypeReference> EvaluatePossible();
};

struct LinxcIncludeStatement
{
    string includeString;
    LinxcParsedFile* includedFile;
    LinxcIncludeStatement();
};

enum LinxcStatementID
{
    LinxcStmt_Include,
    LinxcStmt_Expr,
    LinxcStmt_Return,
    LinxcStmt_TypeDecl,
    LinxcStmt_VarDecl,
    LinxcStmt_FuncDecl,
    //LinxcStmt_TempVarDecl,
    LinxcStmt_Namespace
};
union LinxcStatementData
{
    LinxcIncludeStatement includeStatement;
    LinxcExpression expression;
    LinxcExpression returnStatement;
    LinxcType *typeDeclaration;
    LinxcVar *varDeclaration;
    LinxcFunc *funcDeclaration;
    //LinxcVar tempVarDeclaration;
    LinxcNamespaceScope namespaceScope;

    LinxcStatementData();
};
struct LinxcStatement
{
    LinxcStatementData data;
    LinxcStatementID ID;

    string ToString(IAllocator *allocator);
};

enum LinxcOperatorOrCastID
{
    LinxcOverloadIs_Operator, LinxcOverloadIs_Cast
};
struct LinxcOperatorImpl
{
    LinxcOperatorOrCastID ID;
    bool implicit;
    LinxcTokenID op;

    //the type that we are operating on
    LinxcTypeReference myType;

    //in an operator type, refers to what we are operating with
    //in a cast type, refers to what we are casting to
    LinxcTypeReference otherType;

    string ToString(IAllocator* allocator);
};
u32 LinxcOperatorImplHash(LinxcOperatorImpl A);
bool LinxcOperatorImplEql(LinxcOperatorImpl A, LinxcOperatorImpl B);

struct LinxcOperatorFunc
{
    LinxcOperatorImpl operatorOverride;
    LinxcFunc function;

    string ToString(IAllocator* allocator);
};
#endif