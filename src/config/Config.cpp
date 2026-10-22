/*
 * Copyright (C) 2026 Dmitry Korobkov <dmitry.korobkov.nn@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#undef LOG_MODULE
#define LOG_MODULE "CFG"

#include <string>
#include <vector>
#include <coredecls.h>
#include <console.h>

#include "Config.h"

using namespace console;

namespace config {

namespace {

// Largest serialized parameter: a 255-char string with id, type, length and CRC
constexpr size_t MAX_RECORD_SIZE = 260;

// ByteBuffer over a RAM array, holds a single serialized parameter
class MemoryBuffer : public ByteBuffer
{
public:
    MemoryBuffer(char* data, size_t size) : mData(data), mSize(size) {}

    const char read(unsigned short index) override
    {
        return (index < mSize) ? mData[index] : -1;
    }

    void write(unsigned short index, const char value) override
    {
        if (index < mSize)
        {
            mData[index] = value;
        }
    }

    void commit() override {}
    unsigned short size() override { return mSize; }

private:
    char* mData;
    unsigned short mSize;
};

} // namespace

Config::Config() : mParameters()
{
    add<char>(UNDEFINED, 0xA7);
}

bool Config::hasValue(unsigned char id) const
{
    LOGV("%s", __func__);
    auto it = mParameters.find(id);
    return (it == mParameters.end()) ? false : true;
}

bool Config::remove(unsigned char id)
{
    LOGV("%s", __func__);
    return mParameters.erase(id) > 0;
}

bool Config::read(Journal& journal)
{
    LOGV("%s", __func__);

    for (auto & [key, parameter] : mParameters)
    {
        parameter->mStoredCrc = 0;
    }

    if (!journal.open())
    {
        return false;
    }

    // Records are in write order, the last one of a parameter wins
    std::vector<char> payload;
    while (journal.next(payload))
    {
        auto it = mParameters.find(static_cast<unsigned char>(payload[0]));
        if (it == mParameters.end())
        {
            LOGD("read: id=%u is unknown, skip", static_cast<unsigned char>(payload[0]));
            continue;
        }

        MemoryBuffer buffer(payload.data(), payload.size());
        auto begin = buffer.begin();

        if (it->second->read(begin) != begin)
        {
            it->second->mStoredCrc = crc32(payload.data(), payload.size());
        }
    }

    return true;
}

bool Config::write(Journal& journal)
{
    LOGV("%s", __func__);

    std::vector<char> data(MAX_RECORD_SIZE);
    bool result = true;

    for (auto & [key, parameter] : mParameters)
    {
        size_t size = serialize(*parameter, data.data(), data.size());
        if (size == 0)
        {
            result = false;
            continue;
        }

        uint32_t crc = crc32(data.data(), size);
        if (crc == parameter->mStoredCrc)
        {
            continue;
        }

        if (!journal.append(data.data(), size))
        {
            return snapshot(journal) && result;
        }

        parameter->mStoredCrc = crc;
    }

    return result;
}

size_t Config::serialize(ConfigParameterBase& parameter, char* data, size_t size)
{
    MemoryBuffer buffer(data, size);
    auto begin = buffer.begin();

    // The iterator doesn't stop at the end, reaching it means overflow
    auto end = parameter.write(begin);
    if (end.mCursor >= size)
    {
        LOGE("serialize failed: id=%u is too big", parameter.getId());
        return 0;
    }

    return end.mCursor;
}

bool Config::snapshot(Journal& journal)
{
    LOGD("%s", __func__);

    std::vector<char> data(MAX_RECORD_SIZE);
    std::vector<uint32_t> crcs;

    // Don't erase a sector for a snapshot that can never fit
    uint32_t length = 0;
    for (auto & [key, parameter] : mParameters)
    {
        size_t size = serialize(*parameter, data.data(), data.size());
        length += (size > 0) ? Journal::recordLength(size) : 0;
    }

    if (length > journal.capacity())
    {
        LOGE("snapshot doesn't fit: size=%u, capacity=%u", length, journal.capacity());
        return false;
    }

    if (journal.beginSnapshot())
    {
        bool appended = true;

        for (auto & [key, parameter] : mParameters)
        {
            size_t size = serialize(*parameter, data.data(), data.size());
            if (size == 0)
            {
                crcs.push_back(0);
                continue;
            }

            if (!(appended = journal.append(data.data(), size)))
            {
                break;
            }

            crcs.push_back(crc32(data.data(), size));
        }

        if (appended && journal.endSnapshot())
        {
            auto crc = crcs.begin();
            for (auto & [key, parameter] : mParameters)
            {
                parameter->mStoredCrc = *crc++;
            }
            return true;
        }
    }

    LOGE("snapshot failed");

    // Keep using the previous sector
    journal.open();
    return false;
}

// --------------------------------------------------------

Config& getInstance()
{
    return Config::getInstance();
}

} // namespace