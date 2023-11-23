#include <parser.hpp>
#include <stdio.h>

LinxcParserState::LinxcParserState(LinxcParser *myParser, LinxcParsedFile *currentFile, LinxcTokenizer *myTokenizer, LinxcEndOn endsOn, bool isTopLevel)
{
    this->tokenizer = myTokenizer;
    this->parser = myParser;
    this->parsingFile = currentFile;
    this->endOn = endsOn;
    this->isToplevel = isTopLevel;
    this->currentNamespace = &myParser->globalNamespace;
    this->parentType = NULL;
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
}
collections::vector<ERR_MSG> LinxcParser::ParseFileH(collections::Array<string> includeDirs, string filePath, string fileContents)
{
    collections::vector<ERR_MSG> errors = collections::vector<ERR_MSG>(this->allocator);
    if (this->parsedFiles.Contains(filePath)) //already parsed
    {
        return errors;
    }

    LinxcParsedFile file = LinxcParsedFile();

    LinxcTokenizer tokenizer = LinxcTokenizer(fileContents.buffer, fileContents.length);
    LinxcParserState parserState = LinxcParserState(this, &file, &tokenizer, LinxcEndOn_Eof, true);
    this->ParseCompoundStmtH(&parserState);

    //this->parsedFiles.Add(filePath);

    return errors;
}

LinxcTypeReference LinxcParser::ParseTypeReference(LinxcParserState *state, collections::vector<ERR_MSG>* errors)
{
    //see: notes - type reference parsing.txt
    LinxcTypeReference result;
    result.rawText = string();
    result.lastType = NULL;
    result.pointerCount = 0;

    LinxcTokenizer *tokenizer = state->tokenizer;

    usize refStart = tokenizer->index;
    usize nameEnd = tokenizer->index;

    LinxcParseTypeState parseTypeState = LinxcParseType_ExpectIdentifier;

    while (true)
    {
        if (parseTypeState == LinxcParseType_ExpectIdentifier)
        {
            LinxcToken token = tokenizer->NextUntilValid();
            if (LinxcIsPrimitiveType(token.ID))
            {
                string nextStr = token.ToString(&defaultAllocator);
                result.lastType = state->parser->globalNamespace.types.Get(nextStr);
                nextStr.deinit();
                break;
            }
            if (token.ID != Linxc_Identifier)
            {
                errors->Add(ERR_MSG("Expected identifier"));
                return result;
            }
            parseTypeState = LinxcParseType_ExpectColon;
            nameEnd = token.end;
        }
        else if (parseTypeState == LinxcParseType_ExpectColon)
        {
            LinxcToken token = tokenizer->NextUntilValid();
            if (token.ID == Linxc_ColonColon)
            {
                parseTypeState = LinxcParseType_ExpectIdentifier;
                continue;
            }
            else if (token.ID == Linxc_Asterisk)
            {
                parseTypeState = LinxcParseType_ExpectOnlyPointer;
                result.pointerCount += 1;
            }
            else if (token.ID == Linxc_AngleBracketLeft)
            {

            }
        }
        else if (parseTypeState == LinxcParseType_ExpectOnlyPointer)
        {
            LinxcToken peekNext = tokenizer->PeekNextUntilValid();
            if (peekNext.ID == Linxc_Asterisk)
            {
                result.pointerCount += 1;
                tokenizer->NextUntilValid();
            }
            else
            {
                break;
            }
        }
    }
    if (result.lastType == NULL)
    {
        string baseString = string(tokenizer->buffer + refStart, nameEnd - refStart);

        result.lastType = *this->fullNameToType.Get(baseString);

        baseString.deinit();
    }

    result.rawText = string(this->allocator, tokenizer->buffer + refStart, tokenizer->index - refStart);

    return result;
}

collections::Array<LinxcVar> LinxcParser::ParseFunctionArgs(LinxcParserState *state, collections::vector<ERR_MSG> *errors)
{
    collections::vector<LinxcVar> variables = collections::vector<LinxcVar>(this->allocator);

    LinxcToken peekNext = state->tokenizer->PeekNextUntilValid();
    if (peekNext.ID == Linxc_RParen)
    {
        return variables.ToOwnedArray();
    }

    while (true)
    {
        usize errorsCount = errors->count;
        LinxcTypeReference varType = this->ParseTypeReference(state, errors);
        if (errors->count > errorsCount)
        {
            break;
        }

        LinxcToken varNameToken = state->tokenizer->NextUntilValid();
        if (varNameToken.ID != Linxc_Identifier)
        {
            errors->Add(ERR_MSG("Expected identifier after variable type name!"));
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
    return variables.ToOwnedArray();
}

collections::vector<ERR_MSG> LinxcParser::ParseCompoundStmtH(LinxcParserState *state)
{
    collections::vector<ERR_MSG> errors = collections::vector<ERR_MSG>(this->allocator);
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
                        errors.Add(ERR_MSG("Expected <file to be included> after #include declaration"));
                        return errors;
                    }

                    if (next.end - 1 <= next.start + 1)
                    {
                        errors.Add(ERR_MSG("#include directive is empty!"));
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
                        errors.Add(ERR_MSG("Expected struct name after struct keyword!"));
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

                            collections::vector<ERR_MSG> structParseErrors = this->ParseCompoundStmtH(&nextState);

                            errors.AddAll_deinit(&structParseErrors);

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
                            errors.Add(ERR_MSG("Expected { after struct name!"));
                            return errors;
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

                    usize errorsCount = errors.count;
                    LinxcTypeReference type = ParseTypeReference(state, &errors);
                    if (errors.count > errorsCount)
                    {
                        return errors;
                    }

                    LinxcToken next = tokenizer->NextUntilValid();
                    if (next.ID == Linxc_Identifier) //either variable or function
                    {
                        string name = next.ToString(this->allocator);

                        next = tokenizer->NextUntilValid();
                        if (next.ID == Linxc_LParen)
                        {
                            //function
                            collections::Array<LinxcVar> functionArgs = this->ParseFunctionArgs(state, &errors);

                            LinxcFunc func = LinxcFunc(name, type);
                            func.arguments = functionArgs;

                            if (state->parentType != NULL)
                            {
                                state->parentType->functions.Add(func);
                            }
                            else
                            {
                                state->currentNamespace->functions.Add(func.name, func);
                            }
                            
                            
                        }
                        else if (next.ID == Linxc_Semicolon)
                        {
                            LinxcVar var = LinxcVar(name, type);

                            if (state->parentType != NULL)
                            {
                                state->parentType->variables.Add(var);
                            }
                            else
                            {
                                state->currentNamespace->variables.Add(var.name, var);
                            }
                        }
                    }
                    else
                    {
                        errors.Add(ERR_MSG("Expected identifier after type name"));
                    }
                }
                break;
            case Linxc_Eof:
                {
                    if (state->endOn == LinxcEndOn_RBrace)
                    {
                        errors.Add(ERR_MSG("Expected }"));
                    }
                    else if (state->endOn == LinxcEndOn_Endif)
                    {
                        errors.Add(ERR_MSG("Expected #endif"));
                    }
                    else if (state->endOn == LinxcEndOn_Semicolon)
                    {
                        errors.Add(ERR_MSG("Expected ;"));
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
    return errors;
}