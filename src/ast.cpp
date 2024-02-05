#include <ast.hpp>

bool LinxcTemplateSpecializationsEql(TemplateSpecialization A, TemplateSpecialization B)
{
    if (A.length != B.length)
    {
        return false;
    }
    for (usize i = 0; i < A.length; i++)
    {
        if (A.data[i] != B.data[i])
        {
            return false;
        }
    }
    return true;
}
u32 LinxcTemplateSpecializationsHash(TemplateSpecialization A)
{
    u32 h1 = 7;
    for (usize i = 0; i < A.length; i++)
    {
        u32 h2 = LinxcTypeReferenceHash(A.data[i]);
        h1 = ((h1 << 5) + h1) ^ h2;
    }
    return h1;
}
bool LinxcTypeReferenceEql(LinxcTypeReference A, LinxcTypeReference B)
{
    if (A.lastType != B.lastType || A.isConst != B.isConst || A.pointerCount != B.pointerCount || A.templateArgs.length != B.templateArgs.length)
    {
        return false;
    }
    for (usize i = 0; i < A.templateArgs.length; i++)
    {
        if (A.templateArgs.data[i].AsTypeReference().value != B.templateArgs.data[i].AsTypeReference().value)
        {
            return false;
        }
    }
    return true;
}
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
        h2 = LinxcTypeReferenceHash(A.templateArgs.data[i].AsTypeReference().value);
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
        str.AppendDeinit(this->myType.ToString(GetDefaultAllocator()));
        str.Append(" -> ");
        str.AppendDeinit(this->otherType.ToString(GetDefaultAllocator()));
        //this->operation.cast.
        return str;
    }
    else
    {
        string str = string(allocator);
        str.AppendDeinit(this->myType.ToString(GetDefaultAllocator()));
        str.Append(LinxcTokenIDToString(this->op));
        str.AppendDeinit(this->otherType.ToString(GetDefaultAllocator()));
        return str;
    }
}
string LinxcOperatorFunc::ToString(IAllocator* allocator)
{
    string result = string(allocator);
    result.AppendDeinit(this->operatorOverride.ToString(GetDefaultAllocator()));
    result.Append(" returns ");
    result.AppendDeinit(this->function.returnType.AsTypeReference().value.ToString(GetDefaultAllocator()));
    return result;
}
option<LinxcTypeReference> LinxcOperator::EvaluatePossible()
{
    //if we are scope resolution operators, simply return the type of the rightmost member
    if (this->operatorType == Linxc_ColonColon || this->operatorType == Linxc_Period || this->operatorType == Linxc_Arrow)
    {
        return option<LinxcTypeReference>(this->rightExpr.resolvesTo);
    }
    if (this->leftExpr.resolvesTo.isConst)
    {
        return option<LinxcTypeReference>();
    }
    //TODO: Traits and stuff, maybe
    if (this->leftExpr.resolvesTo.genericTypeName != NULL || this->rightExpr.resolvesTo.genericTypeName != NULL)
    {
        return option<LinxcTypeReference>();
    }
    LinxcOperatorImpl key;
    LinxcOperatorFunc* result = NULL;
    if (this->operatorType == Linxc_Equal)
    {
        if (this->leftExpr.resolvesTo == this->rightExpr.resolvesTo)
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
    this->templateArgs = collections::Array<LinxcExpression>();
}
LinxcTypeReference::LinxcTypeReference(LinxcType *type)
{
    this->lastType = type;
    this->pointerCount = 0;
    this->isConst = false;
    this->templateArgs = collections::Array<LinxcExpression>();
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
bool LinxcTypeReference::operator==(LinxcTypeReference B)
{
    if (this->genericTypeName == B.genericTypeName.buffer && this->genericTypeName.buffer != NULL)
    {
        return true;
    }
    //if we are both function pointers, compare the signature
    if (this->lastType->delegateDecl.name.buffer != NULL && B.lastType->delegateDecl.name.buffer != NULL)
    {
        bool result = this->lastType->delegateDecl == B.lastType->delegateDecl;
        return result;
    }
    if (this->lastType != B.lastType || this->isConst != B.isConst || this->pointerCount != B.pointerCount)// || (this->templateArgs.length != B.templateArgs.length && this->lastType->templateArgs.length != B.lastType->templateArgs.length))
    {
        return false;
    }
    //we only compare the two references' template args if the type actually has templates.
    //This condition may be false if we are comparing the type of a variable in a specialized type,
    //which would retain it's template Args for transpilation but in actuality is already transpiled.
    if (this->lastType->templateArgs.length == B.lastType->templateArgs.length)
    {
        for (usize i = 0; i < this->lastType->templateArgs.length; i++)
        {
            if (this->templateArgs.data[i].AsTypeReference().value != B.templateArgs.data[i].AsTypeReference().value)
            {
                return false;
            }
        }
    }
    return true;
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
    this->allocator = GetDefaultAllocator();
    this->parentNamespace = NULL;
    this->actualNamespace = NULL;
    this->name = string();
    this->functionRefs = collections::hashmap<string, LinxcFunc*>();
    this->subNamespaces = collections::hashmap<string, LinxcPhoneyNamespace>();
    this->typeRefs = collections::hashmap<string, LinxcType*>();
    this->variableRefs = collections::hashmap<string, LinxcVar*>();
    this->typedefs = collections::hashmap<string, LinxcTypeReference>();
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
    this->typedefs = collections::hashmap<string, LinxcTypeReference>(allocator, &stringHash, &stringEql);
}
void LinxcPhoneyNamespace::Add(LinxcPhoneyNamespace* other)
{
    for (usize i = 0; i < other->functionRefs.bucketsCount; i++)
    {
        if (other->functionRefs.buckets[i].initialized)
        {
            for (usize j = 0; j < other->functionRefs.buckets[i].entries.count; j++)
            {
                LinxcFunc* func = other->functionRefs.buckets[i].entries.ptr[j].value;
                this->functionRefs.Add(other->functionRefs.buckets[i].entries.ptr[j].key, func);
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
    for (usize i = 0; i < other->typedefs.bucketsCount; i++)
    {
        if (other->typedefs.buckets[i].initialized)
        {
            for (usize j = 0; j < other->typedefs.buckets[i].entries.count; j++)
            {
                this->typedefs.Add(other->typedefs.buckets[i].entries.ptr[j].key, other->typedefs.buckets[i].entries.ptr[j].value);
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
                    LinxcPhoneyNamespace newEquivalent = LinxcPhoneyNamespace(this->allocator, other->subNamespaces.buckets[i].entries.ptr[j].value.actualNamespace);
                    newEquivalent.Add(&other->subNamespaces.buckets[i].entries.ptr[j].value);
                    this->subNamespaces.Add(newEquivalent.name, newEquivalent);
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
    this->isSpecializedType = TemplateSpecialization();
    this->delegateDecl = LinxcFuncPtr();
    this->name = string();
    this->parentType = NULL;
    this->typeNamespace = NULL;
    this->functions = collections::vector<LinxcFunc>();
    this->subTypes = collections::vector<LinxcType>();
    this->templateArgs = collections::Array<string>();
    this->variables = collections::vector<LinxcVar>();
    this->enumMembers = collections::vector<LinxcEnumMember>();
    this->operatorOverloads = collections::hashmap<LinxcOperatorImpl, LinxcOperatorFunc>();
}
LinxcType::LinxcType(IAllocator *allocator, string name, LinxcNamespace *myNamespace, LinxcType *myParent)
{
    this->isSpecializedType = TemplateSpecialization();
    this->delegateDecl = LinxcFuncPtr();
    this->body = collections::vector<LinxcStatement>();
    this->name = name;
    this->parentType = myParent;
    this->typeNamespace = myNamespace;
    this->functions = collections::vector<LinxcFunc>(allocator);
    this->subTypes = collections::vector<LinxcType>(allocator);
    this->templateArgs = collections::Array<string>();
    this->variables = collections::vector<LinxcVar>(allocator);
    this->enumMembers = collections::vector<LinxcEnumMember>(allocator);
    this->templateSpecializations = collections::hashmap<TemplateSpecialization, LinxcType>(allocator, &LinxcTemplateSpecializationsHash, &LinxcTemplateSpecializationsEql);
    this->operatorOverloads = collections::hashmap<LinxcOperatorImpl, LinxcOperatorFunc>(allocator, &LinxcOperatorImplHash, &LinxcOperatorImplEql);
}
LinxcType LinxcType::Specialize(IAllocator *allocator, collections::Array<string> inputArgs, TemplateSpecialization specialization)
{
    if (this->enumMembers.count > 0)
    {
        return {};
    }
    else if (this->delegateDecl.name.buffer != NULL)
    {
        LinxcFuncPtr newDelegate = this->delegateDecl;
        newDelegate.arguments = collections::Array<LinxcExpression>(allocator, this->delegateDecl.arguments.length);

        for (usize i = 0; i < this->delegateDecl.arguments.length; i++)
        {
            newDelegate.arguments.data[i] = this->delegateDecl.arguments.data[i].SpecializeSignature(allocator, inputArgs, specialization);
            printf("Specialized delegate arg of generic type %s as type %s\n", this->delegateDecl.arguments.data[i].AsTypeReference().value.genericTypeName.buffer, newDelegate.arguments.data[i].AsTypeReference().value.lastType->name.buffer);
        }

        LinxcExpression* newReturnTypePtr = (LinxcExpression*)allocator->Allocate(sizeof(LinxcExpression));
        *newReturnTypePtr = this->delegateDecl.returnType->SpecializeSignature(allocator, inputArgs, specialization);

        newDelegate.returnType = newReturnTypePtr;

        LinxcType clone = *this;

        clone.delegateDecl = newDelegate;
        clone.isSpecializedType = specialization;
        return clone;
    }
    else
    {
        LinxcType clone = LinxcType(allocator, this->name, this->typeNamespace, this->parentType);
        clone.templateSpecializations = this->templateSpecializations; //make sure if anyone tries to 
        clone.isSpecializedType = specialization;
        clone.body = this->body;
        for (usize i = 0; i < this->subTypes.count; i++)
        {
            //must specialize all subtypes
            clone.subTypes.Add(this->subTypes.ptr[i].Specialize(allocator, inputArgs, specialization));
        }
        for (usize i = 0; i < this->variables.count; i++)
        {
            LinxcVar specializedVariable = this->variables.ptr[i];
            //keep memberOf the same
            specializedVariable.type = specializedVariable.type.SpecializeSignature(allocator, inputArgs, specialization);
            clone.variables.Add(specializedVariable);
        }
        for (usize i = 0; i < this->functions.count; i++)
        {
            clone.functions.Add(this->functions.ptr[i].SpecializeSignature(allocator, inputArgs, specialization));
        }
        for (usize i = 0; i < this->operatorOverloads.bucketsCount; i++)
        {
            if (this->operatorOverloads.buckets[i].initialized)
            {
                for (usize j = 0; j < this->operatorOverloads.buckets[i].entries.count; j++)
                {
                    LinxcOperatorImpl impl = this->operatorOverloads.buckets[i].entries.ptr[j].key;
                    LinxcOperatorFunc* originalPtr = &this->operatorOverloads.buckets[i].entries.ptr[j].value;
                    
                    LinxcOperatorFunc specializedOp;
                    specializedOp.operatorOverride = originalPtr->operatorOverride;
                    specializedOp.function = originalPtr->function.SpecializeSignature(allocator, inputArgs, specialization);

                    clone.operatorOverloads.Add(impl, specializedOp);
                }
            }
        }
        return clone;
    }
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
    string result = string(GetDefaultAllocator());

    if (this->parentType != NULL)
    {
        result.AppendDeinit(this->parentType->GetCName(GetDefaultAllocator()));
        result.Append("_");
    }
    else
    {
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
    }

    result.Append(this->name.buffer);

    for (usize i = 0; i < this->isSpecializedType.length; i++)
    {
        result.Append("_");
        string cName = this->isSpecializedType.data[i].GetCName(GetDefaultAllocator(), true);
        result.AppendDeinit(cName);
    }

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

LinxcFuncPtr::LinxcFuncPtr()
{
    this->arguments = collections::Array<LinxcExpression>();
    this->name = string();
    this->necessaryArguments = 0;
    this->returnType = NULL;
}
LinxcFuncPtr::LinxcFuncPtr(string name, LinxcExpression* returnType)
{
    this->arguments = collections::Array<LinxcExpression>();
    this->name = name;
    this->necessaryArguments = 0;
    this->returnType = returnType;
}
bool LinxcFuncPtr::operator==(LinxcFuncPtr B)
{
    if (this->necessaryArguments == B.necessaryArguments && this->arguments.length == B.arguments.length && this->returnType->AsTypeReference().value == B.returnType->AsTypeReference().value)
    {
        for (usize i = 0; i < this->arguments.length; i++)
        {
            LinxcTypeReference ARef = this->arguments.data[i].AsTypeReference().value;
            LinxcTypeReference BRef = B.arguments.data[i].AsTypeReference().value;
            if (ARef != BRef)
            {
                return false;
            }
        }
        return true;
    }
    return false;
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
    this->asType = LinxcType();
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
    this->asType = LinxcType();
}
LinxcFuncPtr LinxcFunc::GetSignature(IAllocator *allocator)
{
    LinxcFuncPtr result = LinxcFuncPtr(this->name, &this->returnType);
    result.necessaryArguments = this->necessaryArguments;
    result.arguments = collections::Array<LinxcExpression>(allocator, this->arguments.length);
    for (usize i = 0; i < this->arguments.length; i++)
    {
        result.arguments.data[i] = this->arguments.data[i].type;
    }
    return result;
}
LinxcFunc LinxcFunc::SpecializeSignature(IAllocator* allocator, collections::Array<string> templateArgs, TemplateSpecialization specialization)
{
    LinxcFunc specializedFunc = LinxcFunc(this->name, this->returnType.SpecializeSignature(allocator, templateArgs, specialization));
    specializedFunc.methodOf = this->methodOf; //may not be equivalent to 'this' if we are transpiling generic subtype of a generic type
    specializedFunc.arguments = collections::Array<LinxcVar>(allocator, this->arguments.length);
    specializedFunc.body = this->body;
    for (usize i = 0; i < this->arguments.length; i++)
    {
        specializedFunc.arguments.data[i] = this->arguments.data[i];
        specializedFunc.arguments.data[i].type = specializedFunc.arguments.data[i].type.SpecializeSignature(allocator, templateArgs, specialization);
    }
    return specializedFunc;
}
string LinxcFunc::GetCName(IAllocator *allocator)//, collections::Array<string> scopeTemplateArgs, collections::Array<LinxcTypeReference> scopeTemplateSpecializations)
{
    string result = string(GetDefaultAllocator());

    /*for (usize i = 0; i < templateSpecializations.length; i++)
    {
        //TODO
        result.AppendDeinit(templateSpecializations.data[i].GetCName(GetDefaultAllocator(), true));
        result.Append("_");
    }*/

    if (this->methodOf != NULL)
    {
        result.Prepend("_");
        string typeCName = this->methodOf->GetCName(GetDefaultAllocator());
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
    string result = string(GetDefaultAllocator());
    result.Append(this->func->name.buffer);
    result.Append("(");
    for (int i = 0; i < this->inputParams.length; i++)
    {
        result.AppendDeinit(this->inputParams.data[i].ToString(GetDefaultAllocator()));
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
    string result = string(GetDefaultAllocator());
    if (this->isConst)
    {
        result.Append("const ");
    }
    if (this->lastType != NULL)
    {
        result.AppendDeinit(this->lastType->GetFullName(GetDefaultAllocator()));
    }
    else
    {
        result.AppendDeinit(this->genericTypeName);
    }
    for (i32 i = 0; i < this->pointerCount; i++)
    {
        result.Append("*");
    }
    return result.CloneDeinit(allocator);
}
string LinxcTypeReference::GetCName(IAllocator* allocator, bool pointerAsPtr, collections::Array<string> scopeTemplateArgs, TemplateSpecialization scopeTemplateSpecializations)
{
    string result = string(GetDefaultAllocator());
    if (this->isConst)
    {
        result.Append("const ");
    }
    if (this->genericTypeName.buffer != NULL)
    {
        option<usize> findResult = scopeTemplateArgs.Contains(this->genericTypeName, &stringEql);
        if (findResult.present)
        {
            //Not sure if wise to pass templateArgs, templateSpecializations to this
            result.AppendDeinit(scopeTemplateSpecializations.data[findResult.value].GetCName(GetDefaultAllocator(), true, scopeTemplateArgs, scopeTemplateSpecializations));
        }
    }
    else
    {
        string cName = this->lastType->GetCName(GetDefaultAllocator());
        result.AppendDeinit(cName);
    }// result.AppendDeinit(this->lastType->GetCName(GetDefaultAllocator()));
    for (usize i = 0; i < this->templateArgs.length; i++)
    {
        result.Append("_");
        string cName = this->templateArgs.data[i].AsTypeReference().value.GetCName(GetDefaultAllocator(), true, scopeTemplateArgs, scopeTemplateSpecializations);
        result.AppendDeinit(cName);
    }
    if (!pointerAsPtr) //pointer as *
    {
        for (i32 i = 0; i < this->pointerCount; i++)
        {
            result.Append("*");
        }
    }
    else
    {
        for (i32 i = 0; i < this->pointerCount; i++)
        {
            result.Append("ptr");
        }
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
            string result = string(GetDefaultAllocator());
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
            result.AppendDeinit(this->data.modifiedExpression->expression.ToString(GetDefaultAllocator()));
            return result.CloneDeinit(allocator);
        }
        case LinxcExpr_Nameof:
        {
            string result = string(GetDefaultAllocator());
            result.Append("nameof(");
            result.AppendDeinit(this->data.nameofCall.ToString(GetDefaultAllocator()));
            result.Append(")");
            return result.CloneDeinit(allocator);
        }
        case LinxcExpr_OperatorCall:
        {
            string result = string(GetDefaultAllocator());
            result.Append("(");
            result.AppendDeinit(this->data.operatorCall->leftExpr.ToString(GetDefaultAllocator()));
            result.Append(LinxcTokenIDToString(this->data.operatorCall->operatorType));
            result.AppendDeinit(this->data.operatorCall->rightExpr.ToString(GetDefaultAllocator()));
            result.Append(")");
            return result.CloneDeinit(allocator);
        }
        case LinxcExpr_Sizeof:
        {
            string result = string(GetDefaultAllocator());
            result.Append("sizeof(");
            result.AppendDeinit(this->data.sizeofCall.ToString(GetDefaultAllocator()));
            result.Append(")");
            return result.CloneDeinit(allocator);
        }
        case LinxcExpr_TypeCast:
        {
            string result = string(GetDefaultAllocator());
            result.Append("(");
            result.AppendDeinit(this->data.typeCast->castToType.ToString(GetDefaultAllocator()));
            result.Append(")");
            result.AppendDeinit(this->data.typeCast->expressionToCast.ToString(GetDefaultAllocator()));
            
            return result.CloneDeinit(allocator);
        }
        case LinxcExpr_Typeof:
        {
            string result = string(GetDefaultAllocator());
            result.Append("typeof(");
            result.AppendDeinit(this->data.typeofCall.ToString(GetDefaultAllocator()));
            result.Append(")");
            return result.CloneDeinit(allocator);
        }
        case LinxcExpr_TypeRef:
            return this->data.typeRef.ToString(allocator);
        case LinxcExpr_Variable:
            return this->data.variable->ToString(allocator);
        default:
            return string(allocator, "NULL");
    }
}
LinxcExpression LinxcExpression::SpecializeSignature(IAllocator *allocator, collections::Array<string> args, TemplateSpecialization with)
{
    LinxcExpression newExpr;
    newExpr.data.typeRef = LinxcTypeReference(NULL);
    newExpr.ID = LinxcExpr_TypeRef;

    if (this->ID == LinxcExpr_OperatorCall)
    {
        LinxcExpression newLeft = this->data.operatorCall->leftExpr.SpecializeSignature(allocator, args, with);
        LinxcExpression newRight = this->data.operatorCall->rightExpr.SpecializeSignature(allocator, args, with);

        LinxcExpression newExpr;
        newExpr.ID = LinxcExpr_OperatorCall;
        newExpr.data.operatorCall->leftExpr = newLeft;
        newExpr.data.operatorCall->operatorType = this->data.operatorCall->operatorType;
        newExpr.data.operatorCall->rightExpr = newRight;
        if (newLeft.resolvesTo != NULL || newRight.resolvesTo != NULL)
        {
            //throw error
        }
        newExpr.resolvesTo = NULL;
        return newExpr;
    }
    if (this->ID == LinxcExpr_TypeRef)
    {
        if (this->data.typeRef.genericTypeName.buffer != NULL)
        {
            option<usize> index = args.Contains(this->data.typeRef.genericTypeName, &stringEql);
            if (index.present)
            {
                LinxcExpression newExpr;
                newExpr.ID = LinxcExpr_TypeRef;
                newExpr.data.typeRef = with.data[index.value];
                newExpr.data.typeRef.pointerCount = this->data.typeRef.pointerCount;
                newExpr.resolvesTo.lastType = NULL;

                return newExpr;
            }
            else return *this;
        }
        else if (this->data.typeRef.templateArgs.length > 0)
        {
            LinxcExpression newExpr;
            newExpr.ID = LinxcExpr_TypeRef;
            newExpr.data.typeRef = this->data.typeRef;
            newExpr.resolvesTo.lastType = NULL;

            newExpr.data.typeRef.templateArgs = collections::Array<LinxcExpression>(allocator, this->data.typeRef.templateArgs.length);
            for (usize i = 0; i < this->data.typeRef.templateArgs.length; i++)
            {
                newExpr.data.typeRef.templateArgs.data[i] = this->data.typeRef.templateArgs.data[i].SpecializeSignature(allocator, args, with);
            }

            return newExpr;
        }
    }
    //don't be concerned about function calls since specialization only cares about the header and signature
    //of types so as to perform type checking, not the contents, such as FuncCall
    /*if (this->ID == LinxcExpr_FuncCall)
    {

    }*/
    return *this;
}
option<LinxcFunc*> LinxcExpression::AsFuncReference()
{
    //cannot check this as a FuncRef by default resolves to the return type
    /*if (this->resolvesTo.lastType != NULL)
    {
        return option<LinxcFunc*>();
    }*/
    if (this->ID == LinxcExpr_FunctionRef)
    {
        return option<LinxcFunc*>(this->data.functionRef);
    }
    if (this->ID == LinxcExpr_OperatorCall)
    {
        return this->data.operatorCall->rightExpr.AsFuncReference();
    }
    return option<LinxcFunc*>();
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
        result.AppendDeinit(this->data.returnStatement.ToString(GetDefaultAllocator()));
        return result;
    }
    case LinxcStmt_FuncDecl:
    {
        string result = string(allocator);
        result.AppendDeinit(this->data.funcDeclaration->returnType.ToString(GetDefaultAllocator()));
        result.Append(" ");
        result.Append(this->data.funcDeclaration->name.buffer);
        result.Append("(");
        for (usize i = 0; i < this->data.funcDeclaration->arguments.length; i++)
        {
            LinxcVar* var = &this->data.funcDeclaration->arguments.data[i];
            result.AppendDeinit(var->type.ToString(GetDefaultAllocator()));
            result.Append(" ");
            result.Append(var->name.buffer);
            if (var->defaultValue.present)
            {
                result.Append(" = ");
                result.AppendDeinit(var->defaultValue.value.ToString(GetDefaultAllocator()));
            }
            if (i < this->data.funcDeclaration->arguments.length - 1)
            {
                result.Append(", ");
            }
        }
        result.Append(") {\n");

        for (usize i = 0; i < this->data.funcDeclaration->body.count; i++)
        {
            result.AppendDeinit(this->data.funcDeclaration->body.ptr[i].ToString(GetDefaultAllocator()));
            result.Append("\n");
        }
        result.Append("}");
        return result;
    }
    case LinxcStmt_Namespace:
    {
        string result = string(GetDefaultAllocator());
        result.Append("namespace ");
        result.Append(" {\n");
        for (usize i = 0; i < this->data.namespaceScope.body.count; i++)
        {
            result.AppendDeinit(this->data.namespaceScope.body.Get(i)->ToString(GetDefaultAllocator()));
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
        result.AppendDeinit(var->type.ToString(GetDefaultAllocator()));
        result.Append(" ");
        result.Append(var->name.buffer);
        if (var->defaultValue.present)
        {
            result.Append(" = ");
            result.AppendDeinit(var->defaultValue.value.ToString(GetDefaultAllocator()));
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
            result.AppendDeinit(this->data.typeDeclaration->body.ptr[i].ToString(GetDefaultAllocator()));
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
        result.AppendDeinit(var->type.ToString(GetDefaultAllocator()));
        result.Append(" ");
        result.Append(var->name.buffer);
        if (var->defaultValue.present)
        {
            result.Append(" = ");
            result.AppendDeinit(var->defaultValue.value.ToString(GetDefaultAllocator()));
        }
        result.Append(";");
        return result;
    }
    }
}