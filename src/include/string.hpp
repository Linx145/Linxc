#ifndef linxccstring
#define linxccstring

#include "Linxc.h"
#include "stdlib.h"
#include "string.h"

struct string
{
    char *buffer;
    usize length;

    string();
    string(const char* source);
    string(const char* source, usize offset, usize length);

    void deinit();
    bool eql(const char *other);
};

bool stringEql(string A, string B);

i32 stringHash(string A);

i32 charHash(const char *A);

#endif