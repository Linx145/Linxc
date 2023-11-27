#include <ast.hpp>

LinxcParsedFile::LinxcParsedFile(IAllocator *allocator, string fullPath, string includeName)
{
    this->definedFuncs = collections::vector<LinxcFunc *>(allocator);
    this->definedMacros = collections::vector<LinxcMacro>(allocator);
    this->definedTypes = collections::vector<LinxcType *>(allocator);
    this->definedVars = collections::vector<LinxcVar *>(allocator);
    this->errors = collections::vector<ERR_MSG>(allocator);
    this->fullPath = fullPath;
    this->includeName = includeName;
}

LinxcTypeReference::LinxcTypeReference()
{
    this->lastType = NULL;
    this->pointerCount = 0;
    this->templateArgs = collections::Array<LinxcTypeReference>();
}
LinxcTypeReference::LinxcTypeReference(LinxcType *type)
{
    this->lastType = type;
    this->pointerCount = 0;
    this->templateArgs = collections::Array<LinxcTypeReference>();
}

LinxcNamespace::LinxcNamespace()
{
    this->parentNamespace;
    this->name = string();
}
LinxcNamespace::LinxcNamespace(IAllocator *allocator, string name)
{
    this->parentNamespace = NULL;
    this->name = name;
    this->functions = collections::hashmap<string, LinxcFunc>(allocator, &stringHash, &stringEql);
    this->subNamespaces = collections::hashmap<string, LinxcNamespace>(allocator, &stringHash, &stringEql);
    this->types = collections::hashmap<string, LinxcType>(allocator, &stringHash, &stringEql);
    this->variables = collections::hashmap<string, LinxcVar>(allocator, &stringHash, &stringEql);
}

LinxcType::LinxcType()
{
    this->name = string();
    this->parentType = NULL;
    this->typeNamespace = NULL;
    this->functions = collections::vector<LinxcFunc>();
    this->subTypes = collections::vector<LinxcType>();
    this->templateArgs = collections::vector<string>();
    this->variables = collections::vector<LinxcVar>();
}
LinxcType::LinxcType(IAllocator *allocator, string name, LinxcNamespace *myNamespace, LinxcType *myParent)
{
    this->name = name;
    this->parentType = myParent;
    this->typeNamespace = myNamespace;
    this->functions = collections::vector<LinxcFunc>(allocator);
    this->subTypes = collections::vector<LinxcType>(allocator);
    this->templateArgs = collections::vector<string>(allocator);
    this->variables = collections::vector<LinxcVar>(allocator);
}
string LinxcType::GetFullName(IAllocator *allocator)
{
    string result = string();
    result.allocator = allocator;

    LinxcNamespace *currentNamespace = this->typeNamespace;
    while (currentNamespace != NULL)
    {
        result.Prepend("::");
        result.Prepend(currentNamespace->name.buffer);
        currentNamespace = currentNamespace->parentNamespace;
    }
    result.Append(this->name.buffer);
    
    return result;
}
LinxcFunc *LinxcType::FindFunction(string name)
{
    for (usize i = 0; i < this->functions.count; i++)
    {
        if (this->functions.Get(i)->name.eql(name.buffer))
        {
            return this->functions.Get(i);
        }
    }
    return NULL;
}
LinxcType *LinxcType::FindSubtype(string name)
{
    for (usize i = 0; i < this->subTypes.count; i++)
    {
        if (this->subTypes.Get(i)->name.eql(name.buffer))
        {
            return this->subTypes.Get(i);
        }
    }
    return NULL;
}
LinxcVar *LinxcType::FindVar(string name)
{
    for (usize i = 0; i < this->variables.count; i++)
    {
        if (this->variables.Get(i)->name.eql(name.buffer))
        {
            return this->variables.Get(i);
        }
    }
    return NULL;
}

LinxcVar::LinxcVar()
{
    this->name = string();
    this->type = LinxcTypeReference();
}
LinxcVar::LinxcVar(string varName, LinxcTypeReference varType)
{
    this->name = varName;
    this->type = varType;
}
string LinxcVar::ToString(IAllocator *allocator)
{
    return string(allocator, this->name.buffer);
}

LinxcFunc::LinxcFunc(string name, LinxcTypeReference returnType)
{
    this->name = name;
    this->methodOf = NULL;
    this->funcNamespace = NULL;
    this->startIndex = 0;
    this->endIndex = 0;
    this->returnType = returnType;
    this->arguments = collections::Array<LinxcVar>();
    this->templateArgs = collections::Array<string>();
}

string LinxcFunctionCall::ToString(IAllocator *allocator)
{
    string result = string(allocator);
    result.Append(this->func->name.buffer);
    result.Append("(");
    for (int i = 0; i < this->inputParams.length; i++)
    {
        result.AppendDeinit(this->inputParams.data[i].ToString(&defaultAllocator));
    }
    result.Append(")");
    return result;
}

string LinxcTypeReference::ToString(IAllocator *allocator)
{
    string result = string(allocator);
    result.AppendDeinit(this->lastType->GetFullName(&defaultAllocator));
    for (i32 i = 0; i < this->pointerCount; i++)
    {
        result.Append("*");
    }
    return result;
}

LinxcExpressionData::LinxcExpressionData()
{
    this->operatorCall = NULL;
}
string LinxcExpression::ToString(IAllocator *allocator)
{
    switch (this->ID)
    {
        case LinxcExpr_DecrementVar:
            string result = string(allocator);
            result.AppendDeinit(this->data.decrementVariable->ToString(&defaultAllocator)); // this->data.decrementVariable.ToString(allocator));
            result.Append("--");
            return result;
        case LinxcExpr_IncrementVar:
            string result = string(allocator);
            result.AppendDeinit(this->data.incrementVariable->ToString(&defaultAllocator));
            result.Append("++");
            return result;
        case LinxcExpr_FuncCall:
            return this->data.functionCall.ToString(allocator);
        case LinxcExpr_FunctionRef:
            return string(allocator, this->data.functionRef->name.buffer);
        case LinxcExpr_Literal:
            return string(allocator, this->data.literal.buffer);
        case LinxcExpr_Modified:
            string result = string(allocator);
            switch (this->data.modifiedExpression->modification)
            {
                case Linxc_Asterisk:
                    result.Append("*");
                    break;
                case Linxc_Minus:
                    result.Append("-");
                    break;
                case Linxc_Bang:
                    result.Append("!");
                    break;
                case Linxc_Tilde:
                    result.Append("~");
                    break;
                case Linxc_Ampersand:
                    result.Append("&");
                    break;
                default:
                    break;
            }
            result.AppendDeinit(this->data.modifiedExpression->expression.ToString(&defaultAllocator));
            return result;
        case LinxcExpr_Nameof:
            string result = string(allocator);
            result.Append("nameof(");
            result.AppendDeinit(this->data.nameofCall.ToString(&defaultAllocator));
            result.Append(")");
            break;
        case LinxcExpr_OperatorCall:
            
        default:
            return string();
    }
}