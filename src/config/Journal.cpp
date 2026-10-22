/*
 * Copyright (C) 2026 Dmitry Korobkov <dmitry.korobkov.nn@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#undef LOG_MODULE
#define LOG_MODULE "CFG:J"

#include <cstring>

#include <Arduino.h>
#include <coredecls.h>
#include <console.h>

#include "Journal.h"

using namespace config;

namespace {

constexpr uint32_t BLANK = 0xFFFFFFFF;

uint32_t align4(uint32_t size)
{
    return (size + 3) & ~3u;
}

} // namespace

Journal::Journal(Flash &flash)
  : mFlash(flash),
    mOpened(false),
    mActive(-1),
    mSequence(0),
    mSector(-1),
    mWriteOffset(0),
    mReadOffset(0),
    mWritable(false)
{}

bool Journal::open()
{
    mOpened = true;
    mActive = -1;
    mSequence = 0;

    for (uint16_t sector = 0; sector < mFlash.sectorCount(); ++sector)
    {
        uint32_t header[3];

        if (!mFlash.read(sector * mFlash.sectorSize(), header, sizeof(header)) ||
            (header[0] != MAGIC) ||
            (header[2] != crc32(header, 2 * sizeof(uint32_t))))
        {
            continue;
        }

        // The newest sector wins, the signed difference survives a wrap
        if ((mActive < 0) || (static_cast<int32_t>(header[1] - mSequence) > 0))
        {
            mActive = sector;
            mSequence = header[1];
        }
    }

    mSector = mActive;
    mReadOffset = HEADER_SIZE;
    mWriteOffset = HEADER_SIZE;
    mWritable = false;

    if (mActive < 0)
    {
        LOGD("open: no journal");
        return false;
    }

    int32_t length;
    while ((length = readRecord(mWriteOffset, nullptr)) > 0)
    {
        mWriteOffset += length;
    }

    // Appending after a broken header is unsafe, the next write starts a new sector
    mWritable = (length == 0);

    LOGD("open: sector=%d, sequence=%u, size=%u, writable=%u",
         mActive, mSequence, mWriteOffset, mWritable);
    return true;
}

bool Journal::next(std::vector<char> &payload)
{
    while ((mActive >= 0) && (mReadOffset < mWriteOffset))
    {
        int32_t length = readRecord(mReadOffset, &payload);
        if (length <= 0)
        {
            break;
        }

        mReadOffset += length;

        // An empty payload is a record that failed its CRC
        if (!payload.empty())
        {
            return true;
        }
    }

    return false;
}

bool Journal::append(const char *data, size_t size)
{
    if (!mOpened)
    {
        open();
    }

    if ((mSector < 0) || !mWritable || (size == 0) || (size > 0xFFFF))
    {
        return false;
    }

    uint32_t length = recordLength(size);
    if (mWriteOffset + length > mFlash.sectorSize())
    {
        return false;
    }

    // Padding stays 0xFF, so it doesn't program any bits
    std::vector<uint32_t> record(length / sizeof(uint32_t), BLANK);
    record.front() = size | (~size << 16);
    memcpy(&record[1], data, size);
    record.back() = crc32(data, size);

    if (!mFlash.write(address(mWriteOffset), record.data(), length))
    {
        // The space may be partially programmed, continue in a new sector
        mWritable = false;
        return false;
    }

    mWriteOffset += length;
    return true;
}

bool Journal::beginSnapshot()
{
    if (!mOpened)
    {
        open();
    }

    if (mFlash.sectorCount() < 2)
    {
        LOGE("snapshot: at least 2 sectors are required");
        return false;
    }

    mSector = (mActive < 0) ? 0 : (mActive + 1) % mFlash.sectorCount();
    mWriteOffset = HEADER_SIZE;
    mWritable = mFlash.erase(mSector);

    LOGD("snapshot: sector=%d", mSector);
    return mWritable;
}

bool Journal::endSnapshot()
{
    uint32_t header[3] = {MAGIC, mSequence + 1, 0};
    header[2] = crc32(header, 2 * sizeof(uint32_t));

    if ((mSector < 0) || !mWritable || !mFlash.write(address(0), header, sizeof(header)))
    {
        return false;
    }

    mActive = mSector;
    mSequence = header[1];
    mReadOffset = HEADER_SIZE;
    return true;
}

uint32_t Journal::recordLength(size_t size)
{
    return sizeof(uint32_t) + align4(size) + sizeof(uint32_t);
}

uint32_t Journal::capacity() const
{
    return mFlash.sectorSize() - HEADER_SIZE;
}

int32_t Journal::readRecord(uint32_t offset, std::vector<char> *payload)
{
    uint32_t header = BLANK;

    if (offset + sizeof(header) > mFlash.sectorSize())
    {
        return 0;
    }

    if (!mFlash.read(address(offset), &header, sizeof(header)))
    {
        return -1;
    }

    if (header == BLANK)
    {
        return 0;
    }

    uint32_t size = header & 0xFFFF;
    uint32_t length = recordLength(size);

    if ((size == 0) ||
        ((header >> 16) != (~size & 0xFFFF)) ||
        (offset + length > mFlash.sectorSize()))
    {
        LOGE("broken record header: sector=%d, offset=%u", mSector, offset);
        return -1;
    }

    if (payload != nullptr)
    {
        payload->clear();

        std::vector<uint32_t> words(align4(size) / sizeof(uint32_t) + 1);
        if (!mFlash.read(address(offset + sizeof(uint32_t)), words.data(), words.size() * sizeof(uint32_t)))
        {
            return -1;
        }

        if (words.back() == crc32(words.data(), size))
        {
            auto bytes = reinterpret_cast<const char *>(words.data());
            payload->assign(bytes, bytes + size);
        }
        else
        {
            LOGE("broken record: sector=%d, offset=%u", mSector, offset);
        }
    }

    return length;
}

uint32_t Journal::address(uint32_t offset) const
{
    return mSector * mFlash.sectorSize() + offset;
}
