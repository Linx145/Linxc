#pragma once
#include <clang-c/Index.h>
#include <string.hpp>
#include <stdio.h>
#include <vector.linxc>
#include <lexer.hpp>
#include <Linxc.h>
#include <ast.hpp>

struct LinxcCursor
{
	CXCursor cursor;
	CXCursorKind kind;
	collections::vector<LinxcCursor> body;

	LinxcCursor(CXCursor thisCursor, collections::vector<LinxcCursor> thisBody);
	void deinit();
	string ToString(IAllocator *allocator, i32 whiteSpaces);
	string GetSpelling(IAllocator *allocator);
};

struct LinxcReflectCState
{
	IAllocator* allocator;
	collections::vector<CXCursor> tokens;

	LinxcReflectCState(IAllocator *myAllocator);
};

CXChildVisitResult CursorVisitor(CXCursor current_cursor, CXCursor parent, CXClientData client_data);

LinxcParsedFile Linxc_ReflectC(LinxcNamespace* globalNamespace, IAllocator* allocator, string fileFullName);