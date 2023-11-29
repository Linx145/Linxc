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
    this->currentFunction = NULL;
    this->parentType = NULL;
    this->varsInScope = collections::hashmap<string, LinxcVar *>(this->parser->allocator, &stringHash, &stringEql);
}
LinxcParser::LinxcParser(IAllocator *allocator)
{
    this->allocator = allocator;
    this->globalNamespace = LinxcNamespace(allocator, string());
    this->fullNameToType = collections::hashmap<string, LinxcType *>(allocator, &stringHash, &stringEql);

    const char *primitiveTypes[13] = {"u8", "u16", "u32", "u64", "i8", "i16", "i32", "i64", "float", "double", "bool", "char", "void"};
    for (i32 i = 0; i < 13; i++)
    {
        string str = string(allocator, primitiveTypes[i]);
        this->globalNamespace.types.Add(str, LinxcType(allocator, str, &this->globalNamespace, NULL));
        this->fullNameToType.Add(str, this->globalNamespace.types.Get(str));
    }

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

    LinxcParsedFile file = LinxcParsedFile(this->allocator, fileFullPath, includeName); //todo
    this->parsingFiles.Add(includeName);

    LinxcTokenizer tokenizer = LinxcTokenizer(fileContents.buffer, fileContents.length);
    LinxcParserState parserState = LinxcParserState(this, &file, &tokenizer, LinxcEndOn_Eof, true);
    option<collections::vector<LinxcStatement>> ast = this->ParseCompoundStmt(&parserState);

    if (ast.present)
    {
        file.ast = ast.value;
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

option<LinxcExpression> LinxcParser::ParseExpressionPrimary(LinxcParserState *state)
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
                        printf("%i\n", nextPrimaryOpt.value.ID);
                        state->parsingFile->errors.Add(ERR_MSG(this->allocator, "Attempting to place a modifying operator on a type name. You can only modify literals and variables."));
                    }

                    LinxcModifiedExpression *modified = (LinxcModifiedExpression*)this->allocator->Allocate(sizeof(LinxcModifiedExpression));
                    modified->expression = expression;
                    modified->modification = token.ID;

                    LinxcExpression result;
                    result.data.modifiedExpression = modified;
                    result.ID = LinxcExpr_Modified;
                    //TODO: check what the modifier does to the expression's original results based on operator overloading
                    result.resolvesTo = expression.resolvesTo;
                    
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
                break;
            }
        case Linxc_LParen:
            {
                option<LinxcExpression> nextPrimaryOpt = this->ParseExpressionPrimary(state);
                if (nextPrimaryOpt.present)
                {
                    LinxcExpression expression = this->ParseExpression(state, nextPrimaryOpt.value, -1);
                    
                    //check if expression is a type reference. If so, then this is a cast.
                    if (expression.resolvesTo.lastType == NULL)
                    {
                        LinxcExpression result;
                        result.data.typeCast = expression.ToHeap(this->allocator);
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
                LinxcExpression result = this->ParseIdentifier(state, option<LinxcExpression>());
                //if the result is a function ref, it may be a function call instead
                //since function calls are not handled by the operator chaining of scope resolutions,
                //we have to handle it here as a primary expression
                if (result.ID == LinxcExpr_FunctionRef)
                {
                    if (state->tokenizer->PeekNextUntilValid().ID == Linxc_LParen)
                    {
                        //parse input args
                        state->tokenizer->NextUntilValid();

                        collections::vector<LinxcExpression> inputArgs = collections::vector<LinxcExpression>(&defaultAllocator);

                        while (true)
                        {
                            LinxcToken peekNext = state->tokenizer->PeekNextUntilValid();
                            if (peekNext.ID == Linxc_RParen)
                            {
                                state->tokenizer->NextUntilValid();
                                break;
                            }
                            else if (peekNext.ID == Linxc_Comma)
                            {
                                state->tokenizer->NextUntilValid();
                            }
                            else
                            {
                                state->parsingFile->errors.Add(ERR_MSG(this->allocator, "Expected , or ) after function input"));
                                break;
                            }
                            option<LinxcExpression> primaryOpt = this->ParseExpressionPrimary(state);
                            if (!primaryOpt.present)
                            {
                                state->parsingFile->errors.Add(ERR_MSG(this->allocator, "Expected expression in function argument"));
                                break;
                            }
                            LinxcExpression fullExpression = this->ParseExpression(state, primaryOpt.value, -1);

                            inputArgs.Add(fullExpression);
                        }
                        if (inputArgs.count > result.data.functionRef->arguments.length)
                        {
                            state->parsingFile->errors.Add(ERR_MSG(this->allocator, "Too many inputs in function"));
                        }
                        LinxcExpression finalResult;
                        finalResult.ID = LinxcExpr_FuncCall;
                        finalResult.data.functionCall.func = result.data.functionRef;
                        finalResult.data.functionCall.inputParams = inputArgs.ToOwnedArrayWith(this->allocator);
                        finalResult.data.functionCall.templateSpecializations = collections::Array<LinxcTypeReference>(); //todo
                        finalResult.resolvesTo = result.data.functionRef->returnType.AsTypeReference().value;

                        return finalResult;
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
            {
                LinxcExpression result;
                result.data.literal = token.ToString(this->allocator);
                result.ID = LinxcExpr_Literal;

                string temp;
                if (token.ID == Linxc_FloatLiteral)
                {
                    temp = string("float");
                }
                else if (token.ID == Linxc_IntegerLiteral)
                {
                    temp = string("i32");
                }
                else if (token.ID == Linxc_CharLiteral)
                {
                    temp = string("char");
                }
                else
                {
                    temp = string("string");
                }
                result.resolvesTo = LinxcTypeReference(this->globalNamespace.types.Get(temp));
                temp.deinit();

                return result;
            }
        default:
            {
                //move back so we can parse token with ParseIdentifier (which handles primitive types too)
                if (LinxcIsPrimitiveType(token.ID))
                {
                    state->tokenizer->Back();
                    LinxcExpression result = this->ParseIdentifier(state, option<LinxcExpression>());
                    //cannot call anything with primitive type so, and result is guaranteed to be type reference so
                    return result;
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
        option<LinxcExpression> rhsOpt = this->ParseExpressionPrimary(state);
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

        lhs.data.operatorCall = operatorCall;
        lhs.ID = LinxcExpr_OperatorCall;
    }
    return lhs;
}
LinxcExpression LinxcParser::ParseIdentifier(LinxcParserState *state, option<LinxcExpression> parentScopeOverride)
{
    LinxcExpression result;
    result.ID = LinxcExpr_OperatorCall;
    LinxcToken token = state->tokenizer->NextUntilValid();
    string identifierName = token.ToString(&defaultAllocator);

    if (LinxcIsPrimitiveType(token.ID))
    {
        LinxcType *type = this->globalNamespace.types.Get(identifierName);
        LinxcTypeReference reference;
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
                result.data.variable = *asLocalVar;
                result.resolvesTo = result.data.variable->type.AsTypeReference().value;
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
        else if (parentScopeOverride.value.ID == LinxcExpr_TypeRef)
        {
            LinxcType *toCheck = parentScopeOverride.value.data.typeRef.lastType;
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
    return result;
}

collections::Array<LinxcVar> LinxcParser::ParseFunctionArgs(LinxcParserState *state)
{
    collections::vector<ERR_MSG> *errors = &state->parsingFile->errors;
    collections::vector<LinxcVar> variables = collections::vector<LinxcVar>(this->allocator);

    LinxcToken peekNext = state->tokenizer->PeekNextUntilValid();
    if (peekNext.ID == Linxc_RParen)
    {
        state->tokenizer->NextUntilValid();
        return variables.ToOwnedArray();
    }

    while (true)
    {
        option<LinxcExpression> primaryOpt = this->ParseExpressionPrimary(state);
        if (!primaryOpt.present) //error encountered
        {
            return collections::Array<LinxcVar>();
        }
        LinxcExpression expression = this->ParseExpression(state, primaryOpt.value, -1);

        //if it resolves to nothing, that means it's a variable type.
        if (expression.resolvesTo.lastType == NULL)
        {
            LinxcToken varNameToken = state->tokenizer->NextUntilValid();
            if (varNameToken.ID != Linxc_Identifier)
            {
                errors->Add(ERR_MSG(this->allocator, "Expected identifier after variable type name!"));
                break;
            }
            string varName = varNameToken.ToString(this->allocator);
            variables.Add(LinxcVar(varName, expression, option<LinxcExpression>()));

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
        else if (expression.ID == LinxcExpr_NamespaceRef)
        {
            errors->Add(ERR_MSG(this->allocator, "Attempted to use a namespace as variable type"));
            break;
        }
        else if (expression.ID == LinxcExpr_Variable)
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

option<collections::vector<LinxcStatement>> LinxcParser::ParseCompoundStmt(LinxcParserState *state)
{
    bool expectSemicolon = false;
    collections::vector<ERR_MSG> *errors = &state->parsingFile->errors;
    collections::vector<LinxcStatement> result = collections::vector<LinxcStatement>(this->allocator);
    //collections::vector<ERR_MSG> errors = collections::vector<ERR_MSG>(this->allocator);
    LinxcTokenizer *tokenizer = state->tokenizer;
    while (true)
    {
        bool toBreak = false;
        usize prevIndex = tokenizer->prevIndex;
        LinxcTokenID prevTokenID = tokenizer->prevTokenID;
        LinxcToken token = tokenizer->Next();
        if (token.ID == Linxc_Semicolon && expectSemicolon)
        {
            expectSemicolon = false;
            continue;
        }
        else if (expectSemicolon)
        {
            errors->Add(ERR_MSG(this->allocator, "Expected semicolon"));
            break;
        }
        switch (token.ID)
        {
            case Linxc_Keyword_include:
                {
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
                        //string macroString = string(this->allocator, tokenizer->buffer + next.start + 1, next.end - 2 - next.start);

                        //printf("included %s\n", macroString.buffer);
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
                        if (state->parentType != NULL)
                        {
                            type.parentType = state->parentType;
                        }
                        else
                            type.typeNamespace = state->currentNamespace;

                        string fullName = type.GetFullName(allocator);

                        LinxcToken next = tokenizer->NextUntilValid();
                        if (next.ID != Linxc_LBrace)
                        {
                            errors->Add(ERR_MSG(this->allocator, "Expected { after struct name!"));
                            toBreak = true;
                            break;
                        }
                        LinxcParserState nextState = LinxcParserState(state->parser, state->parsingFile, state->tokenizer, LinxcEndOn_RBrace, false);
                        nextState.parentType = &type;
                        nextState.currentNamespace = state->currentNamespace;

                        option<collections::vector<LinxcStatement>> structBody = this->ParseCompoundStmt(&nextState);

                        if (!structBody.present)
                        {
                            toBreak = true;
                            break;
                        }
                        type.body = structBody.value;

                        LinxcType *ptr;
                        if (state->parentType != NULL)
                        {
                            state->parentType->subTypes.Add(type);
                            ptr = state->parentType->subTypes.Get(state->parentType->subTypes.count - 1);
                            //as type is now owned by parentType->subTypes, we need to add the pointer to that instead of types directly
                            this->fullNameToType.Add(fullName, ptr);
                        }
                        else
                        {
                            state->currentNamespace->types.Add(type.name, type);
                            ptr = state->currentNamespace->types.Get(type.name);
                            //as type is now owned by currentNamespace->types, we need to add the pointer to that instead of types directly
                            this->fullNameToType.Add(fullName, ptr);
                        }

                        LinxcStatement stmt;
                        stmt.data.typeDeclaration = ptr;
                        stmt.ID = LinxcStmt_TypeDecl;
                        result.Add(stmt);
                        nextState.deinit();
                    }
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
            case Linxc_Identifier:
                {
                    //move backwards
                    tokenizer->Back();

                    option<LinxcExpression> typeExpressionOpt = this->ParseExpressionPrimary(state);
                    if (!typeExpressionOpt.present)
                    {
                        toBreak = true;
                        break;
                    }
                    LinxcExpression expr = this->ParseExpression(state, typeExpressionOpt.value, -1);

                    //resolves to a type name
                    if (expr.resolvesTo.lastType == NULL)
                    {
                        //this is the actual variable's/function return type
                        LinxcTypeReference expectedType = expr.AsTypeReference().value;
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
                                    //errors->Add(ERR_MSG(this->allocator, "Failed to parse variable value"));
                                    toBreak = true;
                                    break;
                                }
                                defaultValue.value = this->ParseExpression(state, primary.value, -1);
                                defaultValue.present = true;
                            }
                            LinxcVar varDecl = LinxcVar(identifier.ToString(this->allocator), expr, defaultValue);
                            
                            if (defaultValue.present && expectedType != defaultValue.value.resolvesTo)
                            {
                                errors->Add(ERR_MSG(this->allocator, "Variable initial value is not of the same type as the variable"));
                            }

                            //not in a function
                            if (state->currentFunction == NULL)
                            {
                                //in a struct
                                if (state->parentType != NULL)
                                {
                                    state->parentType->variables.Add(varDecl);
                                }
                                else //else add to namespace
                                {
                                    state->currentNamespace->variables.Add(varDecl.name, varDecl);
                                }
                            }
                            else //in a function, add as temp variable instead
                            {
                                LinxcStatement stmt;
                                stmt.data.tempVarDeclaration = varDecl;
                                stmt.ID = LinxcStmt_TempVarDecl;
                                result.Add(stmt);

                                state->varsInScope.Add(varDecl.name, &result.Get(result.count - 1)->data.tempVarDeclaration);
                            }
                        }
                        else if (next.ID == Linxc_LParen) //function declaration
                        {
                            collections::Array<LinxcVar> args = this->ParseFunctionArgs(state);
                            next = tokenizer->NextUntilValid();
                            if (next.ID != Linxc_LBrace)
                            {
                                errors->Add(ERR_MSG(this->allocator, "Expected { to be after function name"));
                            }

                            LinxcFunc newFunc = LinxcFunc(identifier.ToString(this->allocator), expr);
                            if (state->parentType != NULL)
                            {
                                newFunc.methodOf = state->parentType;
                            }
                            else
                            {
                                newFunc.funcNamespace = state->currentNamespace;
                            }
                            
                            LinxcParserState nextState = LinxcParserState(state->parser, state->parsingFile, state->tokenizer, LinxcEndOn_RBrace, false);
                            nextState.parentType = state->parentType;
                            nextState.currentNamespace = state->currentNamespace;
                            nextState.currentFunction = &newFunc;

                            option<collections::vector<LinxcStatement>> funcBody = this->ParseCompoundStmt(&nextState);
                            
                            if (!funcBody.present)
                            {
                                toBreak = true;
                                break;
                            }
                            newFunc.body = funcBody.value;
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

                            LinxcStatement stmt;
                            stmt.data.funcDeclaration = ptr;
                            stmt.ID = LinxcStmt_TypeDecl;

                            result.Add(stmt);
                            nextState.deinit();
                        }
                    }
                     //random expressions (EG: functionCall()) are only allowed within functions
                }
                break;
            case Linxc_RBrace:
                {
                    if (state->endOn == LinxcEndOn_RBrace)
                    {
                        toBreak = true;
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