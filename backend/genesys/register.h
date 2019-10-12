/* sane - Scanner Access Now Easy.

   Copyright (C) 2019 Povilas Kanapickas <povilas@radix.lt>

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

   As a special exception, the authors of SANE give permission for
   additional uses of the libraries contained in this release of SANE.

   The exception is that, if you link a SANE library with other files
   to produce an executable, this does not by itself cause the
   resulting executable to be covered by the GNU General Public
   License.  Your use of that executable is in no way restricted on
   account of linking the SANE library code into it.

   This exception does not, however, invalidate any other reasons why
   the executable file might be covered by the GNU General Public
   License.

   If you submit changes to SANE to the maintainers to be included in
   a subsequent release, you agree by submitting the changes that
   those changes may be distributed with this exception intact.

   If you write modifications of your own for SANE, it is your choice
   whether to permit this exception to apply to your modifications.
   If you do not wish that, delete this exception notice.
*/

#ifndef BACKEND_GENESYS_REGISTER_H
#define BACKEND_GENESYS_REGISTER_H

#include <algorithm>
#include <climits>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

struct GenesysRegister
{
    std::uint16_t address = 0;
    std::uint8_t value = 0;
};

inline bool operator<(const GenesysRegister& lhs, const GenesysRegister& rhs)
{
    return lhs.address < rhs.address;
}

struct GenesysRegisterSetState
{
    bool is_lamp_on = false;
    bool is_xpa_on = false;
};

class Genesys_Register_Set
{
public:
    static constexpr unsigned MAX_REGS = 256;

    using container = std::vector<GenesysRegister>;
    using iterator = typename container::iterator;
    using const_iterator = typename container::const_iterator;

    // FIXME: this shouldn't live here, but in a separate struct that contains Genesys_Register_Set
    GenesysRegisterSetState state;

    enum Options {
        SEQUENTIAL = 1
    };

    Genesys_Register_Set()
    {
        registers_.reserve(MAX_REGS);
    }

    // by default the register set is sorted by address. In certain cases it's importand to send
    // the registers in certain order: use the SEQUENTIAL option for that
    Genesys_Register_Set(Options opts) : Genesys_Register_Set()
    {
        if ((opts & SEQUENTIAL) == SEQUENTIAL) {
            sorted_ = false;
        }
    }

    void init_reg(std::uint16_t address, std::uint8_t default_value)
    {
        if (find_reg_index(address) >= 0) {
            set8(address, default_value);
            return;
        }
        GenesysRegister reg;
        reg.address = address;
        reg.value = default_value;
        registers_.push_back(reg);
        if (sorted_)
            std::sort(registers_.begin(), registers_.end());
    }

    bool has_reg(std::uint16_t address) const
    {
        return find_reg_index(address) >= 0;
    }

    void remove_reg(std::uint16_t address)
    {
        int i = find_reg_index(address);
        if (i < 0) {
            throw std::runtime_error("the register does not exist");
        }
        registers_.erase(registers_.begin() + i);
    }

    GenesysRegister& find_reg(std::uint16_t address)
    {
        int i = find_reg_index(address);
        if (i < 0) {
            throw std::runtime_error("the register does not exist");
        }
        return registers_[i];
    }

    const GenesysRegister& find_reg(std::uint16_t address) const
    {
        int i = find_reg_index(address);
        if (i < 0) {
            throw std::runtime_error("the register does not exist");
        }
        return registers_[i];
    }

    GenesysRegister* find_reg_address(std::uint16_t address)
    {
        return &find_reg(address);
    }

    const GenesysRegister* find_reg_address(std::uint16_t address) const
    {
        return &find_reg(address);
    }

    void set8(std::uint16_t address, std::uint8_t value)
    {
        find_reg(address).value = value;
    }

    void set8_mask(std::uint16_t address, std::uint8_t value, std::uint8_t mask)
    {
        auto& reg = find_reg(address);
        reg.value = (reg.value & ~mask) | value;
    }

    void set16(std::uint16_t address, std::uint16_t value)
    {
        find_reg(address).value = (value >> 8) & 0xff;
        find_reg(address + 1).value = value & 0xff;
    }

    void set24(std::uint16_t address, std::uint32_t value)
    {
        find_reg(address).value = (value >> 16) & 0xff;
        find_reg(address + 1).value = (value >> 8) & 0xff;
        find_reg(address + 2).value = value & 0xff;
    }

    std::uint8_t get8(std::uint16_t address) const
    {
        return find_reg(address).value;
    }

    std::uint16_t get16(std::uint16_t address) const
    {
        return (find_reg(address).value << 8) | find_reg(address + 1).value;
    }

    std::uint32_t get24(std::uint16_t address) const
    {
        return (find_reg(address).value << 16) |
               (find_reg(address + 1).value << 8) |
                find_reg(address + 2).value;
    }

    void clear() { registers_.clear(); }
    std::size_t size() const { return registers_.size(); }

    iterator begin() { return registers_.begin(); }
    const_iterator begin() const { return registers_.begin(); }

    iterator end() { return registers_.end(); }
    const_iterator end() const { return registers_.end(); }

private:
    int find_reg_index(std::uint16_t address) const
    {
        if (!sorted_) {
            for (std::size_t i = 0; i < registers_.size(); i++) {
                if (registers_[i].address == address) {
                    return i;
                }
            }
            return -1;
        }

        GenesysRegister search;
        search.address = address;
        auto it = std::lower_bound(registers_.begin(), registers_.end(), search);
        if (it == registers_.end())
            return -1;
        if (it->address != address)
            return -1;
        return std::distance(registers_.begin(), it);
    }

    // registers are stored in a sorted vector
    bool sorted_ = true;
    std::vector<GenesysRegister> registers_;
};

template<class Value>
struct RegisterSetting
{
    using ValueType = Value;
    using AddressType = std::uint16_t;

    RegisterSetting() = default;

    RegisterSetting(AddressType p_address, ValueType p_value) :
        address(p_address), value(p_value)
    {}

    RegisterSetting(AddressType p_address, ValueType p_value, ValueType p_mask) :
        address(p_address), value(p_value), mask(p_mask)
    {}

    AddressType address = 0;
    ValueType value = 0;
    ValueType mask = 0xff;

    bool operator==(const RegisterSetting& other) const
    {
        return address == other.address && value == other.value && mask == other.mask;
    }
};

using GenesysRegisterSetting = RegisterSetting<std::uint8_t>;
using GenesysRegisterSetting16 = RegisterSetting<std::uint16_t>;

template<class Stream, class Value>
void serialize(Stream& str, RegisterSetting<Value>& reg)
{
    serialize(str, reg.address);
    serialize(str, reg.value);
    serialize(str, reg.mask);
}

template<class Value>
class RegisterSettingSet
{
public:
    using ValueType = Value;
    using SettingType = RegisterSetting<ValueType>;
    using AddressType = typename SettingType::AddressType;

    using container = std::vector<SettingType>;
    using iterator = typename container::iterator;
    using const_iterator = typename container::const_iterator;

    RegisterSettingSet() = default;
    RegisterSettingSet(std::initializer_list<SettingType> ilist) :
        registers_(ilist)
    {}

    iterator begin() { return registers_.begin(); }
    const_iterator begin() const { return registers_.begin(); }
    iterator end() { return registers_.end(); }
    const_iterator end() const { return registers_.end(); }

    SettingType& operator[](std::size_t i) { return registers_[i]; }
    const SettingType& operator[](std::size_t i) const { return registers_[i]; }

    std::size_t size() const { return registers_.size(); }
    bool empty() const { return registers_.empty(); }
    void clear() { registers_.clear(); }

    void push_back(SettingType reg) { registers_.push_back(reg); }

    void merge(const RegisterSettingSet& other)
    {
        for (const auto& reg : other) {
            set_value(reg.address, reg.value);
        }
    }

    SettingType& find_reg(AddressType address)
    {
        int i = find_reg_index(address);
        if (i < 0) {
            throw std::runtime_error("the register does not exist");
        }
        return registers_[i];
    }

    const SettingType& find_reg(AddressType address) const
    {
        int i = find_reg_index(address);
        if (i < 0) {
            throw std::runtime_error("the register does not exist");
        }
        return registers_[i];
    }

    ValueType get_value(AddressType address) const
    {
        int index = find_reg_index(address);
        if (index >= 0) {
            return registers_[index].value;
        }
        throw std::out_of_range("Unknown register");
    }

    void set_value(AddressType address, ValueType value)
    {
        int index = find_reg_index(address);
        if (index >= 0) {
            registers_[index].value = value;
            return;
        }
        push_back(SettingType(address, value));
    }

    template<class V>
    friend void serialize(std::istream& str, RegisterSettingSet<V>& reg);
    template<class V>
    friend void serialize(std::ostream& str, RegisterSettingSet<V>& reg);

    bool operator==(const RegisterSettingSet& other) const
    {
        return registers_ == other.registers_;
    }

private:

    int find_reg_index(AddressType address) const
    {
        for (std::size_t i = 0; i < registers_.size(); i++) {
            if (registers_[i].address == address) {
                return i;
            }
        }
        return -1;
    }

    std::vector<SettingType> registers_;
};

using GenesysRegisterSettingSet = RegisterSettingSet<std::uint8_t>;
using GenesysRegisterSettingSet16 = RegisterSettingSet<std::uint16_t>;

template<class Value>
inline void serialize(std::istream& str, RegisterSettingSet<Value>& reg)
{
    using AddressType = typename RegisterSetting<Value>::AddressType;

    reg.clear();
    const std::size_t max_register_address = 1 << (sizeof(AddressType) * CHAR_BIT);
    serialize(str, reg.registers_, max_register_address);
}

template<class Value>
inline void serialize(std::ostream& str, RegisterSettingSet<Value>& reg)
{
    serialize(str, reg.registers_);
}

template<class F, class Value>
void apply_registers_ordered(const RegisterSettingSet<Value>& set,
                             std::initializer_list<std::uint16_t> order, F f)
{
    for (std::uint16_t addr : order) {
        f(set.find_reg(addr));
    }
    for (const auto& reg : set) {
        if (std::find(order.begin(), order.end(), reg.address) != order.end()) {
            continue;
        }
        f(reg);
    }
}

#endif // BACKEND_GENESYS_REGISTER_H
