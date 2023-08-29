#pragma once
#include <stdint.h>
#include <unordered_map>
#include <string>

#define typeof(T) &Reflection::Typeof<T>::type;

namespace Reflection
{
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

    struct Type
    {
        uint32_t ID;
        std::string name;

        std::unordered_map<std::string, uint32_t> nameToVariable;
        void *(**variables)(void *);
        uint32_t numVariables;

        inline void *GetVariable(std::string name, void *instance)
        {
            uint32_t index = nameToVariable[name];
            return variables[index](instance);
        }
    };
}

template <typename T>
Reflection::Type Reflection::Typeof<T>::type;