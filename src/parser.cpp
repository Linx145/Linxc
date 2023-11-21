#include <parser.hpp>
#include <stdio.h>

LinxcParserState::LinxcParserState(LinxcParser *myParser, LinxcTokenizer *myTokenizer, LinxcEndOn endsOn, bool isTopLevel)
{
    this->tokenizer = myTokenizer;
    this->parser = myParser;
    this->endOn = endsOn;
    this->isToplevel = isTopLevel;
    this->currentNamespace = &myParser->globalNamespace;
    this->parentType = NULL;
}

LinxcParser::LinxcParser()
{
    allocator = &defaultAllocator;
    globalNamespace = LinxcNamespace(string());
    parsedFiles = collections::hashmap<string, LinxcParsedFile>(&stringHash, &stringEql);
}
LinxcParser::LinxcParser(IAllocator *allocator)
{
    this->allocator = allocator;
    globalNamespace = LinxcNamespace(string());
    parsedFiles = collections::hashmap<string, LinxcParsedFile>(&stringHash, &stringEql);
}
collections::vector<ERR_MSG> LinxcParser::ParseFileH(collections::Array<string> includeDirs, string filePath, string fileContents)
{
    collections::vector<ERR_MSG> errors = collections::vector<ERR_MSG>(this->allocator);
    if (this->parsedFiles.Contains(filePath)) //already parsed
    {
        return errors;
    }

    LinxcTokenizer tokenizer = LinxcTokenizer(fileContents.buffer, fileContents.length);
    LinxcParserState parserState = LinxcParserState(this, &tokenizer, LinxcEndOn_Eof, true);
    this->ParseCompoundStmtH(&parserState);

    //this->parsedFiles.Add(filePath);

    return errors;
}

LinxcTypeReference LinxcParser::ParseTypeReference(LinxcParserState *state, collections::vector<ERR_MSG>* errors)
{
    LinxcTypeReference result;
    result.rawText = string();
    result.lastType = NULL;
    result.pointerCount = 0;

    LinxcTokenizer *tokenizer = state->tokenizer;

    LinxcNamespace *parentNamespace = state->currentNamespace;
    LinxcType *parentType = state->parentType;

    //after a identifier, expectDoubleColon becomes true.
    //At that point, encountering another identifier terminates the statement. (Eg: mynamespace::type varName) -> varName terminates it
    //Encountering a double colon continues it
    //Encountering < calls ParseTypeReference recursively until either , or >, and expectDoubleColon remains true.
    //Encountering a * increases the pointer count by one and expectDoubleColon remains true.
    bool expectDoubleColon = false;

    usize refStart = tokenizer->index;

    while (true)
    {
        LinxcToken peekNext = tokenizer->PeekNextUntilValid();

        bool isPrimitiveType = LinxcIsPrimitiveType(peekNext.ID);

        //we parse up until the point we hit a * or a < or end
        if (isPrimitiveType || peekNext.ID == Linxc_Identifier)
        {
            if (expectDoubleColon)
            {
                break;
            }
            else
            {
                if (isPrimitiveType)
                {
                    result.lastType = NULL; //type of primitive
                }

                string identifierString = peekNext.ToString(&defaultAllocator);

                //at this point, we need to find the type that it is referencing, or failing that, the namespace.
                //there are two potential ways to find the reference at this point.
                
                //the first, the type they are referring to is declared in the same namespace, or using namespace is utilised, so no namespaceName:: is required.
                //the second, either the type is not in the same namespace, or the user writes namespaceName:: anyways.
                
                //any combination of the two can occur, including nested namespaces or nested structs in both regular and templated structs.
                //as we are parsing type by type or namespace by namespace, we don't bother with too much at this point.
                //However, if the user tries to do namespace::type::namespace, we must throw an error as it is not valid syntax.
    
                //start the first route, checking with parent namespace first
                if (parentType != NULL)
                {

                }
                else //check with parent type for subtypes
                {
                    

                    //parentNamespace should never be NULL
                    if (parentNamespace->subNamespaces.Contains(identifierString))
                    {
                        parentNamespace = parentNamespace->subNamespaces.Get(identifierString);
                    }
                    else if (parentNamespace->types.Contains(identifierString))
                    {
                        parentType = parentNamespace->types.Get(identifierString);
                    }
                    //else, we may be in some other namespace tree, or even in a namespaceless-type.
                    else
                    {
                        //thus, check from parser root namespace
                    }
                    
                }

                identifierString.deinit();
                tokenizer->NextUntilValid();
                expectDoubleColon = true;
            }
        }
        else if (peekNext.ID == Linxc_ColonColon)
        {
            if (expectDoubleColon)
            {
                expectDoubleColon = false;
                tokenizer->NextUntilValid();
            }
            else
            {
                errors->Add(ERR_MSG("Expected identifier or type name, not :: !"));
                //no point continuing
                return result;
            }
        }
    }

    usize refEnd = tokenizer->index;

    result.rawText = string(this->allocator, tokenizer->buffer + refStart, refEnd - refStart);

    return result;
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
                        LinxcType type = LinxcType();
                        type.name = structName.ToString(this->allocator);
                        type.parentType = state->parentType;
                        state->currentNamespace->types.Add(type.name, type);

                        LinxcToken next = tokenizer->NextUntilValid();
                        if (next.ID == Linxc_LBrace)
                        {
                            LinxcParserState nextState = LinxcParserState(state->parser, state->tokenizer, LinxcEndOn_RBrace, false);
                            nextState.parentType = &type;
                            nextState.currentNamespace = state->currentNamespace;

                            collections::vector<ERR_MSG> structParseErrors = this->ParseCompoundStmtH(&nextState);

                            errors.AddAll_deinit(&structParseErrors);

                            if (state->parentType != NULL)
                            {
                                state->parentType->subTypes.Add(type);
                            }
                            else
                            {
                                state->currentNamespace->types.Add(type.name, type);
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
                    tokenizer->index = token.start;
                    tokenizer->prevIndex = prevIndex;
                    tokenizer->prevTokenID = prevTokenID;

                    
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