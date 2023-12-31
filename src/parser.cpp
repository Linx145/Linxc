﻿#include <parser.hpp>
#include <stdio.h>
#include <path.hpp>
#include <ArenaAllocator.hpp>

LinxcParserState::LinxcParserState(LinxcParser *myParser, LinxcParsedFile *currentFile, LinxcTokenizer *myTokenizer, LinxcEndOn endsOn, bool isTopLevel, bool isParsingLinxci)
{
    this->tokenizer = myTokenizer;
    this->parser = myParser;
    this->parsingFile = currentFile;
    this->endOn = endsOn;
    this->isToplevel = isTopLevel;
    this->currentNamespace = &myParser->globalNamespace;
    this->currentFunction = NULL;
    this->parentType = NULL;
    this->varsInScope = collections::hashmap<string, LinxcVar *>(&defaultAllocator, &stringHash, &stringEql);
    this->parsingLinxci = isParsingLinxci;
}
LinxcParser::LinxcParser(IAllocator *allocator)
{
    this->allocator = allocator;
    this->globalNamespace = LinxcNamespace(allocator, string());
    this->thisKeyword = string(allocator, "this");

    const i32 numIntegerTypes = 8;
    const i32 numNumericTypes = 10;// 11; TODO: Deal with char, probably will remove it
    const i32 numPrimitiveTypes = 13;

    //every numeric type can be explicitly compared to every other numeric type
    const char *primitiveTypes[numPrimitiveTypes] = {"u8", "u16", "u32", "u64", "i8", "i16", "i32", "i64", "float", "double", "char", "void", "bool" };
    string nameStrings[numPrimitiveTypes];
    LinxcType* primitiveTypePtrs[numPrimitiveTypes];
    for (i32 i = 0; i < numPrimitiveTypes; i++)
    {
        nameStrings[i] = string(allocator, primitiveTypes[i]);
        this->globalNamespace.types.Add(nameStrings[i], LinxcType(allocator, nameStrings[i], &this->globalNamespace, NULL));
        primitiveTypePtrs[i] = this->globalNamespace.types.Get(nameStrings[i]);
        if (i == 0)
        {
            typeofU8 = primitiveTypePtrs[i];
        }
    }

    //all types support ==, !=
    //all numeric types support +, -, /, *
    //integer types support <<, >>, |, ^, &
    //bools support &&, ||, !

    //using == or != on any type MUST convert it into a bool
    //+, -, /, * MUST return either original type or operatesWith type
    //<<, >>, |, ^, & MUST return original type

    //all implicit casts may be used explicitly
    //an explicit cast may not be used implicitly

    //all integer types cast to all other integer types
    for (i32 i = 0; i < numIntegerTypes; i++)
    {
        for (i32 j = 0; j < numIntegerTypes; j++)
        {
            if (i != j)
            {
                //if casting up and is same sign, implicit
                //else, explicit
                bool sameSign = (i < 4 && j < 4) || (i >= 4 && j >= 4);
                bool implicit = sameSign && j > i;

                LinxcOperatorFunc defaultCast = NewDefaultCast(primitiveTypePtrs, i, j, implicit);
                primitiveTypePtrs[i]->operatorOverloads.Add(defaultCast.operatorOverride, defaultCast);
            }
        }
        //integers cast to float and double implicitly
        LinxcOperatorFunc toFloat = NewDefaultCast(primitiveTypePtrs, i, numIntegerTypes, true);
        primitiveTypePtrs[i]->operatorOverloads.Add(toFloat.operatorOverride, toFloat);

        LinxcOperatorFunc toDouble = NewDefaultCast(primitiveTypePtrs, i, numIntegerTypes + 1, true);
        primitiveTypePtrs[i]->operatorOverloads.Add(toDouble.operatorOverride, toDouble);
    }
    //float and double cast explicitly to all integer types and each other
    for (i32 i = numIntegerTypes; i < numIntegerTypes + 2; i++)
    {
        for (i32 j = 0; j < numIntegerTypes + 2; j++)
        {
            if (i != j)
            {
                LinxcOperatorFunc defaultCast = NewDefaultCast(primitiveTypePtrs, i, j, false);
                primitiveTypePtrs[i]->operatorOverloads.Add(defaultCast.operatorOverride, defaultCast);
            }
        }
    }
    //all numeric types can be +, -, /, *, ==, != with each other
    //TODO: Settle the bitshift and bitwise comparison operators
    //because typeA + typeB = typeB + typeA, we should avoid adding the duplicates
    for (i32 i = 0; i < numNumericTypes; i++)
    {
        LinxcOperatorFunc operatorSet = NewDefaultOperator(primitiveTypePtrs, i, i, Linxc_Equal);
        primitiveTypePtrs[i]->operatorOverloads.Add(operatorSet.operatorOverride, operatorSet);

        for (i32 j = 0; j < numNumericTypes; j++)
        {
            LinxcOperatorImpl opposite;
            opposite.implicit = false;
            opposite.ID = LinxcOverloadIs_Operator;
            opposite.op = Linxc_Plus;
            opposite.myType = LinxcTypeReference(primitiveTypePtrs[j]);
            opposite.otherType = LinxcTypeReference(primitiveTypePtrs[i]);

            if (!primitiveTypePtrs[j]->operatorOverloads.Contains(opposite))
            {
                LinxcOperatorFunc operatorAdd = NewDefaultOperator(primitiveTypePtrs, i, j, Linxc_Plus);
                primitiveTypePtrs[i]->operatorOverloads.Add(operatorAdd.operatorOverride, operatorAdd);

                LinxcOperatorFunc operatorSubtract = NewDefaultOperator(primitiveTypePtrs, i, j, Linxc_Minus);
                primitiveTypePtrs[i]->operatorOverloads.Add(operatorSubtract.operatorOverride, operatorSubtract);

                LinxcOperatorFunc operatorMult = NewDefaultOperator(primitiveTypePtrs, i, j, Linxc_Asterisk);
                primitiveTypePtrs[i]->operatorOverloads.Add(operatorMult.operatorOverride, operatorMult);

                LinxcOperatorFunc operatorDiv = NewDefaultOperator(primitiveTypePtrs, i, j, Linxc_Slash);
                primitiveTypePtrs[i]->operatorOverloads.Add(operatorDiv.operatorOverride, operatorDiv);
            
                LinxcOperatorFunc operatorEquals = NewDefaultOperator(primitiveTypePtrs, i, j, Linxc_EqualEqual);
                primitiveTypePtrs[i]->operatorOverloads.Add(operatorEquals.operatorOverride, operatorEquals);

                LinxcOperatorFunc operatorNotEquals = NewDefaultOperator(primitiveTypePtrs, i, j, Linxc_BangEqual);
                primitiveTypePtrs[i]->operatorOverloads.Add(operatorNotEquals.operatorOverride, operatorNotEquals);
            }
        }
    }
    //bools can be ==, !=, &&, ||
    {
        LinxcOperatorFunc boolEquals = NewDefaultOperator(primitiveTypePtrs, 12, 12, Linxc_EqualEqual);
        LinxcOperatorFunc boolNotEquals = NewDefaultOperator(primitiveTypePtrs, 12, 12, Linxc_BangEqual);
        LinxcOperatorFunc boolAnd = NewDefaultOperator(primitiveTypePtrs, 12, 12, Linxc_AmpersandAmpersand);
        LinxcOperatorFunc boolOr = NewDefaultOperator(primitiveTypePtrs, 12, 12, Linxc_PipePipe);

        primitiveTypePtrs[12]->operatorOverloads.Add(boolEquals.operatorOverride, boolEquals);
        primitiveTypePtrs[12]->operatorOverloads.Add(boolNotEquals.operatorOverride, boolNotEquals);
        primitiveTypePtrs[12]->operatorOverloads.Add(boolAnd.operatorOverride, boolAnd);
        primitiveTypePtrs[12]->operatorOverloads.Add(boolOr.operatorOverride, boolOr);
    }

    this->parsedFiles = collections::hashmap<string, LinxcParsedFile>(allocator, &stringHash, &stringEql);
    this->parsingFiles = collections::hashset<string>(allocator, &stringHash, &stringEql);
    this->includedFiles = collections::vector<string>(allocator);
    this->includeDirectories = collections::vector<string>(allocator);
    this->nameToToken = collections::hashmap<string, LinxcTokenID>(allocator, &stringHash, &stringEql);

    nameToToken.Add(string(nameToToken.allocator, "include"), Linxc_Keyword_include);
    nameToToken.Add(string(nameToToken.allocator, "alignas"), Linxc_Keyword_alignas);
    nameToToken.Add(string(nameToToken.allocator, "alignof"), Linxc_Keyword_alignof);
    nameToToken.Add(string(nameToToken.allocator, "atomic"), Linxc_Keyword_atomic);
    nameToToken.Add(string(nameToToken.allocator, "auto"), Linxc_Keyword_auto);
    nameToToken.Add(string(nameToToken.allocator, "bool"), Linxc_Keyword_bool);
    nameToToken.Add(string(nameToToken.allocator, "break"), Linxc_Keyword_break);
    nameToToken.Add(string(nameToToken.allocator, "case"), Linxc_Keyword_case);
    nameToToken.Add(string(nameToToken.allocator, "char"), Linxc_Keyword_char);
    nameToToken.Add(string(nameToToken.allocator, "complex"), Linxc_Keyword_complex);
    nameToToken.Add(string(nameToToken.allocator, "const"), Linxc_Keyword_const);
    nameToToken.Add(string(nameToToken.allocator, "continue"), Linxc_Keyword_continue);
    nameToToken.Add(string(nameToToken.allocator, "default"), Linxc_Keyword_default);
    nameToToken.Add(string(nameToToken.allocator, "define"), Linxc_Keyword_define);
    nameToToken.Add(string(nameToToken.allocator, "delegate"), Linxc_Keyword_delegate);
    nameToToken.Add(string(nameToToken.allocator, "do"), Linxc_Keyword_do);
    nameToToken.Add(string(nameToToken.allocator, "double"), Linxc_Keyword_double);
    nameToToken.Add(string(nameToToken.allocator, "else"), Linxc_Keyword_else);
    nameToToken.Add(string(nameToToken.allocator, "enum"), Linxc_Keyword_enum);
    nameToToken.Add(string(nameToToken.allocator, "error"), Linxc_Keyword_error);
    nameToToken.Add(string(nameToToken.allocator, "extern"), Linxc_Keyword_extern);
    nameToToken.Add(string(nameToToken.allocator, "false"), Linxc_Keyword_false);
    nameToToken.Add(string(nameToToken.allocator, "float"), Linxc_Keyword_float);
    nameToToken.Add(string(nameToToken.allocator, "for"), Linxc_Keyword_for);
    nameToToken.Add(string(nameToToken.allocator, "goto"), Linxc_Keyword_goto);
    nameToToken.Add(string(nameToToken.allocator, "i16"), Linxc_Keyword_i16);
    nameToToken.Add(string(nameToToken.allocator, "i32"), Linxc_Keyword_i32);
    nameToToken.Add(string(nameToToken.allocator, "i64"), Linxc_Keyword_i64);
    nameToToken.Add(string(nameToToken.allocator, "i8"), Linxc_Keyword_i8);
    nameToToken.Add(string(nameToToken.allocator, "if"), Linxc_Keyword_if);
    nameToToken.Add(string(nameToToken.allocator, "ifdef"), Linxc_Keyword_ifdef);
    nameToToken.Add(string(nameToToken.allocator, "ifndef"), Linxc_Keyword_ifndef);
    nameToToken.Add(string(nameToToken.allocator, "imaginary"), Linxc_Keyword_imaginary);
    nameToToken.Add(string(nameToToken.allocator, "include"), Linxc_Keyword_include);
    nameToToken.Add(string(nameToToken.allocator, "inline"), Linxc_Keyword_inline);
    //nameToToken.Add(string(nameToToken.allocator, "int"), Linxc_Keyword_int);
    //nameToToken.Add(string(nameToToken.allocator, "long"), Linxc_Keyword_long);
    nameToToken.Add(string(nameToToken.allocator, "nameof"), Linxc_Keyword_nameof);
    nameToToken.Add(string(nameToToken.allocator, "namespace"), Linxc_Keyword_namespace);
    nameToToken.Add(string(nameToToken.allocator, "noreturn"), Linxc_Keyword_noreturn);
    nameToToken.Add(string(nameToToken.allocator, "pragma"), Linxc_Keyword_pragma);
    nameToToken.Add(string(nameToToken.allocator, "register"), Linxc_Keyword_register);
    nameToToken.Add(string(nameToToken.allocator, "restrict"), Linxc_Keyword_restrict);
    nameToToken.Add(string(nameToToken.allocator, "return"), Linxc_Keyword_return);
    nameToToken.Add(string(nameToToken.allocator, "short"), Linxc_Keyword_short);
    //nameToToken.Add(string(nameToToken.allocator, "signed"), Linxc_Keyword_signed);
    nameToToken.Add(string(nameToToken.allocator, "sizeof"), Linxc_Keyword_sizeof);
    nameToToken.Add(string(nameToToken.allocator, "static"), Linxc_Keyword_static);
    nameToToken.Add(string(nameToToken.allocator, "struct"), Linxc_Keyword_struct);
    nameToToken.Add(string(nameToToken.allocator, "switch"), Linxc_Keyword_switch);
    nameToToken.Add(string(nameToToken.allocator, "template"), Linxc_Keyword_template);
    nameToToken.Add(string(nameToToken.allocator, "thread_local"), Linxc_Keyword_thread_local);
    nameToToken.Add(string(nameToToken.allocator, "trait"), Linxc_Keyword_trait);
    nameToToken.Add(string(nameToToken.allocator, "true"), Linxc_Keyword_true);
    nameToToken.Add(string(nameToToken.allocator, "typedef"), Linxc_Keyword_typedef);
    nameToToken.Add(string(nameToToken.allocator, "typename"), Linxc_Keyword_typename);
    nameToToken.Add(string(nameToToken.allocator, "typeof"), Linxc_Keyword_typeof);
    nameToToken.Add(string(nameToToken.allocator, "u16"), Linxc_Keyword_u16);
    nameToToken.Add(string(nameToToken.allocator, "u32"), Linxc_Keyword_u32);
    nameToToken.Add(string(nameToToken.allocator, "u64"), Linxc_Keyword_u64);
    nameToToken.Add(string(nameToToken.allocator, "u8"), Linxc_Keyword_u8);
    nameToToken.Add(string(nameToToken.allocator, "union"), Linxc_Keyword_union);
    nameToToken.Add(string(nameToToken.allocator, "void"), Linxc_Keyword_void);
    nameToToken.Add(string(nameToToken.allocator, "volatile"), Linxc_Keyword_volatile);
    nameToToken.Add(string(nameToToken.allocator, "while"), Linxc_Keyword_while);
    nameToToken.Add(string(nameToToken.allocator, "attribute"), Linxc_keyword_attribute);
}
void LinxcParserState::deinit()
{
    this->varsInScope.deinit();
}
void LinxcParser::deinit()
{
    for (usize i = 0; i < this->includedFiles.count; i++)
    {
        this->includedFiles.Get(i)->deinit();
    }
    this->includedFiles.deinit();

    for (usize i = 0; i < this->includeDirectories.count; i++)
    {
        this->includeDirectories.Get(i)->deinit();
    }
    this->includeDirectories.deinit();

    //TODO: deinit parsedFiles, parsingFiles
}
void LinxcParser::AddAllFilesFromDirectory(string directoryPath)
{
    collections::Array<string> result = io::GetFilesInDirectory(this->allocator, directoryPath.buffer);

    for (usize i = 0; i < result.length; i++)
    {
        result.data[i].Prepend("/");
        result.data[i].Prepend(directoryPath.buffer);
        this->includedFiles.Add(result.data[i]);
    }
    result.deinit();
}
LinxcParsedFile *LinxcParser::ParseFile(string fileFullPath, string includeName, string fileContents)
{
    if (this->parsedFiles.Contains(includeName)) //already parsed
    {
        return this->parsedFiles.Get(includeName);
    }
    bool parsingLinxci = false;
    string extension = path::GetExtension(&defaultAllocator, fileFullPath);
    if (extension == ".linxci")
    {
        parsingLinxci = true;
    }

    LinxcParsedFile file = LinxcParsedFile(this->allocator, fileFullPath, includeName);
    this->parsingFiles.Add(includeName);

    LinxcTokenizer tokenizer = LinxcTokenizer(fileContents.buffer, fileContents.length, &this->nameToToken);
    
    if (this->TokenizeFile(&tokenizer, allocator, &file))
    {
        LinxcParserState parserState = LinxcParserState(this, &file, &tokenizer, LinxcEndOn_Eof, true, parsingLinxci);
        option<collections::vector<LinxcStatement>> ast = this->ParseCompoundStmt(&parserState);

        if (ast.present)
        {
            file.ast = ast.value;
        }
    }

    //this->parsedFiles.Add(filePath);
    this->parsingFiles.Remove(includeName);
    this->parsedFiles.Add(includeName, file);
    return this->parsedFiles.Get(includeName);
}
string LinxcParser::FullPathFromIncludeName(string includeName)
{
    for (usize i = 0; i < this->includeDirectories.count; i++)
    {
        string potentialFullPath = string(this->includeDirectories.Get(i)->buffer);
        potentialFullPath.Append("/");
        potentialFullPath.Append(includeName.buffer);

        if (io::FileExists(potentialFullPath.buffer))
        {
            return potentialFullPath;
        }
        else potentialFullPath.deinit();
    }
    return string();
}
LinxcOperatorFunc LinxcParser::NewDefaultCast(LinxcType** primitiveTypePtrs, i32 myTypeIndex, i32 otherTypeIndex, bool isImplicit)
{
    LinxcType* myType = primitiveTypePtrs[myTypeIndex];
    LinxcType* toType = primitiveTypePtrs[otherTypeIndex];
    //i32 toTypeIndex = GetOperationResult((LinxcTokenID)(myTypeIndex + Linxc_Keyword_u8), (LinxcTokenID)(otherTypeIndex + Linxc_Keyword_u8)) - Linxc_Keyword_u8;
    //LinxcTypeReference toType = LinxcTypeReference(primitiveTypePtrs[toTypeIndex]);

    LinxcOperatorImpl cast;
    cast.implicit = isImplicit;
    cast.ID = LinxcOverloadIs_Cast;
    cast.otherType = toType;
    cast.op = Linxc_Invalid;
    cast.myType = LinxcTypeReference(myType);

    LinxcExpression returnType;
    returnType.resolvesTo.lastType = NULL;
    returnType.data.typeRef = toType;
    returnType.ID = LinxcExpr_TypeRef; //what we're casting into
    LinxcFunc castFunc = LinxcFunc(string(), returnType); //these functions don't need names
    
    //cast functions don't have arguments, eg: (float)integer has no argument
    castFunc.arguments = collections::Array<LinxcVar>();

    LinxcOperatorFunc result;
    result.operatorOverride = cast;
    result.function = castFunc;

    return result;
}
LinxcOperatorFunc LinxcParser::NewDefaultOperator(LinxcType** primitiveTypePtrs, i32 myTypeIndex, i32 otherTypeIndex, LinxcTokenID op)
{
    //unsigned comes before signed in the enum
    LinxcType* myType = primitiveTypePtrs[myTypeIndex];
    LinxcType* otherType = primitiveTypePtrs[otherTypeIndex];
    i32 returnTypeIndex = 0;
    
    if (op == Linxc_EqualEqual || op == Linxc_BangEqual)
    {
        returnTypeIndex = 12; //== and != MUST result in bool
    }
    else
    {
        if (myTypeIndex < 8 && otherTypeIndex < 8)
        {
            bool IAmSigned = myTypeIndex >= 4;
            bool otherIsSigned = otherTypeIndex >= 4;
            bool sameSign = IAmSigned == otherIsSigned;//(myTypeIndex < 4 && otherTypeIndex < 4) || (myTypeIndex >= 4 && otherTypeIndex >= 4);

            if (sameSign)
            {
                returnTypeIndex = otherTypeIndex > myTypeIndex ? otherTypeIndex : myTypeIndex;
            }
            else
            {
                //when signed op unsigned, convert signed to unsigned
                if (IAmSigned && !otherIsSigned)
                {
                    i32 meUnsignedIndex = myTypeIndex - 4;
                    returnTypeIndex = otherTypeIndex > meUnsignedIndex ? otherTypeIndex : meUnsignedIndex;
                }
                else// if (!IAmSigned && otherIsSigned)
                {
                    i32 otherUnsignedIndex = otherTypeIndex - 4;
                    returnTypeIndex = otherUnsignedIndex > myTypeIndex ? otherUnsignedIndex : myTypeIndex;
                }
            }
        }
        else
        {
            //float * double = double
            if (myTypeIndex == otherTypeIndex)
            {
                returnTypeIndex = myTypeIndex;
            }
            else
            {
                returnTypeIndex = otherTypeIndex > myTypeIndex ? otherTypeIndex : myTypeIndex;
            }
        }
    }
    
    LinxcType* returnType = primitiveTypePtrs[returnTypeIndex];
    LinxcOperatorImpl operation;
    operation.myType = LinxcTypeReference(myType);
    operation.otherType = LinxcTypeReference(otherType);
    operation.op = op;
    operation.ID = LinxcOverloadIs_Operator;
    operation.implicit = false; //doesn't matter, just make sure we reset it
    
    LinxcExpression returnTypeExpr;
    returnTypeExpr.resolvesTo.lastType = NULL;
    returnTypeExpr.data.typeRef = returnType;
    returnTypeExpr.ID = LinxcExpr_TypeRef; //what we're returning after the operation

    LinxcFunc opFunc = LinxcFunc(string(), returnTypeExpr); //these functions don't need names
    
    //our sole argument is the other type. EG: intVar + floatVar = intVar.Add(floatVar)
    LinxcVar* inputArg = (LinxcVar*)this->allocator->Allocate(sizeof(LinxcVar));
    inputArg->type.ID = LinxcExpr_TypeRef;
    inputArg->type.data.typeRef = LinxcTypeReference(otherType);
    inputArg->type.resolvesTo.lastType = NULL;
    inputArg->name = string(this->allocator, "other");
    opFunc.arguments = collections::Array<LinxcVar>(this->allocator, inputArg, 1);

    LinxcOperatorFunc result;
    result.operatorOverride = operation;
    result.function = opFunc;

    //string debug = result.ToString(&defaultAllocator);
    //printf("%s\n", debug.buffer);
    //debug.deinit();

    return result;
}

bool LinxcParser::TokenizeFile(LinxcTokenizer* tokenizer, IAllocator* allocator, LinxcParsedFile* parsingFile)
{
    collections::hashmap<string, LinxcMacro*> identifierToMacro = collections::hashmap<string, LinxcMacro*>(&defaultAllocator, &stringHash, &stringEql);
    tokenizer->tokenStream = collections::vector<LinxcToken>(allocator);
    bool nextMacroIsAttribute = false;
    while (true)
    {
        LinxcToken token = tokenizer->TokenizeAdvance();

        if (token.ID == Linxc_keyword_attribute)
        {
            nextMacroIsAttribute = true;
        }
        else if (token.ID == Linxc_Hash)
        {
            LinxcToken preprocessorDirective = tokenizer->TokenizeAdvance();
            if (preprocessorDirective.ID == Linxc_Keyword_define)
            {
                LinxcToken name = tokenizer->TokenizeAdvance();
                if (name.ID == Linxc_Identifier)
                {
                    LinxcToken next = tokenizer->TokenizeAdvance();
                    if (next.ID == Linxc_LParen)
                    {
                        collections::vector<LinxcToken> macroBody = collections::vector<LinxcToken>(allocator);
                        collections::vector<LinxcToken> macroArgs = collections::vector<LinxcToken>(&defaultAllocator);
                        bool foundEllipsis = false;

                        LinxcToken macroArg = tokenizer->TokenizeAdvance();
                        if (macroArg.ID != Linxc_RParen)
                        {
                            while (true)
                            {
                                if (macroArg.ID == Linxc_Ellipsis)
                                {
                                    macroArgs.Add(macroArg);
                                    foundEllipsis = true;
                                }
                                if (macroArg.ID == Linxc_Identifier)
                                {
                                    if (foundEllipsis)
                                    {
                                        parsingFile->errors.Add(ERR_MSG(allocator, "Preprocessor: No macro arguments allowed after open-ended argument ... !"));
                                        identifierToMacro.deinit();
                                        return false;
                                    }
                                    else
                                    {
                                        macroArgs.Add(macroArg);
                                    }
                                }
                                LinxcToken afterMacroArg = tokenizer->TokenizeAdvance();
                                if (afterMacroArg.ID == Linxc_RParen)
                                {
                                    break;
                                }
                                else if (afterMacroArg.ID == Linxc_Comma)
                                {
                                    macroArg = tokenizer->TokenizeAdvance();
                                }
                                else
                                {
                                    parsingFile->errors.Add(ERR_MSG(allocator, "Preprocessor: Unexpected token after macro argument. Token after macro argument must be either , or )"));
                                    return false;
                                }
                            }
                        }

                        LinxcToken bodyToken = tokenizer->TokenizeAdvance();
                        while (bodyToken.ID != Linxc_Eof && bodyToken.ID != Linxc_Nl)
                        {
                            macroBody.Add(bodyToken);
                            bodyToken = tokenizer->TokenizeAdvance();
                        }
                        LinxcMacro macro;
                        macro.name = name.ToString(allocator);
                        macro.arguments = macroArgs.ToOwnedArrayWith(allocator);
                        macro.body = macroBody;
                        macro.isFunctionMacro = true;
                        parsingFile->definedMacros.Add(macro);
                        
                        if (nextMacroIsAttribute)
                        {
                            parsingFile->definedAttributes.Add(macro);
                            nextMacroIsAttribute = false;
                        }
                        else identifierToMacro.Add(macro.name, parsingFile->definedMacros.Get(parsingFile->definedMacros.count - 1));
                    }
                    else
                    {
                        collections::vector<LinxcToken> macroBody = collections::vector<LinxcToken>(allocator);

                        while (next.ID != Linxc_Eof && next.ID != Linxc_Nl)
                        {
                            macroBody.Add(next);
                            next = tokenizer->TokenizeAdvance();
                        }
                        LinxcMacro macro;
                        macro.name = name.ToString(allocator);
                        macro.arguments = collections::Array<LinxcToken>();
                        macro.body = macroBody;
                        macro.isFunctionMacro = false;
                        parsingFile->definedMacros.Add(macro);
                        if (nextMacroIsAttribute)
                        {
                            parsingFile->definedAttributes.Add(macro);
                            nextMacroIsAttribute = false;
                        }
                        else identifierToMacro.Add(macro.name, parsingFile->definedMacros.Get(parsingFile->definedMacros.count - 1));
                    }
                }
                else
                {
                    parsingFile->errors.Add(ERR_MSG(allocator, "Preprocessor: Expected non-reserved identifier name after #define directive"));
                    return false;
                }
            }
            else if (preprocessorDirective.ID == Linxc_Keyword_include)
            {
                //parser doesn't care about the opening # so dont need to add that

                tokenizer->tokenStream.Add(preprocessorDirective);

                LinxcToken next = tokenizer->TokenizeAdvance();
                if (next.ID != Linxc_MacroString)
                {
                    parsingFile->errors.Add(ERR_MSG(this->allocator, "Expected <file to be included> after #include declaration"));
                    identifierToMacro.deinit();
                    return false;
                }

                if (next.end - 1 <= next.start + 1)
                {
                    parsingFile->errors.Add(ERR_MSG(this->allocator, "#include directive is empty!"));
                }
                else
                {
                    tokenizer->tokenStream.Add(next);
                }
            }
        }
        else
        {
            if (token.ID == Linxc_Identifier)
            {
                string temp = token.ToString(&defaultAllocator);
                LinxcMacro **potentialMacro = identifierToMacro.Get(temp);
                temp.deinit();
                if (potentialMacro != NULL)
                {
                    LinxcMacro* macro = *potentialMacro;

                    if (macro->isFunctionMacro)
                    {
                        LinxcToken next = tokenizer->TokenizeAdvance();
                        if (next.ID != Linxc_LParen)
                        {
                            parsingFile->errors.Add(ERR_MSG(this->allocator, "Expected ( after function macro identifier"));
                            identifierToMacro.deinit();
                            return false;
                        }
                        next = tokenizer->TokenizeAdvance();

                        if (macro->arguments.length == 0)
                        {
                            if (next.ID != Linxc_RParen)
                            {
                                parsingFile->errors.Add(ERR_MSG(this->allocator, "This macro does not have arguments"));
                                identifierToMacro.deinit();
                                return false;
                            }
                            if (macro->body.count > 0)
                            {
                                for (usize i = 0; i < macro->body.count; i++)
                                {
                                    tokenizer->tokenStream.Add(*macro->body.Get(i));
                                }
                            }
                            continue;
                        }
                        else
                        {
                            i32 expectedArguments = macro->arguments.length;
                            if (macro->arguments.data[expectedArguments - 1].ID == Linxc_Ellipsis)
                            {
                                expectedArguments = -1;
                            }

                            ArenaAllocator arena = ArenaAllocator(&defaultAllocator);
                            
                            collections::vector<LinxcToken> tokensInArg = collections::vector<LinxcToken>(&arena.asAllocator);
                            collections::hashmap<string, collections::Array<LinxcToken>> argsInMacro = collections::hashmap<string, collections::Array<LinxcToken>>(&arena.asAllocator, &stringHash, &stringEql);

                            while (next.ID != Linxc_RParen)
                            {
                                //dont actually add the comma to the tokenstream
                                if (next.ID != Linxc_Comma)
                                    tokensInArg.Add(next);

                                next = tokenizer->TokenizeAdvance();
                                if (next.ID == Linxc_RParen || next.ID == Linxc_Comma)
                                {
                                    collections::Array<LinxcToken> argsTokenStream = tokensInArg.ToOwnedArray();
                                    string currentArgName = macro->arguments.data[argsInMacro.Count].ToString(&arena.asAllocator);
                                    argsInMacro.Add(currentArgName, argsTokenStream);
                                    tokensInArg = collections::vector<LinxcToken>(&arena.asAllocator);
                                }
                                if (next.ID == Linxc_RParen)
                                {
                                    break;
                                }
                            }

                            //expected arguments will be -1 if open ended
                            if (expectedArguments > -1)
                            {
                                if (argsInMacro.Count != expectedArguments)
                                {
                                    parsingFile->errors.Add(ERR_MSG(this->allocator, "Improper amount of arguments provided to macro"));
                                    identifierToMacro.deinit();
                                    argsInMacro.deinit();
                                    return false;
                                }
                            }

                            if (macro->body.count > 0)
                            {
                                for (usize i = 0; i < macro->body.count; i++)
                                {
                                    LinxcToken macroToken = *macro->body.Get(i);

                                    string macroTokenName = macroToken.ToString(&defaultAllocator);
                                    collections::Array<LinxcToken> *inputToArgs = argsInMacro.Get(macroTokenName);
                                    if (inputToArgs != NULL)
                                    {
                                        for (usize j = 0; j < inputToArgs->length; j++)
                                        {
                                            tokenizer->tokenStream.Add(inputToArgs->data[j]);
                                        }
                                    }
                                    else
                                    {
                                        tokenizer->tokenStream.Add(macroToken);
                                    }
                                    macroTokenName.deinit();
                                }
                            }
                            arena.deinit();
                        }
                    }
                    else
                    {
                        if (macro->body.count > 0)
                        {
                            for (usize i = 0; i < macro->body.count; i++)
                            {
                                tokenizer->tokenStream.Add(*macro->body.Get(i));
                            }
                        }
                    }
                    continue;
                }
            }
            tokenizer->tokenStream.Add(token);
        }

        if (token.ID == Linxc_Eof || token.ID == Linxc_Invalid)
        {
            break;
        }
    }
    identifierToMacro.deinit();
    return true;
}
option<LinxcExpression> LinxcParser::ParseExpressionPrimary(LinxcParserState *state, option<LinxcExpression> prevScopeIfAny = option<LinxcExpression>())
{
    LinxcToken token = state->tokenizer->NextUntilValid();
    switch (token.ID)
    {
        case Linxc_Asterisk:
        case Linxc_Minus:
        case Linxc_Bang:
        case Linxc_Ampersand:
        case Linxc_Tilde:
            {
                option<LinxcExpression> nextPrimaryOpt = this->ParseExpressionPrimary(state);
                if (nextPrimaryOpt.present)
                {
                    LinxcExpression expression = this->ParseExpression(state, nextPrimaryOpt.value, 4);

                    //attempting to modify a type name
                    if (expression.resolvesTo.lastType == NULL)
                    {
                        state->parsingFile->errors.Add(ERR_MSG(this->allocator, "Attempting to place a modifying operator on a type name. You can only modify literals and variables."));
                    }
                    else
                    {
                        LinxcModifiedExpression* modified = (LinxcModifiedExpression*)this->allocator->Allocate(sizeof(LinxcModifiedExpression));
                        modified->expression = expression;
                        modified->modification = token.ID;

                        LinxcExpression result;
                        result.data.modifiedExpression = modified;
                        result.ID = LinxcExpr_Modified;
                        //TODO: check what the modifier does to the expression's original results based on operator overloading
                        result.resolvesTo = expression.resolvesTo;

                        if (token.ID == Linxc_Asterisk || token.ID == Linxc_Ampersand)
                        {
                            //attempting to reference/dereference a literal
                            if (expression.ID == LinxcExpr_Literal)
                            {
                                state->parsingFile->errors.Add(ERR_MSG(this->allocator, "Attempting to reference/dereference a literal. This is not possible as literals do not have memory addresses!"));
                            }
                        }

                        if (token.ID == Linxc_Asterisk) //if we put *variableName, our pointer count goes down by 1 when resolved
                        {
                            if (result.resolvesTo.pointerCount > 0)
                            {
                                result.resolvesTo.pointerCount -= 1;
                            }
                            else
                            {
                                state->parsingFile->errors.Add(ERR_MSG(this->allocator, "Attempting to dereference a non-pointer variable"));
                            }
                        }
                        else if (token.ID == Linxc_Ampersand)
                        {
                            result.resolvesTo.pointerCount += 1;
                        }
                        return option<LinxcExpression>(result);
                    }
                }
                break;
            }
        case Linxc_LParen:
            {
                option<LinxcExpression> primaryOpt = this->ParseExpressionPrimary(state);
                if (primaryOpt.present)
                {
                    LinxcExpression expression = this->ParseExpression(state, primaryOpt.value, -1);
                    
                    if (state->tokenizer->PeekNextUntilValid().ID == Linxc_RParen)
                    {
                        state->tokenizer->NextUntilValid();
                    }
                    else
                    {
                        state->parsingFile->errors.Add(ERR_MSG(this->allocator, "Expected )"));
                        return option< LinxcExpression>();
                    }

                    //check if expression is a type reference. If so, then this is a cast.
                    if (expression.resolvesTo.lastType == NULL)
                    {
                        //parse the next thing to be casted
                        option<LinxcExpression> nextPrimaryOpt = this->ParseExpressionPrimary(state);
                        if (!nextPrimaryOpt.present)
                        {
                            return option< LinxcExpression>();
                        }
                        LinxcExpression nextExpression = this->ParseExpression(state, nextPrimaryOpt.value, 3); //3 because cast itself is 3

                        LinxcTypeCast* typeCast = (LinxcTypeCast*)this->allocator->Allocate(sizeof(LinxcTypeCast));
                        typeCast->castToType = expression;
                        typeCast->expressionToCast = nextExpression;

                        LinxcExpression result;
                        result.data.typeCast = typeCast;//expression.ToHeap(this->allocator);
                        result.ID = LinxcExpr_TypeCast;
                        result.resolvesTo = expression.AsTypeReference().value;
                        return option<LinxcExpression>(result);
                    }
                    else //If not, it's a nested expression
                    {
                        return option<LinxcExpression>(expression);
                    }
                }
                else
                    break;
            }
            break;
        case Linxc_Identifier:
            {
                //move back so we can parse token with ParseIdentifier
                state->tokenizer->Back();
                option<LinxcExpression> result = this->ParseIdentifier(state, prevScopeIfAny);
                //if the result is a function ref, it may be a function call instead
                //since function calls are not handled by the operator chaining of scope resolutions,
                //we have to handle it here as a primary expression
                if (!result.present)
                {
                    ERR_MSG msg = ERR_MSG(this->allocator, "No type or variable of name ");
                    msg.AppendDeinit(token.ToString(&defaultAllocator));
                    msg.Append(" exists");
                    if (prevScopeIfAny.present)
                    {
                        msg.Append(" within scope ");
                        msg.AppendDeinit(prevScopeIfAny.value.ToString(&defaultAllocator));
                    }
                    state->parsingFile->errors.Add(msg);
                    return option<LinxcExpression>();
                }
                if (result.value.ID == LinxcExpr_FunctionRef)
                {
                    if (state->tokenizer->PeekNextUntilValid().ID == Linxc_LParen)
                    {
                        //parse input args
                        state->tokenizer->NextUntilValid();

                        collections::vector<LinxcExpression> inputArgs = collections::vector<LinxcExpression>(&defaultAllocator);
                        LinxcToken peekNext = state->tokenizer->PeekNextUntilValid();
                        if (peekNext.ID == Linxc_RParen)
                        {
                            state->tokenizer->NextUntilValid();
                        }
                        else
                        {
                            usize i = 0;
                            //parse function input
                            while (true)
                            {
                                bool tooManyArgs = i >= result.value.data.functionRef->arguments.length;

                                option<LinxcExpression> primaryOpt = this->ParseExpressionPrimary(state);
                                if (!primaryOpt.present)
                                {
                                    break;
                                }
                                LinxcExpression fullExpression = this->ParseExpression(state, primaryOpt.value, -1);

                                if (fullExpression.resolvesTo.lastType == NULL)
                                {
                                    state->parsingFile->errors.Add(ERR_MSG(this->allocator, "Cannot parse a type name as a variable. Did you mean sizeof(), nameof() or typeof() instead?"));
                                }

                                inputArgs.Add(fullExpression);

                                //this wont be present if our variable is open ended and typeless
                                option<LinxcTypeReference> expectedType = result.value.data.functionRef->arguments.data[i].type.AsTypeReference();

                                if (expectedType.present && !tooManyArgs)
                                {
                                    expectedType.value.isConst = result.value.data.functionRef->arguments.data[i].isConst;

                                    //printf("is const: %s\n", expectedType.value.isConst ? "true" : "false");
                                    if (!CanAssign(expectedType.value, fullExpression.resolvesTo))
                                    {
                                        ERR_MSG msg = ERR_MSG(this->allocator, "Argument of type ");
                                        msg.AppendDeinit(fullExpression.resolvesTo.ToString(&defaultAllocator));
                                        msg.Append(" cannot be implicitly converted to parameter type ");
                                        msg.AppendDeinit(expectedType.value.ToString(&defaultAllocator));
                                        state->parsingFile->errors.Add(msg);
                                    }
                                }

                                peekNext = state->tokenizer->PeekNextUntilValid();
                                if (peekNext.ID == Linxc_Comma)
                                {
                                    state->tokenizer->NextUntilValid();
                                }
                                else if (peekNext.ID == Linxc_RParen)
                                {
                                    state->tokenizer->NextUntilValid();
                                    break;
                                }
                                else
                                {
                                    state->parsingFile->errors.Add(ERR_MSG(this->allocator, "Expected , or ) after function input argument"));
                                }
                                //if we reach an open ended function, that means we've come to the end. Do not parse further
                                if (result.value.data.functionRef->arguments.data[i].name != "...")
                                {
                                    i += 1;
                                }
                            }
                        }

                        //this only applies to non-open ended functions
                        if ((result.value.data.functionRef->arguments.length == 0 || result.value.data.functionRef->arguments.data[result.value.data.functionRef->arguments.length - 1].name != "...") && inputArgs.count > result.value.data.functionRef->arguments.length)
                        {
                            ERR_MSG msg = ERR_MSG(this->allocator, "Provided too many input params to function, expected ");
                            msg.Append(result.value.data.functionRef->arguments.length);
                            msg.Append(" arguments, provided ");
                            msg.Append(inputArgs.count);
                            state->parsingFile->errors.Add(msg);
                        }
                        else if (inputArgs.count < result.value.data.functionRef->necessaryArguments)
                        {
                            ERR_MSG msg = ERR_MSG(this->allocator, "Provided too few input params to function, expected ");
                            msg.Append((u64)result.value.data.functionRef->necessaryArguments);
                            msg.Append(" arguments, provided ");
                            msg.Append(inputArgs.count);
                            state->parsingFile->errors.Add(msg);
                        }
                        LinxcExpression finalResult;
                        finalResult.ID = LinxcExpr_FuncCall;
                        finalResult.data.functionCall.func = result.value.data.functionRef;
                        finalResult.data.functionCall.inputParams = inputArgs.ToOwnedArrayWith(this->allocator);
                        finalResult.data.functionCall.templateSpecializations = collections::Array<LinxcTypeReference>(); //todo
                        finalResult.resolvesTo = result.value.data.functionRef->returnType.AsTypeReference().value;

                        return option<LinxcExpression>(finalResult);
                    }
                    else
                    {
                        //don't throw an error as &functionName is a technically 2 valid primary parses
                        return option<LinxcExpression>(result);
                    }
                }
                return result;
            }
            break;
        case Linxc_CharLiteral:
        case Linxc_StringLiteral:
        case Linxc_FloatLiteral:
        case Linxc_IntegerLiteral:
        case Linxc_Keyword_true:
        case Linxc_Keyword_false:
            {
                LinxcExpression result;
                result.data.literal = token.ToString(this->allocator);
                result.ID = LinxcExpr_Literal;

                string temp;
                if (token.ID == Linxc_Keyword_true || token.ID == Linxc_Keyword_false)
                {
                    temp = string("bool");
                }
                else if (token.ID == Linxc_FloatLiteral)
                {
                    temp = string("float");
                }
                else if (token.ID == Linxc_IntegerLiteral)
                {
                    temp = string("i32");
                }
                else if (token.ID == Linxc_CharLiteral)
                {
                    temp = string("u8");
                }
                else if (token.ID == Linxc_StringLiteral)
                {
                    temp = string("u8");
                }
                LinxcType* resolvesToType = this->globalNamespace.types.Get(temp);
                result.resolvesTo = LinxcTypeReference(resolvesToType);
                if (token.ID == Linxc_StringLiteral)
                {
                    result.resolvesTo.isConst = true;
                    result.resolvesTo.pointerCount = 1;
                }
                temp.deinit();
                return option<LinxcExpression>(result);
            }
        default:
            {
                //move back so we can parse token with ParseIdentifier (which handles primitive types too)
                if (LinxcIsPrimitiveType(token.ID))
                {
                    //cannot call anything with primitive type so, and result is guaranteed to be type reference so
                    state->tokenizer->Back();
                    return this->ParseIdentifier(state, option<LinxcExpression>());
                }
            }
            break;
    }
    return option<LinxcExpression>();
}
LinxcExpression LinxcParser::ParseExpression(LinxcParserState *state, LinxcExpression primary, i32 startingPrecedence)
{
    LinxcExpression lhs = primary;
    while (true)
    {
        LinxcToken op = state->tokenizer->PeekNextUntilValid();
        i32 precedence = GetPrecedence(op.ID);
        if (op.ID == Linxc_Eof || op.ID == Linxc_Semicolon || precedence == -1 || precedence < startingPrecedence)
        {
            break;
        }
        state->tokenizer->NextUntilValid();

        option<LinxcExpression> prevScopeIfAny;
        if (op.ID == Linxc_ColonColon || op.ID == Linxc_Period || op.ID == Linxc_Arrow)
        {
            //the lhs is only a valid scope if we are in a scope resolution operator
            prevScopeIfAny = option<LinxcExpression>(lhs);
        }
        else prevScopeIfAny = option<LinxcExpression>();
        option<LinxcExpression> rhsOpt = this->ParseExpressionPrimary(state, prevScopeIfAny);
        if (!rhsOpt.present)
        {
            //state->parsingFile->errors.Add(ERR_MSG(&defaultAllocator, "Error parsing right side of operator"));
            break;
        }

        while (true)
        {
            LinxcToken next = state->tokenizer->PeekNextUntilValid();
            i32 nextPrecedence = GetPrecedence(next.ID);
            i8 nextAssociation = GetAssociation(next.ID);
            if (op.ID == Linxc_Eof || op.ID == Linxc_Semicolon || nextPrecedence == -1 || !((nextPrecedence > precedence) || (nextAssociation == 1 && precedence == nextPrecedence)))
            {
                break;
            }
            i32 nextFuncPrecedence = precedence;
            if (nextPrecedence > precedence)
            {
                nextFuncPrecedence += 1;
            }
            rhsOpt.value = this->ParseExpression(state, rhsOpt.value, nextFuncPrecedence);
        }

        LinxcOperator *operatorCall = (LinxcOperator*)this->allocator->Allocate(sizeof(LinxcOperator));
        operatorCall->leftExpr = lhs;
        operatorCall->rightExpr = rhsOpt.value;
        operatorCall->operatorType = op.ID;

        option<LinxcTypeReference> resolvesTo = operatorCall->EvaluatePossible();
        if (!resolvesTo.present)
        {
            ERR_MSG msg = ERR_MSG(this->allocator, "Type ");
            msg.AppendDeinit(operatorCall->leftExpr.resolvesTo.ToString(&defaultAllocator));
            msg.Append(" cannot be ");
            msg.Append(LinxcTokenIDToString(op.ID));
            msg.Append("'d with ");
            msg.AppendDeinit(operatorCall->rightExpr.resolvesTo.ToString(&defaultAllocator));
            state->parsingFile->errors.Add(msg);
        }
        else
        {
            lhs.resolvesTo = resolvesTo.value;
        }
        lhs.data.operatorCall = operatorCall;
        lhs.ID = LinxcExpr_OperatorCall;
    }
    return lhs;
}
option<LinxcExpression> LinxcParser::ParseIdentifier(LinxcParserState *state, option<LinxcExpression> parentScopeOverride)
{
    LinxcExpression result;
    result.ID = LinxcExpr_None;
    LinxcToken token = state->tokenizer->NextUntilValid();
    string identifierName = token.ToString(&defaultAllocator);

    if (LinxcIsPrimitiveType(token.ID))
    {
        LinxcType *type = this->globalNamespace.types.Get(identifierName);
        LinxcTypeReference reference;
        reference.isConst = false;
        reference.lastType = type;
        reference.templateArgs = collections::Array<LinxcTypeReference>();

        result.ID = LinxcExpr_TypeRef;
        result.data.typeRef = reference;
        result.resolvesTo.lastType = NULL; // = LinxcTypeReference(type);
    }
    else
    {
        //when preceding is none, 
        //  use global namespaces + using namespace statements
        //  if global has struct, use struct as well
        //when preceding is namespace, use that namespace
        //when preceding is struct, use that struct

        //also, must resolve conflicting names error here instead of defaulting to some arbitrary selection order

        if (!parentScopeOverride.present)
        {
            //check local variables

            LinxcVar **asLocalVar = state->varsInScope.Get(identifierName);
            if (asLocalVar != NULL)
            {
                result.ID = LinxcExpr_Variable;
                //this SHOULD point to the location of the var stored in the AST
                result.data.variable = *asLocalVar;
                result.resolvesTo = result.data.variable->type.AsTypeReference().value;
                result.resolvesTo.isConst = result.data.variable->isConst;
            }
            else
            {
                //check state namespaces
                LinxcNamespace* toCheck = state->currentNamespace;
                while (toCheck != NULL)
                {
                    LinxcFunc* asFunction = toCheck->functions.Get(identifierName);
                    if (asFunction != NULL)
                    {
                        result.ID = LinxcExpr_FunctionRef;
                        result.data.functionRef = asFunction;
                        result.resolvesTo = asFunction->returnType.AsTypeReference().value;
                    }
                    else
                    {
                        LinxcVar* asVar = toCheck->variables.Get(identifierName);
                        if (asVar != NULL)
                        {
                            result.ID = LinxcExpr_Variable;
                            result.data.variable = asVar;
                            //this is guaranteed to be present as a variable would only have a typename-resolveable expression as it's type
                            result.resolvesTo = asVar->type.AsTypeReference().value;
                            result.resolvesTo.isConst = asVar->isConst;
                        }
                        else
                        {
                            LinxcType* asType = toCheck->types.Get(identifierName);
                            if (asType != NULL)
                            {
                                result.ID = LinxcExpr_TypeRef;
                                result.data.typeRef = asType;
                                result.resolvesTo.lastType = NULL;
                            }
                            else
                            {
                                LinxcNamespace* asNamespace = toCheck->subNamespaces.Get(identifierName);
                                if (asNamespace != NULL)
                                {
                                    result.ID = LinxcExpr_NamespaceRef;
                                    
                                    result.data.namespaceRef = asNamespace;
                                    result.resolvesTo.lastType = NULL;
                                }
                            }
                        }
                    }

                    toCheck = toCheck->parentNamespace;
                }

                LinxcType* typeCheck = state->parentType;
                if (typeCheck != NULL)
                {
                    LinxcFunc* asFunction = typeCheck->FindFunction(identifierName);
                    if (asFunction != NULL)
                    {
                        result.ID = LinxcExpr_FunctionRef;
                        result.data.functionRef = asFunction;
                        result.resolvesTo = asFunction->returnType.AsTypeReference().value;
                    }
                    else
                    {
                        LinxcVar* asVar = typeCheck->FindVar(identifierName);
                        if (asVar != NULL)
                        {
                            result.ID = LinxcExpr_Variable;
                            result.data.variable = asVar;
                            result.resolvesTo = asVar->type.AsTypeReference().value;
                            result.resolvesTo.isConst = asVar->isConst;
                        }
                        else
                        {
                            LinxcType* asType = typeCheck->FindSubtype(identifierName);
                            if (asType != NULL)
                            {
                                result.ID = LinxcExpr_TypeRef;
                                result.data.typeRef = asType;
                                result.resolvesTo.lastType = NULL;
                            }
                        }
                    }

                    typeCheck = typeCheck->parentType;
                }
            }
        }
        else if (parentScopeOverride.value.ID == LinxcExpr_NamespaceRef)
        {
            LinxcNamespace *toCheck = parentScopeOverride.value.data.namespaceRef;
            //only need to check immediate parent scope's namespace
            LinxcFunc *asFunction = toCheck->functions.Get(identifierName);
            if (asFunction != NULL)
            {
                result.ID = LinxcExpr_FunctionRef;
                result.data.functionRef = asFunction;
                result.resolvesTo = asFunction->returnType.AsTypeReference().value;
            }
            else
            {
                LinxcVar *asVar = toCheck->variables.Get(identifierName);
                if (asVar != NULL)
                {
                    result.ID = LinxcExpr_Variable;
                    result.data.variable = asVar;
                    result.resolvesTo = asVar->type.AsTypeReference().value;
                    result.resolvesTo.isConst = asVar->isConst;
                }
                else
                {
                    LinxcType *asType = toCheck->types.Get(identifierName);
                    if (asType != NULL)
                    {
                        result.ID = LinxcExpr_TypeRef;
                        result.data.typeRef = asType;
                        result.resolvesTo.lastType = NULL;
                    }
                }
            }
        }
        else if (parentScopeOverride.value.ID == LinxcExpr_TypeRef || parentScopeOverride.value.ID == LinxcExpr_Variable)
        {
            LinxcType* toCheck;
            if (parentScopeOverride.value.ID == LinxcExpr_Variable)
            {
                toCheck = parentScopeOverride.value.data.variable->type.AsTypeReference().value.lastType;
            }
            else
            {
                toCheck = parentScopeOverride.value.data.typeRef.lastType;
            }
            //only need to check immediate parent scope's namespace
            LinxcFunc *asFunction = toCheck->FindFunction(identifierName);
            if (asFunction != NULL)
            {
                result.ID = LinxcExpr_FunctionRef;
                result.data.functionRef = asFunction;
                result.resolvesTo = asFunction->returnType.AsTypeReference().value;
            }
            else
            {
                LinxcVar *asVar = toCheck->FindVar(identifierName);
                if (asVar != NULL)
                {
                    result.ID = LinxcExpr_Variable;
                    result.data.variable = asVar;
                    result.resolvesTo = asVar->type.AsTypeReference().value;
                    result.resolvesTo.isConst = asVar->isConst;
                }
                else
                {
                    LinxcType *asType = toCheck->FindSubtype(identifierName);
                    if (asType != NULL)
                    {
                        result.ID = LinxcExpr_TypeRef;
                        result.data.typeRef = asType;
                        result.resolvesTo.lastType = NULL;
                    }
                }
            }
        }
    }
    if (result.ID == LinxcExpr_TypeRef)
    {
        while (state->tokenizer->PeekNextUntilValid().ID == Linxc_Asterisk)
        {
            result.data.typeRef.pointerCount += 1;
            state->tokenizer->NextUntilValid();
        }
    }

    identifierName.deinit();
    if (result.ID == LinxcExpr_None)
    {
        return option<LinxcExpression>();
    }
    return option<LinxcExpression>(result);
}

collections::Array<LinxcVar> LinxcParser::ParseFunctionArgs(LinxcParserState *state, u32* necessaryArguments)
{
    collections::vector<ERR_MSG> *errors = &state->parsingFile->errors;
    collections::vector<LinxcVar> variables = collections::vector<LinxcVar>(this->allocator);

    LinxcToken peekNext = state->tokenizer->PeekNextUntilValid();
    if (peekNext.ID == Linxc_RParen)
    {
        state->tokenizer->NextUntilValid();
        return variables.ToOwnedArray();
    }

    bool foundOptionalVariable = false;
    bool foundEllipsis = false;

    bool isConst = false;

    while (true)
    {
        peekNext = state->tokenizer->PeekNextUntilValid();
        if (peekNext.ID == Linxc_Keyword_const)
        {
            isConst = true;
            state->tokenizer->NextUntilValid();
            continue;
        }
        else if (peekNext.ID == Linxc_Ellipsis)
        {
            state->tokenizer->NextUntilValid();
            //typeless open ended function
            foundEllipsis = true;

            LinxcVar openEndedVar;
            openEndedVar.defaultValue = option<LinxcExpression>();
            openEndedVar.name = string(this->allocator, "...");
            openEndedVar.type.ID = LinxcExpr_None;
            openEndedVar.isConst = isConst;

            variables.Add(openEndedVar);
            
            peekNext = state->tokenizer->PeekNextUntilValid();
            if (peekNext.ID == Linxc_RParen)
            {
                state->tokenizer->NextUntilValid();
                break;
            }
            else
            {
                state->parsingFile->errors.Add(ERR_MSG(this->allocator, "Input params after open-ended argument (...) are not allowed"));
            }
        }

        option<LinxcExpression> primaryOpt = this->ParseExpressionPrimary(state);
        if (!primaryOpt.present) //error encountered
        {
            return collections::Array<LinxcVar>();
        }
        LinxcExpression typeExpression = this->ParseExpression(state, primaryOpt.value, -1);

        //if it resolves to nothing, that means it's a variable type.
        if (typeExpression.resolvesTo.lastType == NULL)
        {
            typeExpression.resolvesTo.isConst = isConst;
            isConst = false;
            LinxcToken varNameToken = state->tokenizer->NextUntilValid();
            if (varNameToken.ID == Linxc_Ellipsis)
            {
                foundEllipsis = true;
            }
            else if (varNameToken.ID != Linxc_Identifier)
            {
                errors->Add(ERR_MSG(this->allocator, "Expected identifier after variable type name"));
                break;
            }
            string varName = varNameToken.ToString(this->allocator);
            LinxcVar var = LinxcVar(varName, typeExpression, option<LinxcExpression>());
            var.isConst = typeExpression.resolvesTo.isConst;
            *necessaryArguments = *necessaryArguments + 1;

            LinxcToken next = state->tokenizer->NextUntilValid();
            if (next.ID == Linxc_Comma)
            {
                if (foundOptionalVariable)
                {
                    state->parsingFile->errors.Add(ERR_MSG(this->allocator, "All function arguments without default values must be placed before those that have"));
                    break;
                }
                else if (foundEllipsis)
                {
                    state->parsingFile->errors.Add(ERR_MSG(this->allocator, "Input params after open-ended argument (...) are not allowed"));
                    break;
                }
            }
            else if (next.ID == Linxc_Equal)
            {
                if (foundEllipsis)
                {
                    state->parsingFile->errors.Add(ERR_MSG(this->allocator, "Open-ended arguments (...) cannot have default values"));
                    break;
                }

                option<LinxcExpression> primaryOpt = this->ParseExpressionPrimary(state);
                if (!primaryOpt.present)
                {
                    break;
                }
                LinxcExpression defaultValueExpression = this->ParseExpression(state, primaryOpt.value, -1);

                if (CanAssign(typeExpression.AsTypeReference().value, defaultValueExpression.resolvesTo))
                {

                    var.defaultValue = option<LinxcExpression>(defaultValueExpression);
                }
                else
                {
                    state->parsingFile->errors.Add(ERR_MSG(this->allocator, "Input argument's initial value is not of the same type as the argument itself, and no implicit cast was found."));
                }
                foundOptionalVariable = true;
            }
            variables.Add(var);
            if (next.ID == Linxc_RParen)
            {
                break;
            }
        }
        else if (typeExpression.ID == LinxcExpr_NamespaceRef)
        {
            errors->Add(ERR_MSG(this->allocator, "Attempted to use a namespace as variable type"));
            break;
        }
        else if (typeExpression.ID == LinxcExpr_Variable)
        {
            errors->Add(ERR_MSG(this->allocator, "Attempted to use another variable as variable type"));
            break;
        }
        else
        {
            errors->Add(ERR_MSG(this->allocator, "Expression not valid as a variable type"));
            break;
        }
    }
    return variables.ToOwnedArray();
}

option<collections::vector<LinxcStatement>> LinxcParser::ParseCompoundStmt(LinxcParserState* state)
{
    bool isComment = false;
    bool expectSemicolon = false;

    //when flagged to true (upon encountering an error), skip parsing the entire file until a semicolon is reached.
    //this is to avoid causing even more errors because the first member of an expression is invalid.
    bool errorSkipUntilSemicolon = false;
    collections::vector<ERR_MSG>* errors = &state->parsingFile->errors;
    collections::vector<LinxcStatement> result = collections::vector<LinxcStatement>(this->allocator);
    //collections::vector<ERR_MSG> errors = collections::vector<ERR_MSG>(this->allocator);
    LinxcTokenizer* tokenizer = state->tokenizer;

    bool nextIsConst = false;
    while (true)
    {
        bool toBreak = false;
        usize prevIndex = tokenizer->prevIndex;
        LinxcTokenID prevTokenID = tokenizer->prevTokenID;
        LinxcToken token = tokenizer->Next();

        if (errorSkipUntilSemicolon)
        {
            if (token.ID == Linxc_Semicolon || token.ID == Linxc_LineComment || token.ID == Linxc_MultiLineComment || token.ID == Linxc_Hash || (token.ID == Linxc_RBrace && state->endOn == LinxcEndOn_RBrace))
            {
                errorSkipUntilSemicolon = false;
            }
            else
            {
                continue;
            }
        }
        if (!isComment)
        {
            if (token.ID == Linxc_Semicolon && expectSemicolon)
            {
                expectSemicolon = false;
                continue;
            }
            else if (expectSemicolon)
            {
                errors->Add(ERR_MSG(this->allocator, "Expected semicolon"));
                expectSemicolon = false; //dont get the same error twice
            }
        }

        switch (token.ID)
        {
        case Linxc_Keyword_const:
        {
            if (isComment)
            {
                continue;
            }
            nextIsConst = true;
        }
        break;
        case Linxc_Nl:
            isComment = false;
            break;
        case Linxc_Keyword_include:
        {
            if (isComment)
            {
                continue;
            }
            if (nextIsConst)
            {
                errors->Add(ERR_MSG(this->allocator, "Cannot declare a include statement as const"));
                nextIsConst = false;
            }

            LinxcToken next = tokenizer->Next();
            if (next.ID != Linxc_MacroString)
            {
                errors->Add(ERR_MSG(this->allocator, "Expected <file to be included> after #include declaration"));
                toBreak = true;
                break;
            }

            if (next.end - 1 <= next.start + 1)
            {
                errors->Add(ERR_MSG(this->allocator, "#include directive is empty!"));
            }
            else
            {
                //-2 because its - (next.start + 1)
                string macroString = string(this->allocator, tokenizer->buffer + next.start + 1, next.end - 2 - next.start);

                LinxcIncludeStatement includeStatement = LinxcIncludeStatement();
                includeStatement.includedFile = NULL;
                includeStatement.includeString = macroString;

                LinxcStatement stmt;
                stmt.data.includeStatement = includeStatement;
                stmt.ID = LinxcStmt_Include;
                result.Add(stmt);
                //printf("included %s\n", macroString.buffer);
            }

            //let's not deal with #includes until we're ready
            //this is probably the biggest can of worms there is
            //20/11/2023
        }
        break;
        //Linxc expects <name> to be after struct keyword. There are no typedef struct {} <name> here.
        case Linxc_Keyword_namespace:
        {
            if (isComment)
            {
                continue;
            }
            if (nextIsConst)
            {
                errors->Add(ERR_MSG(this->allocator, "Cannot declare a namespace as const"));
                nextIsConst = false;
            }

            LinxcToken namespaceName = tokenizer->Next();

            if (namespaceName.ID != Linxc_Identifier)
            {
                errors->Add(ERR_MSG(this->allocator, "Expected a valid namespace name after namespace keyword!"));
            }
            else
            {
                string namespaceNameStrTemp = namespaceName.ToString(&defaultAllocator);
                LinxcNamespace* thisNamespace = state->currentNamespace->subNamespaces.Get(namespaceNameStrTemp);

                if (thisNamespace == NULL)
                {
                    string namespaceNameStr = namespaceName.ToString(this->allocator);
                    LinxcNamespace newNamespace = LinxcNamespace(this->allocator, namespaceNameStr);
                    newNamespace.parentNamespace = state->currentNamespace;
                    state->currentNamespace->subNamespaces.Add(namespaceNameStr, newNamespace);
                    thisNamespace = state->currentNamespace->subNamespaces.Get(namespaceNameStr);
                }

                LinxcToken next = tokenizer->PeekNextUntilValid();
                if (next.ID != Linxc_LBrace)
                {
                    errors->Add(ERR_MSG(this->allocator, "Expected { after namespace name!"));
                    //toBreak = true;
                    //break;
                }
                else tokenizer->NextUntilValid();

                LinxcParserState nextState = LinxcParserState(state->parser, state->parsingFile, state->tokenizer, LinxcEndOn_RBrace, false, state->parsingLinxci);
                //nextState.parentType = state->parentType;
                nextState.currentNamespace = thisNamespace;

                option<collections::vector<LinxcStatement>> namespaceScopeBody = this->ParseCompoundStmt(&nextState);

                LinxcNamespaceScope namespaceScope = LinxcNamespaceScope();
                namespaceScope.referencedNamespace = thisNamespace;
                if (namespaceScopeBody.present)
                {
                    namespaceScope.body = namespaceScopeBody.value;
                }
                LinxcStatement stmt;
                stmt.data.namespaceScope = namespaceScope;
                stmt.ID = LinxcStmt_Namespace;
                result.Add(stmt);

                namespaceNameStrTemp.deinit();
                nextState.deinit();
            }
        }
        break;
        case Linxc_Keyword_struct:
        {
            if (isComment)
            {
                continue;
            }
            if (nextIsConst)
            {
                errors->Add(ERR_MSG(this->allocator, "Cannot declare a struct as const in Linxc"));
                nextIsConst = false;
            }

            LinxcToken structName = tokenizer->Next();

            //printf("STRUCT FOUND: %s\n", structName.ToString(this->allocator).buffer);

            if (structName.ID != Linxc_Identifier)
            {
                errors->Add(ERR_MSG(this->allocator, "Expected a valid struct name after struct keyword!"));
            }
            else
            {
                //declare new struct
                LinxcType type = LinxcType(allocator, structName.ToString(allocator), state->currentNamespace, state->parentType);

                LinxcToken next = tokenizer->PeekNextUntilValid();
                if (next.ID != Linxc_LBrace)
                {
                    errors->Add(ERR_MSG(this->allocator, "Expected { after struct name!"));
                    //toBreak = true;
                    //break;
                }
                else tokenizer->NextUntilValid();

                LinxcType* ptr;
                if (state->parentType != NULL)
                {
                    state->parentType->subTypes.Add(type);
                    ptr = state->parentType->subTypes.Get(state->parentType->subTypes.count - 1);
                }
                else
                {
                    state->currentNamespace->types.Add(type.name, type);
                    ptr = state->currentNamespace->types.Get(type.name);
                }

                LinxcParserState nextState = LinxcParserState(state->parser, state->parsingFile, state->tokenizer, LinxcEndOn_RBrace, false, state->parsingLinxci);
                nextState.parentType = ptr;
                nextState.endOn = LinxcEndOn_RBrace;
                nextState.currentNamespace = state->currentNamespace;
                state->parsingFile->definedTypes.Add(ptr);

                option<collections::vector<LinxcStatement>> structBody = this->ParseCompoundStmt(&nextState);

                if (structBody.present)
                {
                    ptr->body = structBody.value;
                }

                LinxcStatement stmt;
                stmt.data.typeDeclaration = ptr;
                stmt.ID = LinxcStmt_TypeDecl;
                result.Add(stmt);
                nextState.deinit();

                expectSemicolon = true;
            }
        }
        break;
        case Linxc_LineComment:
        {
            isComment = true;
        }
        case Linxc_Keyword_i8:
        case Linxc_Keyword_i16:
        case Linxc_Keyword_i32:
        case Linxc_Keyword_i64:
        case Linxc_Keyword_u8:
        case Linxc_Keyword_u16:
        case Linxc_Keyword_u32:
        case Linxc_Keyword_u64:
        case Linxc_Keyword_float:
        case Linxc_Keyword_double:
        case Linxc_Keyword_char:
        case Linxc_Keyword_bool:
        case Linxc_Keyword_void:
        //for *varType = 5;
        //no other modifier is able to be used in this context
        case Linxc_Asterisk:
        case Linxc_Identifier:
        {
            if (isComment)
            {
                continue;
            }
            //move backwards
            tokenizer->Back();

            option<LinxcExpression> typeExpressionOpt = this->ParseExpressionPrimary(state);
            if (typeExpressionOpt.present)
            {
                LinxcExpression expr = this->ParseExpression(state, typeExpressionOpt.value, -1);

                //resolves to a type name
                if (expr.resolvesTo.lastType == NULL)
                {
                    //this is the actual variable's/function return type
                    
                    LinxcTypeReference expectedType = expr.AsTypeReference().value;

                    if (nextIsConst)
                    {
                        expectedType.isConst = true;
                        nextIsConst = false;
                    }
                    
                    LinxcToken identifier = tokenizer->NextUntilValid();
                    if (identifier.ID != Linxc_Identifier)
                    {
                        ERR_MSG error = ERR_MSG(this->allocator, "Expected identifier after type name, token was ");
                        error.AppendDeinit(identifier.ToString(&defaultAllocator));
                        errors->Add(error);
                        toBreak = true;
                        break;
                    }

                    LinxcToken next = tokenizer->NextUntilValid();
                    if (next.ID == Linxc_Semicolon || next.ID == Linxc_Equal)
                    {
                        option<LinxcExpression> defaultValue;
                        if (next.ID == Linxc_Equal)
                        {
                            option<LinxcExpression> primary = this->ParseExpressionPrimary(state);
                            if (!primary.present)
                            {
                                errorSkipUntilSemicolon = true;
                                break;
                            }
                            defaultValue.value = this->ParseExpression(state, primary.value, -1);
                            defaultValue.present = true;
                            expectSemicolon = true;
                        }
                        LinxcVar varDecl = LinxcVar(identifier.ToString(this->allocator), expr, defaultValue);
                        if (expectedType.isConst)
                        {
                            varDecl.isConst = true;
                        }

                        if (defaultValue.present)
                        {
                            //expected type is not what the default value expression resolves to, and there is no implicit cast for it
                            if (!CanAssign(expectedType, defaultValue.value.resolvesTo))
                            {
                                ERR_MSG msg = ERR_MSG(this->allocator, "Variable's initial value is not of the same type as the variable itself, and no implicit cast was found.");
                                if (defaultValue.value.resolvesTo.CanCastTo(expectedType, false))
                                {
                                    msg.Append(" An explicit cast is required.");
                                }
                                else if (expectedType.lastType == typeofU8 && defaultValue.value.resolvesTo.lastType == typeofU8 && defaultValue.value.resolvesTo.isConst && !expectedType.isConst)
                                {
                                    msg.Append(" String literals (eg: \"Hello World\") may only be assigned to const u8*.");
                                }
                                errors->Add(msg);
                            }
                        }

                        //not in a function
                        if (state->currentFunction == NULL)
                        {
                            LinxcVar* ptr = NULL;
                            //in a struct
                            if (state->parentType != NULL)
                            {
                                state->parentType->variables.Add(varDecl);
                                ptr = state->parentType->variables.Get(state->parentType->variables.count - 1);
                            }
                            else //else add to namespace
                            {
                                state->currentNamespace->variables.Add(varDecl.name, varDecl);
                                ptr = state->currentNamespace->variables.Get(varDecl.name);

                                state->parsingFile->definedVars.Add(ptr);
                            }

                            LinxcStatement stmt;
                            stmt.data.varDeclaration = ptr;
                            stmt.ID = LinxcStmt_VarDecl;
                            result.Add(stmt);
                        }
                        else //in a function, add as temp variable instead
                        {
                            //cant do this as upon adding results to the parent ast, the
                            //value of tempPtr would change
                            //we should instead push it to the heap

                            /*LinxcStatement stmt;
                            stmt.data.tempVarDeclaration = varDecl;
                            stmt.ID = LinxcStmt_TempVarDecl;
                            result.Add(stmt);

                            //printf("Added temp variable %s\n", stmt.ToString(this->allocator).buffer);

                            LinxcVar* tempPtr = &result.Get(result.count - 1)->data.tempVarDeclaration;
                            state->varsInScope.Add(varDecl.name, tempPtr);*/

                            LinxcVar* ptr = (LinxcVar*)this->allocator->Allocate(sizeof(LinxcVar));
                            *ptr = varDecl;

                            LinxcStatement stmt;
                            stmt.data.varDeclaration = ptr;
                            stmt.ID = LinxcStmt_VarDecl;
                            result.Add(stmt);
                            state->varsInScope.Add(varDecl.name, ptr);
                        }
                    }
                    else if (next.ID == Linxc_LParen) //function declaration
                    {
                        u32 necessaryArgs = 0;
                        collections::Array<LinxcVar> args = this->ParseFunctionArgs(state, &necessaryArgs);
                        LinxcToken next = tokenizer->PeekNextUntilValid();
                        if (next.ID != Linxc_LBrace)
                        {
                            errors->Add(ERR_MSG(this->allocator, "Expected { after function name"));
                            //toBreak = true;
                            //break;
                        }
                        else tokenizer->NextUntilValid();

                        LinxcFunc newFunc = LinxcFunc(identifier.ToString(this->allocator), expr);
                        newFunc.arguments = args;
                        newFunc.necessaryArguments = necessaryArgs;
                        if (state->parentType != NULL)
                        {
                            newFunc.methodOf = state->parentType;
                        }
                        else
                        {
                            newFunc.funcNamespace = state->currentNamespace;
                        }

                        LinxcFunc* ptr = NULL;

                        if (state->parentType != NULL)
                        {
                            state->parentType->functions.Add(newFunc);
                            ptr = state->parentType->functions.Get(state->parentType->functions.count - 1);
                        }
                        else
                        {
                            state->currentNamespace->functions.Add(newFunc.name, newFunc);
                            ptr = state->currentNamespace->functions.Get(newFunc.name);
                        }

                        LinxcParserState nextState = LinxcParserState(state->parser, state->parsingFile, state->tokenizer, LinxcEndOn_RBrace, false, state->parsingLinxci);
                        nextState.parentType = state->parentType;
                        nextState.endOn = LinxcEndOn_RBrace;
                        nextState.currentNamespace = state->currentNamespace;
                        nextState.currentFunction = ptr;
                        for (usize i = 0; i < args.length; i++)
                        {
                            nextState.varsInScope.Add(args.data[i].name, &args.data[i]);
                        }

                        LinxcVar *thisVar;

                        if (state->parentType != NULL)
                        {
                            //the 'this' keyword counts as a variable within a function's scope if it is within a struct

                            thisVar = (LinxcVar*)allocator->Allocate(sizeof(LinxcVar));
                            thisVar->name = this->thisKeyword;
                            thisVar->type = state->parentType->AsExpression();
                            thisVar->type.data.typeRef.pointerCount += 1;
                            nextState.varsInScope.Add(this->thisKeyword, thisVar);

                            for (usize i = 0; i < state->parentType->variables.count; i++)
                            {
                                LinxcVar* memberVariable = state->parentType->variables.Get(i);
                                //printf("Member variable %s in type %s\n", memberVariable->name.buffer, state->parentType->name.buffer);
                                nextState.varsInScope.Add(memberVariable->name, memberVariable);
                            }
                        }

                        option<collections::vector<LinxcStatement>> funcBody = this->ParseCompoundStmt(&nextState);

                        if (funcBody.present)
                        {
                            ptr->body = funcBody.value;
                        }

                        state->parsingFile->definedFuncs.Add(ptr);

                        LinxcStatement stmt;
                        stmt.data.funcDeclaration = ptr;
                        stmt.ID = LinxcStmt_FuncDecl;

                        result.Add(stmt);

                        nextState.deinit();
                    }
                }
                else //random expressions (EG: functionCall()) are only allowed within functions
                {
                    if (nextIsConst)
                    {
                        errors->Add(ERR_MSG(this->allocator, "Cannot declare an expression as const"));
                        nextIsConst = false;
                    }
                    if (state->currentFunction != NULL)
                    {
                        LinxcStatement stmt;
                        stmt.data.expression = expr;
                        stmt.ID = LinxcStmt_Expr;
                        result.Add(stmt);
                        expectSemicolon = true;
                    }
                    else
                    {
                        errors->Add(ERR_MSG(this->allocator, "Standalone expressions are only allowed within the body of a function"));
                    }
                }
            }
            else
            {
                toBreak = true;
                break;
            }
        }
        break;
        case Linxc_Keyword_return:
        {
            if (isComment)
            {
                continue;
            }
            expectSemicolon = true;
            if (state->currentFunction == NULL)
            {
                state->parsingFile->errors.Add(ERR_MSG(this->allocator, "Attempting to use return statement outside of a function body"));
                break;
            }
            if (tokenizer->PeekNextUntilValid().ID == Linxc_Semicolon)
            {
                //return; statement. Only valid in functions that return void

                if (state->currentFunction->returnType.AsTypeReference().value.lastType->name != "void")
                {
                    state->parsingFile->errors.Add(ERR_MSG(this->allocator, "Empty return statement not allowed in function that expects a return type"));
                }
                break;
            }
            option<LinxcExpression> primary = ParseExpressionPrimary(state);
            if (primary.present)
            {
                LinxcExpression returnExpression = ParseExpression(state, primary.value, -1);
                if (returnExpression.resolvesTo.lastType == NULL)
                {
                    errors->Add(ERR_MSG(this->allocator, "Cannot return a type name"));
                    break;
                }
                if (CanAssign(state->currentFunction->returnType.AsTypeReference().value, returnExpression.resolvesTo))
                {
                    LinxcStatement stmt;
                    stmt.data.returnStatement = returnExpression;
                    stmt.ID = LinxcStmt_Return;
                    result.Add(stmt);
                }
                else
                {
                    errors->Add(ERR_MSG(this->allocator, "Returned type does not match expected function return type, and cannot be converted to it"));
                }
            }
        }
        break;
        case Linxc_RBrace:
        {
            if (isComment)
            {
                continue;
            }
            if (state->endOn == LinxcEndOn_RBrace)
            {
                toBreak = true;
            }
            else
            {
                errors->Add(ERR_MSG(this->allocator, "Unexpected }"));
            }
        }
        break;
        case Linxc_Eof:
        {
            if (state->endOn == LinxcEndOn_RBrace)
            {
                errors->Add(ERR_MSG(this->allocator, "Expected }"));
            }
            else if (state->endOn == LinxcEndOn_Endif)
            {
                errors->Add(ERR_MSG(this->allocator, "Expected #endif"));
            }
            else if (state->endOn == LinxcEndOn_Semicolon)
            {
                errors->Add(ERR_MSG(this->allocator, "Expected ;"));
            }
            toBreak = true;
        }
        break;
        default:
            break;
        }
        if (toBreak)
        {
            break;
        }
    }
    return option<collections::vector<LinxcStatement>>(result);
}

void LinxcParser::TranspileFile(LinxcParsedFile* parsedFile, const char* outputPathC, const char* outputPathH)
{
    FILE* fs;
    if (fopen_s(&fs, outputPathH, "w") == 0)
    {
        //transpile header
        for (usize i = 0; i < parsedFile->ast.count; i++)
        {
            TranspileStatementH(fs, parsedFile->ast.Get(i));
        }
        fclose(fs);
    }
    //transpile source
    if (fopen_s(&fs, outputPathC, "w") == 0)
    {
        string swappedExtension = path::SwapExtension(&defaultAllocator, parsedFile->includeName, ".h");
        fprintf(fs, "#include <%s>\n", swappedExtension.buffer);
        swappedExtension.deinit();
        //we only care about functions atm
        for (usize i = 0; i < parsedFile->definedFuncs.count; i++)
        {
            LinxcFunc* func = parsedFile->definedFuncs.ptr[i];
            this->TranspileFunc(fs, func);
            fprintf(fs, "\n{\n");
            for (usize j = 0; j < func->body.count; j++)
            {
                this->TranspileStatementC(fs, func->body.Get(j));
                fprintf(fs, ";\n");
            }
            fprintf(fs, "}\n");
        }
        fclose(fs);
    }
}
void LinxcParser::TranspileStatementH(FILE* fs, LinxcStatement* stmt)
{
    if (stmt->ID == LinxcStmt_Include)
    {
        string includeName = stmt->data.includeStatement.includeString;
        string extension = path::GetExtension(&defaultAllocator, includeName);
        if (extension == ".linxc")
        {
            includeName = path::SwapExtension(&defaultAllocator, includeName, ".h");
            //we only set the above to true here as before, we are just referencing the includeName as part of the include
            //statement. If we deinited that, we would have modified the statement in the AST
        }

        //make sure no harpy brain programmer uses '\' to specify include paths 
        string replaced = ReplaceChar(&defaultAllocator, includeName, '\\', '/');
        fprintf(fs, "#include <%s>\n", includeName.buffer);
        replaced.deinit();

        if (includeName == "Linxc.h")
        {
            fprintf(fs, "#include <stdbool.h>\n");
        }

        if (extension == ".linxc")
        {
            includeName.deinit();
        }
        extension.deinit();
    }
    else if (stmt->ID == LinxcStmt_Namespace)
    {
        //dont need to do any funny state incrementing here
        for (usize i = 0; i < stmt->data.namespaceScope.body.count; i++)
        {
            TranspileStatementH(fs, stmt->data.namespaceScope.body.Get(i));
        }
    }
    else if (stmt->ID == LinxcStmt_TypeDecl)
    {
        fprintf(fs, "typedef struct {\n");
        string typeName = stmt->data.typeDeclaration->GetCName(&defaultAllocator);
        for (usize i = 0; i < stmt->data.typeDeclaration->variables.count; i++)
        {
            fprintf(fs, "   ");
            this->TranspileVar(fs, stmt->data.typeDeclaration->variables.Get(i));
            fprintf(fs, ";\n");
        }
        fprintf(fs, "} %s;\n", typeName.buffer);
        typeName.deinit();

        for (usize i = 0; i < stmt->data.typeDeclaration->functions.count; i++)
        {
            this->TranspileFunc(fs, stmt->data.typeDeclaration->functions.Get(i));
            fprintf(fs, ";\n");
        }
    }
    else if (stmt->ID == LinxcStmt_VarDecl)
    {
        this->TranspileVar(fs, stmt->data.varDeclaration);
    }
    else if (stmt->ID == LinxcStmt_FuncDecl)
    {
        this->TranspileFunc(fs, stmt->data.funcDeclaration);
        fprintf(fs, ";\n");
    }
}
void LinxcParser::TranspileFunc(FILE *fs, LinxcFunc* func)
{
    LinxcTypeReference typeRef = func->returnType.AsTypeReference().value;

    string returnTypeName = typeRef.GetCName(&defaultAllocator);
    fprintf(fs, "%s ", returnTypeName.buffer);
    returnTypeName.deinit();

    string funcName = func->GetCName(&defaultAllocator);
    fprintf(fs, "%s(", funcName.buffer);
    funcName.deinit();

    //if we are a member function of a struct, the first argument will always be 'this'
    if (func->methodOf != NULL)
    {
        string thisTypeName = func->methodOf->GetCName(&defaultAllocator);
        fprintf(fs, "%s *this", thisTypeName.buffer);
        if (func->arguments.length > 0)
        {
            fprintf(fs, ", ");
        }
    }
    for (usize i = 0; i < func->arguments.length; i++)
    {
        this->TranspileVar(fs, &func->arguments.data[i]);
        if (i < func->arguments.length - 1)
        {
            fprintf(fs, ", ");
        }
    }

    fprintf(fs, ")");
}
void LinxcParser::TranspileExpr(FILE* fs, LinxcExpression* expr)
{
    switch (expr->ID)
    {
    case LinxcExpr_None:
        break;
    case LinxcExpr_Literal:
    {
        fprintf(fs, "%s", expr->data.literal.buffer);
    }
    break;
    case LinxcExpr_Variable:
    {
        fprintf(fs, "%s", expr->data.variable->name.buffer);
    }
    break;
    case LinxcExpr_Modified:
    {
        fprintf(fs, "%s", LinxcTokenIDToString(expr->data.modifiedExpression->modification));
        this->TranspileExpr(fs, &expr->data.modifiedExpression->expression);
    }
    break;
    case LinxcExpr_FuncCall:
    {
        string name = expr->data.functionCall.func->GetCName(&defaultAllocator);
        fprintf(fs, "%s(", name.buffer);
        name.deinit();
        for (usize i = 0; i < expr->data.functionCall.inputParams.length; i++)
        {
            this->TranspileExpr(fs, &expr->data.functionCall.inputParams.data[i]);
            if (i < expr->data.functionCall.inputParams.length - 1)
            {
                fprintf(fs, ", ");
            }
        }
        fprintf(fs, "}");
        //expr->data.functionCall.func->
    }
    case LinxcExpr_FunctionRef:
    {
        string name = expr->data.functionRef->GetCName(&defaultAllocator);
        fprintf(fs, "%s(", name.buffer);
        name.deinit();
    }
    break;
    case LinxcExpr_OperatorCall:
    {
        //when transpiling an operator with a scope resolution operation, we should ignore the
        //resolution and only transpile the right side until we reach the variable/function/type.
        //this is so as to prevent duplicate namespace scopes when transpiled to C, as we need
        //to transpile the variable/function/type as it's complete C friendly name.
        //If we were to transpile it as is, and make the C friendly name by transpiling scope resolution operators,
        //variables/functions/types contained in a using'd namespace would be improperly transpiled
        if (expr->data.operatorCall->operatorType == Linxc_ColonColon)
        {
            this->TranspileExpr(fs, &expr->data.operatorCall->rightExpr);
        }
        else
        {
            //todo: Convert custom operators to functions
            LinxcTokenID opType = expr->data.operatorCall->operatorType;
            bool writePriority = opType != Linxc_ColonColon && opType != Linxc_Arrow && opType != Linxc_Period && opType != Linxc_Equal;
            if (writePriority)
            {
                fprintf(fs, "(");
            }
            this->TranspileExpr(fs, &expr->data.operatorCall->leftExpr);
            //if not ::, ->, ., yes space
            if (opType != Linxc_ColonColon && opType != Linxc_Arrow && opType != Linxc_Period)
            {
                fprintf(fs, " %s ", LinxcTokenIDToString(expr->data.operatorCall->operatorType));
            }
            //no space
            else fprintf(fs, "%s", LinxcTokenIDToString(expr->data.operatorCall->operatorType));
            this->TranspileExpr(fs, &expr->data.operatorCall->rightExpr);
            if (writePriority)
            {
                fprintf(fs, ")");
            }
        }
    }
    break;
    default:
        break;
    }
}
void LinxcParser::TranspileVar(FILE* fs, LinxcVar* var)
{
    if (var->isConst)
    {
        fprintf(fs, "const ");
    }
    LinxcTypeReference typeRef = var->type.AsTypeReference().value;
    string fullName = typeRef.GetCName(&defaultAllocator);
    fprintf(fs, "%s ", fullName.buffer);
    fullName.deinit();
    fprintf(fs, "%s", var->name.buffer);
    if (var->defaultValue.present)
    {
        fprintf(fs, " = "); //temp
        this->TranspileExpr(fs, &var->defaultValue.value);
    }
    //else fprintf(fs, ";\n");
}
void LinxcParser::TranspileStatementC(FILE* fs, LinxcStatement* stmt)
{
    if (stmt->ID == LinxcStmt_Expr)
    {
        this->TranspileExpr(fs, &stmt->data.expression);
    }
    else if (stmt->ID == LinxcStmt_Return)
    {
        fprintf(fs, "return ");
        this->TranspileExpr(fs, &stmt->data.returnStatement);
    }
    else if (stmt->ID == LinxcStmt_VarDecl)
    {
        this->TranspileVar(fs, stmt->data.varDeclaration);
    }
}