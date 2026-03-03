/*
 * Copyright (C) 2026 Dmitry Korobkov <dmitry.korobkov.nn@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#undef LOG_MODULE
#define LOG_MODULE "CFG:C"

#include <macros/byte.h>
#include <console.h>

#include "PersistCounter.h"

using namespace config;

PersistCounter::PersistCounter(Type value, unsigned char capacity)
    : mValue(value),
      mCapacity(capacity),
      mSlot(0)
{
}

ByteBuffer::iterator PersistCounter::read(ByteBuffer::iterator &it)
{
    auto nextIt = it;

    LOGV("read begin: cursor=%u", nextIt.mCursor);

    if ((*nextIt++ == FLAG) && (*nextIt++ == sizeof(Type)))
    {
        mCapacity = *nextIt++;

        for (unsigned char sx = 0; sx < mCapacity; ++sx)
        {
            mValue = 0;

            for (unsigned char ix = 0; ix < sizeof(Type); ++ix)
            {
                mValue |= BYTE_SET(ix, 0x00, *nextIt++);
            }

            if (*nextIt == FLAG)
            {
                mSlot = sx;

                LOGD("read: capacity=%u, slot=%u, value=%u", mCapacity, mSlot, mValue);

                // Skip rest slots and flag
                nextIt += ((mCapacity - 1) - sx) * sizeof(Type) + 1;

                LOGV("read end: cursor=%u", nextIt.mCursor);
                return nextIt;
            }
        }
    }

    LOGE("read failed: start flag not found");
    return it;
}

ByteBuffer::iterator PersistCounter::write(ByteBuffer::iterator &it)
{
    auto nextIt = it;

    LOGV("write: cursor=%u", nextIt.mCursor);

    *nextIt++ = FLAG;
    *nextIt++ = sizeof(Type);
    *nextIt++ = mCapacity;

    mValue %= PersistCounter::MASK;
    mSlot = (mSlot + 1) % mCapacity;

    nextIt += mSlot * sizeof(Type);

    LOGD("write: capacity=%u, slot=%u, value=%u", mCapacity, mSlot, mValue);

    for (unsigned char ix = 0; ix < sizeof(Type); ++ix)
    {
        *nextIt++ = NBYTE(ix, mValue);
    }

    *nextIt++ = FLAG;

    // Skip next slots
    nextIt += ((mCapacity - 1) - mSlot) * sizeof(Type);

    LOGV("write: cursor=%u", nextIt.mCursor);
    return nextIt;
}

PersistCounter &PersistCounter::operator++()
{
    ++mValue;
    return *this;
}

PersistCounter &PersistCounter::operator+=(Type value)
{
    mValue += value;
    return *this;
}

PersistCounter &PersistCounter::operator=(const Type &value)
{
    mValue = value;
    return *this;
}

PersistCounter::operator Type() const
{
    return mValue;
}

const PersistCounter::Type &PersistCounter::get()
{
    return mValue;
}

void PersistCounter::set(PersistCounter::Type value)
{
    mValue = value % MASK;
}
