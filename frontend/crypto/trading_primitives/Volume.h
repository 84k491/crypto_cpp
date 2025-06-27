#pragma once

#include "Side.h"

#include <cmath>
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
    UnsignedVolume() = default;

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

    UnsignedVolume operator+(const UnsignedVolume & other) const
    {
        return {m_value + other.m_value};
    }

    UnsignedVolume & operator+=(const UnsignedVolume & other)
    {
        m_value += other.m_value;
        return *this;
    }

    bool operator<(const UnsignedVolume & other) const
    {
        return m_value < other.m_value;
    }

    bool operator==(const UnsignedVolume & other) const
    {
        return std::fabs(this->value() - other.value()) <= std::numeric_limits<double>::epsilon();
    }

    bool operator<=(const UnsignedVolume & other) const
    {
        return *this == other || *this < other;
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
        : m_value(volume.value() * side.sign())
    {
    }

    std::pair<UnsignedVolume, Side> as_unsigned_and_side() const
    {
        if (m_value < 0) {
            return {UnsignedVolume(-m_value), Side::sell()};
        }
        return {UnsignedVolume(m_value), Side::buy()};
    }

    int sign() const
    {
        if (m_value < 0) {
            return -1;
        }
        if (m_value > 0) {
            return 1;
        }
        return 0;
    }

    SignedVolume operator*(int other) const
    {
        return SignedVolume(m_value * other);
    }

    SignedVolume operator+(const SignedVolume & other) const
    {
        return SignedVolume(m_value + other.m_value);
    }

    SignedVolume & operator+=(const SignedVolume & other)
    {
        m_value += other.m_value;
        return *this;
    }

    auto value() const { return m_value; }

    bool is_zero() const { return m_value == 0.; }

private:
    double m_value = {};
};
