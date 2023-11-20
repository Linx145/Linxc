#include <parser.hpp>
#include <stdio.h>

LinxcParserState::LinxcParserState(LinxcParser *myParser, LinxcTokenizer *myTokenizer, LinxcEndOn endsOn, bool isTopLevel)
{
    this->tokenizer = myTokenizer;
    this->parser = myParser;
    this->endOn = endsOn;
    this->isToplevel = isTopLevel;
    this->namespaces = collections::vector<LinxcNamespace *>();
}
LinxcNamespace *LinxcParserState::GetCurrentNamespace()
{
    if (this->namespaces.count > 0)
    {
        return *this->namespaces.Get(this->namespaces.count - 1);
    }
    else
        return &this->parser->globalNamespace;
}

LinxcParser::LinxcParser()
{
    allocator = &defaultAllocator;
    globalNamespace = LinxcNamespace(string());
    parsedFiles = collections::hashset<string>(&stringHash, &stringEql);
}
LinxcParser::LinxcParser(IAllocator *allocator)
{
    this->allocator = allocator;
    globalNamespace = LinxcNamespace(string());
    parsedFiles = collections::hashset<string>(&stringHash, &stringEql);
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
    //this->currentFilePath = filePath;

    this->parsedFiles.Add(filePath);

    return errors;
}

collections::vector<ERR_MSG> LinxcParser::ParseCompoundStmtH(LinxcParserState *state)
{
    collections::vector<ERR_MSG> errors = collections::vector<ERR_MSG>(this->allocator);
    LinxcTokenizer *tokenizer = state->tokenizer;
    while (true)
    {
        bool toBreak = false;
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

                    string fileName = string(this->allocator, tokenizer->buffer + next.start, next.end - next.start);

                    printf("included %s\n", fileName.buffer);

                    fileName.deinit();
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