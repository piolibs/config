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
    FLOAT
};

class ConfigParameterBase {
public:
    static const unsigned char INVALID_ID = (-1);

    ConfigParameterBase(ConfigParameterType type, unsigned char id);
    virtual ~ConfigParameterBase() {}

    unsigned char getId();
    ConfigParameterType getType();

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

    T get() { return mValue; }
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
  : ConfigParameterBase(ConfigParameterType::ARRAY, id),
    ConfigParameterValue<std::vector<T>>()
{
    static_assert(std::is_arithmetic<T>::value, "Not an arithmetic type");
};

template <typename T>
ConfigParameter<std::vector<T>>::ConfigParameter(unsigned char id,
                                                 const std::vector<T> &value)
  : ConfigParameterBase(ConfigParameterType::ARRAY, id),
    ConfigParameterValue<std::vector<T>>(value)
{
    static_assert(std::is_arithmetic<T>::value, "Not an arithmetic type");
};

template <typename T>
ByteBuffer::iterator
ConfigParameter<std::vector<T>>::read(ByteBuffer::iterator &it)
{
    auto nextIt = ConfigParameterBase::read(it);

    LOGV("read begin: id=%u, cursor=%u", getId(), nextIt.mCursor);

    if (nextIt != it)
    {
        auto &vector = this->value();
        vector.clear();

        char length = *nextIt++;

        LOGI_ADD("read: id=%u, sz=%u, value={", getId(), length);
        for (unsigned char ix = 0; ix < length; ++ix) {

            T value = 0;
            for (unsigned char bx = 0; bx< sizeof(T); ++bx) {
                value |= BYTE_SET(bx, 0x00, *nextIt++);
            }

            LOGD_ADD(" %u", value);
            vector.push_back(value);
        }

        char checksum = *nextIt++;

        LOGI_ADD(" }, cs=0x%X", checksum);
        LOGI_FLUSH();

        char expected = checksum::Checksum(checksum::Checksum::CRC8)
                            .calculate<typename std::vector<T>::iterator>(vector.begin(), vector.end());
        if (checksum != expected) {

            LOGE("read failed: id=%d, cursor=%u, cs=0x%X",
                 getId(), nextIt.mCursor, checksum);
            return it;
        }
    }

    LOGV("read end: id=%u, cursor=%u", getId(), nextIt.mCursor);
    return nextIt;
}

template <typename T>
ByteBuffer::iterator
ConfigParameter<std::vector<T>>::write(ByteBuffer::iterator &it) {

    auto vector = this->get();
    auto nextIt = ConfigParameterBase::write(it);

    LOGV("write begin: id=%u, cursor=%u", getId(), nextIt.mCursor);

    char checksum = checksum::Checksum(checksum::Checksum::CRC8)
                        .calculate<typename std::vector<T>::iterator>(vector.begin(), vector.end());

    *nextIt++ = vector.size();

    LOGI_ADD("write: id=%u, sz=%u, value={", getId(), vector.size());
    for (unsigned char ix = 0; ix < vector.size(); ++ix)
    {
        LOGI_ADD(" %u", vector[ix]);
        for (unsigned char bx = 0; bx < sizeof(T); ++bx)
        {
            *nextIt++ = NBYTE(bx, vector[ix]);
        }
    }
    LOGI_ADD(" }, cs=0x%X", checksum);
    LOGI_FLUSH();

    *nextIt++ = checksum;

    LOGV("write end: id=%u, cursor=%u", getId(), nextIt.mCursor);

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
