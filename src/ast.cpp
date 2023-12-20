#include <ast.hpp>

u32 LinxcTypeReferenceHash(LinxcTypeReference A)
{
    //dont bother with the full name
    u32 h1 = 7;
    if (A.lastType != NULL)
    {
        stringHash(A.lastType->name);
    }
    u32 h2 = 0;
    for (usize i = 0; i < A.templateArgs.length; i++)
    {
        h2 = LinxcTypeReferenceHash(A.templateArgs.data[i]);
        h1 = ((h1 << 5) + h1) ^ h2;
        //result *= 11 + LinxcTypeReferenceHash(A.templateArgs.data[i]);
    }
    h2 = A.pointerCount;
    h1 = ((h1 << 5) + h1) ^ h2;
    return h1;
}
u32 LinxcOperatorImplHash(LinxcOperatorImpl A)
{
    u32 h1 = LinxcTypeReferenceHash(A.myType);
    u32 h2 = 0;
    if (A.ID == LinxcOverloadIs_Operator)
    {
        h2 = A.op;
    }
    else
    {
        h2 = A.implicit ? 7 : 11;
    }
    h1 = ((h1 << 5) + h1) ^ h2;

    h2 = LinxcTypeReferenceHash(A.otherType);
    return ((h1 << 5) + h1) ^ h2;
}
bool LinxcOperatorImplEql(LinxcOperatorImpl A, LinxcOperatorImpl B)
{
    //If A is implicit and B is explicit but they resolve to the same value, then the cast is considered equal.
    //Thus, there is no need to check their implicity
    return A.myType == B.myType && A.otherType == B.otherType && A.ID == B.ID && A.op == B.op && A.implicit == B.implicit;
}
string LinxcOperatorImpl::ToString(IAllocator* allocator)
{
    if (this->ID == LinxcOverloadIs_Cast)
    {
        string str = string(allocator);
        if (this->implicit)
        {
            str.Append("implicit ");
        }
        else str.Append("explicit ");
        str.AppendDeinit(this->myType.ToString(&defaultAllocator));
        str.Append(" -> ");
        str.AppendDeinit(this->otherType.ToString(&defaultAllocator));
        //this->operation.cast.
        return str;
    }
    else
    {
        string str = string(allocator);
        str.AppendDeinit(this->myType.ToString(&defaultAllocator));
        str.Append(LinxcTokenIDToString(this->op));
        str.AppendDeinit(this->otherType.ToString(&defaultAllocator));
        return str;
    }
}
string LinxcOperatorFunc::ToString(IAllocator* allocator)
{
    string result = string(allocator);
    result.AppendDeinit(this->operatorOverride.ToString(&defaultAllocator));
    result.Append(" returns ");
    result.AppendDeinit(this->function.returnType.AsTypeReference().value.ToString(&defaultAllocator));
    return result;
}
option<LinxcTypeReference> LinxcOperator::EvaluatePossible()
{
    //if we are scope resolution operators, simply return the type of the rightmost member
    if (this->operatorType == Linxc_ColonColon || this->operatorType == Linxc_Period || this->operatorType == Linxc_Arrow)
    {
        return option<LinxcTypeReference>(this->rightExpr.resolvesTo);
    }
    LinxcOperatorImpl key;
    LinxcOperatorFunc* result = NULL;
    if (this->operatorType == Linxc_Equal)
    {
        if (this->rightExpr.resolvesTo == this->leftExpr.resolvesTo)
        {
            return option<LinxcTypeReference>(this->rightExpr.resolvesTo);
        }
        //if the operator is =, don't bother checking whether typeA = typeB, as Linxc
        //dictates that only typeA = typeA, however, a implicit cast may be performed on
        //typeB to convert it to typeA, so check that instead

        key.ID = LinxcOverloadIs_Cast;
        key.implicit = true;
        key.myType = this->rightExpr.resolvesTo;
        key.otherType = this->leftExpr.resolvesTo;
        key.op = Linxc_Invalid;

        result = this->rightExpr.resolvesTo.lastType->operatorOverloads.Get(key);
        if (result != NULL)
        {
            return option<LinxcTypeReference>(result->function.returnType.AsTypeReference());
        }
    }

    key.ID = LinxcOverloadIs_Operator;
    key.implicit = false;
    key.myType = this->leftExpr.resolvesTo;
    key.otherType = this->rightExpr.resolvesTo;
    key.op = this->operatorType;

    //op= operators dont exist and cannot be overriden, 
    //we instead check for their presence with the op itself, which can be overriden and whose
    //effects would apply to the op= operator
    switch (this->operatorType)
    {
    case Linxc_PlusEqual:
        key.op = Linxc_Plus;
        break;
    case Linxc_MinusEqual:
        key.op = Linxc_MinusEqual;
        break;
    case Linxc_AsteriskEqual:
        key.op = Linxc_Asterisk;
        break;
    case Linxc_SlashEqual:
        key.op = Linxc_Slash;
        break;
    default:
        break;
    }

    result = this->leftExpr.resolvesTo.lastType->operatorOverloads.Get(key);
    if (result != NULL)
    {
        //todo: precalculate these?
        return option<LinxcTypeReference>(result->function.returnType.AsTypeReference());
    }
    else
    {
        key.ID = LinxcOverloadIs_Operator;
        key.implicit = false;
        key.myType = this->rightExpr.resolvesTo;
        key.otherType = this->leftExpr.resolvesTo;
        key.op = this->operatorType;

        result = this->rightExpr.resolvesTo.lastType->operatorOverloads.Get(key);
        if (result != NULL)
        {
            return option<LinxcTypeReference>(result->function.returnType.AsTypeReference());
        }

        return option<LinxcTypeReference>();
    }
}

LinxcParsedFile::LinxcParsedFile()
{
    this->isLinxcH = false;
    this->mustTranspileC = false;
    this->definedFuncs = collections::vector<LinxcFunc *>();
    this->definedMacros = collections::vector<LinxcMacro>();
    //this->definedTypes = collections::vector<LinxcType *>();
    //this->definedVars = collections::vector<LinxcVar *>();
    this->errors = collections::vector<ERR_MSG>();
    this->fullPath = string();
    this->includeName = string();
    this->ast = collections::vector<LinxcStatement>();
}
LinxcParsedFile::LinxcParsedFile(IAllocator *allocator, string fullPath, string includeName)
{
    this->isLinxcH = false;
    this->mustTranspileC = false;
    this->definedFuncs = collections::vector<LinxcFunc *>(allocator);
    this->definedMacros = collections::vector<LinxcMacro>(allocator);
    //this->definedTypes = collections::vector<LinxcType *>(allocator);
    //this->definedVars = collections::vector<LinxcVar *>(allocator);
    this->errors = collections::vector<ERR_MSG>(allocator);
    this->fullPath = fullPath;
    this->includeName = includeName;
    this->ast = collections::vector<LinxcStatement>();
}

LinxcTypeReference::LinxcTypeReference()
{
    this->lastType = NULL;
    this->pointerCount = 0;
    this->isConst = false;
    this->templateArgs = collections::Array<LinxcTypeReference>();
}
LinxcTypeReference::LinxcTypeReference(LinxcType *type)
{
    this->lastType = type;
    this->pointerCount = 0;
    this->isConst = false;
    this->templateArgs = collections::Array<LinxcTypeReference>();
}
bool LinxcTypeReference::CanCastTo(LinxcTypeReference type, bool implicitly)
{
    //any pointer type can cast to void pointer type
    if (this->pointerCount > 0 && type.pointerCount > 0 && type.lastType->name == "void")
    {
        return true;
    }
    LinxcOperatorImpl castImpl;
    castImpl.ID = LinxcOverloadIs_Cast;
    castImpl.implicit = implicitly;
    castImpl.myType = *this;
    castImpl.op = Linxc_Invalid;
    castImpl.otherType = type;
    return lastType->operatorOverloads.Contains(castImpl);
}

LinxcNamespace::LinxcNamespace()
{
    this->parentNamespace = NULL;
    this->name = string();
    this->functions = collections::hashmap<string, LinxcFunc>();
    this->subNamespaces = collections::hashmap<string, LinxcNamespace>();
    this->types = collections::hashmap<string, LinxcType>();
    this->variables = collections::hashmap<string, LinxcVar>();
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
LinxcPhoneyNamespace::LinxcPhoneyNamespace()
{
    this->allocator = &defaultAllocator;
    this->parentNamespace = NULL;
    this->actualNamespace = NULL;
    this->name = string();
    this->functionRefs = collections::hashmap<string, LinxcFunc*>();
    this->subNamespaces = collections::hashmap<string, LinxcPhoneyNamespace>();
    this->typeRefs = collections::hashmap<string, LinxcType*>();
    this->variableRefs = collections::hashmap<string, LinxcVar*>();
}
LinxcPhoneyNamespace::LinxcPhoneyNamespace(IAllocator* allocator, LinxcNamespace* thisActualNamespace)
{
    this->allocator = allocator;
    this->parentNamespace = NULL;
    this->actualNamespace = thisActualNamespace;
    this->name = thisActualNamespace->name;
    this->functionRefs = collections::hashmap<string, LinxcFunc*>(allocator, &stringHash, &stringEql);
    this->subNamespaces = collections::hashmap<string, LinxcPhoneyNamespace>(allocator, &stringHash, &stringEql);
    this->typeRefs = collections::hashmap<string, LinxcType*>(allocator, &stringHash, &stringEql);
    this->variableRefs = collections::hashmap<string, LinxcVar*>(allocator, &stringHash, &stringEql);
}
void LinxcPhoneyNamespace::Add(LinxcPhoneyNamespace* other)
{
    for (usize i = 0; i < other->functionRefs.bucketsCount; i++)
    {
        if (other->functionRefs.buckets[i].initialized)
        {
            for (usize j = 0; j < other->functionRefs.buckets[i].entries.count; j++)
            {
                this->functionRefs.Add(other->functionRefs.buckets[i].entries.ptr[j].key, other->functionRefs.buckets[i].entries.ptr[j].value);
            }
        }
    }
    for (usize i = 0; i < other->typeRefs.bucketsCount; i++)
    {
        if (other->typeRefs.buckets[i].initialized)
        {
            for (usize j = 0; j < other->typeRefs.buckets[i].entries.count; j++)
            {
                this->typeRefs.Add(other->typeRefs.buckets[i].entries.ptr[j].key, other->typeRefs.buckets[i].entries.ptr[j].value);
            }
        }
    }
    for (usize i = 0; i < other->variableRefs.bucketsCount; i++)
    {
        if (other->variableRefs.buckets[i].initialized)
        {
            for (usize j = 0; j < other->variableRefs.buckets[i].entries.count; j++)
            {
                this->variableRefs.Add(other->variableRefs.buckets[i].entries.ptr[j].key, other->variableRefs.buckets[i].entries.ptr[j].value);
            }
        }
    }

    for (usize i = 0; i < other->subNamespaces.bucketsCount; i++)
    {
        if (other->subNamespaces.buckets[i].initialized)
        {
            for (usize j = 0; j < other->subNamespaces.buckets[i].entries.count; j++)
            {
                LinxcPhoneyNamespace* phoneyNamespaceEquivalent = this->subNamespaces.Get(other->subNamespaces.buckets[i].entries.ptr[j].key);
                if (phoneyNamespaceEquivalent != NULL)
                {
                    phoneyNamespaceEquivalent->Add(&other->subNamespaces.buckets[i].entries.ptr[j].value);
                }
                else
                {
                    //clone it, if not, any modification to this namespace would modify the other as well
                    LinxcPhoneyNamespace newEquivalent = LinxcPhoneyNamespace(this->allocator, this->actualNamespace);
                    newEquivalent.Add(&other->subNamespaces.buckets[i].entries.ptr[j].value);
                    this->subNamespaces.Add(other->subNamespaces.buckets[i].entries.ptr[j].key, newEquivalent);
                }
            }
        }
    }
}
LinxcNamespaceScope::LinxcNamespaceScope()
{
    this->body = collections::vector<LinxcStatement>();
    this->referencedNamespace = NULL;
}
LinxcIncludeStatement::LinxcIncludeStatement()
{
    this->includedFile = NULL;
    this->includeString = string();
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
    this->enumMembers = collections::vector<LinxcEnumMember>();
    this->operatorOverloads = collections::hashmap<LinxcOperatorImpl, LinxcOperatorFunc>();
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
    this->enumMembers = collections::vector<LinxcEnumMember>(allocator);
    this->operatorOverloads = collections::hashmap<LinxcOperatorImpl, LinxcOperatorFunc>(allocator, &LinxcOperatorImplHash, &LinxcOperatorImplEql);
}
string LinxcType::GetFullName(IAllocator *allocator)
{
    string result = string();
    result.allocator = allocator;

    LinxcNamespace *currentNamespace = this->typeNamespace;
    while (currentNamespace != NULL && currentNamespace->name.buffer != NULL)
    {
        result.Prepend("::");
        result.Prepend(currentNamespace->name.buffer);
        currentNamespace = currentNamespace->parentNamespace;
    }
    result.Append(this->name.buffer);
    
    return result;
}
string LinxcType::GetCName(IAllocator* allocator)
{
    string result = string(&defaultAllocator);

    LinxcType* currentParentType = this->parentType;
    while (currentParentType != NULL)
    {
        if (currentParentType->name.buffer != NULL)
        {
            result.Prepend("_");
            result.Prepend(currentParentType->name.buffer);
        }
        currentParentType = currentParentType->parentType;
    }

    LinxcNamespace* currentNamespace = this->typeNamespace;
    while (currentNamespace != NULL)
    {
        if (currentNamespace->name.buffer != NULL)
        {
            result.Prepend("_");
            result.Prepend(currentNamespace->name.buffer);
        }
        currentNamespace = currentNamespace->parentNamespace;
    }
    result.Append(this->name.buffer);

    return result.CloneDeinit(allocator);
}
LinxcEnumMember* LinxcType::FindEnumMember(string name)
{
    for (usize i = 0; i < this->enumMembers.count; i++)
    {
        if (this->enumMembers.Get(i)->name.eql(name.buffer))
        {
            return this->enumMembers.Get(i);
        }
    }
    return NULL;
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
LinxcExpression LinxcType::AsExpression()
{
    LinxcExpression expr;
    expr.resolvesTo.lastType = NULL;
    expr.data.typeRef = LinxcTypeReference(this);
    expr.ID = LinxcExpr_TypeRef;

    return expr;
}

LinxcVar::LinxcVar()
{
    this->isConst = false;
    this->name = string();
    this->type = LinxcExpression();
    this->defaultValue = option<LinxcExpression>();
    this->memberOf = NULL;
}
LinxcVar::LinxcVar(string varName, LinxcExpression varType, option<LinxcExpression> defaultVal)
{
    this->isConst = false;
    this->name = varName;
    this->type = varType;
    this->defaultValue = defaultVal;
    this->memberOf = NULL;
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
    this->necessaryArguments = 0;
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
    this->necessaryArguments = 0;
}
string LinxcFunc::GetCName(IAllocator *allocator)
{
    string result = string(&defaultAllocator);

    if (this->methodOf != NULL)
    {
        result.Prepend("_");
        string typeCName = this->methodOf->GetCName(&defaultAllocator);
        result.Prepend(typeCName.buffer);
        typeCName.deinit();
    }
    else
    {
        LinxcNamespace* currentNamespace = this->funcNamespace;
        while (currentNamespace != NULL && currentNamespace->name.buffer != NULL)
        {
            result.Prepend("_");
            result.Prepend(currentNamespace->name.buffer);
            currentNamespace = currentNamespace->parentNamespace;
        }
    }
    result.Append(this->name.buffer);

    return result.CloneDeinit(allocator);
}

string LinxcFunctionCall::ToString(IAllocator *allocator)
{
    string result = string(&defaultAllocator);
    result.Append(this->func->name.buffer);
    result.Append("(");
    for (int i = 0; i < this->inputParams.length; i++)
    {
        result.AppendDeinit(this->inputParams.data[i].ToString(&defaultAllocator));
        if (i < this->inputParams.length - 1)
        {
            result.Append(", ");
        }
    }
    result.Append(")");
    return result.CloneDeinit(allocator);
}

string LinxcTypeReference::ToString(IAllocator *allocator)
{
    string result = string(&defaultAllocator);
    if (this->isConst)
    {
        result.Append("const ");
    }
    result.AppendDeinit(this->lastType->GetFullName(&defaultAllocator));
    for (i32 i = 0; i < this->pointerCount; i++)
    {
        result.Append("*");
    }
    return result.CloneDeinit(allocator);
}
string LinxcTypeReference::GetCName(IAllocator* allocator)
{
    string result = string(&defaultAllocator);
    if (this->isConst)
    {
        result.Append("const ");
    }
    result.AppendDeinit(this->lastType->GetCName(&defaultAllocator));
    for (i32 i = 0; i < this->pointerCount; i++)
    {
        result.Append("*");
    }
    return result.CloneDeinit(allocator);
}

LinxcExpressionData::LinxcExpressionData()
{
    this->operatorCall = NULL;
}
string LinxcExpression::ToString(IAllocator *allocator)
{
    switch (this->ID)
    {
        case LinxcExpr_FuncCall:
        {
            return this->data.functionCall.ToString(allocator);
        }
        case LinxcExpr_FunctionRef:
        {
            return string(allocator, this->data.functionRef->name.buffer);
        }
        case LinxcExpr_Literal:
        {
            return string(allocator, this->data.literal.buffer);
        }
        case LinxcExpr_Modified:
        {
            string result = string(&defaultAllocator);
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
            return result.CloneDeinit(allocator);
        }
        case LinxcExpr_Nameof:
        {
            string result = string(&defaultAllocator);
            result.Append("nameof(");
            result.AppendDeinit(this->data.nameofCall.ToString(&defaultAllocator));
            result.Append(")");
            return result.CloneDeinit(allocator);
        }
        case LinxcExpr_OperatorCall:
        {
            string result = string(&defaultAllocator);
            result.Append("(");
            result.AppendDeinit(this->data.operatorCall->leftExpr.ToString(&defaultAllocator));
            result.Append(LinxcTokenIDToString(this->data.operatorCall->operatorType));
            result.AppendDeinit(this->data.operatorCall->rightExpr.ToString(&defaultAllocator));
            result.Append(")");
            return result.CloneDeinit(allocator);
        }
        case LinxcExpr_Sizeof:
        {
            string result = string(&defaultAllocator);
            result.Append("sizeof(");
            result.AppendDeinit(this->data.sizeofCall.ToString(&defaultAllocator));
            result.Append(")");
            return result.CloneDeinit(allocator);
        }
        case LinxcExpr_TypeCast:
        {
            string result = string(&defaultAllocator);
            result.Append("(");
            result.AppendDeinit(this->data.typeCast->castToType.ToString(&defaultAllocator));
            result.Append(")");
            result.AppendDeinit(this->data.typeCast->expressionToCast.ToString(&defaultAllocator));
            
            return result.CloneDeinit(allocator);
        }
        case LinxcExpr_Typeof:
        {
            string result = string(&defaultAllocator);
            result.Append("typeof(");
            result.AppendDeinit(this->data.typeofCall.ToString(&defaultAllocator));
            result.Append(")");
            return result.CloneDeinit(allocator);
        }
        case LinxcExpr_TypeRef:
            return this->data.typeRef.ToString(allocator);
        case LinxcExpr_Variable:
            return this->data.variable->ToString(allocator);
        default:
            return string("NULL");
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
        return option<LinxcTypeReference>(this->data.typeRef);
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
    this->includeStatement = LinxcIncludeStatement();
}
string LinxcStatement::ToString(IAllocator *allocator)
{
    switch (this->ID)
    {
    case LinxcStmt_Include:
    {
        string result = string(allocator, "#include <");
        result.Append(this->data.includeStatement.includeString.buffer);
        result.Append(">");
        return result;
    }
    case LinxcStmt_Expr:
    {
        return this->data.expression.ToString(allocator);
    }
    case LinxcStmt_Return:
    {
        string result = string(allocator, "return ");
        result.AppendDeinit(this->data.returnStatement.ToString(&defaultAllocator));
        return result;
    }
    case LinxcStmt_FuncDecl:
    {
        string result = string(allocator);
        result.AppendDeinit(this->data.funcDeclaration->returnType.ToString(&defaultAllocator));
        result.Append(" ");
        result.Append(this->data.funcDeclaration->name.buffer);
        result.Append("(");
        for (usize i = 0; i < this->data.funcDeclaration->arguments.length; i++)
        {
            LinxcVar* var = &this->data.funcDeclaration->arguments.data[i];
            result.AppendDeinit(var->type.ToString(&defaultAllocator));
            result.Append(" ");
            result.Append(var->name.buffer);
            if (var->defaultValue.present)
            {
                result.Append(" = ");
                result.AppendDeinit(var->defaultValue.value.ToString(&defaultAllocator));
            }
            if (i < this->data.funcDeclaration->arguments.length - 1)
            {
                result.Append(", ");
            }
        }
        result.Append(") {\n");

        for (usize i = 0; i < this->data.funcDeclaration->body.count; i++)
        {
            result.AppendDeinit(this->data.funcDeclaration->body.ptr[i].ToString(&defaultAllocator));
            result.Append("\n");
        }
        result.Append("}");
        return result;
    }
    case LinxcStmt_Namespace:
    {
        string result = string(&defaultAllocator);
        result.Append("namespace ");
        result.Append(" {\n");
        for (usize i = 0; i < this->data.namespaceScope.body.count; i++)
        {
            result.AppendDeinit(this->data.namespaceScope.body.Get(i)->ToString(&defaultAllocator));
            result.Append("\n");
        }
        result.Append("}");
        //this->data.namespaceScope
        return result.CloneDeinit(allocator);
    }
    /*case LinxcStmt_TempVarDecl:
    {
        string result = string(allocator);
        LinxcVar* var = &this->data.tempVarDeclaration;
        if (var->isConst)
        {
            result.Append("const ");
        }
        result.AppendDeinit(var->type.ToString(&defaultAllocator));
        result.Append(" ");
        result.Append(var->name.buffer);
        if (var->defaultValue.present)
        {
            result.Append(" = ");
            result.AppendDeinit(var->defaultValue.value.ToString(&defaultAllocator));
        }
        result.Append(";");
        return result;
    }*/
    case LinxcStmt_TypeDecl:
    {
        string result = string(allocator, "struct ");
        result.Append(this->data.typeDeclaration->name.buffer);
        result.Append(" { \n");
        for (usize i = 0; i < this->data.typeDeclaration->body.count; i++)
        {
            result.AppendDeinit(this->data.typeDeclaration->body.ptr[i].ToString(&defaultAllocator));
            result.Append("\n");
        }
        result.Append("}");
        return result;
    }
    case LinxcStmt_VarDecl:
    {
        string result = string(allocator);
        LinxcVar* var = this->data.varDeclaration;
        if (var->isConst)
        {
            result.Append("const ");
        }
        result.AppendDeinit(var->type.ToString(&defaultAllocator));
        result.Append(" ");
        result.Append(var->name.buffer);
        if (var->defaultValue.present)
        {
            result.Append(" = ");
            result.AppendDeinit(var->defaultValue.value.ToString(&defaultAllocator));
        }
        result.Append(";");
        return result;
    }
    }
}