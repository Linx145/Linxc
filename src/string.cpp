#include "string.hpp"
#include "stdio.h"
#include "math.h"

string::string()
{
    this->allocator = &defaultAllocator;
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
void string::Append(i64 integer)
{
    //max integer is 19 characters, with - sign its 20
    char chars[20];
    chars[19] = '\0';
    usize index = 19;
    i64 positive = integer < 0 ? -integer : integer;
    while (positive > 100)
    {
        index -= 2;
        memcpy(chars + index, digits2(positive % 100), 2);
        positive /= 100;
    }
    if (positive < 10)
    {
        index -= 1;
        chars[index] = '0' + positive;

        if (integer < 0)
        {
            index -= 1;
            *(chars + index) = '-';
        }
        
        this->Append(chars + index);
        return;
    }
    index -= 2;
    memcpy(chars + index, digits2(positive), 2);
    if (integer < 0)
    {
        index -= 1;
        *(chars + index) = '-';
    }
    this->Append(chars + index);
}
void string::Append(u64 integer)
{
    //max integer is 20 characters
    char chars[21];
    chars[20] = '\0';
    usize index = 20;
    while (integer > 100)
    {
        index -= 2;
        memcpy(chars + index, digits2(integer % 100), 2);
        integer /= 100;
    }
    if (integer < 10)
    {
        index -= 1;
        chars[index] = '0' + integer;

        this->Append(chars + index);
        return;
    }
    index -= 2;
    memcpy(chars + index, digits2(integer), 2);
    this->Append(chars + index);
}
void string::Append(double value)
{
    if (value == 0.0)
    {
        this->Append("0.0");
        return;
    }
    char chars[16];
    i32 result = snprintf(chars, 16, "%lf", value);
    if (result >= 16)
    {
        result = 16;
    }
    chars[result - 1] = '\0';
    this->Append(chars);
}
void string::Append(float value)
{
    if (value == 0.0f)
    {
        this->Append("0.0");
        return;
    }
    char chars[16];
    i32 result = snprintf(chars, 16, "%f", value);
    if (result >= 16)
    {
        result = 16;
    }
    chars[result - 1] = '\0';
    this->Append(chars);
}
bool string::operator==(const char* other)
{
    if (this->buffer == NULL)
    {
        return false;
    }
    return strcmp(this->buffer, other) == 0;
}
bool string::operator!=(const char* other)
{
    return !(*this==other);
}

bool stringEql(string A, string B)
{
    if (A.buffer == NULL && B.buffer == NULL)
    {
        return true;
    }
    return A.eql(B.buffer);
}
u32 stringHash(string A)
{
    if (A.buffer == NULL)
    {
        return 7;
    }
    u32 hash = 7;
    for (usize i = 0; i < A.length - 1; i++)
    {
        hash = hash * 31 + A.buffer[i];
    }

    return hash;
}
u32 charHash(const char *A)
{
    u32 hash = 7;
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