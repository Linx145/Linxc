#ifndef linxch
#define linxch
#include "Reflection.h"

#define delegate(name, returns, ...) typedef returns (*name)(__VA_ARGS__)
#define trait struct
#define impl_trait(name)

typedef signed char i8;
typedef short i16;
typedef int i32;
typedef long long i64;
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef unsigned long long usize;

struct Object
{
    void *ptr;
    Reflection::Type *type;
};

/// All calls are replaced by the transpiler to point to the self variable when used in a trait function
#define trait_self() Object{}
#endif