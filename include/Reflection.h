#pragma once
#include <stdint.h>
#include <unordered_map>
#include <string>
#include <vector>

#define typeof(T) &Reflection::Typeof<T>::type;

namespace Reflection
{
    void initReflection();

    struct Type
    {
        uint32_t ID;
        std::string name;

        //variables.push_back([](void *instance) -> void * { return &((Type*)instance)->ID; });

        std::unordered_map<std::string, void *(*)(void *)> variables;
        std::unordered_map<std::string, void *> functions;

        inline void *GetVariable(std::string name, void *instance)
        {
            return variables[name](instance);
        }
    };

    struct Database
    {
        //to be filled in by Linxc compiler
        std::unordered_map<std::string, Type*> nameToType;

        inline Type *GetType(std::string name)
        {
            return nameToType[name];
        }
    };

    template <typename T>
    struct Typeof
    {
        static Type type;
    };
}

template <typename T>
Reflection::Type Reflection::Typeof<T>::type;