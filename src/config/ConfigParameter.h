/*
 * Copyright (C) 2026 Dmitry Korobkov <dmitry.korobkov.nn@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once
#pragma push_macro("LOG_MODULE")

#undef LOG_MODULE
#define LOG_MODULE "CFG:P"

#include <type_traits>
#include <vector>
#include <memory>
#include <Arduino.h>
#include <IPAddress.h>
#include <console.h>
#include <checksum.h>

#include "ByteBuffer.h"
#include "PersistCounter.h"

namespace config {

enum class ConfigParameterType {
    INVALID = 0,
    BYTE,
    NUMBER,
    STRING,
    IP_ADDRESS,
    ARRAY,
    COUNTER,
};

class ConfigParameterBase {
public:
    static const unsigned char INVALID_ID = (-1);

    ConfigParameterBase(ConfigParameterType type, unsigned char id, bool isValid);
    virtual ~ConfigParameterBase() {}

    unsigned char getId();
    ConfigParameterType getType();

    bool isValid();

    virtual ByteBuffer::iterator read(ByteBuffer::iterator& it);
    virtual ByteBuffer::iterator write(ByteBuffer::iterator& it);

protected:
    ConfigParameterType mType;
    unsigned char mId;
    bool mIsValid;
};

template <typename T>
class ConfigParameterValue {
public:
    ConfigParameterValue() = default;
    explicit ConfigParameterValue(const T &value) : mValue(value) {}
    explicit ConfigParameterValue(T &&value) : mValue(std::move(value)) {}

    T& value() { return mValue; }

    const T& get() const { return mValue; }

    void set(const T &value) { mValue = value; }
    void set(T &&value) { mValue = std::move(value); }
protected:
    T mValue;
};

template <typename T>
class ConfigParameter
  : public ConfigParameterBase,
    public ConfigParameterValue<T> {
public:
    explicit ConfigParameter(unsigned char id = INVALID_ID);
    explicit ConfigParameter(unsigned char id, const T& value);

    ByteBuffer::iterator read(ByteBuffer::iterator& it) override;
    ByteBuffer::iterator write(ByteBuffer::iterator& it) override;
};

// Class ConfigParameter<vector<T>>

template <typename T>
class ConfigParameter<std::vector<T>>
  : public ConfigParameterBase,
    public ConfigParameterValue<std::vector<T>>
{
public:
    ConfigParameter(unsigned char id = INVALID_ID);
    ConfigParameter(unsigned char id, const std::vector<T>& value);

    ByteBuffer::iterator read(ByteBuffer::iterator& it) override;
    ByteBuffer::iterator write(ByteBuffer::iterator& it) override;
};

template <typename T>
ConfigParameter<std::vector<T>>::ConfigParameter(unsigned char id)
  : ConfigParameterBase(ConfigParameterType::ARRAY, id, false),
    ConfigParameterValue<std::vector<T>>()
{
    static_assert(std::is_arithmetic<T>::value, "Not an arithmetic type");
};

template <typename T>
ConfigParameter<std::vector<T>>::ConfigParameter(unsigned char id,
                                                 const std::vector<T> &value)
  : ConfigParameterBase(ConfigParameterType::ARRAY, id, true),
    ConfigParameterValue<std::vector<T>>(value)
{
    static_assert(std::is_arithmetic<T>::value, "Not an arithmetic type");
};

template <typename T>
ByteBuffer::iterator
ConfigParameter<std::vector<T>>::read(ByteBuffer::iterator &it)
{
    auto vector = this->get();
    auto nextIt = ConfigParameterBase::read(it);

    if (nextIt != it)
    {
        char length = *nextIt++;

        vector.clear();

        for (unsigned char ix = 0; ix < length; ++ix) {

            T value = 0;

            for (unsigned char ix = 0; ix < sizeof(T); ++ix) {
                value |= BYTE_SET(ix, 0x00, *nextIt++);
            }

            vector.push_back(value);
        }

        char checksum = checksum::Checksum(checksum::Checksum::CRC8)
                            .calculate<typename std::vector<T>::iterator>(vector.begin(), vector.end());

        if (checksum != *nextIt++) {

            LOG("Invalid checksum (0x%X): id=%d, type=%d",
                        checksum, mId, mType);
            return it;
        }

        LOG_ADD("CFG <= R [%02d]:", mId);
        for (auto &value : vector) {
            LOG_ADD(" %x", value);
        }
        LOG_ADD(", sz=%d, CS=0x%X", vector.size(), checksum);
        LOG_FLUSH();
    }

    return nextIt;
}

template <typename T>
ByteBuffer::iterator
ConfigParameter<std::vector<T>>::write(ByteBuffer::iterator &it) {

    auto vector = this->get();
    auto nextIt = ConfigParameterBase::write(it);

    char checksum = checksum::Checksum(checksum::Checksum::CRC8)
                        .calculate<typename std::vector<T>::iterator>(vector.begin(), vector.end());

    *nextIt++ = vector.size();

    for (unsigned char ix = 0; ix < vector.size(); ++ix)
    {
        for (unsigned char jx = 0; jx < sizeof(T); ++jx) {

            *nextIt++ = vector[ix];
        }
    }

    *nextIt++ = checksum;

    LOG_ADD("CFG => W [%02d]:", mId);
    for (auto &value : vector) {
        LOG_ADD(" %x", value);
    }
    LOG_ADD(", sz=%d, CS=0x%X", vector.size(), checksum);
    LOG_FLUSH();

    return nextIt;
}

// Class ConfigParameter<PersistCounter>

template <>
class ConfigParameter<PersistCounter>
  : public ConfigParameterBase,
    public ConfigParameterValue<PersistCounter>
{
public:
    ConfigParameter(unsigned char id = INVALID_ID);
    ConfigParameter(unsigned char id, const PersistCounter& counter);

    ByteBuffer::iterator read(ByteBuffer::iterator &it);
    ByteBuffer::iterator write(ByteBuffer::iterator &it);
};

} // namespace

#pragma pop_macro("LOG_MODULE")
