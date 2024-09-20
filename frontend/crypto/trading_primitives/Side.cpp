#include "Side.h"

SideEnum Side::value() const
{
    return m_side;
}

Side Side::opposite() const
{
    return m_side == SideEnum::Buy ? SideEnum::Sell : SideEnum::Buy;
}

int Side::sign() const
{
    return m_side == SideEnum::Buy ? 1 : -1;
}

Side Side::buy()
{
    return {SideEnum::Buy};
}

Side Side::sell()
{
    return {SideEnum::Sell};
}

std::string Side::to_string() const
{
    if (m_side == SideEnum::Buy) {
        return "Buy";
    }
    return "Sell";
}

std::ostream & operator<<(std::ostream & os, const Side & side)
{
    os << (side.m_side == SideEnum::Buy ? "Buy" : "Sell");
    return os;
}

bool operator==(const Side & lhs, const Side & rhs)
{
    return lhs.m_side == rhs.m_side;
}

