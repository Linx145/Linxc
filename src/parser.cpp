#include <parser.hpp>
#include <stdio.h>
#include <path.hpp>
//#include <reflectC.hpp>
#include <ArenaAllocator.hpp>

LinxcParserState::LinxcParserState(LinxcParser *myParser, LinxcParsedFile *currentFile, LinxcTokenizer *myTokenizer, LinxcEndOn endsOn, bool isTopLevel, bool isParsingLinxci)
{
    this->commaIsSemicolon = false;
    this->tokenizer = myTokenizer;
    this->parser = myParser;
    this->parsingFile = currentFile;
    this->endOn = endsOn;
    this->isToplevel = isTopLevel;
    this->currentNamespace = NULL;
    this->currentFunction = NULL;
    this->parentType = NULL;
    this->varsInScope = collections::hashmap<string, LinxcVar *>(GetDefaultAllocator(), &stringHash, &stringEql);
    this->parsingLinxci = isParsingLinxci;
}
LinxcParser::LinxcParser(IAllocator *allocator)
{
    this->allocator = allocator;
    this->globalNamespace = LinxcNamespace(allocator, string());
    this->thisKeyword = string(allocator, "this");

    const i32 numIntegerTypes = 8;
    const i32 numNumericTypes = 10;
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
        if (i == 11)
        {
            typeofVoid = primitiveTypePtrs[i];
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
    //all numeric types can be +, -, /, *, ==, !=, >, <, >=, <= with each other
    // we dont need to add +=, -=, *=, /= as those cannot be overriden and are implied by overriding the +, -, * and / operators instead
    //TODO: Settle the bitshift and bitwise comparison operators
    //because typeA + typeB = typeB + typeA, we should avoid adding the duplicates
    for (i32 i = 0; i < numNumericTypes; i++)
    {
        LinxcOperatorFunc operatorSet = NewDefaultOperator(primitiveTypePtrs, i, i, Linxc_Equal);
        primitiveTypePtrs[i]->operatorOverloads.Add(operatorSet.operatorOverride, operatorSet);

        //int + float = not possible
        //float + int = possible
        i32 iterateUntil = numNumericTypes;
        if (i < numIntegerTypes)
        {
            iterateUntil = numIntegerTypes;
        }

        for (i32 j = 0; j < iterateUntil; j++)
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
            
                LinxcOperatorFunc operatorMoreThan = NewDefaultOperator(primitiveTypePtrs, i, j, Linxc_AngleBracketRight);
                primitiveTypePtrs[i]->operatorOverloads.Add(operatorMoreThan.operatorOverride, operatorMoreThan);

                LinxcOperatorFunc operatorLessThan = NewDefaultOperator(primitiveTypePtrs, i, j, Linxc_AngleBracketLeft);
                primitiveTypePtrs[i]->operatorOverloads.Add(operatorLessThan.operatorOverride, operatorLessThan);

                LinxcOperatorFunc operatorMoreThanEquals = NewDefaultOperator(primitiveTypePtrs, i, j, Linxc_AngleBracketRightEqual);
                primitiveTypePtrs[i]->operatorOverloads.Add(operatorMoreThanEquals.operatorOverride, operatorMoreThanEquals);

                LinxcOperatorFunc operatorLessThanEquals = NewDefaultOperator(primitiveTypePtrs, i, j, Linxc_AngleBracketLeftEqual);
                primitiveTypePtrs[i]->operatorOverloads.Add(operatorLessThanEquals.operatorOverride, operatorLessThanEquals);
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
    this->includedFiles = collections::hashmap<string, LinxcIncludedFile>(allocator, &stringHash, &stringEql);
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
    nameToToken.Add(string(nameToToken.allocator, "def_delegate"), Linxc_Keyword_def_delegate);
    nameToToken.Add(string(nameToToken.allocator, "do"), Linxc_Keyword_do);
    nameToToken.Add(string(nameToToken.allocator, "double"), Linxc_Keyword_double);
    nameToToken.Add(string(nameToToken.allocator, "else"), Linxc_Keyword_else);
    nameToToken.Add(string(nameToToken.allocator, "enum"), Linxc_Keyword_enum);
    nameToToken.Add(string(nameToToken.allocator, "error"), Linxc_Keyword_error);
    nameToToken.Add(string(nameToToken.allocator, "extern"), Linxc_Keyword_extern);
    nameToToken.Add(string(nameToToken.allocator, "false"), Linxc_Keyword_false);
    nameToToken.Add(string(nameToToken.allocator, "float"), Linxc_Keyword_float);
    nameToToken.Add(string(nameToToken.allocator, "for"), Linxc_Keyword_for);
    //nameToToken.Add(string(nameToToken.allocator, "goto"), Linxc_Keyword_goto);
    nameToToken.Add(string(nameToToken.allocator, "i8"), Linxc_Keyword_i8);
    nameToToken.Add(string(nameToToken.allocator, "i16"), Linxc_Keyword_i16);
    nameToToken.Add(string(nameToToken.allocator, "i32"), Linxc_Keyword_i32);
    nameToToken.Add(string(nameToToken.allocator, "i64"), Linxc_Keyword_i64);
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
    //nameToToken.Add(string(nameToToken.allocator, "thread_local"), Linxc_Keyword_thread_local);
    //nameToToken.Add(string(nameToToken.allocator, "trait"), Linxc_Keyword_trait);
    nameToToken.Add(string(nameToToken.allocator, "true"), Linxc_Keyword_true);
    nameToToken.Add(string(nameToToken.allocator, "typedef"), Linxc_Keyword_typedef);
    nameToToken.Add(string(nameToToken.allocator, "typename"), Linxc_Keyword_typename);
    nameToToken.Add(string(nameToToken.allocator, "typeof"), Linxc_Keyword_typeof);
    nameToToken.Add(string(nameToToken.allocator, "u8"), Linxc_Keyword_u8);
    nameToToken.Add(string(nameToToken.allocator, "u16"), Linxc_Keyword_u16);
    nameToToken.Add(string(nameToToken.allocator, "u32"), Linxc_Keyword_u32);
    nameToToken.Add(string(nameToToken.allocator, "u64"), Linxc_Keyword_u64);
    nameToToken.Add(string(nameToToken.allocator, "union"), Linxc_Keyword_union);
    nameToToken.Add(string(nameToToken.allocator, "void"), Linxc_Keyword_void);
    //nameToToken.Add(string(nameToToken.allocator, "volatile"), Linxc_Keyword_volatile);
    nameToToken.Add(string(nameToToken.allocator, "while"), Linxc_Keyword_while);
    nameToToken.Add(string(nameToToken.allocator, "attribute"), Linxc_keyword_attribute);
    nameToToken.Add(string(nameToToken.allocator, "uselang"), Linxc_keyword_uselang);
    nameToToken.Add(string(nameToToken.allocator, "enduselang"), Linxc_keyword_enduselang);
    nameToToken.Add(string(nameToToken.allocator, "endif"), Linxc_Keyword_endif);
}
void LinxcParserState::deinit()
{
    this->varsInScope.deinit();
}
void LinxcParser::deinit()
{
    this->includedFiles.deinit();
    this->includeDirectories.deinit();
    this->parsedFiles.deinit();
    this->parsingFiles.deinit();
    //TODO: deinit parsedFiles, parsingFiles
}
void LinxcParser::PrintAllErrors()
{
    for (usize i = 0; i < this->parsedFiles.bucketsCount; i++)
    {
        if (this->parsedFiles.buckets[i].initialized)
        {
            for (usize j = 0; j < this->parsedFiles.buckets[i].entries.count; j++)
            {
                for (usize c = 0; c < this->parsedFiles.buckets[i].entries.ptr[j].value.errors.count; c++)
                {
                    printf("%s: %s\n", this->parsedFiles.buckets[i].entries.ptr[j].value.includeName.buffer, this->parsedFiles.buckets[i].entries.ptr[j].value.errors.ptr[c].buffer);
                }
            }
        }
    }
}
bool LinxcParser::Compile(const char* outputDirectory)
{
    if (GetDefaultAllocator()->allocFunction == NULL)
    {
        printf("Default allocator not assigned\n");
    }
    else printf("Default allocator found.\n");
    
    bool foundError = false;
    for (usize i = 0; i < this->includedFiles.bucketsCount; i++)
    {
        if (this->includedFiles.buckets[i].initialized)
        {
            for (usize j = 0; j < this->includedFiles.buckets[i].entries.count; j++)
            {
                LinxcIncludedFile includedFile = this->includedFiles.buckets[i].entries.Get(j)->value;
                if (!this->parsedFiles.Contains(includedFile.includeName))
                {
                    string contents = io::ReadFile(GetDefaultAllocator(), includedFile.fullNameAndPath.buffer);
                    if (contents.buffer != NULL)
                    {
                        LinxcParsedFile* result = this->ParseFile(includedFile.fullNameAndPath, includedFile.includeName, contents);
                        contents.deinit();
                        if (result->errors.count > 0)
                        {
                            foundError = true;
                        }
                    }
                    else
                    {
                        printf("Error reading file %s\n", includedFile.fullNameAndPath.buffer);
                        foundError = true;
                        break;
                    }
                }
            }
        }
        if (foundError)
        {
            break;
        }
    }
    if (foundError)
    {
        return false;
    }

    /*string cmd = string(GetDefaultAllocator());

    for (usize i = 0; i < this->parsedFiles.bucketsCount; i++)
    {
        if (this->parsedFiles.buckets[i].initialized)
        {
            for (usize j = 0; j < this->parsedFiles.buckets[i].entries.count; j++)
            {
                LinxcParsedFile* parsedFile = &this->parsedFiles.buckets[i].entries.ptr[j].value;

                if (!parsedFile->isLinxcH)
                {
                    string outputPath = this->parsedFiles.buckets[i].entries.Get(j)->value.includeName.Clone(GetDefaultAllocator());

                    outputPath.Prepend("/");
                    outputPath.Prepend(outputDirectory);

                    string pathC = path::SwapExtension(GetDefaultAllocator(), outputPath, ".c");
                    string pathH = path::SwapExtension(GetDefaultAllocator(), outputPath, ".h");

                    bool transpiledC = this->TranspileFile(parsedFile, pathC.buffer, pathH.buffer);

                    if (transpiledC)
                    {
                        cmd.Append(" ");
                        cmd.Append(pathC.buffer);
                    }

                    outputPath.deinit();
                    pathC.deinit();
                    pathH.deinit();
                }
            }
        }
    }

    i32 result = 0;

    //compile
    if (this->appName.buffer != NULL)
    {
        cmd.Prepend("clang");
        cmd.Append(" -I");
        cmd.Append(this->linxcstdLocation.buffer);
        cmd.Append(" -I");
        cmd.Append(outputDirectory);
        cmd.Append(" -o");

        string outputFile = string(GetDefaultAllocator(), outputDirectory);
        outputFile.Append("/\"");
        outputFile.Append(this->appName.buffer);
        outputFile.Append("\".exe");

        cmd.Append(outputFile.buffer);

        printf("%s\n\n", cmd.buffer);
        result = system(cmd.buffer);

        if (result == 0)
        {
            result = system(outputFile.buffer);
        }
        outputFile.deinit();
    }
    //i32 run = system(outputFile.buffer);
    printf("\n");

    cmd.deinit();
    
    return result == 0;*/
}
void LinxcParser::SetLinxcStdLocation(string path)
{
    this->linxcstdLocation = path;
    this->includeDirectories.Add(path);
}
void LinxcParser::AddAllFilesFromDirectory(string directoryPath)
{
    collections::Array<string> result = io::GetFilesInDirectory(GetDefaultAllocator(), directoryPath.buffer);

    for (usize i = 0; i < result.length; i++)
    {
        string fullPathAndName = string(GetDefaultAllocator(), directoryPath.buffer);
        fullPathAndName.Append("/");
        fullPathAndName.Append(result.data[i].buffer);
        
        LinxcIncludedFile includedFile;
        includedFile.includeName = result.data[i];
        includedFile.fullNameAndPath = fullPathAndName.CloneDeinit(this->allocator);
        //result.data[i].Prepend("/");
        //result.data[i].Prepend(directoryPath.buffer);
        this->includedFiles.Add(result.data[i], includedFile);
    }
    result.deinit();
}
LinxcParsedFile *LinxcParser::ParseFile(string fileFullPath, string includeName, string fileContents)
{
    if (this->parsedFiles.Contains(includeName)) //already parsed
    {
        return this->parsedFiles.Get(includeName);
    }
    if (this->parsingFiles.Contains(includeName))
    {
        return NULL;
    }
    bool parsingLinxci = false;
    string extension = path::GetExtension(GetDefaultAllocator(), fileFullPath);
    if (extension == ".h" || extension == ".hpp")
    {
        parsingLinxci = true;
    }

    LinxcParsedFile file = LinxcParsedFile(this->allocator, fileFullPath, includeName);
    if (extension == ".linxch")
    {
        file.isLinxcH = true;
    }
    this->parsingFiles.Add(includeName);

    LinxcTokenizer tokenizer = LinxcTokenizer(fileContents.buffer, fileContents.length, &this->nameToToken);
    
    if (this->TokenizeFile(&tokenizer, allocator, &file))
    {
        LinxcParserState parserState = LinxcParserState(this, &file, &tokenizer, LinxcEndOn_Eof, true, parsingLinxci);
        file.fileNamespace = LinxcPhoneyNamespace(this->allocator, &this->globalNamespace);
        parserState.currentNamespace = &file.fileNamespace;
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
        string potentialFullPath = string(GetDefaultAllocator(), this->includeDirectories.Get(i)->buffer);
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
    returnType.priority = false;
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
    
    if (op == Linxc_EqualEqual || op == Linxc_BangEqual || op == Linxc_AngleBracketLeft || op == Linxc_AngleBracketRight || op == Linxc_AngleBracketLeftEqual || op == Linxc_AngleBracketRightEqual)
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
    returnTypeExpr.priority = false;
    returnTypeExpr.ID = LinxcExpr_TypeRef; //what we're returning after the operation

    LinxcFunc opFunc = LinxcFunc(string(), returnTypeExpr); //these functions don't need names
    
    //our sole argument is the other type. EG: intVar + floatVar = intVar.Add(floatVar)
    LinxcVar* inputArg = (LinxcVar*)this->allocator->Allocate(sizeof(LinxcVar));
    inputArg->type.ID = LinxcExpr_TypeRef;
    inputArg->type.data.typeRef = LinxcTypeReference(otherType);
    inputArg->type.resolvesTo.lastType = NULL;
    inputArg->memberOf = NULL;
    inputArg->name = string(this->allocator, "other");
    opFunc.arguments = collections::Array<LinxcVar>(this->allocator, inputArg, 1);

    LinxcOperatorFunc result;
    result.operatorOverride = operation;
    result.function = opFunc;

    return result;
}

bool LinxcParser::TokenizeFile(LinxcTokenizer* tokenizer, IAllocator* allocator, LinxcParsedFile* parsingFile)
{
    collections::hashmap<string, LinxcMacro*> identifierToMacro = collections::hashmap<string, LinxcMacro*>(GetDefaultAllocator(), &stringHash, &stringEql);
    LinxcMacro compilingFlagMacro;
    compilingFlagMacro.isFunctionMacro = false;
    compilingFlagMacro.name = string(allocator, "LINXC_COMPILING");
    identifierToMacro.Add(compilingFlagMacro.name, &compilingFlagMacro);

    tokenizer->tokenStream = collections::vector<LinxcToken>(allocator);
    bool nextMacroIsAttribute = false;
    bool skipUntilEndif = false;
    i32 expectEndif = 0;
    while (true)
    {
        LinxcToken token = tokenizer->TokenizeAdvance();

        if (token.ID == Linxc_Eof && expectEndif > 0)
        {
            parsingFile->errors.Add(ERR_MSG(this->allocator, "Expected #endif before end of file"));
            identifierToMacro.deinit();
            return false;
        }

        if (skipUntilEndif && token.ID != Linxc_Hash)
        {
            continue;
        }

        if (token.ID == Linxc_keyword_attribute)
        {
            nextMacroIsAttribute = true;
        }
        else if (token.ID == Linxc_Hash)
        {
            LinxcToken preprocessorDirective = tokenizer->TokenizeAdvance();

            if (preprocessorDirective.ID == Linxc_Keyword_endif)
            {
                if (expectEndif > 0)
                {
                    expectEndif -= 1;
                    if (expectEndif == 0 && skipUntilEndif)
                    {
                        skipUntilEndif = false;
                    }
                }
                else
                {
                    parsingFile->errors.Add(ERR_MSG(this->allocator, "Unexpected #endif statement"));
                }
            }
            else if (!skipUntilEndif)
            {
                if (preprocessorDirective.ID == Linxc_Keyword_define)
                {
                    LinxcToken name = tokenizer->TokenizeAdvance();
                    if (name.ID == Linxc_Identifier)
                    {
                        LinxcToken next = tokenizer->TokenizeAdvance();
                        if (next.ID == Linxc_LParen)
                        {
                            collections::vector<LinxcToken> macroBody = collections::vector<LinxcToken>(allocator);
                            collections::vector<LinxcToken> macroArgs = collections::vector<LinxcToken>(GetDefaultAllocator());
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
                                            compilingFlagMacro.name.deinit();
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

                            /*if (nextMacroIsAttribute)
                            {
                                parsingFile->definedAttributes.Add(macro);
                                nextMacroIsAttribute = false;
                            }
                            else */
                            identifierToMacro.Add(macro.name, parsingFile->definedMacros.Get(parsingFile->definedMacros.count - 1));
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
                            /*if (nextMacroIsAttribute)
                            {
                                parsingFile->definedAttributes.Add(macro);
                                nextMacroIsAttribute = false;
                            }
                            else */identifierToMacro.Add(macro.name, parsingFile->definedMacros.Get(parsingFile->definedMacros.count - 1));
                        }
                    }
                    else
                    {
                        parsingFile->errors.Add(ERR_MSG(allocator, "Preprocessor: Expected non-reserved identifier name after #define directive"));
                        return false;
                    }
                }
                else if (preprocessorDirective.ID == Linxc_Keyword_ifdef)
                {
                    LinxcToken macroToCheck = tokenizer->TokenizeAdvance();
                    string macroToCheckString = macroToCheck.ToString(GetDefaultAllocator());

                    expectEndif += 1;
                    if (!identifierToMacro.Contains(macroToCheckString))
                    {
                        skipUntilEndif = true;
                    }
                    macroToCheckString.deinit();
                }
                else if (preprocessorDirective.ID == Linxc_Keyword_ifndef)
                {
                    LinxcToken macroToCheck = tokenizer->TokenizeAdvance();
                    string macroToCheckString = macroToCheck.ToString(GetDefaultAllocator());

                    expectEndif += 1;
                    if (identifierToMacro.Contains(macroToCheckString))
                    {
                        skipUntilEndif = true;
                    }
                    macroToCheckString.deinit();
                }
                else if (preprocessorDirective.ID == Linxc_Keyword_pragma)
                {
                    //skip pragma declarations for now
                    tokenizer->TokenizeAdvance();
                }
                else if (preprocessorDirective.ID == Linxc_Keyword_include)
                {
                    //parser doesn't care about the opening # so dont need to add that

                    tokenizer->tokenStream.Add(preprocessorDirective);

                    LinxcToken next = tokenizer->TokenizeAdvance();
                    if (next.ID != Linxc_MacroString && next.ID != Linxc_StringLiteral)
                    {
                        parsingFile->errors.Add(ERR_MSG(this->allocator, "Expected <file to be included> after #include declaration"));
                        compilingFlagMacro.name.deinit();
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
            //else if (preprocessorDirective.ID == Linxc_Keyword)
        }
        else
        {
            if (token.ID == Linxc_Identifier)
            {
                string temp = token.ToString(GetDefaultAllocator());
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
                            compilingFlagMacro.name.deinit();
                            identifierToMacro.deinit();
                            return false;
                        }
                        next = tokenizer->TokenizeAdvance();

                        if (macro->arguments.length == 0)
                        {
                            if (next.ID != Linxc_RParen)
                            {
                                parsingFile->errors.Add(ERR_MSG(this->allocator, "This macro does not have arguments"));
                                compilingFlagMacro.name.deinit();
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

                            ArenaAllocator arena = ArenaAllocator(GetDefaultAllocator());
                            
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
                                    compilingFlagMacro.name.deinit();
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

                                    string macroTokenName = macroToken.ToString(GetDefaultAllocator());
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
    compilingFlagMacro.name.deinit();
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

                    //&functionName should resolve to the function ptr type
                    if (token.ID == Linxc_Ampersand && expression.AsFuncReference().present)
                    {
                        LinxcFunc* funcRef = expression.AsFuncReference().value;
                        result.resolvesTo = LinxcTypeReference(&funcRef->asType);
                    }


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
                    result.priority = false;
                    result.data.typeCast = typeCast;//expression.ToHeap(this->allocator);
                    result.ID = LinxcExpr_TypeCast;
                    result.resolvesTo = expression.AsTypeReference().value;
                    return option<LinxcExpression>(result);
                }
                else //If not, it's a nested expression
                {
                    expression.priority = true;
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
                ERR_MSG msg = ERR_MSG(this->allocator, "No type, function, namespace or variable of name ");
                msg.AppendDeinit(token.ToString(GetDefaultAllocator()));
                msg.Append(" exists");
                if (prevScopeIfAny.present && (prevScopeIfAny.value.ID != LinxcExpr_NamespaceRef || prevScopeIfAny.value.data.namespaceRef->actualNamespace->name.buffer != NULL))
                {
                    msg.Append(" within scope ");
                    msg.AppendDeinit(prevScopeIfAny.value.ToString(GetDefaultAllocator()));
                }
                state->parsingFile->errors.Add(msg);
                return option<LinxcExpression>();
            }

            collections::Array<LinxcExpression> potentialTemplateArgs = collections::Array<LinxcExpression>();
            if ((result.value.ID == LinxcExpr_TypeRef || result.value.ID == LinxcExpr_FunctionRef) && state->tokenizer->PeekNextUntilValid().ID == Linxc_AngleBracketLeft)
            {
                if ((result.value.ID == LinxcExpr_TypeRef && result.value.data.typeRef.lastType != NULL && result.value.data.typeRef.lastType->templateArgs.length > 0) || (result.value.ID == LinxcExpr_FunctionRef && result.value.data.functionRef->templateArgs.length > 0))
                {
                    collections::vector<LinxcExpression> templateArgs = collections::vector<LinxcExpression>(GetDefaultAllocator());
                    state->tokenizer->NextUntilValid();
                    bool expectComma = false;
                    while (true)
                    {
                        LinxcToken peekNext = state->tokenizer->PeekNextUntilValid();
                        if (peekNext.ID == Linxc_AngleBracketRight)
                        {
                            state->tokenizer->NextUntilValid();
                            break;
                        }
                        else if (peekNext.ID == Linxc_Comma)
                        {
                            if (expectComma)
                            {
                                state->tokenizer->NextUntilValid();
                                expectComma = false;
                            }
                            else
                            {
                                state->parsingFile->errors.Add(ERR_MSG(this->allocator, "Unexpected comma"));
                            }
                        }
                        option<LinxcExpression> primaryExprOpt = this->ParseExpressionPrimary(state);
                        if (primaryExprOpt.present)
                        {
                            LinxcExpression expr = this->ParseExpression(state, primaryExprOpt.value, GetPrecedence(Linxc_ColonColon));
                            if (expr.resolvesTo.lastType != NULL)
                            {
                                state->parsingFile->errors.Add(ERR_MSG(this->allocator, "Invalid type as template argument"));
                            }
                            else
                            {
                                templateArgs.Add(expr);
                                expectComma = true;
                            }
                        }
                        else break;
                    }

                    //specialize type
                    if (result.value.ID == LinxcExpr_TypeRef)
                    {
                        if (result.value.data.typeRef.lastType->templateArgs.length == templateArgs.count)
                        {
                            //add reference to type
                            TemplateSpecialization templateArgsTyperef = TemplateSpecialization(this->allocator, result.value.data.typeRef.lastType->templateArgs.length);
                            bool dontAdd = false;
                            for (usize i = 0; i < templateArgs.count; i++)
                            {
                                //dont specialize if we are an incomplete generic (Which may occur with nested generic types)
                                LinxcTypeReference asTypeReference = templateArgs.ptr[i].AsTypeReference().value;
                                if (asTypeReference.genericTypeName.buffer != NULL)
                                {
                                    dontAdd = true;
                                    break;
                                }
                                templateArgsTyperef.data[i] = asTypeReference;
                            }
                            if (!dontAdd)
                            {
                                LinxcType* specializedSignature = result.value.data.typeRef.lastType->templateSpecializations.Get(templateArgsTyperef);
                                if (specializedSignature != NULL)
                                {
                                    //if the specializations list already contains such a specialization, dispose of the specialization
                                    templateArgsTyperef.deinit();
                                }
                                else
                                {
                                    LinxcType toAdd = result.value.data.typeRef.lastType->Specialize(this->allocator, result.value.data.typeRef.lastType->templateArgs, templateArgsTyperef);
                                    //printf("%s\n", result.value.data.typeRef.lastType->templateArgs.data[0].buffer);
                                    specializedSignature = result.value.data.typeRef.lastType->templateSpecializations.Add(templateArgsTyperef, toAdd);
                                }
                                //printf("Specialized into type %s\n", specializedSignature.arg);
                                result.value.data.typeRef.lastType = specializedSignature;
                            }
                            else templateArgsTyperef.deinit();

                            //even if we have the specialized signature, still set template args as specialized signature is not accounted for when transpiling
                            result.value.data.typeRef.templateArgs = templateArgs.ToOwnedArrayWith(this->allocator);
                            
                            templateArgs.deinit();
                        }
                        else
                        {
                            ERR_MSG msg = ERR_MSG(this->allocator, "Type expects ");
                            msg.Append((u64)result.value.data.typeRef.lastType->templateArgs.length);
                            msg.Append(" template arguments, but provided ");
                            msg.Append((u64)templateArgs.count);
                            state->parsingFile->errors.Add(msg);
                            templateArgs.deinit();
                        }
                    }
                    else //forward it just in case we are referring to a function
                    {
                        potentialTemplateArgs = templateArgs.ToOwnedArrayWith(this->allocator);
                    }
                }
                else
                {
                    if (result.value.ID == LinxcExpr_TypeRef)
                    {
                        ERR_MSG msg = ERR_MSG(this->allocator, "Type ");
                        msg.AppendDeinit(token.ToString(GetDefaultAllocator()));
                        msg.Append(" is not a generic type");
                        state->parsingFile->errors.Add(msg);
                    }
                    else
                    {
                        ERR_MSG msg = ERR_MSG(this->allocator, "Function ");
                        msg.AppendDeinit(token.ToString(GetDefaultAllocator()));
                        msg.Append(" is not a generic function");
                        state->parsingFile->errors.Add(msg);
                    }
                }
            }

            bool isReferencingFuncPointer = false;
            //todo: Make it function with operators as well, since ParseIdentifier may actually return an
            //operator after inserting 'this' into a member variable reference
            if (result.value.ID == LinxcExpr_Variable || result.value.ID == LinxcExpr_OperatorCall)
            {
                LinxcTypeReference varType = result.value.resolvesTo;//.variable->type.AsTypeReference().value;

                //printf("Found reference to variable %s\n", result.value.data.variable->name.buffer);
                if (varType.pointerCount == 0 && state->tokenizer->PeekNextUntilValid().ID == Linxc_LBracket)
                {
                    ERR_MSG msg = ERR_MSG(this->allocator, "Attempting to index into a type that does not support indexing");
                    state->parsingFile->errors.Add(msg);
                }

                while (varType.pointerCount > 0)
                {
                    //array semantics
                    if (state->tokenizer->PeekNextUntilValid().ID == Linxc_LBracket)
                    {

                        state->tokenizer->NextUntilValid();
                        option<LinxcExpression> primaryExprOpt = this->ParseExpressionPrimary(state);
                        if (primaryExprOpt.present)
                        {
                            LinxcExpression indexerExpression = this->ParseExpression(state, primaryExprOpt.value, -1);

                            if (indexerExpression.resolvesTo.lastType != NULL)
                            {
                                LinxcExpression* indexerExpressionPtr = (LinxcExpression*)this->allocator->Allocate(sizeof(LinxcExpression));
                                *indexerExpressionPtr = indexerExpression;

                                LinxcIndexerCall indexerCall;
                                indexerCall.indexerOperator = NULL; //todo
                                indexerCall.inputParams = indexerExpressionPtr;
                                if (result.value.ID == LinxcExpr_Variable)
                                {
                                    indexerCall.variableToIndex = result.value.data.variable;
                                    result.value.ID = LinxcExpr_IndexerCall;
                                    result.value.data.indexerCall = indexerCall;
                                }
                                else
                                {
                                    indexerCall.variableToIndex = result.value.data.operatorCall->rightExpr.data.variable;
                                    result.value.data.operatorCall->rightExpr.ID = LinxcExpr_IndexerCall;
                                    result.value.data.operatorCall->rightExpr.data.indexerCall = indexerCall;
                                }

                                if (indexerCall.indexerOperator != NULL)
                                {
                                    varType = indexerCall.indexerOperator->function.returnType.AsTypeReference().value;
                                }
                                else if (varType.pointerCount > 0)
                                {
                                    varType.pointerCount -= 1;
                                }

                                result.value.resolvesTo = varType;
                            }
                            else
                            {
                                state->parsingFile->errors.Add(ERR_MSG(this->allocator, "Cannot use a type, function or namespace name to index an array type"));
                            }
                        }

                        if (state->tokenizer->PeekNextUntilValid().ID != Linxc_RBracket)
                        {
                            state->parsingFile->errors.Add(ERR_MSG(this->allocator, "Expected closing ]"));
                        }
                        else state->tokenizer->NextUntilValid();
                    }
                    else break;
                }

                if (varType.lastType != NULL)
                {
                    if (varType.lastType->delegateDecl.name.buffer != NULL)
                    {
                        isReferencingFuncPointer = true;
                    }
                }
            }

            if (result.value.ID == LinxcExpr_FunctionRef || isReferencingFuncPointer)
            {
                if (state->tokenizer->PeekNextUntilValid().ID == Linxc_LParen)
                {
                    state->tokenizer->NextUntilValid();

                    //PARSE FUNCTION POINTER INVOCATION
                    if (isReferencingFuncPointer)
                    {
                        LinxcTypeReference varType = result.value.resolvesTo;
                        LinxcFuncPtr* funcPtrDecl = &varType.lastType->delegateDecl;

                        collections::vector<LinxcExpression> inputArgs = collections::vector<LinxcExpression>(GetDefaultAllocator());
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
                                bool tooManyArgs = i >= funcPtrDecl->arguments.length;

                                option<LinxcExpression> primaryOpt = this->ParseExpressionPrimary(state);
                                if (!primaryOpt.present)
                                {
                                    break;
                                }
                                LinxcExpression fullExpression = this->ParseExpression(state, primaryOpt.value, -1);

                                if (fullExpression.resolvesTo.lastType == NULL && fullExpression.resolvesTo.genericTypeName.buffer == NULL)
                                {
                                    state->parsingFile->errors.Add(ERR_MSG(this->allocator, "Cannot parse a type name as a variable. Did you mean sizeof(), nameof() or typeof() instead?"));
                                }

                                inputArgs.Add(fullExpression);

                                //this wont be present if our variable is open ended and typeless
                                option<LinxcTypeReference> expectedType = funcPtrDecl->arguments.data[i].AsTypeReference();

                                if (expectedType.present && !tooManyArgs)
                                {
                                    if (!CanAssign(expectedType.value, fullExpression.resolvesTo))
                                    {
                                        ERR_MSG msg = ERR_MSG(this->allocator, "Argument of type ");
                                        msg.AppendDeinit(fullExpression.resolvesTo.ToString(GetDefaultAllocator()));
                                        msg.Append(" cannot be implicitly converted to parameter type ");
                                        msg.AppendDeinit(expectedType.value.ToString(GetDefaultAllocator()));
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
                                    ERR_MSG msg = ERR_MSG(this->allocator, "Expected , or ) after function input argument. Got ");
                                    msg.AppendDeinit(peekNext.ToString(GetDefaultAllocator()));
                                    msg.Append(" instead.");
                                    state->parsingFile->errors.Add(msg);
                                }

                                i += 1;
                            }
                        }

                        if (inputArgs.count > funcPtrDecl->arguments.length)
                        {
                            ERR_MSG msg = ERR_MSG(this->allocator, "Provided too many input params to function pointer invocation, expected ");
                            msg.Append(funcPtrDecl->arguments.length);
                            msg.Append(" arguments, provided ");
                            msg.Append(inputArgs.count);
                            state->parsingFile->errors.Add(msg);
                        }
                        else if (inputArgs.count < funcPtrDecl->necessaryArguments)
                        {
                            ERR_MSG msg = ERR_MSG(this->allocator, "Provided too few input params to function pointer invocation, expected ");
                            msg.Append((u64)funcPtrDecl->necessaryArguments);
                            msg.Append(" arguments, provided ");
                            msg.Append(inputArgs.count);
                            state->parsingFile->errors.Add(msg);
                        }

                        if (result.value.ID == LinxcExpr_Variable)
                        {
                            LinxcExpression finalResult;

                            finalResult.ID = LinxcExpr_FuncPtrCall;
                            finalResult.data.functionPointerCall.inputParams = inputArgs.ToOwnedArrayWith(this->allocator);
                            finalResult.data.functionPointerCall.variable = result.value.data.variable;
                            finalResult.data.functionPointerCall.functionPtrType = &result.value.data.variable->type.AsTypeReference().value.lastType->delegateDecl;
                            finalResult.resolvesTo = finalResult.data.functionPointerCall.functionPtrType->returnType->AsTypeReference().value;

                            return option<LinxcExpression>(finalResult);
                        }
                        else
                        {
                            LinxcExpression finalResult;

                            finalResult.ID = LinxcExpr_FuncPtrCall;
                            finalResult.data.functionPointerCall.inputParams = inputArgs.ToOwnedArrayWith(this->allocator);
                            finalResult.data.functionPointerCall.variable = result.value.data.operatorCall->rightExpr.data.variable;
                            finalResult.data.functionPointerCall.functionPtrType = &result.value.data.operatorCall->rightExpr.data.variable->type.AsTypeReference().value.lastType->delegateDecl;
                            finalResult.resolvesTo = finalResult.data.functionPointerCall.functionPtrType->returnType->AsTypeReference().value;

                            result.value.data.operatorCall->rightExpr = finalResult;
                        }
                    }
                    //PARSE FUNCTION INVOCATION
                    else
                    {
                        //parse input args
                        LinxcFunc* func = result.value.data.functionRef;

                        collections::vector<LinxcExpression> inputArgs = collections::vector<LinxcExpression>(GetDefaultAllocator());
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
                                bool tooManyArgs = i >= func->arguments.length;

                                option<LinxcExpression> primaryOpt = this->ParseExpressionPrimary(state);
                                if (!primaryOpt.present)
                                {
                                    break;
                                }
                                LinxcExpression fullExpression = this->ParseExpression(state, primaryOpt.value, -1);

                                if (fullExpression.resolvesTo.lastType == NULL && fullExpression.resolvesTo.genericTypeName.buffer == NULL)
                                {
                                    state->parsingFile->errors.Add(ERR_MSG(this->allocator, "Cannot parse a type name as a variable. Did you mean sizeof(), nameof() or typeof() instead?"));
                                }

                                inputArgs.Add(fullExpression);

                                //this wont be present if our variable is open ended and typeless
                                option<LinxcTypeReference> expectedType = func->arguments.data[i].type.AsTypeReference();

                                if (expectedType.present && !tooManyArgs)
                                {
                                    expectedType.value.isConst = func->arguments.data[i].isConst;

                                    //printf("is const: %s\n", expectedType.value.isConst ? "true" : "false");
                                    if (!CanAssign(expectedType.value, fullExpression.resolvesTo))
                                    {
                                        ERR_MSG msg = ERR_MSG(this->allocator, "Argument of type ");
                                        msg.AppendDeinit(fullExpression.resolvesTo.ToString(GetDefaultAllocator()));
                                        msg.Append(" cannot be implicitly converted to parameter type ");
                                        msg.AppendDeinit(expectedType.value.ToString(GetDefaultAllocator()));
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
                                    ERR_MSG msg = ERR_MSG(this->allocator, "Expected , or ) after function input argument. Got ");
                                    msg.AppendDeinit(peekNext.ToString(GetDefaultAllocator()));
                                    msg.Append(" instead.");
                                    state->parsingFile->errors.Add(msg);
                                }
                                //if we reach an open ended function, that means we've come to the end. Do not parse further
                                //func->arguments.data[i].name can be NULL if parsing function pointer
                                if (expectedType.present)//func->arguments.data[i].name.buffer != NULL && func->arguments.data[i].name != "...")
                                {
                                    i += 1;
                                }
                            }
                        }

                        //this only applies to non-open ended functions
                        if ((func->arguments.length == 0 || func->arguments.data[func->arguments.length - 1].name != "...") && inputArgs.count > func->arguments.length)
                        {
                            ERR_MSG msg = ERR_MSG(this->allocator, "Provided too many input params to function, expected ");
                            msg.Append(func->arguments.length);
                            msg.Append(" arguments, provided ");
                            msg.Append(inputArgs.count);
                            state->parsingFile->errors.Add(msg);
                        }
                        else if (inputArgs.count < func->necessaryArguments)
                        {
                            ERR_MSG msg = ERR_MSG(this->allocator, "Provided too few input params to function, expected ");
                            msg.Append((u64)func->necessaryArguments);
                            msg.Append(" arguments, provided ");
                            msg.Append(inputArgs.count);
                            state->parsingFile->errors.Add(msg);
                        }

                        LinxcExpression finalResult;

                        finalResult.ID = LinxcExpr_FuncCall;
                        finalResult.data.functionCall.thisAsParam = NULL;
                        finalResult.data.functionCall.func = result.value.data.functionRef;
                        finalResult.data.functionCall.inputParams = inputArgs.ToOwnedArrayWith(this->allocator);
                        finalResult.data.functionCall.templateArgs = potentialTemplateArgs;
                        finalResult.resolvesTo = result.value.data.functionRef->returnType.AsTypeReference().value;

                        return option<LinxcExpression>(finalResult);
                    }
                }
                else
                {
                    //don't throw an error as &functionName is a technically 2 valid primary parses
                    return result;
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
            result.priority = false;
            result.data.literal = token.ToString(this->allocator);
            result.ID = LinxcExpr_Literal;

            string temp;
            if (token.ID == Linxc_Keyword_true || token.ID == Linxc_Keyword_false)
            {
                temp = string(GetDefaultAllocator(), "bool");
            }
            else if (token.ID == Linxc_FloatLiteral)
            {
                temp = string(GetDefaultAllocator(), "float");
            }
            else if (token.ID == Linxc_IntegerLiteral)
            {
                temp = string(GetDefaultAllocator(), "i32");
            }
            else if (token.ID == Linxc_CharLiteral)
            {
                temp = string(GetDefaultAllocator(), "u8");
            }
            else if (token.ID == Linxc_StringLiteral)
            {
                temp = string(GetDefaultAllocator(), "u8");
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

        //increment
        if (op.ID == Linxc_PlusPlus || op.ID == Linxc_MinusMinus)
        {
            LinxcOperator* incrementCall = (LinxcOperator*)this->allocator->Allocate(sizeof(LinxcOperator));
            incrementCall->leftExpr = lhs;
            incrementCall->rightExpr.ID = LinxcExpr_Literal;
            incrementCall->rightExpr.resolvesTo = LinxcTypeReference(incrementCall->leftExpr.resolvesTo.lastType);
            incrementCall->rightExpr.data.literal = string(this->allocator, "1");
            incrementCall->operatorType = op.ID == Linxc_PlusPlus ? Linxc_PlusEqual : Linxc_MinusEqual;
            
            option<LinxcTypeReference> resolvesTo = incrementCall->EvaluatePossible();
            if (!resolvesTo.present)
            {
                ERR_MSG msg = ERR_MSG(this->allocator, "Type ");
                msg.AppendDeinit(incrementCall->leftExpr.resolvesTo.ToString(GetDefaultAllocator()));
                if (op.ID == Linxc_PlusPlus)
                {
                    msg.Append(" cannot be incremented");
                }
                else msg.Append(" cannot be decremented");
                state->parsingFile->errors.Add(msg);
            }
            else
            {
                lhs.resolvesTo = resolvesTo.value;
                lhs.data.operatorCall = incrementCall;
                lhs.ID = LinxcExpr_OperatorCall;
            }

            state->tokenizer->NextUntilValid();
        }

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
            //state->parsingFile->errors.Add(ERR_MSG(GetDefaultAllocator(), "Error parsing right side of operator"));
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
            if (operatorCall->leftExpr.resolvesTo.lastType != NULL && operatorCall->rightExpr.resolvesTo.lastType != NULL)
            {
                printf("Left type: %i, right type: %i\n", operatorCall->leftExpr.resolvesTo.templateArgs.length, operatorCall->rightExpr.resolvesTo.templateArgs.length);
            }
            ERR_MSG msg = ERR_MSG(this->allocator, "Type ");
            msg.AppendDeinit(operatorCall->leftExpr.resolvesTo.ToString(GetDefaultAllocator()));
            msg.Append(" cannot be ");
            msg.Append(LinxcTokenIDToString(op.ID));
            msg.Append("'d with ");
            msg.AppendDeinit(operatorCall->rightExpr.resolvesTo.ToString(GetDefaultAllocator()));
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
    result.priority = false;
    result.ID = LinxcExpr_None;
    LinxcToken token = state->tokenizer->NextUntilValid();
    string identifierName = token.ToString(GetDefaultAllocator());

    if (LinxcIsPrimitiveType(token.ID))
    {
        LinxcType *type = this->globalNamespace.types.Get(identifierName);
        LinxcTypeReference reference;
        reference.isConst = false;
        reference.lastType = type;

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

            LinxcVar** asLocalVar = state->varsInScope.Get(identifierName);
            if (asLocalVar != NULL)
            {
                LinxcVar* localVar = *asLocalVar;
                if (localVar->memberOf != NULL)
                {
                    LinxcVar* thisVar = *state->varsInScope.Get(this->thisKeyword);
                    LinxcExpression thisExpr;
                    thisExpr.priority = false;
                    thisExpr.resolvesTo.isConst = thisVar->isConst;
                    thisExpr.resolvesTo = thisVar->type.AsTypeReference().value;
                    thisExpr.ID = LinxcExpr_Variable;
                    thisExpr.data.variable = thisVar;

                    LinxcExpression varExpr;
                    varExpr.priority = false;
                    varExpr.ID = LinxcExpr_Variable;
                    varExpr.data.variable = localVar;
                    varExpr.resolvesTo = localVar->type.AsTypeReference().value;
                    varExpr.resolvesTo.isConst = localVar->isConst;

                    result.ID = LinxcExpr_OperatorCall;
                    LinxcOperator* thisDereference = (LinxcOperator*)this->allocator->Allocate(sizeof(LinxcOperator));
                    thisDereference->leftExpr = thisExpr;
                    thisDereference->operatorType = Linxc_Arrow;
                    thisDereference->rightExpr = varExpr;
                    result.data.operatorCall = thisDereference;
                    result.resolvesTo = varExpr.resolvesTo;
                }
                else
                {
                    result.ID = LinxcExpr_Variable;
                    result.data.variable = *asLocalVar;
                    result.resolvesTo = result.data.variable->type.AsTypeReference().value;
                    result.resolvesTo.isConst = result.data.variable->isConst;
                }
            }
            else
            {
                //check state namespaces
                LinxcPhoneyNamespace* toCheck = state->currentNamespace;
                while (toCheck != NULL)
                {
                    LinxcFunc* asFunction = toCheck->functionRefs.GetCopyOr(identifierName, NULL);
                    if (asFunction != NULL)
                    {
                        result.ID = LinxcExpr_FunctionRef;
                        result.data.functionRef = asFunction;
                        result.resolvesTo = asFunction->returnType.AsTypeReference().value;
                        break;
                    }
                    else
                    {
                        LinxcVar* asVar = toCheck->variableRefs.GetCopyOr(identifierName, NULL);
                        if (asVar != NULL)
                        {
                            result.ID = LinxcExpr_Variable;
                            result.data.variable = asVar;
                            //this is guaranteed to be present as a variable would only have a typename-resolveable expression as it's type
                            result.resolvesTo = asVar->type.AsTypeReference().value;
                            result.resolvesTo.isConst = asVar->isConst;
                            break;
                        }
                        else
                        {
                            LinxcType* asType = toCheck->typeRefs.GetCopyOr(identifierName, NULL);
                            if (asType != NULL)
                            {
                                result.ID = LinxcExpr_TypeRef;
                                result.data.typeRef = asType;
                                result.resolvesTo.lastType = NULL;
                                break;
                            }
                            else
                            {
                                LinxcTypeReference* asTypeReference = toCheck->typedefs.Get(identifierName);
                                if (asTypeReference != NULL)
                                {
                                    result.ID = LinxcExpr_TypeRef;
                                    result.data.typeRef = *asTypeReference;
                                    result.resolvesTo.lastType = NULL;
                                    break;
                                }
                                else
                                {
                                    LinxcPhoneyNamespace* asNamespace = toCheck->subNamespaces.Get(identifierName);
                                    if (asNamespace != NULL)
                                    {
                                        result.ID = LinxcExpr_NamespaceRef;
                                        //TODO: Something tells me this is a terrible idea to pass this as pointer, who knows though
                                        result.data.namespaceRef = asNamespace;
                                        result.resolvesTo.lastType = NULL;
                                        break;
                                    }
                                }
                            }
                        }
                    }

                    toCheck = toCheck->parentNamespace;
                }

                LinxcType* typeCheck = state->parentType;
                while (typeCheck != NULL)
                {
                    //printf("Checking for identifier %s in type %s\n", identifierName.buffer, typeCheck->name.buffer);
                    if (typeCheck->templateArgs.Contains(identifierName, &stringEql).present)
                    {
                        result.ID = LinxcExpr_TypeRef;
                        result.data.typeRef = LinxcTypeReference(NULL);
                        result.data.typeRef.genericTypeName = identifierName.Clone(this->allocator);
                        result.resolvesTo.lastType = NULL;
                    }
                    else
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
                    }

                    typeCheck = typeCheck->parentType;
                }
            }

        }
        else if (parentScopeOverride.value.ID == LinxcExpr_NamespaceRef)
        {
            LinxcPhoneyNamespace* toCheck = parentScopeOverride.value.data.namespaceRef;
            //only need to check immediate parent scope's namespace
            LinxcFunc* asFunction = toCheck->functionRefs.GetCopyOr(identifierName, NULL);
            if (asFunction != NULL)
            {
                result.ID = LinxcExpr_FunctionRef;
                result.data.functionRef = asFunction;
                result.resolvesTo = asFunction->returnType.AsTypeReference().value;
            }
            else
            {
                LinxcVar* asVar = toCheck->variableRefs.GetCopyOr(identifierName, NULL);
                if (asVar != NULL)
                {
                    result.ID = LinxcExpr_Variable;
                    result.data.variable = asVar;
                    result.resolvesTo = asVar->type.AsTypeReference().value;
                    result.resolvesTo.isConst = asVar->isConst;
                }
                else
                {
                    LinxcType* asType = toCheck->typeRefs.GetCopyOr(identifierName, NULL);
                    if (asType != NULL)
                    {
                        result.ID = LinxcExpr_TypeRef;
                        result.data.typeRef = asType;
                        result.resolvesTo.lastType = NULL;
                    }
                    else
                    {
                        LinxcTypeReference* asTypeReference = toCheck->typedefs.Get(identifierName);
                        if (asTypeReference != NULL)
                        {
                            result.ID = LinxcExpr_TypeRef;
                            result.data.typeRef = *asTypeReference;
                            result.resolvesTo.lastType = NULL;
                        }
                        else
                        {
                            LinxcPhoneyNamespace* asNamespace = toCheck->subNamespaces.Get(identifierName);
                            if (asNamespace != NULL)
                            {
                                result.ID = LinxcExpr_NamespaceRef;
                                result.data.namespaceRef = asNamespace;
                                result.resolvesTo.lastType = NULL;
                            }
                        }
                    }
                }
            }
        }
        else if (parentScopeOverride.value.ID == LinxcExpr_TypeRef || parentScopeOverride.value.ID == LinxcExpr_Variable || parentScopeOverride.value.ID == LinxcExpr_FuncCall)
        {
            LinxcType* toCheck;
            collections::Array<LinxcExpression> templateArgs = collections::Array<LinxcExpression>();
            if (parentScopeOverride.value.ID == LinxcExpr_Variable)
            {
                LinxcTypeReference typeRef = parentScopeOverride.value.data.variable->type.AsTypeReference().value;
                toCheck = typeRef.lastType;
                templateArgs = typeRef.templateArgs;
                //printf("var name: %s, template args: %i\n", parentScopeOverride.value.data.variable->name.buffer, typeRef.templateArgs.length);
            }
            else if (parentScopeOverride.value.ID == LinxcExpr_FuncCall)
            {
                LinxcTypeReference typeRef = parentScopeOverride.value.data.functionCall.func->returnType.AsTypeReference().value;
                toCheck = typeRef.lastType;
                templateArgs = typeRef.templateArgs;
            }
            else
            {
                toCheck = parentScopeOverride.value.data.typeRef.lastType;
            }
            //only need to check immediate parent scope's namespace
            LinxcFunc* asFunction = toCheck->FindFunction(identifierName);
            if (asFunction != NULL)
            {
                result.ID = LinxcExpr_FunctionRef;
                result.data.functionRef = asFunction;
                result.resolvesTo = asFunction->returnType.AsTypeReference().value;
            }
            else
            {
                LinxcVar* asVar = toCheck->FindVar(identifierName);
                if (asVar != NULL)
                {
                    result.ID = LinxcExpr_Variable;
                    result.data.variable = asVar;
                    result.resolvesTo = asVar->type.AsTypeReference().value;
                    result.resolvesTo.templateArgs = templateArgs;
                    result.resolvesTo.isConst = asVar->isConst;
                }
                else
                {
                    LinxcType* asType = toCheck->FindSubtype(identifierName);
                    if (asType != NULL)
                    {
                        result.ID = LinxcExpr_TypeRef;
                        result.data.typeRef.templateArgs = templateArgs;
                        result.data.typeRef.lastType = asType;
                        result.resolvesTo.lastType = NULL;
                    }
                    else
                    {
                        LinxcEnumMember* asEnumMember = toCheck->FindEnumMember(identifierName);
                        if (asEnumMember != NULL)
                        {
                            result.ID = LinxcExpr_EnumMemberRef;
                            result.data.enumMemberRef = asEnumMember;
                            result.resolvesTo.lastType = toCheck;
                        }
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
            //dont set memberOf as we are function argument, not type field
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
    collections::Array<string> nextTemplateArgs = collections::Array<string>();
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
            if (token.ID == Linxc_Semicolon || (token.ID == Linxc_Comma && state->commaIsSemicolon) || token.ID == Linxc_LineComment || token.ID == Linxc_MultiLineComment || token.ID == Linxc_Hash || (token.ID == Linxc_RBrace && state->endOn == LinxcEndOn_RBrace) || (token.ID == Linxc_RParen && state->endOn == LinxcEndOn_RParen))
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
            if (token.ID == Linxc_Semicolon && state->endOn == LinxcEndOn_Semicolon)
            {
                break;
            }
            else if (token.ID == Linxc_RParen && state->endOn == LinxcEndOn_RParen)
            {
                break;
            }
            else if (((token.ID == Linxc_Semicolon && !state->commaIsSemicolon) || (token.ID == Linxc_Comma && state->commaIsSemicolon)) && expectSemicolon)
            {
                expectSemicolon = false;
                continue;
            }
            else if (expectSemicolon)
            {
                if (!state->commaIsSemicolon)
                {
                    errors->Add(ERR_MSG(this->allocator, "Expected semicolon ;"));
                }
                else
                {
                    errors->Add(ERR_MSG(this->allocator, "Expected comma ,"));
                }
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
            if (next.ID != Linxc_MacroString && next.ID != Linxc_StringLiteral)
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

                string extension = path::GetExtension(GetDefaultAllocator(), macroString);

                if (extension == ".linxc" || extension == ".hpp" || (extension == ".h" && macroString != "Linxc.h"))
                {
                    //parse that file, if we can find it
                    //and it hasnt been parsed yet
                    LinxcParsedFile* alreadyParsed = this->parsedFiles.Get(macroString);
                    if (alreadyParsed != NULL)
                    {
                        LinxcIncludeStatement includeStatement = LinxcIncludeStatement();
                        includeStatement.includedFile = alreadyParsed;
                        includeStatement.includeString = path::SwapExtension(this->allocator, macroString, ".h");

                        state->currentNamespace->Add(&alreadyParsed->fileNamespace);

                        LinxcStatement stmt;
                        stmt.data.includeStatement = includeStatement;
                        stmt.ID = LinxcStmt_Include;
                        result.Add(stmt);
                        macroString.deinit();
                    }
                    else
                    {
                        //search files
                        LinxcIncludedFile* includedFile = this->includedFiles.Get(macroString);
                        if (includedFile == NULL)
                        {
                            //search include directories for file
                            for (usize i = 0; i < this->includeDirectories.count; i++)
                            {
                                string filePath = this->includeDirectories.ptr[i].Clone(GetDefaultAllocator());
                                filePath.Append("/");
                                filePath.Append(macroString.buffer);

                                if (io::FileExists(filePath.buffer))
                                {
                                    LinxcIncludedFile newIncludedFile;
                                    newIncludedFile.fullNameAndPath = filePath.CloneDeinit(this->allocator);
                                    newIncludedFile.includeName = macroString.Clone(this->allocator);
                                    includedFile = this->includedFiles.Add(newIncludedFile.includeName, newIncludedFile);
                                    break;
                                }
                                else filePath.deinit();

                                filePath = this->includeDirectories.ptr[i].Clone(GetDefaultAllocator());
                                filePath.Append("/");
                                filePath.AppendDeinit(path::SwapExtension(GetDefaultAllocator(), macroString, ".linxch"));

                                if (io::FileExists(filePath.buffer))
                                {
                                    LinxcIncludedFile newIncludedFile;
                                    newIncludedFile.fullNameAndPath = filePath.CloneDeinit(this->allocator);
                                    newIncludedFile.includeName = macroString.Clone(this->allocator);
                                    includedFile = this->includedFiles.Add(newIncludedFile.includeName, newIncludedFile);
                                    break;
                                }
                                else filePath.deinit();
                            }
                        }

                        if (includedFile != NULL)
                        {
                            string fileContents = io::ReadFile(GetDefaultAllocator(), includedFile->fullNameAndPath.buffer);
                            printf("Parsing file %s\n", includedFile->fullNameAndPath.buffer);
                            LinxcParsedFile* parsedInclude = this->ParseFile(includedFile->fullNameAndPath, includedFile->includeName, fileContents);
                            if (parsedInclude->errors.count > 0)
                            {
                                errors->Add(ERR_MSG(this->allocator, "Included file has errors, stopping compilation for this file"));
                                toBreak = true;
                                break;
                            }
                            else
                            {
                                LinxcIncludeStatement includeStatement = LinxcIncludeStatement();
                                includeStatement.includedFile = parsedInclude;
                                includeStatement.includeString = path::SwapExtension(this->allocator, macroString, ".h");

                                state->currentNamespace->Add(&parsedInclude->fileNamespace);

                                LinxcStatement stmt;
                                stmt.data.includeStatement = includeStatement;
                                stmt.ID = LinxcStmt_Include;
                                result.Add(stmt);
                                macroString.deinit();
                            }
                            fileContents.deinit();
                        }
                        else
                        {
                            ERR_MSG msg = ERR_MSG(this->allocator, "Could not find included file ");
                            msg.Append(macroString.buffer);
                            errors->Add(msg);
                        }
                    }
                }

                extension.deinit();
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
                string namespaceNameStrTemp = namespaceName.ToString(GetDefaultAllocator());
                //LinxcNamespace* thisNamespace = state->currentNamespace->subNamespaces.Get(namespaceNameStrTemp);
                LinxcPhoneyNamespace* thisNamespace = state->currentNamespace->subNamespaces.Get(namespaceNameStrTemp);

                if (thisNamespace == NULL)
                {
                    string namespaceNameStr = namespaceName.ToString(this->allocator);
                    LinxcNamespace newNamespace = LinxcNamespace(this->allocator, namespaceNameStr);
                    newNamespace.parentNamespace = state->currentNamespace->actualNamespace;
                    thisNamespace = state->currentNamespace->AddNamespaceToOrigin(namespaceNameStr, newNamespace);
                    thisNamespace->parentNamespace = state->currentNamespace;
                    thisNamespace->name = namespaceNameStr;
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
                namespaceScope.referencedNamespace = thisNamespace->actualNamespace;
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
        case Linxc_Keyword_template:
        {
            if (isComment)
            {
                continue;
            }
            if (nextIsConst)
            {
                errors->Add(ERR_MSG(this->allocator, "Cannot declare a template as const in Linxc"));
                nextIsConst = false;
            }
            if (tokenizer->PeekNextUntilValid().ID != Linxc_AngleBracketLeft)
            {
                errors->Add(ERR_MSG(this->allocator, "Expected <"));
            }
            else tokenizer->NextUntilValid();

            collections::vector<string> templateArgs = collections::vector<string>(GetDefaultAllocator());

            bool expectComma = false;
            while (true)
            {
                LinxcToken peekNext = state->tokenizer->PeekNextUntilValid();
                if (peekNext.ID == Linxc_AngleBracketRight)
                {
                    break;
                }
                else if (peekNext.ID == Linxc_Comma)
                {
                    if (expectComma)
                    {
                        state->tokenizer->NextUntilValid();
                        expectComma = false;
                    }
                    else
                    {
                        state->parsingFile->errors.Add(ERR_MSG(this->allocator, "Unexpected comma"));
                    }
                }
                else if (peekNext.ID == Linxc_Keyword_typename)
                {
                    state->tokenizer->NextUntilValid();
                    LinxcToken genericTypeName = state->tokenizer->NextUntilValid();
                    if (genericTypeName.ID != Linxc_Identifier)
                    {
                        state->parsingFile->errors.Add(ERR_MSG(this->allocator, "Expected valid identifier"));
                    }
                    string genericTypeNameStr = genericTypeName.ToString(this->allocator);
                    templateArgs.Add(genericTypeNameStr);
                }
                else
                {
                    state->parsingFile->errors.Add(ERR_MSG(this->allocator, "Expected keyword 'typename'"));
                }

                expectComma = true;
            }

            nextTemplateArgs = templateArgs.ToOwnedArrayWith(this->allocator);
        }
        break;
        case Linxc_Keyword_enum:
        {
            if (isComment)
            {
                continue;
            }
            if (nextIsConst)
            {
                errors->Add(ERR_MSG(this->allocator, "Cannot declare an enum as const in Linxc"));
                nextIsConst = false;
            }
            if (nextTemplateArgs.data != NULL)
            {
                errors->Add(ERR_MSG(this->allocator, "Enum type cannot have templates"));
                nextTemplateArgs.deinit();
            }
            LinxcToken enumName = tokenizer->NextUntilValid();
            //printf("STRUCT FOUND: %s\n", structName.ToString(this->allocator).buffer);

            if (enumName.ID != Linxc_Identifier)
            {
                errors->Add(ERR_MSG(this->allocator, "Expected a valid enum name after enum keyword!"));
            }
            else
            {
                if (tokenizer->NextUntilValid().ID != Linxc_LBrace)
                {
                    errors->Add(ERR_MSG(this->allocator, "Expected { after enum name"));
                }

                LinxcType enumType = LinxcType(allocator, enumName.ToString(allocator), state->currentNamespace->actualNamespace, state->parentType);
                //LinxcEnum enumerable = LinxcEnum(this->allocator, enumName.ToString(allocator));

                if (tokenizer->PeekNextUntilValid().ID != Linxc_RBrace)
                {
                    collections::hashmap<string, LinxcEnumMember> nameToEnumMember = collections::hashmap<string, LinxcEnumMember>(this->allocator, &stringHash, &stringEql);

                    i32 maxCount = 0;
                    while (true)
                    {
                        LinxcToken nextToken = tokenizer->NextUntilValid();
                        if (nextToken.ID != Linxc_Identifier)
                        {
                            errors->Add(ERR_MSG(this->allocator, "Expected enum item name to be valid identifier"));
                            break;
                        }
                        string memberName = nextToken.ToString(this->allocator);
                        LinxcToken next = tokenizer->NextUntilValid();
                        if (next.ID == Linxc_Equal)
                        {
                            next = tokenizer->NextUntilValid();
                            string literalString = next.ToString(GetDefaultAllocator());
                            i32 value = 0;
                            if (next.ID == Linxc_Identifier)
                            {
                                LinxcEnumMember* memberRef = nameToEnumMember.Get(literalString);
                                if (memberRef != NULL)
                                {
                                    value = memberRef->value;
                                }
                                else
                                {
                                    errors->Add(ERR_MSG(this->allocator, "Expected enum member to equate to a constant expression"));
                                }
                            }
                            else if (next.ID != Linxc_IntegerLiteral)
                            {
                                errors->Add(ERR_MSG(this->allocator, "Expected enum member to have an integer literal!"));
                            }
                            else
                            {
                                value = atoi(literalString.buffer);
                            }
                            literalString.deinit();

                            LinxcEnumMember member;
                            member.name = memberName;
                            member.value = value;
                            if (nameToEnumMember.Contains(memberName))
                            {
                                ERR_MSG msg = ERR_MSG(this->allocator, "Enum already has member of name ");
                                msg.Append(memberName.buffer);
                                errors->Add(msg);
                                memberName.deinit();
                            }
                            else nameToEnumMember.Add(memberName, member);
                            maxCount = value + 1;

                            next = tokenizer->NextUntilValid();
                            if (next.ID == Linxc_RBrace)
                            {
                                break;
                            }
                            else if (next.ID == Linxc_Comma)
                            {
                                continue;
                            }
                            else
                            {
                                errors->Add(ERR_MSG(this->allocator, "Expected , or } after enum member declaration"));
                            }
                        }
                        else if (next.ID == Linxc_RBrace || next.ID == Linxc_Comma)
                        {
                            LinxcEnumMember member;
                            member.name = memberName;
                            member.value = maxCount;
                            if (nameToEnumMember.Contains(memberName))
                            {
                                ERR_MSG msg = ERR_MSG(this->allocator, "Enum already has member of name ");
                                msg.Append(memberName.buffer);
                                errors->Add(msg);
                                memberName.deinit();
                            }
                            else nameToEnumMember.Add(memberName, member);
                            maxCount += 1;
                            if (next.ID == Linxc_RBrace)
                            {
                                break;
                            }
                        }
                        else
                        {
                            errors->Add(ERR_MSG(this->allocator, "Expected }, = or , after enum member name"));
                            break;
                        }
                    }

                    for (usize i = 0; i < nameToEnumMember.bucketsCount; i++)
                    {
                        if (nameToEnumMember.buckets[i].initialized)
                        {
                            for (usize j = 0; j < nameToEnumMember.buckets[i].entries.count; j++)
                            {
                                enumType.enumMembers.Add(nameToEnumMember.buckets[i].entries.ptr[j].value);
                            }
                        }
                    }
                    nameToEnumMember.deinit();
                }
                LinxcType* ptr = state->currentNamespace->AddTypeToOrigin(enumType.name, enumType);//types.Add(enumType.name, enumType);

                LinxcStatement stmt;
                stmt.ID = LinxcStmt_TypeDecl;
                stmt.data.typeDeclaration = ptr;
                result.Add(stmt);

                expectSemicolon = true;
            }
        }
        break;
        case Linxc_Keyword_typedef:
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
            if (nextTemplateArgs.length > 0)
            {
                errors->Add(ERR_MSG(this->allocator, "typedef cannot have templates"));
                nextTemplateArgs.deinit();
            }
            
            option<LinxcExpression> primaryExprOpt = this->ParseExpressionPrimary(state);
            if (primaryExprOpt.present)
            {
                LinxcExpression existingTypeToAliasExpr = this->ParseExpression(state, primaryExprOpt.value, -1);
                if (!existingTypeToAliasExpr.AsTypeReference().present)
                {
                    errors->Add(ERR_MSG(this->allocator, "Attempting to typedef a non-type"));
                }
                else
                {
                    LinxcTypeReference existingTypeToAlias = existingTypeToAliasExpr.AsTypeReference().value;

                    LinxcToken next = tokenizer->NextUntilValid();
                    if (next.ID != Linxc_Identifier)
                    {
                        ERR_MSG msg = ERR_MSG(this->allocator, "Invalid alias for type: ");
                        msg.AppendDeinit(next.ToString(GetDefaultAllocator()));
                        errors->Add(msg);
                    }
                    else
                    {
                        string aliasName = next.ToString(this->allocator);

                        state->currentNamespace->typedefs.Add(aliasName, existingTypeToAlias);
                    }
                }
            }
            else
            {
                //todo: Pre-declare type
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

            LinxcToken structName = tokenizer->NextUntilValid();

            //printf("STRUCT FOUND: %s\n", structName.ToString(this->allocator).buffer);

            if (structName.ID != Linxc_Identifier)
            {
                errors->Add(ERR_MSG(this->allocator, "Expected a valid struct name after struct keyword!"));
            }
            else
            {
                //declare new struct
                LinxcType type = LinxcType(allocator, structName.ToString(allocator), state->currentNamespace->actualNamespace, state->parentType);

                LinxcToken next = tokenizer->PeekNextUntilValid();
                if (next.ID != Linxc_LBrace)
                {
                    errors->Add(ERR_MSG(this->allocator, "Expected { after struct name"));
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
                    ptr = state->currentNamespace->AddTypeToOrigin(type.name, type);
                }

                if (nextTemplateArgs.data != NULL)
                {
                    ptr->templateArgs = nextTemplateArgs;
                    nextTemplateArgs = collections::Array<string>();
                }

                LinxcParserState nextState = LinxcParserState(state->parser, state->parsingFile, state->tokenizer, LinxcEndOn_RBrace, false, state->parsingLinxci);
                nextState.parentType = ptr;
                nextState.endOn = LinxcEndOn_RBrace;
                nextState.currentNamespace = state->currentNamespace;
                //state->parsingFile->definedTypes.Add(ptr);

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
        case Linxc_Keyword_def_delegate:
        {
            if (state->tokenizer->NextUntilValid().ID != Linxc_LParen)
            {
                state->parsingFile->errors.Add(ERR_MSG(this->allocator, "Expected ( after def_delegate declaration"));
                errorSkipUntilSemicolon = true;
                break;
            }
            LinxcToken delegateTypeName = state->tokenizer->NextUntilValid();
            if (delegateTypeName.ID != Linxc_Identifier)
            {
                state->parsingFile->errors.Add(ERR_MSG(this->allocator, "Expected identifier for delegate type name"));
                errorSkipUntilSemicolon = true;
                break;
            }
            string delegateTypeNameStr = delegateTypeName.ToString(this->allocator);

            if (state->tokenizer->NextUntilValid().ID != Linxc_Comma)
            {
                state->parsingFile->errors.Add(ERR_MSG(this->allocator, "Expected ,"));
                errorSkipUntilSemicolon = true;
                break;
            }

            option<LinxcExpression> primaryExprOpt = this->ParseExpressionPrimary(state);
            if (!primaryExprOpt.present)
            {
                errorSkipUntilSemicolon = true;
                break;
            }
            LinxcExpression returnTypeExpr = this->ParseExpression(state, primaryExprOpt.value, -1);
            
            if (returnTypeExpr.resolvesTo.lastType != NULL)
            {
                state->parsingFile->errors.Add(ERR_MSG(this->allocator, "Second argument of def_delegate must be the delegate's return type, or 'void'"));
                errorSkipUntilSemicolon = true;
                break;
            }

            LinxcToken peekNext = tokenizer->PeekNextUntilValid();
            collections::vector<LinxcExpression> argTypes = collections::vector<LinxcExpression>(GetDefaultAllocator());
            if (peekNext.ID == Linxc_Comma)
            {
                tokenizer->NextUntilValid();

                while (true)
                {
                    primaryExprOpt = this->ParseExpressionPrimary(state);
                    if (!primaryExprOpt.present)
                    {
                        break;
                    }
                    LinxcExpression argTypeExpr = this->ParseExpression(state, primaryExprOpt.value, -1);
                    if (argTypeExpr.resolvesTo.lastType != NULL)
                    {
                        ERR_MSG msg = ERR_MSG(this->allocator, "3rd argument and onwards of a delegate definition must refer to a type");
                        state->parsingFile->errors.Add(msg);
                        errorSkipUntilSemicolon = true;
                        break;
                    }
                    argTypes.Add(argTypeExpr);

                    LinxcToken next = tokenizer->NextUntilValid();
                    if (next.ID == Linxc_RParen)
                    {
                        break;
                    }
                    else if (next.ID == Linxc_Comma)
                    {
                        continue;
                    }
                    else
                    {
                        state->parsingFile->errors.Add(ERR_MSG(this->allocator, "Expected a , or closing ) after delegate argument type declaration"));
                        errorSkipUntilSemicolon = true;
                        break;
                    }
                }
            }
            else if (peekNext.ID == Linxc_RParen)
            {
                tokenizer->NextUntilValid();
            }
            else
            {
                state->parsingFile->errors.Add(ERR_MSG(this->allocator, "Unexpected token after delegate return type declaration, must be either a comma or ) if delegate does not have any input variables"));
                errorSkipUntilSemicolon = true;
                break;
            }

            LinxcType delegateType = LinxcType(this->allocator, delegateTypeNameStr, state->currentNamespace->actualNamespace, state->parentType);

            LinxcExpression* returnTypeExprPtr = (LinxcExpression*)this->allocator->Allocate(sizeof(LinxcExpression));
            *returnTypeExprPtr = returnTypeExpr;

            delegateType.delegateDecl = LinxcFuncPtr(delegateTypeNameStr, returnTypeExprPtr);//LinxcFunc(delegateTypeNameStr, typeExpr);
            delegateType.delegateDecl.necessaryArguments = argTypes.count;
            delegateType.delegateDecl.arguments = argTypes.ToOwnedArrayWith(this->allocator);
            LinxcType* ptr;
            if (state->parentType != NULL)
            {
                state->parentType->subTypes.Add(delegateType);
                ptr = state->parentType->subTypes.Get(state->parentType->subTypes.count - 1);
            }
            else
            {
                ptr = state->currentNamespace->AddTypeToOrigin(delegateType.name, delegateType);
            }

            LinxcStatement stmt;
            stmt.data.typeDeclaration = ptr;
            stmt.ID = LinxcStmt_TypeDecl;
            result.Add(stmt);

            expectSemicolon = true;
        }
        break;
        case Linxc_keyword_uselang:
        {
            if (state->tokenizer->NextUntilValid().ID != Linxc_LParen)
            {
                state->parsingFile->errors.Add(ERR_MSG(this->allocator, "Expected ( after uselang declaration"));
                errorSkipUntilSemicolon = true;
            }
            LinxcToken next = state->tokenizer->NextUntilValid();
            if (next.ID != Linxc_StringLiteral)
            {
                state->parsingFile->errors.Add(ERR_MSG(this->allocator, "Expected string literal within uselang() specifying which language to use"));
            }
            string languageStr = string(this->allocator, state->tokenizer->buffer + next.start + 1, next.end - next.start - 2);
            LinxcUseLang useLang;
            useLang.languageUsed = languageStr;
            useLang.body = collections::vector<LinxcToken>(this->allocator);

            if (state->tokenizer->NextUntilValid().ID != Linxc_RParen)
            {
                state->parsingFile->errors.Add(ERR_MSG(this->allocator, "Expected )"));
            }

            while (true)
            {
                next = tokenizer->Next();
                if (next.ID == Linxc_Eof)
                {
                    errors->Add(ERR_MSG(this->allocator, "Expected enduselang declaration"));
                    break;
                }
                else if (next.ID == Linxc_keyword_enduselang)
                {
                    break;
                }
                else useLang.body.Add(next);
            }

            LinxcStatement stmt;
            stmt.ID = LinxcStmt_UseLang;
            stmt.data.useLang = useLang;
            result.Add(stmt);
        }
        break;
        case Linxc_LineComment:
        {
            isComment = true;
        }
        break;
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
        case Linxc_CharLiteral:
        case Linxc_StringLiteral:
        case Linxc_IntegerLiteral:
        case Linxc_FloatLiteral:
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
                        error.AppendDeinit(identifier.ToString(GetDefaultAllocator()));
                        errors->Add(error);
                        toBreak = true;
                        break;
                    }

                    LinxcToken next = tokenizer->NextUntilValid();
                    if (((next.ID == Linxc_Semicolon && (!state->commaIsSemicolon || state->endOn == LinxcEndOn_Semicolon)) || (next.ID == Linxc_Comma && state->commaIsSemicolon)) || next.ID == Linxc_Equal)
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
                                varDecl.memberOf = state->parentType;
                                state->parentType->variables.Add(varDecl);
                                ptr = state->parentType->variables.Get(state->parentType->variables.count - 1);
                            }
                            else //else add to namespace
                            {
                                ptr = state->currentNamespace->AddVariableToOrigin(varDecl.name, varDecl);//variables.Add(varDecl.name, varDecl);

                                //state->parsingFile->definedVars.Add(ptr);
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

                        if (next.ID == Linxc_Semicolon && state->endOn == LinxcEndOn_Semicolon)
                        {
                            toBreak = true;
                            break;
                        }
                    }
                    else if (next.ID == Linxc_LParen) //function declaration
                    {
                        u32 necessaryArgs = 0;
                        collections::Array<LinxcVar> args = this->ParseFunctionArgs(state, &necessaryArgs);
                        LinxcToken next = tokenizer->PeekNextUntilValid();
                        bool noBody = false;

                        if (next.ID != Linxc_LBrace)
                        {
                            if ((state->parsingLinxci || state->parsingFile->isLinxcH) && next.ID == Linxc_Semicolon)
                            {
                                noBody = true;
                                tokenizer->NextUntilValid();
                            }
                            else errors->Add(ERR_MSG(this->allocator, "Expected { after function name"));
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
                            newFunc.funcNamespace = state->currentNamespace->actualNamespace;
                        }

                        LinxcFunc* ptr = NULL;

                        if (state->parentType != NULL)
                        {
                            state->parentType->functions.Add(newFunc);
                            ptr = state->parentType->functions.Get(state->parentType->functions.count - 1);
                        }
                        else
                        {
                            ptr = state->currentNamespace->AddFunctionToOrigin(newFunc.name, newFunc);
                        }

                        if (!noBody)
                        {
                            LinxcParserState nextState = LinxcParserState(state->parser, state->parsingFile, state->tokenizer, LinxcEndOn_RBrace, false, state->parsingLinxci);
                            nextState.parentType = state->parentType;
                            nextState.endOn = LinxcEndOn_RBrace;
                            nextState.currentNamespace = state->currentNamespace;
                            nextState.currentFunction = ptr;
                            for (usize i = 0; i < args.length; i++)
                            {
                                nextState.varsInScope.Add(args.data[i].name, &args.data[i]);
                            }

                            LinxcVar* thisVar;

                            if (state->parentType != NULL)
                            {
                                //the 'this' keyword counts as a variable within a function's scope if it is within a struct

                                thisVar = (LinxcVar*)allocator->Allocate(sizeof(LinxcVar));
                                thisVar->name = this->thisKeyword;
                                thisVar->type = state->parentType->AsExpression();
                                thisVar->type.data.typeRef.pointerCount += 1;
                                thisVar->memberOf = NULL;
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
                                state->parsingFile->mustTranspileC = true;
                                ptr->body = funcBody.value;
                            }

                            nextState.deinit();
                        }
                        ptr->asType = LinxcType(this->allocator, ptr->name, ptr->funcNamespace, ptr->methodOf);
                        ptr->asType.delegateDecl = ptr->GetSignature(this->allocator);

                        state->parsingFile->definedFuncs.Add(ptr);

                        LinxcStatement stmt;
                        stmt.data.funcDeclaration = ptr;
                        stmt.ID = LinxcStmt_FuncDecl;

                        result.Add(stmt);
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
        case Linxc_Keyword_if:
        {
            if (isComment)
            {
                continue;
            }
            if (nextIsConst)
            {
                errors->Add(ERR_MSG(this->allocator, "Cannot declare an if statement const"));
                nextIsConst = false;
            }
            LinxcToken hopefullyParen = state->tokenizer->NextUntilValid();
            if (hopefullyParen.ID != Linxc_LParen)
            {
                errors->Add(ERR_MSG(this->allocator, "Expected opening ( after if statement"));
            }
            LinxcExpression ifCondition;
            ifCondition.resolvesTo.lastType = NULL;

            option<LinxcExpression> primaryOpt = this->ParseExpressionPrimary(state);
            if (primaryOpt.present)
            {
                ifCondition = this->ParseExpression(state, primaryOpt.value, -1);
                if (ifCondition.resolvesTo.lastType == NULL || ifCondition.resolvesTo.lastType->name != "bool")
                {
                    errors->Add(ERR_MSG(this->allocator, "Expression within the if statement should resolve to a boolean"));
                }

            }
            hopefullyParen = state->tokenizer->NextUntilValid();
            if (hopefullyParen.ID != Linxc_RParen)
            {
                errors->Add(ERR_MSG(this->allocator, "Expected closing ) after the expression in an if statement"));
            }

            if (state->tokenizer->PeekNextUntilValid().ID == Linxc_LBrace)
            {
                state->tokenizer->NextUntilValid();

                //need a new state as it is a new scope
                LinxcParserState nextState = LinxcParserState(state->parser, state->parsingFile, state->tokenizer, LinxcEndOn_RBrace, false, false);
                nextState.currentFunction = state->currentFunction;
                nextState.currentNamespace = state->currentNamespace;
                nextState.parentType = state->parentType;
                nextState.varsInScope = state->varsInScope.Clone(GetDefaultAllocator());

                option<collections::vector<LinxcStatement>> ifStatementResult = this->ParseCompoundStmt(&nextState);
                
                if (ifStatementResult.present && primaryOpt.present)
                {
                    LinxcIfStatement ifStatement;
                    ifStatement.result = ifStatementResult.value;
                    ifStatement.condition = ifCondition;

                    LinxcStatement stmt;
                    stmt.data.ifStatement = ifStatement;
                    stmt.ID = LinxcStmt_If;

                    result.Add(stmt);
                }
               
                nextState.deinit();

                if (state->tokenizer->PeekNextUntilValid().ID == Linxc_Keyword_else)
                {
                    state->tokenizer->NextUntilValid();
                    //parse else statement
                    
                    if (state->tokenizer->PeekNextUntilValid().ID == Linxc_LBrace)
                    {
                        state->tokenizer->NextUntilValid();

                        nextState = LinxcParserState(state->parser, state->parsingFile, state->tokenizer, LinxcEndOn_RBrace, false, false);
                        nextState.currentFunction = state->currentFunction;
                        nextState.parentType = state->parentType;
                        nextState.varsInScope = state->varsInScope.Clone(GetDefaultAllocator());

                        option<collections::vector<LinxcStatement>> elseStatementResult = this->ParseCompoundStmt(&nextState);
                    
                        if (elseStatementResult.present)
                        {
                            //elseStatementResult.value;
                            LinxcStatement stmt;
                            stmt.data.elseStatement = elseStatementResult.value;
                            stmt.ID = LinxcStmt_Else;

                            result.Add(stmt);
                        }

                        nextState.deinit();
                    }
                    else
                    {
                        nextState = LinxcParserState(state->parser, state->parsingFile, state->tokenizer, LinxcEndOn_SingleStatement, false, false);
                        nextState.currentFunction = state->currentFunction;
                        nextState.parentType = state->parentType;
                        nextState.currentNamespace = state->currentNamespace;
                        nextState.varsInScope = state->varsInScope.Clone(GetDefaultAllocator());

                        option<collections::vector<LinxcStatement>> elseStatementResult = this->ParseCompoundStmt(&nextState);

                        if (elseStatementResult.present)
                        {
                            if (elseStatementResult.value.count == 1)
                            {
                                LinxcStatement stmt;
                                stmt.data.elseStatement = elseStatementResult.value;
                                stmt.ID = LinxcStmt_Else;

                                result.Add(stmt);
                            }
                            else
                            {
                                errors->Add(ERR_MSG(this->allocator, "Expected something after else statement"));
                            }
                            //elseStatementResult.value;

                        }

                        nextState.deinit();
                    }
                }
            }
            else
            {
                LinxcParserState nextState = LinxcParserState(state->parser, state->parsingFile, state->tokenizer, LinxcEndOn_SingleStatement, false, false);
                nextState.currentFunction = state->currentFunction;
                nextState.parentType = state->parentType;
                nextState.currentNamespace = state->currentNamespace;
                nextState.varsInScope = state->varsInScope.Clone(GetDefaultAllocator());

                option<collections::vector<LinxcStatement>> ifStatementResult = ParseCompoundStmt(&nextState);

                if (ifStatementResult.present)
                {
                    //check
                    //note: this is not an error of the compiler as doing if (condition) ; will throw this error, it is 100% the user's fault
                    if (ifStatementResult.value.count == 1)
                    {
                        LinxcStatement stmt;
                        stmt.ID = LinxcStmt_If;
                        stmt.data.ifStatement.condition = ifCondition;
                        stmt.data.ifStatement.result = ifStatementResult.value;

                        result.Add(stmt);
                    }
                    else
                    {
                        errors->Add(ERR_MSG(this->allocator, "Expected something after if statement"));
                    }
                }

                nextState.deinit();
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
                LinxcTypeReference expectedReturnType = state->currentFunction->returnType.AsTypeReference().value;
                if (expectedReturnType.lastType != this->typeofVoid)
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
        case Linxc_Keyword_for:
        {
            LinxcToken next = tokenizer->NextUntilValid();
            if (next.ID != Linxc_LParen)
            {
                errors->Add(ERR_MSG(this->allocator, "Expected ( after for statement"));
            }
            //for (stmt; expression; stmt;)
            LinxcParserState nextState = LinxcParserState(this, state->parsingFile, tokenizer, LinxcEndOn_Semicolon, false, false);
            nextState.currentFunction = state->currentFunction;
            nextState.currentNamespace = state->currentNamespace;
            nextState.parentType = state->parentType;
            nextState.varsInScope = state->varsInScope.Clone(GetDefaultAllocator());
            nextState.commaIsSemicolon = true;

            option<collections::vector<LinxcStatement>> init = this->ParseCompoundStmt(&nextState);
            if (!init.present)
            {
                toBreak = true;
                break;
            }
            for (usize i = 0; i < init.value.count; i++)
            {
                if (init.value.Get(i)->ID != LinxcStmt_VarDecl && init.value.Get(i)->ID != LinxcStmt_Expr)
                {
                    errors->Add(ERR_MSG(this->allocator, "Only variable declarations or expressions are allowed in the initialization statement of a for loop"));
                    toBreak = true;
                    break;
                }
            }

            option<LinxcExpression> primaryExprOpt = this->ParseExpressionPrimary(&nextState);
            if (!primaryExprOpt.present)
            {
                toBreak = true;
                break;
            }
            LinxcExpression conditionExpr = this->ParseExpression(&nextState, primaryExprOpt.value, -1);
            if (conditionExpr.resolvesTo.lastType == NULL || conditionExpr.resolvesTo.lastType->name != "bool")
            {
                errors->Add(ERR_MSG(this->allocator, "For loop condition must result in a boolean"));
                toBreak = true;
                break;
            }

            nextState.endOn = LinxcEndOn_RParen;
            option<collections::vector<LinxcStatement>> step = this->ParseCompoundStmt(&nextState);
            if (!step.present)
            {
                toBreak = true;
                break;
            }
            for (usize i = 0; i < step.value.count; i++)
            {
                if (step.value.Get(i)->ID != LinxcStmt_Expr)
                {
                    errors->Add(ERR_MSG(this->allocator, "Only expressions are allowed in the step statement of a for loop"));
                    toBreak = true;
                    break;
                }
            }

            nextState.commaIsSemicolon = false;
            if (tokenizer->PeekNextUntilValid().ID == Linxc_LBrace)
            {
                tokenizer->NextUntilValid();
                nextState.endOn = LinxcEndOn_RBrace;
            }
            else
                nextState.endOn = LinxcEndOn_SingleStatement;

            option<collections::vector<LinxcStatement>> body = this->ParseCompoundStmt(&nextState);
            if (!body.present)
            {
                toBreak = true;
                break;
            }

            LinxcForLoopStatement forLoop;
            forLoop.initialization = init.value;
            forLoop.condition = conditionExpr;
            forLoop.step = step.value;
            forLoop.body = body.value;

            LinxcStatement stmt;
            stmt.ID = LinxcStmt_For;
            stmt.data.forLoop = forLoop;

            result.Add(stmt);

            nextState.deinit();
        }
        break;
        case Linxc_RParen: //EndOnRParen settled before the while loop, so if we encounter a random ) here, always throw an error (Unless it's a comment)
        {
            if (isComment)
            {
                continue;
            }
            else
            {
                errors->Add(ERR_MSG(this->allocator, "Unexpected )"));
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
        case Linxc_keyword_enduselang:
        {
            if (isComment)
            {
                continue;
            }
            errors->Add(ERR_MSG(this->allocator, "Unexpected enduselang declaration"));
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
            else if (state->endOn == LinxcEndOn_SingleStatement)
            {
                errors->Add(ERR_MSG(this->allocator, "Expected a statement"));
            }
            toBreak = true;
        }
        break;
        default:
            break;
        }
        if (toBreak || (result.count >= 1 && state->endOn == LinxcEndOn_SingleStatement))
        {
            break;
        }
    }
    return option<collections::vector<LinxcStatement>>(result);
}

bool LinxcParser::TranspileFile(LinxcParsedFile* parsedFile, const char* outputPathC, const char* outputPathH)
{
    bool transpiledC = false;
    FILE* fs;
    if (parsedFile->ast.count > 0)
    {
        fs = io::CreateDirectoriesAndFile(outputPathH);
        //transpile header
        if (fs != NULL)
        {
            fprintf(fs, "#pragma once\n#include <Linxc.h>\n#include <stdbool.h>\n");
            for (usize i = 0; i < parsedFile->ast.count; i++)
            {
                TranspileStatementH(fs, parsedFile->ast.Get(i));
            }
            fclose(fs);
        }
    }
    else if (io::FileExists(outputPathH))
    {
        remove(outputPathH);
    }
    //transpile source
    if (parsedFile->mustTranspileC)
    {
        fs = io::CreateDirectoriesAndFile(outputPathC);
        if (fs != NULL)
        {
            string swappedExtension = path::SwapExtension(GetDefaultAllocator(), parsedFile->includeName, ".h");
            fprintf(fs, "#include <%s>\n", swappedExtension.buffer);
            swappedExtension.deinit();

            this->TranspileCompoundStmtC(fs, parsedFile->ast, NULL, collections::Array<string>(), collections::Array<LinxcTypeReference>());

            fclose(fs);

            transpiledC = true;
        }
    }
    else if (io::FileExists(outputPathC))
    {
        remove(outputPathC);
    }

    return transpiledC;
}
void LinxcParser::TranspileStatementH(FILE* fs, LinxcStatement* stmt)
{
    if (stmt->ID == LinxcStmt_Include)
    {
        string includeName = stmt->data.includeStatement.includeString;
        string extension = path::GetExtension(GetDefaultAllocator(), includeName);
        if (extension == ".linxc")
        {
            includeName = path::SwapExtension(GetDefaultAllocator(), includeName, ".h");
            //we only set the above to true here as before, we are just referencing the includeName as part of the include
            //statement. If we deinited that, we would have modified the statement in the AST
        }

        //make sure no harpy brain programmer uses '\' to specify include paths 
        string replaced = ReplaceChar(GetDefaultAllocator(), includeName.buffer, '\\', '/');
        fprintf(fs, "#include <%s>\n", includeName.buffer);
        replaced.deinit();

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
        if (stmt->data.typeDeclaration->templateArgs.length > 0 )
        {
            //collections::Array<string> typeTemplateArgs = templateArgs.CloneAdd(GetDefaultAllocator(), stmt->data.typeDeclaration->templateArgs);

            collections::hashmap<TemplateSpecialization, LinxcType>* specializations = &stmt->data.typeDeclaration->templateSpecializations;
            for (usize i = 0; i < specializations->bucketsCount; i++)
            {
                if (specializations->buckets[i].initialized)
                {
                    for (usize j = 0; j < specializations->buckets[i].entries.count; j++)
                    {
                        //collections::Array<LinxcTypeReference> typeTemplateSpecializations = templateSpecializations.CloneAdd(GetDefaultAllocator(), specializations->buckets[i].entries.ptr[j].key);
                        TranspileTypeH(fs, &specializations->buckets[i].entries.ptr[j].value);// , typeTemplateArgs, typeTemplateSpecializations);
                        //typeTemplateSpecializations.deinit();
                    }
                }
            }

            //typeTemplateArgs.deinit();
        }
        else TranspileTypeH(fs, stmt->data.typeDeclaration);
    }
    else if (stmt->ID == LinxcStmt_VarDecl)
    {
        i32 tempIndex = 0;
        //dont need to pass templateArgs and templateSpecializations as it should have been specialized for us already
        this->TranspileVar(fs, stmt->data.varDeclaration, &tempIndex, collections::Array<string>(), TemplateSpecialization());
    }
    else if (stmt->ID == LinxcStmt_FuncDecl)
    {
        this->TranspileFuncH(fs, stmt->data.funcDeclaration);
        fprintf(fs, ";\n");
    }
}
void LinxcParser::TranspileTypeH(FILE* fs, LinxcType* type)//, collections::Array<string> templateArgs, collections::Array<LinxcTypeReference> templateSpecialization)
{
    if (type->enumMembers.count > 0)
    {
        fprintf(fs, "typedef enum {\n");
        for (usize i = 0; i < type->enumMembers.count; i++)
        {
            fprintf(fs, "%s = %i", type->enumMembers.ptr[i].name.buffer, type->enumMembers.ptr[i].value);
            if (i < type->enumMembers.count - 1)
            {
                fprintf(fs, ",\n");
            }
        }
        string typeName = type->GetCName(GetDefaultAllocator());
        fprintf(fs, "\n} %s;\n", typeName.buffer);
        typeName.deinit();
    }
    else if (type->delegateDecl.name.buffer != NULL) //is delegate
    {
        //typedef returns (*name)(__VA_ARGS__)
        string returnTypeName = type->delegateDecl.returnType->AsTypeReference().value.GetCName(GetDefaultAllocator(), false, collections::Array<string>(), TemplateSpecialization());
        string delegateName = type->GetCName(GetDefaultAllocator());

        fprintf(fs, "typedef %s (*%s)(", returnTypeName.buffer, delegateName.buffer);
        for (usize i = 0; i < type->delegateDecl.arguments.length; i++)
        {
            LinxcTypeReference delegateArgType = type->delegateDecl.arguments.data[i].AsTypeReference().value;
            string argumentTypeName = delegateArgType.GetCName(GetDefaultAllocator(), false, collections::Array<string>(), TemplateSpecialization());
            
            fprintf(fs, "%s", argumentTypeName.buffer);
            argumentTypeName.deinit();
            if (i < type->delegateDecl.arguments.length - 1)
            {
                fprintf(fs, ", ");
            }
        }
        fprintf(fs, ");\n");

        returnTypeName.deinit();
        delegateName.deinit();
    }
    else
    {
        //transpile subtypes first
        for (usize i = 0; i < type->subTypes.count; i++)
        {
            LinxcType* subType = type->subTypes.Get(i);
            
            if (subType->templateSpecializations.Count > 0)
            {
                //collections::Array<string> newTemplateArgs = templateArgs.CloneAdd(GetDefaultAllocator(), subType->templateArgs);

                for (usize x = 0; x < subType->templateSpecializations.bucketsCount; x++)
                {
                    if (subType->templateSpecializations.buckets[x].initialized)
                    {
                        for (usize y = 0; y < subType->templateSpecializations.buckets[x].entries.count; y++)
                        {
                            //collections::Array<LinxcTypeReference> newTemplateSpecializations = templateSpecialization.CloneAdd(GetDefaultAllocator(), subType->templateSpecializations.buckets[x].entries.ptr[y].key);

                            TranspileTypeH(fs, subType);// , newTemplateArgs, newTemplateSpecializations);
                            //newTemplateSpecializations.deinit();
                        }
                    }
                }
                //newTemplateArgs.deinit();
            }
            else TranspileTypeH(fs, subType);// , templateArgs, templateSpecialization);
        }

        fprintf(fs, "typedef struct {\n");
        for (usize i = 0; i < type->variables.count; i++)
        {
            fprintf(fs, "   ");
            i32 tempIndex = 0;
            this->TranspileVar(fs, type->variables.Get(i), &tempIndex, collections::Array<string>(), TemplateSpecialization());//templateArgs, templateSpecialization);
            fprintf(fs, ";\n");
        }
        string typeName = type->GetCName(GetDefaultAllocator());// , templateArgs, templateSpecialization);
        fprintf(fs, "} %s;\n", typeName.buffer);
        typeName.deinit();

        for (usize i = 0; i < type->functions.count; i++)
        {
            //TODO: Template function shenanigans
            if (type->functions.Get(i)->templateArgs.length == 0)
            {
                if (type->isSpecializedType.length > 0)
                {
                    printf("transpiling func off type specialized with %s\n", type->isSpecializedType.data[0].lastType->name.buffer);
                }
                this->TranspileFuncH(fs, type->functions.Get(i));// , templateArgs, templateSpecialization);
                fprintf(fs, ";\n");
            }
        }
    }
}
void LinxcParser::TranspileFuncHeader(FILE *fs, LinxcFunc* func)//, collections::Array<string> templateArgs, collections::Array<LinxcTypeReference> templateSpecializations)
{
    LinxcTypeReference typeRef = func->returnType.AsTypeReference().value;

    string returnTypeName = typeRef.GetCName(GetDefaultAllocator());
    fprintf(fs, "%s ", returnTypeName.buffer);
    returnTypeName.deinit();

    string funcName = func->GetCName(GetDefaultAllocator());// , templateArgs, templateSpecializations);

    fprintf(fs, "%s(", funcName.buffer);
    funcName.deinit();

    //if we are a member function of a struct, the first argument will always be 'this'
    if (func->methodOf != NULL)
    {
        string thisTypeName = func->methodOf->GetCName(GetDefaultAllocator());// , templateArgs, templateSpecializations);

        fprintf(fs, "%s *this", thisTypeName.buffer);
        if (func->arguments.length > 0)
        {
            fprintf(fs, ", ");
        }
    }
    for (usize i = 0; i < func->arguments.length; i++)
    {
        i32 typeIndex = 0;
        if (func->arguments.data[i].name == "...")
        {
            fprintf(fs, "...");
        }
        else this->TranspileVar(fs, &func->arguments.data[i], &typeIndex, collections::Array<string>(), TemplateSpecialization());// , templateArgs, templateSpecializations);
        if (i < func->arguments.length - 1)
        {
            fprintf(fs, ", ");
        }
    }

    fprintf(fs, ")");
}
void LinxcParser::TranspileExpr(FILE* fs, LinxcExpression* expr, bool writePriority, collections::Array<string> templateArgs, collections::Array<LinxcTypeReference> templateSpecializations)
{
    switch (expr->ID)
    {
    case LinxcExpr_None:
        break;
    case LinxcExpr_TypeRef:
    {
        string typeString = expr->data.typeRef.GetCName(GetDefaultAllocator(), false, templateArgs, templateSpecializations);
        fprintf(fs, "%s", typeString.buffer);
        typeString.deinit();
    }
    break;
    case LinxcExpr_Literal:
    {
        fprintf(fs, "%s", expr->data.literal.buffer);
    }
    break;
    case LinxcExpr_EnumMemberRef:
    {
        fprintf(fs, "%s", expr->data.enumMemberRef->name.buffer);
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
        this->TranspileExpr(fs, &expr->data.modifiedExpression->expression, false, templateArgs, templateSpecializations);
    }
    break;
    case LinxcExpr_TypeCast:
    {
        string typeString = expr->data.typeCast->castToType.AsTypeReference().value.GetCName(GetDefaultAllocator());
        fprintf(fs, "(%s)", typeString.buffer);
        typeString.deinit();
        this->TranspileExpr(fs, &expr->data.typeCast->expressionToCast, false, templateArgs, templateSpecializations);
    }
    break;
    case LinxcExpr_IndexerCall:
    {
        if (expr->data.indexerCall.indexerOperator == NULL)
        {
            fprintf(fs, "%s[", expr->data.indexerCall.variableToIndex->name.buffer);
            this->TranspileExpr(fs, expr->data.indexerCall.inputParams, false, templateArgs, templateSpecializations);
            fprintf(fs, "]");
        }
        
    }
    break;
    case LinxcExpr_FuncPtrCall:
    {
        fprintf(fs, "%s(", expr->data.functionPointerCall.variable->name.buffer);
        for (usize i = 0; i < expr->data.functionPointerCall.inputParams.length; i++)
        {
            this->TranspileExpr(fs, &expr->data.functionPointerCall.inputParams.data[i], false, templateArgs, templateSpecializations);
            if (i < expr->data.functionPointerCall.inputParams.length - 1)
            {
                fprintf(fs, ", ");
            }
        }
        fprintf(fs, ")");
    }
    break;
    case LinxcExpr_FuncCall:
    {
        string name = expr->data.functionCall.func->GetCName(GetDefaultAllocator());// , templateArgs, templateSpecializations);
        fprintf(fs, "%s(", name.buffer);
        name.deinit();
        if (expr->data.functionCall.thisAsParam != NULL)
        {
            this->TranspileExpr(fs, expr->data.functionCall.thisAsParam, false, templateArgs, templateSpecializations);
            if (expr->data.functionCall.inputParams.length > 0)
            {
                fprintf(fs, ", ");
            }
        }
        for (usize i = 0; i < expr->data.functionCall.inputParams.length; i++)
        {
            this->TranspileExpr(fs, &expr->data.functionCall.inputParams.data[i], false, templateArgs, templateSpecializations);
            if (i < expr->data.functionCall.inputParams.length - 1)
            {
                fprintf(fs, ", ");
            }
        }
        fprintf(fs, ")");
    }
    break;
    case LinxcExpr_FunctionRef:
    {
        string name = expr->data.functionRef->GetCName(GetDefaultAllocator());// , templateArgs, templateSpecializations);
        fprintf(fs, "%s", name.buffer);
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
            //special case is given if the left is a reference type with template specializations.
            //we will simply append the template specializations to the scope of the right and transpile,
            //so as to allow it to transpile all variables correctly when transpiling it's parents
            //(AKA: The original left expr)
            if (expr->data.operatorCall->leftExpr.ID == LinxcExpr_TypeRef && expr->data.operatorCall->leftExpr.data.typeRef.lastType != NULL)
            {
                collections::Array<LinxcExpression>* leftExprSpecializations = &expr->data.operatorCall->leftExpr.data.typeRef.templateArgs;
                collections::Array<string> newTemplateArgs = templateArgs.CloneAdd(GetDefaultAllocator(), expr->data.operatorCall->leftExpr.data.typeRef.lastType->templateArgs);
                collections::Array<LinxcTypeReference> newTemplateSpecializations = collections::Array<LinxcTypeReference>(GetDefaultAllocator(), templateSpecializations.length + leftExprSpecializations->length);
                for (usize i = 0; i < newTemplateSpecializations.length; i++)
                {
                    if (i < templateSpecializations.length)
                    {
                        newTemplateSpecializations.data[i] = templateSpecializations.data[i];
                    }
                    else
                    {
                        newTemplateSpecializations.data[i] = leftExprSpecializations->data[i - templateSpecializations.length].AsTypeReference().value;
                    }
                }

                this->TranspileExpr(fs, &expr->data.operatorCall->rightExpr, false, newTemplateArgs, newTemplateSpecializations);

                newTemplateArgs.deinit();
                newTemplateSpecializations.deinit();
            }
            else this->TranspileExpr(fs, &expr->data.operatorCall->rightExpr, false, templateArgs, templateSpecializations);
        }
        else
        {
            //todo: Convert custom operators to functions
            LinxcTokenID opType = expr->data.operatorCall->operatorType;

            bool writePriorityForNext = true;
                switch (opType)
                {
                case Linxc_ColonColon:
                case Linxc_Arrow:
                case Linxc_Period:
                case Linxc_Equal:
                    // case Linxc_EqualEqual:
                    // case Linxc_AngleBracketRight:
                    // case Linxc_AngleBracketLeft:
                    // case Linxc_AngleBracketRightEqual:
                    // case Linxc_AngleBracketLeftEqual:
                    writePriorityForNext = false;
                    break;
                default:
                    break;
                }

            if (writePriority || expr->priority)
            {
                fprintf(fs, "(");
            }
            this->TranspileExpr(fs, &expr->data.operatorCall->leftExpr, writePriorityForNext, templateArgs, templateSpecializations);
            //if not ::, ->, ., yes space
            if (opType != Linxc_ColonColon && opType != Linxc_Arrow && opType != Linxc_Period)
            {
                fprintf(fs, " %s ", LinxcTokenIDToString(expr->data.operatorCall->operatorType));
            }//no space
            else fprintf(fs, "%s", LinxcTokenIDToString(expr->data.operatorCall->operatorType));
            
            this->TranspileExpr(fs, &expr->data.operatorCall->rightExpr, writePriorityForNext, templateArgs, templateSpecializations);
            if (writePriority || expr->priority)
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
void LinxcParser::TranspileVar(FILE* fs, LinxcVar* var, i32* tempIndex, collections::Array<string> templateArgs, collections::Array<LinxcTypeReference> templateSpecialization)
{
    if (var->memberOf == NULL && var->defaultValue.present)
    {
        LinxcExpression* expr = &var->defaultValue.value;
        this->RotateFuncCallExpression(expr, &expr, NULL, NULL, templateArgs, templateSpecialization);
        this->SegregateFuncCallExpression(fs, expr, tempIndex, templateArgs, templateSpecialization);
    }
    if (var->isConst)
    {
        fprintf(fs, "const ");
    }
    this->TranspileExpr(fs, &var->type, false, templateArgs, templateSpecialization);
    fprintf(fs, " %s", var->name.buffer);
    if (var->defaultValue.present)
    {
        fprintf(fs, " = "); //temp
        this->TranspileExpr(fs, &var->defaultValue.value, false, templateArgs, templateSpecialization);
    }
    //else fprintf(fs, ";\n");
}
void LinxcParser::RotateFuncCallExpression(LinxcExpression* expr, LinxcExpression** exprRootMutable, LinxcExpression* parent, LinxcExpression* grandParent, collections::Array<string> templateArgs, collections::Array<LinxcTypeReference> templateSpecializations)
{
    LinxcExpression* exprRoot = *exprRootMutable;
    if (expr->ID == LinxcExpr_FuncCall && expr->data.functionCall.func->methodOf != NULL)
    {
        if (parent != NULL && parent->ID == LinxcExpr_OperatorCall && (parent->data.operatorCall->operatorType == Linxc_Period || parent->data.operatorCall->operatorType == Linxc_Arrow))
        {
            if (grandParent != NULL && grandParent->ID == LinxcExpr_OperatorCall && (grandParent->data.operatorCall->operatorType == Linxc_Period || grandParent->data.operatorCall->operatorType == Linxc_Arrow))
            {
                if (&parent->data.operatorCall->leftExpr == expr)
                {
                    //if grandParent is exprRoot
                    if (grandParent == exprRoot)
                    {
                        //parent becomes root
                        //(left, due to precedence) sibling of parent becomes our first input
                        //because sibling of parent would have lived in the exprRoot, which is now replaced by parent, we need to reallocate it
                        LinxcExpression* siblingOfParentHeap = (LinxcExpression*)this->allocator->Allocate(sizeof(LinxcExpression));
                        *siblingOfParentHeap = grandParent->data.operatorCall->leftExpr;
                        *exprRoot = *parent;
                        expr->data.functionCall.thisAsParam = siblingOfParentHeap;
                    }
                    else
                    {
                        LinxcExpression* originalRootHeap = (LinxcExpression*)this->allocator->Allocate(sizeof(LinxcExpression));
                        *originalRootHeap = *exprRoot;
                        *grandParent = grandParent->data.operatorCall->leftExpr;
                        *exprRoot = *parent;
                        //since we set exprRoot to parent, we must change the address of exprRoot to become parent
                        //if we want to check parent = exprRoot in the future
                        expr->data.functionCall.thisAsParam = originalRootHeap;
                    }
                    *exprRootMutable = parent;

                    //dont reference parent anymore as it now lives in exprRoot, can dispose parent actually
                    //this->RotateFuncCallExpression(&expr->data.operatorCall->rightExpr, exprRoot, expr, parent);
                }
                else //we are right
                {
                    if (parent == exprRoot)
                    {
                        //parent gets disposed of as we are now the root
                        LinxcExpression* newInput = (LinxcExpression*)this->allocator->Allocate(sizeof(LinxcExpression));
                        *newInput = parent->data.operatorCall->leftExpr;
                        //whatever was to the left of our parent becomes our input
                        //we need to heap allocate the thing on the left now
                        //since our parent is getting nuked, there would be no place for it to live on otherwise

                        expr->data.functionCall.thisAsParam = newInput;
                        *exprRoot = *expr;
                    }
                    else
                    {
                        //expr sibling becomes parent
                        //root becomes param
                        //param resolves to becomes expr sibling
                        //we become root
                        LinxcExpression* newInput = (LinxcExpression*)this->allocator->Allocate(sizeof(LinxcExpression));
                        exprRoot->resolvesTo = parent->data.operatorCall->leftExpr.resolvesTo;
                        *newInput = *exprRoot;
                        grandParent->data.operatorCall->rightExpr = parent->data.operatorCall->leftExpr;
                        grandParent->resolvesTo = parent->data.operatorCall->leftExpr.resolvesTo;
                        expr->data.functionCall.thisAsParam = newInput;
                        *exprRoot = *expr;
                    }
                    //nothing to rotate as we are at end of chain due to precedence parsing
                }
            }
            else
            {
                //possible in the case of A.function()

                if (&parent->data.operatorCall->rightExpr == expr)
                {
                    LinxcExpression* newInput = (LinxcExpression*)this->allocator->Allocate(sizeof(LinxcExpression));
                    *newInput = parent->data.operatorCall->leftExpr;
                    expr->data.functionCall.thisAsParam = newInput;

                    *parent = *expr;
                    //expr->data.functionCall.thisAsParam = parent->data.operatorCall->leftExpr;
                }
                //else do nothing as function().A is correct
            }
        }
    }
    else if (expr->ID == LinxcExpr_OperatorCall)
    {
        this->RotateFuncCallExpression(&expr->data.operatorCall->leftExpr, exprRootMutable, expr, parent, templateArgs, templateSpecializations);
        this->RotateFuncCallExpression(&expr->data.operatorCall->rightExpr, exprRootMutable, expr, parent, templateArgs, templateSpecializations);
    }
    //else leave the expression alone
}
void LinxcParser::SegregateFuncCallExpression(FILE* fs, LinxcExpression* rotatedExpr, i32* tempIndex, collections::Array<string> templateArgs, collections::Array<LinxcTypeReference> templateSpecializations)
{
    if (rotatedExpr->ID == LinxcExpr_FuncCall)
    {
        //At this stage, thisAsParam can either be a variable of the type that the function (was)
        //a member of, a pointer variable of said type, or an expression.
        //if it resolves to a pointer, (which is in the case of functionA()->functionB()), we don't
        //need to do anything as we can pass the thing directly in since its a pointer already
        //if its a regular variable (as in functionA().functionB()), we need to transpile
        //functionA to it's own variable first, then pass that as a pointer into functionB().
        //if it's an expression (X + Y).functionB(), we also need to transpile it to it's own variable
        if (rotatedExpr->data.functionCall.thisAsParam != NULL)
        {
            LinxcTypeReference thisType = rotatedExpr->data.functionCall.thisAsParam->resolvesTo;
            //cannot be more than 1
            //if functionA() returns A**, we cannot do functionA()->functionB() as pointer types have no methods
            if (thisType.pointerCount == 0)
            {
                if (rotatedExpr->data.functionCall.thisAsParam->ID == LinxcExpr_Variable)
                {
                    LinxcExpression newTypeExpr;
                    newTypeExpr.priority = false;
                    newTypeExpr.resolvesTo.lastType = NULL;
                    newTypeExpr.data.typeRef = rotatedExpr->data.functionCall.thisAsParam->data.variable->type.AsTypeReference().value;
                    newTypeExpr.ID = LinxcExpr_TypeRef;

                    LinxcExpression referencedTypeExpr;
                    referencedTypeExpr.ID = LinxcExpr_Modified;
                    referencedTypeExpr.priority = false;
                    referencedTypeExpr.resolvesTo = rotatedExpr->data.functionCall.thisAsParam->data.variable->type.AsTypeReference().value;
                    referencedTypeExpr.resolvesTo.pointerCount += 1;
                    
                    LinxcModifiedExpression* modifiedExpr = (LinxcModifiedExpression*)this->allocator->Allocate(sizeof(LinxcModifiedExpression));
                    modifiedExpr->modification = Linxc_Ampersand;
                    modifiedExpr->expression = *rotatedExpr->data.functionCall.thisAsParam;//the original expr
                    
                    referencedTypeExpr.data.modifiedExpression = modifiedExpr;

                    *rotatedExpr->data.functionCall.thisAsParam = referencedTypeExpr;
                }
                else
                {
                    SegregateFuncCallExpression(fs, rotatedExpr->data.functionCall.thisAsParam, tempIndex, templateArgs, templateSpecializations);

                    string fullName = rotatedExpr->data.functionCall.thisAsParam->resolvesTo.GetCName(GetDefaultAllocator());
                    fprintf(fs, "%s ", fullName.buffer);
                    fullName.deinit();

                    fprintf(fs, "_temp%i = ", *tempIndex);

                    this->TranspileExpr(fs, rotatedExpr->data.functionCall.thisAsParam, false, templateArgs, templateSpecializations);

                    fprintf(fs, ";\n");

                    //finally, set our thisAsParam to a reference to the new variable
                    LinxcVar newVar;
                    LinxcExpression newVarType;
                    newVarType.data.typeRef = thisType;
                    newVarType.data.typeRef.pointerCount += 1;
                    newVarType.priority = false;
                    newVarType.resolvesTo.lastType = NULL;
                    newVarType.ID = LinxcExpr_TypeRef;
                    newVar.type = newVarType;
                    newVar.name = string(this->allocator, "_temp");
                    newVar.name.Append((i64)*tempIndex);
                    newVar.isConst = false;
                    newVar.memberOf = NULL;

                    LinxcVar* varPtr = (LinxcVar*)this->allocator->Allocate(sizeof(LinxcVar));
                    *varPtr = newVar;

                    LinxcExpression newThisAsParam;
                    newThisAsParam.ID = LinxcExpr_Variable;
                    newThisAsParam.resolvesTo = newVarType.data.typeRef;
                    newThisAsParam.priority = false;
                    newThisAsParam.data.variable = varPtr;

                    LinxcModifiedExpression* modifiedExpr = (LinxcModifiedExpression*)this->allocator->Allocate(sizeof(LinxcModifiedExpression));
                    modifiedExpr->modification = Linxc_Ampersand;
                    modifiedExpr->expression = newThisAsParam;//the original expr

                    LinxcExpression referencedTypeExpr;
                    referencedTypeExpr.ID = LinxcExpr_Modified;
                    referencedTypeExpr.priority = false;
                    referencedTypeExpr.resolvesTo = thisType;
                    referencedTypeExpr.resolvesTo.pointerCount += 1;
                    referencedTypeExpr.data.modifiedExpression = modifiedExpr;

                    *rotatedExpr->data.functionCall.thisAsParam = referencedTypeExpr;

                    *tempIndex += 1;
                }
            }
                //if exactly 1, don't need to do anything, pass it directly
            
        }
    }
    else if (rotatedExpr->ID == LinxcExpr_OperatorCall)
    {
        this->SegregateFuncCallExpression(fs, &rotatedExpr->data.operatorCall->leftExpr, tempIndex, templateArgs, templateSpecializations);
        this->SegregateFuncCallExpression(fs, &rotatedExpr->data.operatorCall->rightExpr, tempIndex, templateArgs, templateSpecializations);
    }
}
void LinxcParser::TranspileCompoundStmtC(FILE* fs, collections::vector<LinxcStatement> stmts, i32* tempIndex, collections::Array<string> templateArgs, collections::Array<LinxcTypeReference> templateSpecializations)
{
    for (usize i = 0; i < stmts.count; i++)
    {
        LinxcStatement* resultStmt = stmts.Get(i);
        this->TranspileStatementC(fs, resultStmt, tempIndex, templateArgs, templateSpecializations);
        if (resultStmt->ID != LinxcStmt_Namespace)
        {
            if (resultStmt->ID != LinxcStmt_Include && resultStmt->ID != LinxcStmt_For && resultStmt->ID != LinxcStmt_If && resultStmt->ID != LinxcStmt_Else && resultStmt->ID != LinxcStmt_TypeDecl && resultStmt->ID != LinxcStmt_FuncDecl)
            {
                fprintf(fs, ";\n");
            }
            else
            {
                fprintf(fs, "\n");
            }
        }
    }
}
void LinxcParser::TranspileTypeC(FILE* fs, LinxcType* type, collections::Array<string> templateArgs, collections::Array<LinxcTypeReference> templateSpecializations)
{
    if (type->templateArgs.length > 0)
    {
        collections::Array<string> typeTemplateArgs = templateArgs.CloneAdd(GetDefaultAllocator(), type->templateArgs);
        for (usize i = 0; i < type->templateSpecializations.bucketsCount; i++)
        {
            if (type->templateSpecializations.buckets[i].initialized)
            {
                for (usize j = 0; j < type->templateSpecializations.buckets[i].entries.count; j++)
                {
                    collections::Array<LinxcTypeReference> typeTemplateSpecializations = templateSpecializations.CloneAdd(GetDefaultAllocator(), type->templateSpecializations.buckets[i].entries.ptr[j].key);

                    for (usize c = 0; c < type->subTypes.count; c++)
                    {
                        TranspileTypeC(fs, type->templateSpecializations.buckets[i].entries.ptr[j].value.subTypes.Get(c), typeTemplateArgs, typeTemplateSpecializations);
                    }
                    for (usize c = 0; c < type->functions.count; c++)
                    {
                        TranspileFuncC(fs, type->templateSpecializations.buckets[i].entries.ptr[j].value.functions.Get(c), typeTemplateArgs, typeTemplateSpecializations);
                    }

                    typeTemplateSpecializations.deinit();
                }
            }
        }
        typeTemplateArgs.deinit();
    }
    else
    {
        for (usize c = 0; c < type->subTypes.count; c++)
        {
            TranspileTypeC(fs, type->subTypes.Get(c), templateArgs, templateSpecializations);
        }
        for (usize c = 0; c < type->functions.count; c++)
        {
            TranspileFuncC(fs, type->functions.Get(c), templateArgs, templateSpecializations);
        }
    }
}
void LinxcParser::TranspileStatementC(FILE* fs, LinxcStatement* stmt, i32* tempIndex, collections::Array<string> templateArgs, collections::Array<LinxcTypeReference> templateSpecializations)
{
    if (stmt->ID == LinxcStmt_TypeDecl)
    {
        LinxcType* type = stmt->data.typeDeclaration;
        TranspileTypeC(fs, type, templateArgs, templateSpecializations);
    }
    else if (stmt->ID == LinxcStmt_FuncDecl)
    {
        this->TranspileFuncC(fs, stmt->data.funcDeclaration, templateArgs, templateSpecializations);
    }
    else if (stmt->ID == LinxcStmt_Namespace)
    {
        TranspileCompoundStmtC(fs, stmt->data.namespaceScope.body, NULL, collections::Array<string>(), collections::Array<LinxcTypeReference>());
    }
    else if (stmt->ID == LinxcStmt_Expr)
    {
        LinxcExpression *expr = &stmt->data.expression;
        this->RotateFuncCallExpression(expr, &expr, NULL, NULL, templateArgs, templateSpecializations);
        this->SegregateFuncCallExpression(fs, expr, tempIndex, templateArgs, templateSpecializations);
        
        this->TranspileExpr(fs, expr, false, templateArgs, templateSpecializations);
    }
    else if (stmt->ID == LinxcStmt_For)
    {
        LinxcForLoopStatement* forLoop = &stmt->data.forLoop;
        LinxcExpression* conditionExpr = &forLoop->condition;

        //we cannot actually transpile for loops back into for loops.
        //this is because if someone did for (structureA A; A.index < 5; A.Method().MethodB()),
        //we would not be able to segregate the final statement.
        //Instead, we transpile it as a while loop within a scope (So that the initialized variable doesn't clash with anything else

        fprintf(fs, "{\n");
        this->TranspileCompoundStmtC(fs, forLoop->initialization, tempIndex, templateArgs, templateSpecializations);

        fprintf(fs, "while (true) {\n");

        this->RotateFuncCallExpression(conditionExpr, &conditionExpr, NULL, NULL, templateArgs, templateSpecializations);
        this->SegregateFuncCallExpression(fs, conditionExpr, tempIndex, templateArgs, templateSpecializations);

        i32 conditionIndex = *tempIndex;
        fprintf(fs, "bool _condition%i = ", conditionIndex);
        *tempIndex += 1;

        this->TranspileExpr(fs, conditionExpr, false, templateArgs, templateSpecializations);
        fprintf(fs, ";\n");

        fprintf(fs, "if (!_condition%i) break;\n", conditionIndex);

        this->TranspileCompoundStmtC(fs, forLoop->body, tempIndex, templateArgs, templateSpecializations);

        this->TranspileCompoundStmtC(fs, forLoop->step, tempIndex, templateArgs, templateSpecializations);

        fprintf(fs, "}\n");
        fprintf(fs, "}");
    }
    else if (stmt->ID == LinxcStmt_Return)
    {
        LinxcExpression* expr = &stmt->data.returnStatement;
        this->RotateFuncCallExpression(expr, &expr, NULL, NULL, templateArgs, templateSpecializations);
        this->SegregateFuncCallExpression(fs, expr, tempIndex, templateArgs, templateSpecializations);
        fprintf(fs, "return ");
        this->TranspileExpr(fs, expr, false, templateArgs, templateSpecializations);
    }
    else if (stmt->ID == LinxcStmt_VarDecl)
    {
        this->TranspileVar(fs, stmt->data.varDeclaration, tempIndex, templateArgs, templateSpecializations);
    }
    else if (stmt->ID == LinxcStmt_If)
    {
        LinxcExpression* expr = &stmt->data.ifStatement.condition;
        this->RotateFuncCallExpression(expr, &expr, NULL, NULL, templateArgs, templateSpecializations);
        this->SegregateFuncCallExpression(fs, expr, tempIndex, templateArgs, templateSpecializations);
        fprintf(fs, "if (");
        this->TranspileExpr(fs, expr, false, templateArgs, templateSpecializations);
        fprintf(fs, ")\n{\n");
        this->TranspileCompoundStmtC(fs, stmt->data.ifStatement.result, tempIndex, templateArgs, templateSpecializations); //use same tempIndex
        fprintf(fs, "}");
    }
    else if (stmt->ID == LinxcStmt_Else)
    {
        fprintf(fs, "else");
        if (stmt->data.elseStatement.count > 1)
        {
            fprintf(fs, "\n{\n");
            this->TranspileCompoundStmtC(fs, stmt->data.elseStatement, tempIndex, templateArgs, templateSpecializations);
            fprintf(fs, "}");
        }
        else
        {
            fprintf(fs, "\n");
            this->TranspileStatementC(fs, stmt->data.elseStatement.Get(0), tempIndex, templateArgs, templateSpecializations);
            //this->TranspileCompoundStmtC(fs, stmt->data.elseStatement);
        }
    }
}

void LinxcParser::TranspileFuncH(FILE* fs, LinxcFunc* func)//, collections::Array<string> templateArgs, collections::Array<LinxcTypeReference> templateSpecializations)
{
    if (func->templateArgs.length > 0)
    {
        //TODO:
    }
    else
    {
        this->TranspileFuncHeader(fs, func);// , templateArgs, templateSpecializations);
    }
}
void LinxcParser::TranspileFuncC(FILE* fs, LinxcFunc* func, collections::Array<string> templateArgs, collections::Array<LinxcTypeReference> templateSpecializations)
{
    if (func->templateArgs.length > 0)
    {
        //TODO:
    }
    else
    {
        this->TranspileFuncHeader(fs, func);// , templateArgs, templateSpecializations);
        fprintf(fs, "\n{\n");

        i32 tempIndex = 0;

        this->TranspileCompoundStmtC(fs, func->body, &tempIndex, templateArgs, templateSpecializations);
        fprintf(fs, "}");
    }
}