#include "string.hpp"
#include "stdio.h"

string::string()
{
    this->buffer = (char *)0;
    this->length = 0;
}

string::string(const char* source)
{
    this->length = strlen(source) + 1;
    this->buffer = (char *)malloc(this->length);
    strcpy(this->buffer, source);
    this->buffer[this->length - 1] = '\0';
}

string::string(const char* source, usize offset, usize length)
{
    this->buffer = (char*)malloc(length + 1);
    this->buffer[length] = '\0';
    this->length = length + 1;
    memcpy(this->buffer, (void*)(source + offset), length);
}

void string::deinit()
{
    free(buffer);
}
bool string::eql(const char *other)
{
    return strcmp(buffer, other) == 0;
}

bool stringEql(string A, string B)
{
    return A.eql(B.buffer);
}
i32 stringHash(string A)
{
    i32 hash = 7;
    for (usize i = 0; i < A.length - 1; i++)
    {
        hash = hash * 31 + A.buffer[i];
    }

    return hash;
}
i32 charHash(const char *A)
{
    i32 hash = 7;
    usize i = 0;
    while (true)
    {
        if (A[i] == '\0')
        {
            break;
        }
        else
        {
            hash = hash * 31 + A[i];
        }
        i += 1;
    }
    return hash;
}