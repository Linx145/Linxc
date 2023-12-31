#ifndef linxccstring
#define linxccstring

#include "Linxc.h"
#include "stdlib.h"
#include "allocators.hpp"
#include "string.h"
#include "option.linxc"

struct string
{
    IAllocator *allocator;
    char *buffer;
    usize length;

    string();
    string(const char* source);
    string(const char* source, usize length);
    string(IAllocator *myAllocator);
    string(IAllocator *myAllocator, const char* source);
    string(IAllocator *myAllocator, const char* source, usize length);

    void deinit();
    bool eql(const char *other);

    void Prepend(const char *other);
    void Append(const char *other);
    void Append(i64 value);
    void Append(u64 value);
    void Append(double value);
    void Append(float value);

    void PrependDeinit(string other);
    void AppendDeinit(string other);

    inline string CloneDeinit(IAllocator* allocator)
    {
        string result = string(allocator, this->buffer);
        this->deinit();
        return result;
    }

    bool operator==(const char* other);
    bool operator!=(const char* other);
};

bool stringEql(string A, string B);

u32 stringHash(string A);

u32 charHash(const char *A);

option<usize> FindFirst(const char *buffer, char character);

option<usize> FindLast(const char *buffer, char character);

string ReplaceChar(IAllocator *allocator, string input, char toReplace, char replaceWith);

inline const char* digits2(usize value)
{
    return &"0001020304050607080910111213141516171819"
        "2021222324252627282930313233343536373839"
        "4041424344454647484950515253545556575859"
        "6061626364656667686970717273747576777879"
        "8081828384858687888990919293949596979899"[value * 2];
}

#endif