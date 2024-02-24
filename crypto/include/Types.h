#pragma once

#include "Enums.h"

#include <optional>
#include <ostream>
#include <utility>

class SignedVolume;
class UnsignedVolume
{
    friend std::ostream & operator<<(std::ostream & os, const UnsignedVolume & volume);
    friend class SignedVolume;
    UnsignedVolume(double value)
        : m_value(value)
    {
    }

public:
    UnsignedVolume(const UnsignedVolume & other)
        : m_value(other.m_value)
    {
    }

    UnsignedVolume(UnsignedVolume && other) noexcept
        : m_value(other.m_value)
    {
    }

    UnsignedVolume & operator=(const UnsignedVolume & other) = default;
    UnsignedVolume & operator=(UnsignedVolume && other) noexcept
    {
        m_value = other.m_value;
        return *this;
    }

    static std::optional<UnsignedVolume> from(double value)
    {
        if (value < 0) {
            return std::nullopt;
        }
        return UnsignedVolume(value);
    }

    auto value() const { return m_value; }

    UnsignedVolume & operator+=(const UnsignedVolume & other)
    {
        m_value += other.m_value;
        return *this;
    }

    bool operator<(const UnsignedVolume & other) const
    {
        return m_value < other.m_value;
    }

    std::optional<UnsignedVolume> operator-(const UnsignedVolume & other) const
    {
        if (m_value < other.m_value) {
            return std::nullopt;
        }
        return UnsignedVolume(m_value - other.m_value);
    }

    std::optional<UnsignedVolume> operator-=(const UnsignedVolume & other)
    {
        if (m_value < other.m_value) {
            return std::nullopt;
        }
        m_value -= other.m_value;
        return *this;
    }

private:
    double m_value = {};
};
std::ostream & operator<<(std::ostream & os, const UnsignedVolume & volume);

class SignedVolume
{
public:
    SignedVolume() = default;
    explicit SignedVolume(double value)
        : m_value(value)
    {
    }

    SignedVolume(const UnsignedVolume & volume, Side side)
        : m_value(volume.value() * (side == Side::Buy ? 1 : -1))
    {
    }

    std::pair<UnsignedVolume, Side> as_unsigned_and_side() const
    {
        if (m_value < 0) {
            return {UnsignedVolume(-m_value), Side::Sell};
        }
        return {UnsignedVolume(m_value), Side::Buy};
    }

    auto value() const { return m_value; }

private:
    double m_value = {};
};
