#pragma once

#include <Linxc.h>
#include <ast.hpp>
#include <vector.hpp>
#include <hashset.hpp>
#include <string.hpp>
#include <lexer.hpp>
#include <array.hpp>
#include <io.hpp>

typedef struct LinxcParserState LinxcParserState;
typedef struct LinxcParser LinxcParser;

enum LinxcEndOn
{
    LinxcEndOn_Semicolon, //note: Does not end on a comma even if commaIsSemicolon is set to true
    LinxcEndOn_RBrace,
    LinxcEndOn_RParen, //for the final compound statement in a for loop
    LinxcEndOn_Endif,
    LinxcEndOn_Eof,
    LinxcEndOn_SingleStatement
};

enum LinxcParseTypeState
{
    LinxcParseType_NextOrEnd,
    LinxcParseType_ExpectIdentifier,
    LinxcParseType_ExpectOnlyPointer
};

//can probably make this constexpr, but linxc doesn't support it as C doesn't
inline i8 GetAssociation(LinxcTokenID ID)
{
    switch (ID)
    {
        case Linxc_Arrow:
        case Linxc_Minus:
        case Linxc_Plus:
        case Linxc_Slash:
        case Linxc_Percent:
        case Linxc_AmpersandAmpersand:
        case Linxc_PipePipe:
        case Linxc_EqualEqual:
        case Linxc_BangEqual:
        case Linxc_AngleBracketLeft:
        case Linxc_AngleBracketLeftEqual:
        case Linxc_AngleBracketRight:
        case Linxc_AngleBracketRightEqual:
        case Linxc_Period:
        case Linxc_ColonColon:
            return 1; //left to right ->
        default:
            return -1; //<- right to left
    }
};
inline i32 GetPrecedence(LinxcTokenID ID)
{
    switch (ID)
    {
        case Linxc_ColonColon:
            return 6;
        case Linxc_Arrow:
        case Linxc_Period:
            return 5;
        //Reserved for pointer dereference (*), NOT(!), bitwise not(~) and pointer reference (&) =>
        //  return 4;
        case Linxc_Asterisk:
        case Linxc_Slash:
        case Linxc_Percent:
            return 3;
        case Linxc_Plus:
        case Linxc_Minus:
        case Linxc_Ampersand:
        case Linxc_Caret:
        case Linxc_Tilde:
        case Linxc_Pipe:
        case Linxc_AngleBracketLeft:
        case Linxc_AngleBracketRight:
            return 2;
        case Linxc_PipePipe:
        case Linxc_BangEqual:
        case Linxc_EqualEqual:
        case Linxc_AmpersandAmpersand:
            return 1;
        case Linxc_Equal:
        case Linxc_PlusEqual:
        case Linxc_MinusEqual:
        case Linxc_AsteriskEqual:
        case Linxc_PercentEqual:
        case Linxc_SlashEqual:
            return 0;
        default:
            return -1;
    }
};
inline option<bool> IsSigned(LinxcTokenID tokenID)
{
    if (tokenID >= Linxc_Keyword_u8 && tokenID <= Linxc_Keyword_u64)
    {
        return option<bool>(true);
    }
    else if (tokenID >= Linxc_Keyword_i8 && tokenID <= Linxc_Keyword_i64)
    {
        return option<bool>(false);
    }
    else return option<bool>();
}
inline LinxcTokenID GetOperationResult(LinxcTokenID num1, LinxcTokenID num2)
{
    option<bool> num1IsSigned = IsSigned(num1);
    option<bool> num2IsSigned = IsSigned(num2);
    if (num1IsSigned.present && num2IsSigned.present)
    {
        if (num1IsSigned.value == num2IsSigned.value)
        {
            //both have same signature, so return the larger bit number
            return num2 >= num1 ? num2 : num1;
        }
        else
        {
            //if u8 + i32, return i32
            //if u64 + i32, return u64

            //in LinxcTokenID enum, unsigned integers come before signed integers.
            //thus, if we want to change signed to unsigned, we -4

            LinxcTokenID num1SameSignature = !num1IsSigned.value ? num1 : (LinxcTokenID)(num1 - 4);
            LinxcTokenID num2SameSignature = !num2IsSigned.value ? num2 : (LinxcTokenID)(num2 - 4);

            return num2SameSignature >= num1SameSignature ? num2 : num1;
        }
    }
    else return Linxc_Invalid;
}

struct LinxcIncludedFile
{
    string includeName;
    string fullNameAndPath;
};

struct LinxcParserState
{
    LinxcParser *parser;
    /// We keep a pointer to a tokenizer for each parser state in case multiple files are parsed at once or in a call stack
    LinxcTokenizer *tokenizer;
    LinxcParsedFile *parsingFile;
    LinxcPhoneyNamespace *currentNamespace;
    LinxcType *parentType;
    LinxcFunc *currentFunction;
    bool isToplevel;
    LinxcEndOn endOn;
    collections::hashmap<string, LinxcVar *> varsInScope;
    bool parsingLinxci;
    bool commaIsSemicolon;

    void deinit();
    LinxcParserState(LinxcParser *myParser, LinxcParsedFile *currentFile, LinxcTokenizer *myTokenizer, LinxcEndOn endOn, bool isTopLevel, bool isParsingLinxci);
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
    collections::hashmap<string, LinxcIncludedFile> includedFiles;
    /// Maps includeName to parsed file and data.
    collections::hashset<string> parsingFiles;
    collections::hashmap<string, LinxcParsedFile> parsedFiles;
    collections::hashmap<string, LinxcTokenID> nameToToken;
    LinxcType* typeofU8;
    LinxcType* typeofVoid;
    LinxcNamespace globalNamespace;
    string thisKeyword;
    string linxcstdLocation;
    string appName;

    LinxcParser(IAllocator *allocator);

    //Call after parsing the opening ( of the function declaration, ends after parsing the closing )
    collections::Array<LinxcVar> ParseFunctionArgs(LinxcParserState *state, u32* necessaryArguments);
    //Parses an entire file, parsing include directives under it and their relative files if unparsed thusfar.
    LinxcParsedFile *ParseFile(string fileFullPath, string includeName, string fileContents);
    bool TokenizeFile(LinxcTokenizer* tokenizer, IAllocator* allocator, LinxcParsedFile* parsingFile);
    //Parses a compound statement and returns it given the state. Returns invalid if an error is encountered
    option<collections::vector<LinxcStatement>> ParseCompoundStmt(LinxcParserState *state);
    //Parses a single, non-operator expression
    option<LinxcExpression> ParseExpressionPrimary(LinxcParserState *state, option<LinxcExpression> prevScopeIfAny);
    //Given a primary expression, parse following expressions and join them with operators in appropriate order
    LinxcExpression ParseExpression(LinxcParserState *state, LinxcExpression primary, i32 startingPrecedence);
    // parses a single identifier and returns either a func reference, type reference or variable reference. searches for references within the provided parentScopeOverride if any, if not, takes the values from all current namespace scopes in state and using namespace; declarations as well.
    option<LinxcExpression> ParseIdentifier(LinxcParserState *state, option<LinxcExpression> parentScopeOverride);

    void deinit();
    void SetLinxcStdLocation(string path);
    void AddAllFilesFromDirectory(string directoryPath);
    string FullPathFromIncludeName(string includeName);

    inline bool CanAssign(LinxcTypeReference variableType, LinxcTypeReference exprResult)
    {
        //special case for string literals
        //we can assign non const to const, but this is not the case for string literals\
        //we have to run this check first because linxcref comparison or cancastto don't take into account const of input expression
        if (variableType.lastType == typeofU8 && exprResult.lastType == typeofU8 && exprResult.isConst)
        {
            return variableType.pointerCount == 1 && exprResult.pointerCount == 1 && variableType.isConst;
        }
        if (variableType.genericTypeName.buffer != NULL)
        {
            return true;
        }
        //if exprResult is a generic type, lower the typechecker shields
        else if ((exprResult.genericTypeName.buffer == NULL) || variableType == exprResult || exprResult.CanCastTo(variableType, true))
        {
            return true;
        }

        return false;
    }
    LinxcOperatorFunc NewDefaultCast(LinxcType** primitiveTypePtrs, i32 myTypeIndex, i32 otherTypeIndex, bool isImplicit);
    LinxcOperatorFunc NewDefaultOperator(LinxcType** primitiveTypePtrs, i32 myTypeIndex, i32 otherTypeIndex, LinxcTokenID op);

    bool TranspileFile(LinxcParsedFile *parsedFile, const char* outputPathC, const char* outputPathH);
    void TranspileStatementH(FILE* fs, LinxcStatement* stmt);
    void TranspileFuncHeader(FILE* fs, LinxcFunc* func);
    void TranspileFuncH(FILE* fs, LinxcFunc* func);
    void TranspileFuncC(FILE* fs, LinxcFunc* func, TemplateArgs templateArgs, TemplateSpecialization templateSpecializations);
    void TranspileVar(FILE* fs, LinxcVar* var, i32* tempIndex, TemplateArgs templateArgs, TemplateSpecialization templateSpecialization);
    void TranspileTypeH(FILE* fs, LinxcType* type);
    void TranspileTypeC(FILE* fs, LinxcType* type, TemplateArgs templateArgs, TemplateSpecialization templateSpecializations);
    void TranspileExpr(FILE* fs, LinxcExpression* expr, bool writePriority, TemplateArgs templateArgs, TemplateSpecialization templateSpecializations);
    void TranspileStatementC(FILE* fs, LinxcStatement* stmt, i32* tempIndex, TemplateArgs templateArgs, TemplateSpecialization templateSpecializations);
    void TranspileCompoundStmtC(FILE* fs, collections::vector<LinxcStatement> stmts, i32* tempIndex, TemplateArgs templateArgs, TemplateSpecialization templateSpecializations);
    void RotateFuncCallExpression(LinxcExpression* expr, LinxcExpression** exprRootMutable, LinxcExpression* parent, LinxcExpression* grandParent, TemplateArgs templateArgs, TemplateSpecialization templateSpecializations);
    void SegregateFuncCallExpression(FILE* fs, LinxcExpression* rotatedExpr, i32* tempIndex, TemplateArgs templateArgs, TemplateSpecialization templateSpecializations);

    bool Compile(const char* outputDirectory);
    void PrintAllErrors();
};