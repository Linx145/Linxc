#pragma once
#define CONSTRUCTOR(T, ...) T(__VA_ARGS__)
#define DESTRUCTOR(T) ~T()
#define NAMESPACE(T)