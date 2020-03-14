/* sane - Scanner Access Now Easy.

   Copyright (C) 2020 Povilas Kanapickas <povilas@radix.lt>

   This file is part of the SANE package.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA.
*/

#ifndef BACKEND_GENESYS_VALUE_FILTER_H
#define BACKEND_GENESYS_VALUE_FILTER_H

#include <algorithm>
#include <initializer_list>
#include <iostream>
#include <vector>

namespace genesys {

struct AnyTag {};
constexpr AnyTag VALUE_FILTER_ANY{};

template<class T>
class ValueFilterAny
{
public:
    ValueFilterAny() : matches_any_{false} {}
    ValueFilterAny(AnyTag) : matches_any_{true} {}
    ValueFilterAny(std::initializer_list<T> values) :
        matches_any_{false},
        values_{values}
    {}

    bool matches(T value) const
    {
        if (matches_any_)
            return true;
        auto it = std::find(values_.begin(), values_.end(), value);
        return it != values_.end();
    }

    bool operator==(const ValueFilterAny& other) const
    {
        return matches_any_ == other.matches_any_ && values_ == other.values_;
    }

    bool matches_any() const { return matches_any_; }
    const std::vector<T>& values() const { return values_; }

private:
    bool matches_any_ = false;
    std::vector<T> values_;

    template<class Stream, class U>
    friend void serialize(Stream& str, ValueFilterAny<U>& x);
};

template<class T>
std::ostream& operator<<(std::ostream& out, const ValueFilterAny<T>& values)
{
    if (values.matches_any()) {
        out << "ANY";
        return out;
    }
    out << format_vector_indent_braced(4, "", values.values());
    return out;
}

template<class Stream, class T>
void serialize(Stream& str, ValueFilterAny<T>& x)
{
    serialize(str, x.matches_any_);
    serialize_newline(str);
    serialize(str, x.values_);
}


} // namespace genesys

#endif // BACKEND_GENESYS_VALUE_FILTER_H
