#ifndef linxccstring
#define linxccstring

#include "Linxc.h"
#include "stdlib.h"
#include "allocators.hpp"
#include "string.h"

struct string
{
    IAllocator *allocator;
    char *buffer;
    usize length;

    string();
    string(const char* source);
    string(const char* source, usize length);
    string(IAllocator *myAllocator, const char* source);
    string(IAllocator *myAllocator, const char* source, usize length);

    void deinit();
    bool eql(const char *other);

    void Prepend(const char *other);
    void Append(const char *other);
};

bool stringEql(string A, string B);

i32 stringHash(string A);

i32 charHash(const char *A);

#endif