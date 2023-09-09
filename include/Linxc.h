#pragma once
#define namespace(T)
#define delegate(name, returns, ...) typedef returns (*name)(__VA_ARGS__) 