#pragma once
#include "Linxc.h"

template <typename T>
struct option
{
    T value;
    bool present;

    option()
    {
        present = false;
    }
    option(T value)
    {
        this->value = value;
        present = true;
    }
};