/*
 * Copyright (C) 2026 Dmitry Korobkov <dmitry.korobkov.nn@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#undef LOG_MODULE
#define LOG_MODULE "CFG:P"

#include <vector>
#include <console.h>
#include <checksum.h>

#include "PersistCounter.h"
#include "ConfigParameter.h"

using namespace console;
using namespace checksum;
using namespace config;

// Class ConfigParameterBase

ConfigParameterBase::ConfigParameterBase(ConfigParameterType type, unsigned char id, bool isValid = false)
  : mType(type), mId(id), mIsValid(isValid)
{}

unsigned char ConfigParameterBase::getId()
{
    return this->mId;
}

ConfigParameterType ConfigParameterBase::getType()
{
    return this->mType;
}

bool ConfigParameterBase::isValid()
{
    return mIsValid;
}

ByteBuffer::iterator ConfigParameterBase::read(ByteBuffer::iterator &it)
{
    ByteBuffer::iterator nextIt = it;

    char id = *nextIt++;
    char type = *nextIt++;

    if (id != mId)
    {
        LOG("Skip parameter: id=%d (!= %d)", mId, id);
        return it;
    }

    if (type != static_cast<char>(mType))
    {
        LOG("Skip parameter: id=%d, type=%d (!= %d)", mId, mType, type);
        return it;
    }

    return nextIt;
}

ByteBuffer::iterator ConfigParameterBase::write(ByteBuffer::iterator &it)
{
    ByteBuffer::iterator nextIt = it;

    /* LOG("write: id=0x%x, type=0x%x", mId, mType); */

    *nextIt++ = static_cast<char>(mId);
    *nextIt++ = static_cast<char>(mType);

    return nextIt;
}

// Class ConfigParameter<char>

template <>
ConfigParameter<char>::ConfigParameter(unsigned char id)
  : ConfigParameterBase(ConfigParameterType::BYTE, id),
    ConfigParameterValue(0)
{};

template <>
ConfigParameter<char>::ConfigParameter(unsigned char id, const char &value)
  : ConfigParameterBase(ConfigParameterType::BYTE, id),
    ConfigParameterValue(value)
{};

template <>
ByteBuffer::iterator ConfigParameter<char>::read(ByteBuffer::iterator &it)
{
    auto nextIt = ConfigParameterBase::read(it);
    LOGV("read begin: id=%u, cursor=%u", getId(), nextIt.mCursor);

    if (nextIt != it)
    {
        char value = *nextIt++;

        char checksum = *nextIt++;
        char expected = Checksum(Checksum::CRC8)
                            .calculate(reinterpret_cast<char *>(&value), sizeof(char));
        if (checksum != expected)
        {
            LOGE("read failed, invalid checksum: id=%d, cursor=%u, cs=0x%X",
                 getId(), nextIt.mCursor, checksum);
            return it;
        }

        LOGI("read: id=%u, value=%u, cs=%u", getId(), value, checksum);
        set(value);
    }

    LOGV("read end: id=%u, cursor=%u", getId(), nextIt.mCursor);
    return nextIt;
}

template <>
ByteBuffer::iterator ConfigParameter<char>::write(ByteBuffer::iterator &it)
{
    auto value = this->get();
    auto nextIt = ConfigParameterBase::write(it);

    LOGV("write begin: id=%u, cursor=%u", getId(), nextIt.mCursor);

    char checksum = Checksum(Checksum::CRC8)
        .calculate(reinterpret_cast<char *>(&value), sizeof(char));

    *nextIt++ = value;
    *nextIt++ = checksum;

    LOGI("write: id=%u, value=%u, cs=0x%X", getId(), value, checksum);
    LOGV("write end: id=%u, cursor=%u", getId(), nextIt.mCursor);

    return nextIt;
}

// Class ConfigParameter<int>

template <>
ConfigParameter<int>::ConfigParameter(unsigned char id)
  : ConfigParameterBase(ConfigParameterType::NUMBER, id),
    ConfigParameterValue(0)
{};

template <>
ConfigParameter<int>::ConfigParameter(unsigned char id, const int &value)
  : ConfigParameterBase(ConfigParameterType::NUMBER, id),
    ConfigParameterValue(value)
{};

template <>
ByteBuffer::iterator ConfigParameter<int>::read(ByteBuffer::iterator &it)
{
    auto nextIt = ConfigParameterBase::read(it);
    LOGV("read begin: id=%u, cursor=%u", getId(), nextIt.mCursor);

    if (nextIt != it)
    {
        int value = 0;

        for (unsigned char ix = 0; ix < sizeof(int); ++ix) {

            value |= BYTE_SET(ix, 0x00, *nextIt++);
        }

        char checksum = *nextIt++;
        char expected = Checksum(Checksum::CRC8)
                            .calculate(reinterpret_cast<char *>(&value), sizeof(int));
        if (checksum != expected)
        {
            LOGE("read failed, invalid checksum: id=%d, cursor=%u, cs=0x%X",
                 getId(), nextIt.mCursor, checksum);
            return it;
        }

        LOGI("read: id=%u, value=%u, cs=%u", getId(), value, checksum);
        set(value);
    }

    LOGV("read end: id=%u, cursor=%u", getId(), nextIt.mCursor);
    return nextIt;
}

template <>
ByteBuffer::iterator ConfigParameter<int>::write(ByteBuffer::iterator &it)
{
    auto value = this->get();
    auto nextIt = ConfigParameterBase::write(it);

    LOGV("write begin: id=%u, cursor=%u", getId(), nextIt.mCursor);

    char checksum = Checksum(Checksum::CRC8)
        .calculate(reinterpret_cast<char*>(&value), sizeof(int));

    for (unsigned char ix = 0; ix < sizeof(int); ++ix)
    {
        *nextIt++ = NBYTE(ix, value);
    }

    *nextIt++ = checksum;

    LOGI("write: id=%u, value=%u, cs=0x%X", getId(), value, checksum);
    LOGV("write end: id=%u, cursor=%u", getId(), nextIt.mCursor);

    return nextIt;
}

// Class ConfigParameter<float>

template <>
ConfigParameter<float>::ConfigParameter(unsigned char id)
    : ConfigParameterBase(ConfigParameterType::FLOAT, id),
      ConfigParameterValue(0.0f){};

template <>
ConfigParameter<float>::ConfigParameter(unsigned char id, const float &value)
    : ConfigParameterBase(ConfigParameterType::FLOAT, id),
      ConfigParameterValue(value){};

template <>
ByteBuffer::iterator ConfigParameter<float>::read(ByteBuffer::iterator &it)
{
    auto nextIt = ConfigParameterBase::read(it);
    LOGV("read begin: id=%u, cursor=%u", getId(), nextIt.mCursor);

    if (nextIt != it)
    {
        float value = 0.0;
        uint32_t data = 0;

        for (unsigned char ix = 0; ix < sizeof(float); ++ix)
        {
            data |= BYTE_SET(ix, 0x00, *nextIt++);
        }

        value = static_cast<float>(data);

        char checksum = *nextIt++;
        char expected = Checksum(Checksum::CRC8)
                            .calculate(reinterpret_cast<char *>(&value), sizeof(float));
        if (checksum != expected)
        {
            LOGE("read failed, invalid checksum: id=%d, cursor=%u, cs=0x%X",
                 getId(), nextIt.mCursor, checksum);
            return it;
        }

        LOGI("read: id=%u, value=%.2f, cs=%u", getId(), value, checksum);
        set(value);
    }

    LOGV("read end: id=%u, cursor=%u", getId(), nextIt.mCursor);
    return nextIt;
}

template <>
ByteBuffer::iterator ConfigParameter<float>::write(ByteBuffer::iterator &it)
{
    auto value = this->get();
    auto nextIt = ConfigParameterBase::write(it);

    LOGV("write begin: id=%u, cursor=%u", getId(), nextIt.mCursor);

    uint32_t data = static_cast<uint32_t>(value);

    for (unsigned char ix = 0; ix < sizeof(data); ++ix)
    {
        *nextIt++ = NBYTE(ix, data);
    }

    char checksum = Checksum(Checksum::CRC8)
                        .calculate(reinterpret_cast<char *>(&value), sizeof(float));

    *nextIt++ = checksum;

    LOGI("write: id=%u, value=%.2f, cs=0x%X", getId(), value, checksum);
    LOGV("write end: id=%u, cursor=%u", getId(), nextIt.mCursor);

    return nextIt;
}

// Class ConfigParameter<string>

template <>
ConfigParameter<std::string>::ConfigParameter(unsigned char id)
  : ConfigParameterBase(ConfigParameterType::STRING, id),
    ConfigParameterValue("")
{};

template <>
ConfigParameter<std::string>::ConfigParameter(unsigned char id,
                                              const std::string &value)
  : ConfigParameterBase(ConfigParameterType::STRING, id),
    ConfigParameterValue(value)
{};

template <>
ByteBuffer::iterator
ConfigParameter<std::string>::read(ByteBuffer::iterator &it)
{
    auto nextIt = ConfigParameterBase::read(it);

    LOGV("read begin: id=%u, cursor=%u", getId(), nextIt.mCursor);

    if (nextIt != it)
    {
        std::string value;
        char length = *nextIt++;

        for (unsigned char ix = 0; ix < length; ++ix)
        {
            value += *nextIt++;
        }

        char checksum = *nextIt++;
        char expected = Checksum(Checksum::CRC8)
            .calculate(value.c_str(), value.length());

        if (checksum != expected)
        {
            LOGE("read failed, invalid checksum: id=%d, cursor=%u, cs=0x%X",
                 getId(), nextIt.mCursor, checksum);
            return it;
        }

        LOGI("read: id=%u, value='%s', cs=%u", getId(), value.c_str(), checksum);
        set(value);
    }

    LOGV("read end: id=%u, cursor=%u", getId(), nextIt.mCursor);
    return nextIt;
}

template <>
ByteBuffer::iterator
ConfigParameter<std::string>::write(ByteBuffer::iterator &it)
{
    auto value = this->get();
    auto nextIt = ConfigParameterBase::write(it);

    LOGV("write begin: id=%u, cursor=%u", getId(), nextIt.mCursor);

    char checksum = Checksum(Checksum::CRC8)
        .calculate(value.c_str(), value.length());

    *nextIt++ = value.length();

    for (unsigned char ix = 0; ix < value.length(); ++ix)
    {
        *nextIt++ = value[ix];
    }

    *nextIt++ = checksum;

    LOGI("write: id=%u, value='%s', cs=0x%X", getId(), value.c_str(), checksum);
    LOGV("write end: id=%u, cursor=%u", getId(), nextIt.mCursor);

    return nextIt;
}

// Class ConfigParameter<IPAddress>

template <>
ConfigParameter<IPAddress>::ConfigParameter(unsigned char id)
  : ConfigParameterBase(ConfigParameterType::IP_ADDRESS, id),
    ConfigParameterValue()
{};

template <>
ConfigParameter<IPAddress>::ConfigParameter(unsigned char id,
                                            const IPAddress &value)
  : ConfigParameterBase(ConfigParameterType::IP_ADDRESS, id),
    ConfigParameterValue(value)
{};

template <>
ByteBuffer::iterator
ConfigParameter<IPAddress>::read(ByteBuffer::iterator &it)
{
    auto nextIt = ConfigParameterBase::read(it);

    LOGV("read begin: id=%u, cursor=%u", getId(), nextIt.mCursor);

    if (nextIt != it)
    {
        unsigned int data = 0;

        for (unsigned char ix = 0; ix < sizeof(int); ++ix)
        {
            data |= BYTE_SET(ix, 0x00, *nextIt++);
        }

        char checksum = *nextIt++;
        char expected = Checksum(Checksum::CRC8)
                            .calculate(reinterpret_cast<char *>(&data), sizeof(int));
        if (checksum != expected)
        {
            LOGE("read failed, invalid checksum: id=%d, cursor=%u, cs=0x%X",
                 getId(), nextIt.mCursor, checksum);
            return it;
        }

        auto value = IPAddress(data);

        LOGI("read: id=%u, value='%s', cs=0x%X", getId(), value.toString().c_str(), checksum);
        set(value);
    }

    LOGV("read end: id=%u, cursor=%u", getId(), nextIt.mCursor);
    return nextIt;
}

template <>
ByteBuffer::iterator
ConfigParameter<IPAddress>::write(ByteBuffer::iterator &it)
{
    auto value = this->get();
    auto nextIt = ConfigParameterBase::write(it);

    LOGV("write begin: id=%u, cursor=%u", getId(), nextIt.mCursor);

    unsigned int address = static_cast<unsigned int>(value);
    char checksum = Checksum(Checksum::CRC8)
        .calculate(reinterpret_cast<char *>(&address), sizeof(int));

    for (unsigned char ix = 0; ix < sizeof(int); ++ix)
    {
        *nextIt++ = NBYTE(ix, address);
    }

    *nextIt++ = checksum;

    LOGI("write: id=%u, value='%s', cs=0x%X", getId(), value.toString().c_str(), checksum);
    LOGV("write end: id=%u, cursor=%u", getId(), nextIt.mCursor);

    return nextIt;
}

// Class ConfigParameter<PersistCounter>

ConfigParameter<PersistCounter>::ConfigParameter(unsigned char id)
  : ConfigParameterBase(ConfigParameterType::COUNTER, id),
    ConfigParameterValue(PersistCounter(1))
{
    LOGD("%s", __func__);
}

ConfigParameter<PersistCounter>::ConfigParameter(unsigned char id, const PersistCounter &counter)
  : ConfigParameterBase(ConfigParameterType::COUNTER, id),
    ConfigParameterValue(counter)
{
    LOGD("%s", __func__);
}

ByteBuffer::iterator ConfigParameter<PersistCounter>::read(ByteBuffer::iterator &it)
{
    auto nextIt = ConfigParameterBase::read(it);

    LOGV("read begin: id=%u, cursor=%u", getId(), nextIt.mCursor);

    if (nextIt != it)
    {
        auto counter = this->get();
        nextIt = counter.read(nextIt);

        char checksum = *nextIt++;
        char expected = Checksum(Checksum::CRC8)
                            .calculate(reinterpret_cast<const char *>(&counter.get()),
                                       sizeof(PersistCounter::Type));
        if (checksum != expected)
        {
            LOGE("read failed, invalid checksum: id=%d, cursor=%u, cs=0x%X",
                 getId(), nextIt.mCursor, checksum);
            return it;
        }

        LOGI("read: id=%u, value=%u, cs=%u", getId(), counter.get(), checksum);
        set(counter);
    }

    LOGV("read end: id=%u, cursor=%u", getId(), nextIt.mCursor);
    return nextIt;
}

ByteBuffer::iterator ConfigParameter<PersistCounter>::write(ByteBuffer::iterator &it)
{
    auto counter = this->get();
    auto nextIt = ConfigParameterBase::write(it);

    LOGV("write begin: id=%u, cursor=%u", getId(), nextIt.mCursor);

    nextIt = counter.write(nextIt);

    char checksum = Checksum(Checksum::CRC8)
        .calculate(reinterpret_cast<const char*>(&counter.get()),
                   sizeof(PersistCounter::Type));

    *nextIt++ = checksum;

    LOGI("write: id=%u, value=%u, cs=0x%X", getId(), counter.get(), checksum);
    LOGV("write end: id=%u, cursor=%u", getId(), nextIt.mCursor);
    return nextIt;
}
