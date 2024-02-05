#pragma once

#include "string.hpp"
#include "vector.hpp"
#include "hashmap.hpp"
#include "hashset.hpp"
#include "lexer.hpp"

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
typedef struct LinxcPhoneyNamespace LinxcPhoneyNamespace;

typedef collections::Array<LinxcTypeReference> TemplateSpecialization;
typedef collections::Array<string> TemplateArgs;

struct LinxcFunctionCall
{
    LinxcFunc *func;
    collections::Array<LinxcExpression> inputParams;
    LinxcExpression* thisAsParam;
    collections::Array<LinxcExpression> templateArgs;

    string ToString(IAllocator *allocator);
};

struct LinxcIndexerCall
{
    LinxcOperatorFunc* indexerOperator;
    LinxcVar* variableToIndex;
    LinxcExpression* inputParams;
};

struct LinxcEnumMember
{
    string name;
    i32 value;
};

struct LinxcFuncPtr
{
    string name;
    LinxcExpression* returnType;
    collections::Array<LinxcExpression> arguments;
    u16 necessaryArguments;

    LinxcFuncPtr();
    LinxcFuncPtr(string name, LinxcExpression* returnType);
    bool operator==(LinxcFuncPtr B);
};
struct LinxcFunctionPointerCall
{
    LinxcVar* variable;
    collections::Array<LinxcExpression> inputParams;
    LinxcFuncPtr* functionPtrType;
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
    collections::hashmap<LinxcOperatorImpl, LinxcOperatorFunc> operatorOverloads;

    //Enum properties
    collections::vector<LinxcEnumMember> enumMembers;
    TemplateSpecialization isSpecializedType;

    //Function Pointer properties
    LinxcFuncPtr delegateDecl;

    //template stuff
    TemplateArgs templateArgs;
    collections::hashmap<TemplateSpecialization, LinxcType> templateSpecializations;

    LinxcType();
    LinxcType(IAllocator *allocator, string name, LinxcNamespace *myNamespace, LinxcType *myParent);

    LinxcType Specialize(IAllocator* allocator, TemplateArgs inputArgs, TemplateSpecialization specialization);

    LinxcType *FindSubtype(string name);
    LinxcFunc *FindFunction(string name);
    LinxcVar *FindVar(string name);
    LinxcEnumMember* FindEnumMember(string name);

    string GetFullName(IAllocator *allocator);
    string GetCName(IAllocator* allocator);

    LinxcExpression AsExpression();
};

// A type reference consists of (chain of namespace to parent type to type)<template args, each another typereference> (pointers)
struct LinxcTypeReference
{
    //We only need to store 1 LinxcType as it is a linked list that leads to parent types if any, and the namespace chain.
    LinxcType *lastType;
    string genericTypeName;

    collections::Array<LinxcExpression> templateArgs;

    u32 pointerCount;

    bool isConst;

    LinxcTypeReference();
    LinxcTypeReference(LinxcType *type);
    string ToString(IAllocator *allocator);
    string GetCName(IAllocator* allocator, bool pointerAsPtr = false, TemplateArgs templateArgs = TemplateArgs(), TemplateSpecialization templateSpecializations = TemplateSpecialization());

    bool CanCastTo(LinxcTypeReference type, bool implicitly);
    //dont need to check const as only const u8* is a special type
    //we parse that within EvaluatePossible
    bool operator==(LinxcTypeReference B);
    inline bool operator!=(LinxcTypeReference B)
    {
        return !(*this==B);
    }
};
bool LinxcTypeReferenceEql(LinxcTypeReference A, LinxcTypeReference B);
u32 LinxcTypeReferenceHash(LinxcTypeReference A);

bool LinxcTemplateSpecializationsEql(TemplateSpecialization A, TemplateSpecialization B);
u32 LinxcTemplateSpecializationsHash(TemplateSpecialization A);

enum LinxcExpressionID
{
    LinxcExpr_None,
    LinxcExpr_OperatorCall, 
    LinxcExpr_Literal,
    LinxcExpr_Variable,
    LinxcExpr_FunctionRef,
    LinxcExpr_TypeRef,
    LinxcExpr_EnumMemberRef,
    LinxcExpr_NamespaceRef,
    LinxcExpr_TypeCast,
    LinxcExpr_Modified,
    LinxcExpr_Indexer,
    LinxcExpr_FuncCall,
    LinxcExpr_FuncPtrCall,
    LinxcExpr_Sizeof,
    LinxcExpr_Nameof,
    LinxcExpr_Typeof,
    LinxcExpr_IndexerCall
};
union LinxcExpressionData
{
    LinxcOperator *operatorCall; //Eg: X + Y
    string literal; //Eg: "Hello World"
    LinxcVar *variable; //Eg: varName
    LinxcFunc *functionRef; //Eg: funcName <- is incomplete
    LinxcTypeReference typeRef; //Eg: typeName <- is incomplete
    LinxcEnumMember* enumMemberRef;
    LinxcPhoneyNamespace *namespaceRef; //Eg: namespaceName <- is incomplete
    LinxcTypeCast *typeCast; //Eg: (typeName)
    LinxcModifiedExpression *modifiedExpression; //Eg: *varName
    LinxcFunctionCall functionCall; // Eg: Function(expression1, expression2, ...);
    LinxcFunctionPointerCall functionPointerCall;
    LinxcTypeReference sizeofCall; //Eg: sizeof(type reference)
    LinxcTypeReference nameofCall; //Eg: nameof(type reference)
    LinxcTypeReference typeofCall; //Eg: typeof(type reference)
    LinxcIndexerCall indexerCall;

    LinxcExpressionData();
};
struct LinxcExpression
{
    LinxcExpressionData data;
    LinxcExpressionID ID;
    //what this expression returns. Can be any primitive type, void, or NULL (which is what variable type name expressions resolve to)
    LinxcTypeReference resolvesTo;
    bool priority;

    string ToString(IAllocator *allocator);
    //In the case that ID is a LinxcTypeReference or operator that eventually resolves to one,
    //resolvesTo.type will be NULL, as this expression itself *is* a type name.
    //However, because LinxcTypeReference may not be immediately accessible,
    //call this function to parse the potential operator tree and retrieve the final type.
    option<LinxcTypeReference> AsTypeReference();
    option<LinxcFunc*> AsFuncReference();
    LinxcExpression SpecializeSignature(IAllocator* allocator, collections::Array<string> args, TemplateSpecialization with);
    //void Specialize(collections::Array<string> args, TemplateSpecialization with);
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
    LinxcType asType;

    LinxcFunc();
    LinxcFunc(string name, LinxcExpression returnType);
    LinxcFuncPtr GetSignature(IAllocator* allocator);
    LinxcFunc SpecializeSignature(IAllocator* allocator, collections::Array<string> templateArgs, TemplateSpecialization specialization);
    string GetCName(IAllocator* allocator);// , collections::Array<string> scopeTemplateArgs, collections::Array<LinxcTypeReference> scopeTemplateSpecializations);
};

/// Represents a variable in Linxc, including it's type, name and optionally default value.
struct LinxcVar
{
    //must resolve to a LinxcTypeReference
    LinxcExpression type;
    string name;
    option<LinxcExpression> defaultValue;
    LinxcType* memberOf;
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
struct LinxcPhoneyNamespace
{
    IAllocator* allocator;
    LinxcNamespace* actualNamespace;
    LinxcPhoneyNamespace* parentNamespace;
    string name;
    collections::hashmap<string, LinxcVar*> variableRefs;
    collections::hashmap<string, LinxcFunc*> functionRefs;
    collections::hashmap<string, LinxcType*> typeRefs;
    collections::hashmap<string, LinxcTypeReference> typedefs;
    collections::hashmap<string, LinxcPhoneyNamespace> subNamespaces; //dont need pointer here as internal is pointer already

    LinxcPhoneyNamespace();
    LinxcPhoneyNamespace(IAllocator* allocator, LinxcNamespace* thisActualNamespace);

    inline LinxcVar* AddVariableToOrigin(string name, LinxcVar variable)
    {
        LinxcVar* result = this->actualNamespace->variables.Add(name, variable);
        this->variableRefs.Add(name, result);
        return result;
    }
    inline LinxcFunc* AddFunctionToOrigin(string name, LinxcFunc func)
    {
        LinxcFunc* result = this->actualNamespace->functions.Add(name, func);
        this->functionRefs.Add(name, result);
        return result;
    }
    inline LinxcType* AddTypeToOrigin(string name, LinxcType type)
    {
        LinxcType* result = this->actualNamespace->types.Add(name, type);
        this->typeRefs.Add(name, result);
        return result;
    }
    inline LinxcPhoneyNamespace* AddNamespaceToOrigin(string name, LinxcNamespace linxcNamespace)
    {
        LinxcNamespace *actualSubNamespace = actualNamespace->subNamespaces.Add(name, linxcNamespace);
        return this->subNamespaces.Add(name, LinxcPhoneyNamespace(actualNamespace->functions.allocator, actualSubNamespace));
    }
    void Add(LinxcPhoneyNamespace* other);
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

    //Needed so we can keep track of what functions to transpile when transpiling this file
    collections::vector<LinxcFunc*> definedFuncs;

    collections::vector<ERR_MSG> errors;

    collections::vector<LinxcStatement> ast;

    LinxcPhoneyNamespace fileNamespace;

    bool mustTranspileC;
    bool isLinxcH;

    LinxcParsedFile();
    LinxcParsedFile(IAllocator *allocator, string fullPath, string includeName);
};

//A modified expression is an expression that is either a dereferenced pointer, a type to pointer conversion or an inverted/NOT/negative expression
struct LinxcModifiedExpression
{
    LinxcExpression expression;
    //either *, -, !, ~, &, ++, --
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

struct LinxcIfStatement
{
    LinxcExpression condition;
    collections::vector<LinxcStatement> result;
};

struct LinxcForLoopStatement
{
    collections::vector<LinxcStatement> initialization;
    LinxcExpression condition;
    collections::vector<LinxcStatement> step;
    collections::vector<LinxcStatement> body;
};

struct LinxcUseLang
{
    string languageUsed;
    collections::vector<LinxcToken> body;
};

enum LinxcStatementID
{
    LinxcStmt_Include,
    LinxcStmt_Expr,
    LinxcStmt_Return,
    LinxcStmt_TypeDecl,
    LinxcStmt_VarDecl,
    LinxcStmt_FuncDecl,
    LinxcStmt_Namespace,
    LinxcStmt_If,
    LinxcStmt_Else,
    LinxcStmt_UseLang,
    LinxcStmt_For,
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
    LinxcIfStatement ifStatement;
    collections::vector<LinxcStatement> elseStatement;
    LinxcUseLang useLang;
    LinxcForLoopStatement forLoop;

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