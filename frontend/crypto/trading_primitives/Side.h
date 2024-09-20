#pragma once

#include "Enums.h"
#include <ostream>

class Side
{
    friend std::ostream & operator<<(std::ostream & os, const Side & side);
    friend bool operator==(const Side & lhs, const Side & rhs);
public:
    Side(SideEnum side)
        : m_side(side)
    {
    }

    static Side buy();
    static Side sell();

    SideEnum value() const;
    Side opposite() const;
    int sign() const;
    std::string to_string() const;

private:
    SideEnum m_side;
};
