#pragma once

#ifndef NULL
#define NULL 0
#endif

#define def_delegate(name, returns, ...) typedef returns (*name)(__VA_ARGS__)
#define trait struct
#define impl_trait(name)
#define IsAttribute

typedef signed char i8;
typedef short i16;
typedef int i32;
typedef long long i64;
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef unsigned long long usize;