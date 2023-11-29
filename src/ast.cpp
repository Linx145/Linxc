#include <ast.hpp>

LinxcParsedFile::LinxcParsedFile()
{
    this->definedFuncs = collections::vector<LinxcFunc *>();
    this->definedMacros = collections::vector<LinxcMacro>();
    this->definedTypes = collections::vector<LinxcType *>();
    this->definedVars = collections::vector<LinxcVar *>();
    this->errors = collections::vector<ERR_MSG>();
    this->fullPath = string();
    this->includeName = string();
    this->ast = collections::vector<LinxcStatement>();
}
LinxcParsedFile::LinxcParsedFile(IAllocator *allocator, string fullPath, string includeName)
{
    this->definedFuncs = collections::vector<LinxcFunc *>(allocator);
    this->definedMacros = collections::vector<LinxcMacro>(allocator);
    this->definedTypes = collections::vector<LinxcType *>(allocator);
    this->definedVars = collections::vector<LinxcVar *>(allocator);
    this->errors = collections::vector<ERR_MSG>(allocator);
    this->fullPath = fullPath;
    this->includeName = includeName;
    this->ast = collections::vector<LinxcStatement>();
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
    this->body = collections::vector<LinxcStatement>();
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
    this->type = LinxcExpression();
    this->defaultValue = option<LinxcExpression>();
}
LinxcVar::LinxcVar(string varName, LinxcExpression varType, option<LinxcExpression> defaultVal)
{
    this->name = varName;
    this->type = varType;
    this->defaultValue = defaultVal;
}
string LinxcVar::ToString(IAllocator *allocator)
{
    return string(allocator, this->name.buffer);
}

LinxcFunc::LinxcFunc()
{
    this->body = collections::vector<LinxcStatement>();
    this->name = string();
    this->methodOf = NULL;
    this->funcNamespace = NULL;
    this->returnType = LinxcExpression();
    this->arguments = collections::Array<LinxcVar>();
    this->templateArgs = collections::Array<string>();
}
LinxcFunc::LinxcFunc(string name, LinxcExpression returnType)
{
    this->body = collections::vector<LinxcStatement>();
    this->name = name;
    this->methodOf = NULL;
    this->funcNamespace = NULL;
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
        {
            string result = string(allocator);
            result.AppendDeinit(this->data.decrementVariable->ToString(&defaultAllocator)); // this->data.decrementVariable.ToString(allocator));
            result.Append("--");
            return result;
        }
        case LinxcExpr_IncrementVar:
        {
            string result = string(allocator);
            result.AppendDeinit(this->data.incrementVariable->ToString(&defaultAllocator));
            result.Append("++");
            return result;
        }
        case LinxcExpr_FuncCall:
            return this->data.functionCall.ToString(allocator);
        case LinxcExpr_FunctionRef:
            return string(allocator, this->data.functionRef->name.buffer);
        case LinxcExpr_Literal:
            return string(allocator, this->data.literal.buffer);
        case LinxcExpr_Modified:
        {
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
        }
        case LinxcExpr_Nameof:
        {
            string result = string(allocator);
            result.Append("nameof(");
            result.AppendDeinit(this->data.nameofCall.ToString(&defaultAllocator));
            result.Append(")");
            return result;
        }
        case LinxcExpr_OperatorCall:
        {
            string result = string(allocator);
            result.AppendDeinit(this->data.operatorCall->leftExpr.ToString(&defaultAllocator));
            result.Append(TokenIDToString(this->data.operatorCall->operatorType));
            result.AppendDeinit(this->data.operatorCall->rightExpr.ToString(&defaultAllocator));
            return result;
        }
        case LinxcExpr_Sizeof:
        {
            string result = string(allocator);
            result.Append("sizeof(");
            result.AppendDeinit(this->data.sizeofCall.ToString(&defaultAllocator));
            result.Append(")");
            return result;
        }
        case LinxcExpr_TypeCast:
        {
            string result = string(allocator);
            result.Append("(");
            result.AppendDeinit(this->data.typeCast->ToString(&defaultAllocator));
            result.Append(")");
            return result;
        }
        case LinxcExpr_Typeof:
        {
            string result = string(allocator);
            result.Append("typeof(");
            result.AppendDeinit(this->data.typeofCall.ToString(&defaultAllocator));
            result.Append(")");
            return result;
        }
        case LinxcExpr_TypeRef:
            return this->data.typeRef.ToString(allocator);
        case LinxcExpr_Variable:
            return this->data.variable->ToString(allocator);
        default:
            return string();
    }
}
option<LinxcTypeReference> LinxcExpression::AsTypeReference()
{
    if (this->resolvesTo.lastType != NULL)
    {
        return option<LinxcTypeReference>();
    }
    if (this->ID == LinxcExpr_TypeRef)
    {
        return this->data.typeRef;
    }
    if (this->ID == LinxcExpr_OperatorCall)
    {
        return this->data.operatorCall->rightExpr.AsTypeReference();
    }
    return option<LinxcTypeReference>();
}
LinxcExpression *LinxcExpression::ToHeap(IAllocator *allocator)
{
    LinxcExpression *result = (LinxcExpression*)allocator->Allocate(sizeof(LinxcExpression));
    result->data = this->data;
    result->ID = this->ID;
    result->resolvesTo = this->resolvesTo;
    return result;
}

LinxcStatementData::LinxcStatementData()
{
    this->includeStatement = NULL;
}