#include <reflectC.hpp>

LinxcCursor::LinxcCursor(CXCursor thisCursor, collections::vector<LinxcCursor> thisBody)
{
    this->cursor = thisCursor;
    this->kind = clang_getCursorKind(this->cursor);
    this->body = thisBody;
}
void LinxcCursor::deinit()
{
    this->body.deinit();
}
string LinxcCursor::GetSpelling(IAllocator* allocator)
{
    string result;
    CXString str = clang_getCursorSpelling(this->cursor);
    result = string(allocator, clang_getCString(str));
    clang_disposeString(str);
    return result;
}
string LinxcCursor::ToString(IAllocator* allocator, i32 whiteSpaces)
{
    string myWhitespaces = string(&defaultAllocator);
    if (whiteSpaces > 0)
    {
        myWhitespaces.buffer = (char*)defaultAllocator.Allocate(sizeof(char) * (whiteSpaces + 2));
        myWhitespaces.length = whiteSpaces + 2;
        for (usize i = 0; i < whiteSpaces + 1; i++)
        {
            myWhitespaces.buffer[i] = ' ';
        }
        myWhitespaces.buffer[whiteSpaces + 1] = '\0';
    }

    string result = string(&defaultAllocator);

    CXString str = clang_getCursorKindSpelling(this->kind);
    if (myWhitespaces.buffer != NULL)
        result.Append(myWhitespaces.buffer);
    result.Append(clang_getCString(str));
    result.Append(" ");
    clang_disposeString(str);
    str = clang_getCursorSpelling(this->cursor);
    result.Append(clang_getCString(str));
    clang_disposeString(str);

    if (this->body.count > 0)
    {
        result.Append("\n");
        for (i32 i = 0; i < this->body.count; i++)
        {
            result.AppendDeinit(this->body.ptr[i].ToString(&defaultAllocator, whiteSpaces + 1));
            if (i < this->body.count - 1)
            {
                result.Append("\n");
            }
        }
    }

    return result.CloneDeinit(allocator);
}
LinxcReflectCState::LinxcReflectCState(IAllocator * myAllocator)
{
    allocator = myAllocator;
    this->tokens = collections::vector<CXCursor>(myAllocator);
}
void LinxcGetStructDeclInfo(LinxcNamespace* globalNamespace, IAllocator* allocator, LinxcCursor* cursor, collections::hashmap<string, LinxcType*>* typeMap)
{
    string structName = cursor->GetSpelling(&defaultAllocator);
    if (!typeMap->Contains(structName))
    {
        //printf("struct %s\n", structName.buffer);
        string structNamePersistent = structName.CloneDeinit(allocator);

        LinxcType* typePtr = (LinxcType*)allocator->Allocate(sizeof(LinxcType));
        LinxcType type = LinxcType(allocator, structNamePersistent, NULL, NULL);

        for (usize j = 0; j < cursor->body.count; j++)
        {
            LinxcCursor* var = cursor->body.Get(j);
            if (var->kind == CXCursor_FieldDecl)
            {
                i32 pointerCount = 0;
                CXType varCXType = clang_getCursorType(var->cursor);
                while (varCXType.kind == CXType_Pointer)
                {
                    pointerCount += 1;
                    varCXType = clang_getPointeeType(varCXType);
                }
                CXString varCXTypeSpelling = clang_getTypeSpelling(varCXType);

                const char* cstr = NULL;

                switch (varCXType.kind)
                {
                case CXType_Void:
                    cstr = "void";
                    break;
                case CXType_Bool:
                    cstr = "bool";
                    break;
                case CXType_Int:
                    cstr = "i32";
                    break;
                case CXType_LongLong:
                    cstr = "i64";
                    break;
                case CXType_UInt:
                    cstr = "u32";
                    break;
                case CXType_Short:
                    cstr = "i16";
                    break;
                case CXType_UShort:
                    cstr = "u16";
                    break;
                case CXType_Char_S:
                case CXType_SChar:
                    cstr = "i8";
                    break;
                case CXType_Char_U:
                case CXType_UChar:
                    cstr = "u8";
                    break;
                case CXType_ULongLong:
                    cstr = "u64";
                    break;
                default:
                    cstr = clang_getCString(varCXTypeSpelling);
                    break;
                }

                //printf(" %s\n", cstr);
                //we now have the accurate string representation of the type (hopefully)
                //now, we check for the presence of the type
                string searchString = string(&defaultAllocator, cstr);
                LinxcType* varType = globalNamespace->types.Get(searchString);
                if (varType != NULL)
                {
                    LinxcExpression typeExpr;
                    typeExpr.ID = LinxcExpr_TypeRef;
                    typeExpr.resolvesTo.lastType = NULL;
                    typeExpr.data.typeRef.pointerCount = pointerCount;
                    typeExpr.data.typeRef.isConst = clang_isConstQualifiedType(varCXType) != 0;
                    typeExpr.data.typeRef.templateArgs = collections::Array<LinxcTypeReference>();
                    typeExpr.data.typeRef.lastType = varType;
                    LinxcVar field = LinxcVar(var->GetSpelling(allocator), typeExpr, option<LinxcExpression>());
                    type.variables.Add(field);
                }
                else
                {
                    LinxcType** typeMapContains = typeMap->Get(searchString);
                    if (typeMapContains != NULL)
                    {
                        varType = *typeMapContains;
                        LinxcExpression typeExpr;
                        typeExpr.ID = LinxcExpr_TypeRef;
                        typeExpr.resolvesTo.lastType = NULL;
                        typeExpr.data.typeRef.pointerCount = pointerCount;
                        typeExpr.data.typeRef.isConst = clang_isConstQualifiedType(varCXType) != 0;
                        typeExpr.data.typeRef.templateArgs = collections::Array<LinxcTypeReference>();
                        typeExpr.data.typeRef.lastType = varType;
                        LinxcVar field = LinxcVar(var->GetSpelling(allocator), typeExpr, option<LinxcExpression>());
                        type.variables.Add(field);
                    }
                }
                searchString.deinit();

                clang_disposeString(varCXTypeSpelling);
            }
        }

        //by default, the types in the definedType of the file live within the namespaces.
        //However, c types from a c file ain't concatenated into the namespace for obvious reasons
        *typePtr = type;
        typeMap->Add(structNamePersistent, typePtr);

        structName.deinit();
    }
    else structName.deinit();
}
CXChildVisitResult CursorVisitor(CXCursor current, CXCursor parent, CXClientData client_data)
{
    collections::vector<LinxcCursor>* result = (collections::vector<LinxcCursor>*)client_data;

    if (current.kind == CXCursor_StructDecl || current.kind == CXCursor_EnumDecl || current.kind == CXCursor_FunctionDecl)// || current.kind == CXCursor_TypedefDecl)
    {
        collections::vector<LinxcCursor> body = collections::vector<LinxcCursor>(result->allocator);
        clang_visitChildren(current, &CursorVisitor, &body);
        LinxcCursor stmt = LinxcCursor(current, body);
        result->Add(stmt);
    }
    else
    {
        result->Add(LinxcCursor(current, collections::vector<LinxcCursor>(result->allocator)));
    }

    return CXChildVisit_Continue;
}
LinxcParsedFile Linxc_ReflectC(LinxcNamespace* globalNamespace, IAllocator *allocator, string fileFullName)
{
    LinxcParsedFile file = LinxcParsedFile(allocator, fileFullName, string(allocator));

    collections::hashmap<string, LinxcType*> typeMap = collections::hashmap<string, LinxcType*>(&defaultAllocator, &stringHash, &stringEql);

	CXIndex index = clang_createIndex(0, 0);
    const char** args = NULL;//(const char**)allocator->Allocate(sizeof(void*));
    //args[0] = "-D CIMGUI_DEFINE_ENUMS_AND_STRUCTS";
	CXTranslationUnit tu = clang_parseTranslationUnit(index, fileFullName.buffer, args, 0, NULL, 0, CXTranslationUnit_SkipFunctionBodies); //

	if (tu != NULL)
	{
		CXCursor cursor = clang_getTranslationUnitCursor(tu);

        LinxcCursor root = LinxcCursor(cursor, collections::vector<LinxcCursor>(allocator));

        //gather all cursors
        clang_visitChildren(
            cursor, //Root cursor
            &CursorVisitor,
            &root.body //client_data
            );

        //convert cursors
        for (usize i = 0; i < root.body.count; i++)
        {
            LinxcCursor* cursor = root.body.Get(i);

            //typedef struct {} structName
            /*if (cursor->kind == CXCursor_TypedefDecl)
            {
                if (cursor->body.count == 1 && cursor->body.Get(0)->kind == CXCursor_StructDecl)
                {
                    LinxcGetStructDeclInfo(globalNamespace, allocator, cursor->body.Get(0), &typeMap);
                }
            }
            else*/ if (cursor->kind == CXCursor_StructDecl)
            {
                LinxcGetStructDeclInfo(globalNamespace, allocator, cursor, &typeMap);
            }
            else if (cursor->kind == CXCursor_EnumDecl)
            {
                
            }
        }

        clang_disposeTranslationUnit(tu);
        clang_disposeIndex(index);

        for (usize i = 0; i < typeMap.bucketsCount; i++)
        {
            if (typeMap.buckets[i].initialized)
            {
                for (usize j = 0; j < typeMap.buckets[i].entries.count; j++)
                {
                    file.definedTypes.Add(typeMap.buckets[i].entries.ptr[j].value);
                }
            }
        }
        typeMap.deinit();
        return file;
	}
}