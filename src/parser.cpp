#include <parser.hpp>
#include <stdio.h>

LinxcParseIdentifierResultData::LinxcParseIdentifierResultData()
{
    this->functionReference = NULL;
}
LinxcParseIdentifierResult::LinxcParseIdentifierResult()
{
    this->ID = LinxcParseIdentifierResult_None;
    this->value.namespaceReference = NULL;
}
LinxcParserState::LinxcParserState(LinxcParser *myParser, LinxcParsedFile *currentFile, LinxcTokenizer *myTokenizer, LinxcEndOn endsOn, bool isTopLevel)
{
    this->tokenizer = myTokenizer;
    this->parser = myParser;
    this->parsingFile = currentFile;
    this->endOn = endsOn;
    this->isToplevel = isTopLevel;
    this->currentNamespace = &myParser->globalNamespace;
    this->parentType = NULL;
    this->varsInScope = collections::hashmap<string, LinxcVar *>(this->parser->allocator, &stringHash, &stringEql);
}
LinxcParser::LinxcParser(IAllocator *allocator)
{
    this->allocator = allocator;
    this->globalNamespace = LinxcNamespace(allocator, string());

    const char *primitiveTypes[13] = {"u8", "u16", "u32", "u64", "i8", "i16", "i32", "i64", "float", "double", "bool", "char", "void"};
    for (i32 i = 0; i < 13; i++)
    {
        string str = string(allocator, primitiveTypes[i]);
        this->globalNamespace.types.Add(str, LinxcType(allocator, str, &this->globalNamespace, NULL));
        this->fullNameToType.Add(str, this->globalNamespace.types.Get(str));
    }

    this->fullNameToType = collections::hashmap<string, LinxcType *>(allocator, &stringHash, &stringEql);
    this->parsedFiles = collections::hashmap<string, LinxcParsedFile>(allocator, &stringHash, &stringEql);
    this->parsingFiles = collections::hashset<string>(allocator, &stringHash, &stringEql);
    this->includedFiles = collections::vector<string>(allocator);
    this->includeDirectories = collections::vector<string>(allocator);
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
LinxcParsedFile *LinxcParser::ParseFile(collections::Array<string> includeDirs, string fileFullPath, string includeName, string fileContents)
{
    if (this->parsedFiles.Contains(includeName)) //already parsed
    {
        return this->parsedFiles.Get(includeName);
    }

    LinxcParsedFile file = LinxcParsedFile(this->allocator, fileFullPath, includeName); //todo
    this->parsingFiles.Add(includeName);

    LinxcTokenizer tokenizer = LinxcTokenizer(fileContents.buffer, fileContents.length);
    LinxcParserState parserState = LinxcParserState(this, &file, &tokenizer, LinxcEndOn_Eof, true);
    this->ParseCompoundStmt(&parserState);

    //this->parsedFiles.Add(filePath);
    this->parsingFiles.Remove(includeName);
    this->parsedFiles.Add(includeName, file);
    return this->parsedFiles.Get(includeName);
}

LinxcParseIdentifierResult LinxcParser::ParseIdentifier(LinxcParserState *state)
{
    LinxcParseIdentifierResult result = LinxcParseIdentifierResult();

    LinxcParseTypeState parseTypeState = LinxcParseType_ExpectIdentifier;

    LinxcTokenizer *tokenizer = state->tokenizer;

    while (true)
    {
        if (parseTypeState == LinxcParseType_ExpectIdentifier)
        {
            LinxcToken token = tokenizer->NextUntilValid();
            if (LinxcIsPrimitiveType(token.ID))
            {
                string name = token.ToString(&defaultAllocator);
                
                result.value.typeReference = LinxcTypeReference(state->parser->globalNamespace.types.Get(name));
                result.ID = LinxcParseIdentifierResult_Type;
                name.deinit();

                parseTypeState = LinxcParseType_ExpectOnlyPointer;
            }
            else
            {
                //something illegal
                if (token.ID != Linxc_Identifier)
                {
                    state->parsingFile->errors.Add(ERR_MSG(this->allocator, "Expected identifier"));
                    break;
                }
                // TODO: using namespace; checks

                string name = token.ToString(&defaultAllocator);

                //check if we are parsing a type already.
                //if that is the case, we do not need to check for namespaces as types cannot have
                //namespaces within them (That'll be stupid)
                //We only need to check for variables, subtypes and functions
                if (result.ID == LinxcParseIdentifierResult_Type)
                {
                    LinxcType *lastType = result.value.typeReference.lastType;
                    usize references = 0;

                    LinxcType *subType = lastType->FindSubtype(name);
                    if (subType != NULL)
                    {
                        result.value.typeReference.lastType = subType;
                        result.ID = LinxcParseIdentifierResult_Type;
                        references++;
                    }
                    LinxcVar *var = lastType->FindVar(name);
                    if (var != NULL)
                    {
                        if (references == 0)
                        {
                            result.value.variableReference = var;
                            result.ID = LinxcParseIdentifierResult_Variable;
                        }
                        references++;
                    }
                    if (references < 2)
                    {
                        LinxcFunc *func = lastType->FindFunction(name);
                        if (func != NULL)
                        {
                            if (references == 0)
                            {
                                result.value.functionReference = func;
                                result.ID = LinxcParseIdentifierResult_Func;
                            }
                            references++;
                        }
                    }
                    if (references >= 2)
                    {
                        state->parsingFile->errors.Add(ERR_MSG(this->allocator, "Multiple structs, namespaces or static variables are sharing the same name!"));
                    }
                }
                else if (result.ID == LinxcParseIdentifierResult_Namespace || result.ID == LinxcParseIdentifierResult_None)
                {
                    bool continueParseAfterCurrentNamespace = true;
                    LinxcNamespace *checkingNamespace = NULL;
                    if (result.value.namespaceReference != NULL)
                    {
                        checkingNamespace = result.value.namespaceReference;
                        //set this to false as checkingNamespace would be set to the previous namespace from the previous parsed
                        //identifier. That means the parent namespace would have already been parsed, so don't parse the parent again.
                        continueParseAfterCurrentNamespace = false;
                    }
                    else checkingNamespace = state->currentNamespace;

                    while (checkingNamespace != NULL)
                    {
                        u8 references = 0;
                        //check if variable exists
                        if (checkingNamespace->variables.Contains(name))
                        {
                            if (references == 0)
                            {
                                //is a variable
                                result.value.variableReference = checkingNamespace->variables.Get(name);
                                result.ID = LinxcParseIdentifierResult_Variable;

                                //break as no point checking the remaining namespaces for presence of the identifier
                                break;
                            }
                            references++;
                        }
                        //check if struct exists
                        if (checkingNamespace->types.Contains(name))
                        {
                            if (references == 0)
                            {
                                //is a struct
                                result.value.typeReference = LinxcTypeReference(checkingNamespace->types.Get(name));
                                result.ID = LinxcParseIdentifierResult_Type;

                                //break as no point checking the remaining namespaces for presence of the identifier
                                break;
                            }
                            references++;
                        }
                        //check if namespace exists
                        //micro-optimisation: If we checked both prior and both returned a result, that means
                        //there is already an error. No point checking the third to confirm said error.
                        if (references < 2 && checkingNamespace->subNamespaces.Contains(name))
                        {
                            if (references == 0)
                            {
                                //is a namespace
                                result.value.namespaceReference = checkingNamespace->subNamespaces.Get(name);

                                //note that currentParsingNamespace refers to the namespace in the 
                                //chain of identifiers, not the namespace that we are currently iterating 
                                //through to find the identifier. As such, break.
                                break;
                            }
                            references++;
                        }
                        //check if function exists
                        if (references < 2 && checkingNamespace->functions.Contains(name))
                        {
                            if (references == 0)
                            {
                                //is a function
                                result.value.functionReference = checkingNamespace->functions.Get(name);
                                result.ID = LinxcParseIdentifierResult_Func;

                                break;
                            }
                        }

                        if (references >= 2)
                        {
                            state->parsingFile->errors.Add(ERR_MSG(this->allocator, "Multiple structs, namespaces or static variables are sharing the same name!"));
                        }
                        if (continueParseAfterCurrentNamespace)
                        {
                            checkingNamespace = checkingNamespace->parentNamespace;
                        }
                    }
                }
                else
                {
                    state->parsingFile->errors.Add(ERR_MSG(this->allocator, "Attempted to check function or variable for sub-type or namespace!"));
                }
                name.deinit();
            }
            //if we've reached a variable or function reference, then there can be nothing afterwards 
            //but an operator/expression/whatever, so return it
            if (result.ID == LinxcParseIdentifierResult_Variable || result.ID == LinxcParseIdentifierResult_Func)
            {
                return result;
            }
            else
            {
                parseTypeState = LinxcParseType_NextOrEnd;
            }
        }
        else if (parseTypeState == LinxcParseType_NextOrEnd)
        {
            LinxcToken peekNext = tokenizer->PeekNextUntilValid();
            if (peekNext.ID == Linxc_ColonColon)
            {
                parseTypeState = LinxcParseType_ExpectIdentifier;
                tokenizer->NextUntilValid();
            }
            else if (peekNext.ID == Linxc_Asterisk)
            {
                if (result.ID == LinxcParseIdentifierResult_Variable)
                {
                    result.value.typeReference.pointerCount = 1;
                    parseTypeState = LinxcParseType_ExpectOnlyPointer;
                    tokenizer->NextUntilValid();
                }
                else if (result.ID == LinxcParseIdentifierResult_Namespace)
                {
                    state->parsingFile->errors.Add(ERR_MSG(this->allocator, "Attempting to convert namespace to pointer"));
                    break;
                }
            }
            //to handle templates
            else
            {
                //encountered an unrecognised token, probably part of next expression/declaration or whatever
                //that's not our job to parse it, so return
                break;
            }
        }
        else //expect only pointer
        {
            LinxcToken peekNext = tokenizer->PeekNextUntilValid();
            if (peekNext.ID == Linxc_ColonColon)
            {
                tokenizer->NextUntilValid();
                state->parsingFile->errors.Add(ERR_MSG(this->allocator, "Attempting to enter scope of a pointer!"));
                break;
            }
            else if (peekNext.ID == Linxc_AngleBracketLeft)
            {
                tokenizer->NextUntilValid();
                state->parsingFile->errors.Add(ERR_MSG(this->allocator, "Pointer types can never be template types!"));
                break;
            }
            else if (peekNext.ID == Linxc_Asterisk)
            {
                result.value.typeReference.pointerCount += 1;
                tokenizer->NextUntilValid();
            }
            else
            {
                break;
            }
        }
    }

    return result;
}

collections::Array<LinxcVar> LinxcParser::ParseFunctionArgs(LinxcParserState *state)
{
    collections::vector<ERR_MSG> *errors = &state->parsingFile->errors;
    collections::vector<LinxcVar> variables = collections::vector<LinxcVar>(this->allocator);

    LinxcToken peekNext = state->tokenizer->PeekNextUntilValid();
    if (peekNext.ID == Linxc_RParen)
    {
        return variables.ToOwnedArray();
    }

    while (true)
    {
        usize errorsCount = errors->count;
        LinxcParseIdentifierResult parseResult = this->ParseIdentifier(state);
        if (errors->count > errorsCount)
        {
            break;
        }
        if (parseResult.ID == LinxcParseIdentifierResult_Type)
        {
            LinxcTypeReference varType = parseResult.value.typeReference;
            LinxcToken varNameToken = state->tokenizer->NextUntilValid();
            if (varNameToken.ID != Linxc_Identifier)
            {
                errors->Add(ERR_MSG(this->allocator, "Expected identifier after variable type name!"));
                break;
            }
            string varName = varNameToken.ToString(this->allocator);
            variables.Add(LinxcVar(varName, varType));

            LinxcToken next = state->tokenizer->NextUntilValid();
            if (next.ID == Linxc_Comma)
            {
            }
            else if (next.ID == Linxc_Equal)
            {
                //TODO: variable default values, to be impl'd when expression parsing becomes a thing again
            }
            else if (next.ID == Linxc_RParen)
            {
                break;
            }
        }
        else if (parseResult.ID == LinxcParseIdentifierResult_Namespace)
        {
            errors->Add(ERR_MSG(this->allocator, "Attempted to use a namespace as variable type"));
            break;
        }
        else if (parseResult.ID == LinxcParseIdentifierResult_Variable)
        {
            errors->Add(ERR_MSG(this->allocator, "Attempted to use another variable as variable type"));
            break;
        }
        else
        {
            errors->Add(ERR_MSG(this->allocator, "Failed to parse variable type"));
            break;
        }
    }
    return variables.ToOwnedArray();
}

option<LinxcCompoundStmt> LinxcParser::ParseCompoundStmt(LinxcParserState *state)
{
    collections::vector<ERR_MSG> *errors = &state->parsingFile->errors;
    LinxcCompoundStmt result = collections::vector<LinxcStatement>(this->allocator);
    //collections::vector<ERR_MSG> errors = collections::vector<ERR_MSG>(this->allocator);
    LinxcTokenizer *tokenizer = state->tokenizer;
    while (true)
    {
        bool toBreak = false;
        usize prevIndex = tokenizer->prevIndex;
        LinxcTokenID prevTokenID = tokenizer->prevTokenID;
        LinxcToken token = tokenizer->Next();
        switch (token.ID)
        {
            case Linxc_Keyword_include:
                {
                    LinxcToken next = tokenizer->Next();
                    if (next.ID != Linxc_MacroString)
                    {
                        errors->Add(ERR_MSG(this->allocator, "Expected <file to be included> after #include declaration"));
                        return option<LinxcCompoundStmt>();
                    }

                    if (next.end - 1 <= next.start + 1)
                    {
                        errors->Add(ERR_MSG(this->allocator, "#include directive is empty!"));
                    }
                    else 
                    {
                        string macroString = string(this->allocator, tokenizer->buffer + next.start + 1, next.end - 1 - next.start);

                        printf("included %s\n", macroString.buffer);

                        macroString.deinit();
                    }
                    
                    //let's not deal with #includes until we're ready
                    //this is probably the biggest can of worms there is
                    //20/11/2023
                }
                break;
            //Linxc expects <name> to be after struct keyword. There are no typedef struct {} <name> here.
            case Linxc_Keyword_struct:
                {
                    LinxcToken structName = tokenizer->Next();

                    if (structName.ID != Linxc_Identifier)
                    {
                        errors->Add(ERR_MSG(this->allocator, "Expected struct name after struct keyword!"));
                    }
                    else
                    {
                        //declare new struct
                        LinxcType type = LinxcType(allocator, structName.ToString(allocator), state->currentNamespace, state->parentType);
                        string fullName = type.GetFullName(allocator);

                        LinxcToken next = tokenizer->NextUntilValid();
                        if (next.ID == Linxc_LBrace)
                        {
                            LinxcParserState nextState = LinxcParserState(state->parser, state->parsingFile, state->tokenizer, LinxcEndOn_RBrace, false);
                            nextState.parentType = &type;
                            nextState.currentNamespace = state->currentNamespace;

                            option<collections::vector<LinxcStatement>> structBody = this->ParseCompoundStmt(&nextState);

                            if (structBody.present)
                            {
                                result.AddAll_deinit(&structBody.value);
                            }
                            else
                            {
                                //error, send error up the chain
                                //BILLIONS must option<>
                                return option<LinxcCompoundStmt>();
                            }
                            //state->errors->AddAll_deinit(&structParseErrors);

                            if (state->parentType != NULL)
                            {
                                state->parentType->subTypes.Add(type);
                                //as type is now owned by parentType->subTypes, we need to add the pointer to that instead of types directly
                                this->fullNameToType.Add(fullName, state->parentType->subTypes.Get(state->parentType->subTypes.count - 1));
                            }
                            else
                            {
                                state->currentNamespace->types.Add(type.name, type);
                                //as type is now owned by currentNamespace->types, we need to add the pointer to that instead of types directly
                                this->fullNameToType.Add(fullName, state->currentNamespace->types.Get(type.name));
                            }
                        }
                        else
                        {
                            errors->Add(ERR_MSG(this->allocator, "Expected { after struct name!"));
                            return option<LinxcCompoundStmt>();
                        }
                    }
                }
                break;
            //either a function or constructor
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
            case Linxc_Identifier:
                {
                    // parse reference type
                    // void *; is valid
                    // collections::hashmap<i32, collections::Array<string>> is also valid
                    // need to account for both

                    //move backwards
                    tokenizer->Back();

                    // usize errorsCount = errors->count;
                    // LinxcTypeReference type = ParseIdentifier(state);
                    // if (errors->count > errorsCount)
                    // {
                    //     return option<LinxcCompoundStmt>();
                    // }

                    // LinxcToken next = tokenizer->NextUntilValid();
                    // if (next.ID == Linxc_Identifier) //either variable or function
                    // {
                    //     string name = next.ToString(this->allocator);

                    //     next = tokenizer->NextUntilValid();
                    //     if (next.ID == Linxc_LParen)
                    //     {
                    //         //function
                    //         collections::Array<LinxcVar> functionArgs = this->ParseFunctionArgs(state);

                    //         LinxcFunc func = LinxcFunc(name, type);
                    //         func.arguments = functionArgs;

                    //         if (state->parentType != NULL)
                    //         {
                    //             state->parentType->functions.Add(func);
                    //         }
                    //         else
                    //         {
                    //             state->currentNamespace->functions.Add(func.name, func);
                    //         }
                            
                            
                    //     }
                    //     else if (next.ID == Linxc_Semicolon)
                    //     {
                    //         LinxcVar var = LinxcVar(name, type);

                    //         if (state->parentType != NULL)
                    //         {
                    //             state->parentType->variables.Add(var);
                    //         }
                    //         else
                    //         {
                    //             state->currentNamespace->variables.Add(var.name, var);
                    //         }
                    //     }
                    // }
                    // else
                    // {
                    //     errors->Add(ERR_MSG(this->allocator, "Expected identifier after type name"));
                    // }
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
    return result;
}