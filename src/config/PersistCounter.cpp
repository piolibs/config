/*
 * Copyright (C) 2026 Dmitry Korobkov <dmitry.korobkov.nn@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <macros/byte.h>
#include <console.h>

#include "PersistCounter.h"

using namespace config;

PersistCounter::PersistCounter(unsigned char size)
  : mValue(0),
    mSize(size),
    mSlot(0)
{
    LOG("PersistCounter (constructor), sz: %u", size);
}

ByteBuffer::iterator PersistCounter::read(ByteBuffer::iterator &it)
{
    auto nextIt = it;

    LOG(" [r] begin: %u", nextIt.mCursor);

    if ((*nextIt++ == FLAG) && (*nextIt++ == sizeof(Type)))
    {
        mSize = *nextIt++;

        for (unsigned char sx = 0; sx < mSize; ++sx)
        {
            for (unsigned char ix = 0; ix < sizeof(Type); ++ix)
            {
                mValue |= BYTE_SET(ix, 0x00, *nextIt++);
            }

            if (*nextIt == FLAG)
            {
                mSlot = sx;

                LOG(" [r] size:%u slot:%u value:%u", mSize, mSlot, mValue);

                // Skip rest slots and flag
                nextIt += ((mSize - 1) - sx) * sizeof(Type) + 1;

                LOG(" [r] end: %u", nextIt.mCursor);
                return nextIt;
            }
        }
    }

    LOG(" [r] not found, use defaults");

    // Set default values otherwise
    mValue = 0;
    mSlot = 0;

    LOG(" [r] end: %u", it.mCursor);
    return it;
}

ByteBuffer::iterator PersistCounter::write(ByteBuffer::iterator &it)
{
    auto nextIt = it;

    LOG(" [w] begin: %u", nextIt.mCursor);

    *nextIt++ = FLAG;
    *nextIt++ = sizeof(Type);
    *nextIt++ = mSize;

    mValue %= PersistCounter::MASK;
    mSlot = (mSlot + 1) % mSize;

    LOG(" [w] size:%u slot:%u value:%u", mSize, mSlot, mValue);

    nextIt += mSlot * sizeof(Type);

    for (unsigned char ix = 0; ix < sizeof(Type); ++ix)
    {
        *nextIt++ = NBYTE(ix, mValue);
    }

    *nextIt++ = FLAG;

    // Skip next slots
    nextIt += ((mSize - 1) - mSlot) * sizeof(Type);

    LOG(" [w] end: %u", nextIt.mCursor);
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

const PersistCounter::Type& PersistCounter::get()
{
    return mValue;
}

void PersistCounter::set(PersistCounter::Type value)
{
    mValue = value % MASK;
}
