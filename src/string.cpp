#include "string.hpp"
#include "stdio.h"

string::string()
{
    this->allocator = NULL;
    this->buffer = NULL;
    this->length = 0;
}

string::string(const char* source)
{
    this->allocator = &defaultAllocator;
    this->length = strlen(source) + 1;
    this->buffer = (char *)malloc(this->length);
    strcpy(this->buffer, source);
    this->buffer[this->length - 1] = '\0';
}

string::string(const char* source, usize length)
{
    this->allocator = &defaultAllocator;
    this->buffer = (char*)malloc(length + 1);
    this->buffer[length] = '\0';
    this->length = length + 1;
    memcpy(this->buffer, source, length);
}

string::string(IAllocator *myAllocator)
{
    this->allocator = myAllocator;
    this->buffer = NULL;
    this->length = 0;
}

string::string(IAllocator *myAllocator, const char* source)
{
    this->allocator = myAllocator;
    this->length = strlen(source) + 1;
    this->buffer = (char *)myAllocator->Allocate(this->length);
    strcpy(this->buffer, source);
    this->buffer[this->length - 1] = '\0';
}

string::string(IAllocator *myAllocator, const char* source, usize length)
{
    this->allocator = myAllocator;
    this->buffer = (char*)myAllocator->Allocate(length + 1);
    this->buffer[length] = '\0';
    this->length = length + 1;
    memcpy(this->buffer, source, length);
}

void string::deinit()
{
    this->allocator->Free(buffer);
}
bool string::eql(const char *other)
{
    if (this->buffer == NULL)
    {
        return false;
    }
    return strcmp(buffer, other) == 0;
}
void string::Append(const char *other)
{
    usize otherLen = strlen(other);
    usize newLength = otherLen + this->length;
    if (this->length == 0)
    {
        newLength += 1;
    }
    char *newBuffer = (char*)this->allocator->Allocate(newLength);

    if (this->buffer != NULL)
    {
        memcpy(newBuffer, this->buffer, this->length - 1);
        memcpy(newBuffer + this->length - 1, other, otherLen);
    }
    else
    {
        memcpy(newBuffer, other, otherLen);
    }
    newBuffer[newLength - 1] = '\0';

    if (this->buffer != NULL)
    {
        this->allocator->Free(this->buffer);
    }
    this->buffer = newBuffer;
    this->length = newLength;
}
void string::Prepend(const char *other)
{
    usize otherLen = strlen(other);
    usize newLength = otherLen + this->length;
    if (this->length == 0)
    {
        newLength += 1;
    }
    char *newBuffer = (char*)this->allocator->Allocate(newLength);

    if (this->buffer != NULL)
    {
        memcpy(newBuffer, other, otherLen);
        memcpy(newBuffer + otherLen, this->buffer, this->length - 1);
    }
    else
    {
        memcpy(newBuffer, other, otherLen);
    }
    newBuffer[newLength - 1] = '\0';

    if (this->buffer != NULL)
    {
        this->allocator->Free(this->buffer);
    }
    this->buffer = newBuffer;
    this->length = newLength;
}
void string::AppendDeinit(string other)
{
    this->Append(other.buffer);
    other.deinit();
}
void string::PrependDeinit(string other)
{
    this->Prepend(other.buffer);
    other.deinit();
}
bool string::operator==(const char* other)
{
    if (this->buffer == NULL)
    {
        return false;
    }
    return strcmp(this->buffer, other) == 0;
}

bool stringEql(string A, string B)
{
    if (A.buffer == NULL && B.buffer == NULL)
    {
        return true;
    }
    return A.eql(B.buffer);
}
i32 stringHash(string A)
{
    if (A.buffer == NULL)
    {
        return 7;
    }
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
option<usize> FindFirst(const char *buffer, char character)
{
    usize i = 0;
    while (buffer[i] != '\0' || character == '\0')
    {
        if (buffer[i] == character)
        {
            return option<usize>(i);
        }
        i++;
    }

    return option<usize>();
}
option<usize> FindLast(const char *buffer, char character)
{
    option<usize> result = option<usize>();
    usize i = 0;

    while (buffer[i] != '\0' || character == '\0')
    {
        if (buffer[i] == character)
        {
            result.value = i;
            result.present = true;
        }
        i++;
    }

    return result;
}