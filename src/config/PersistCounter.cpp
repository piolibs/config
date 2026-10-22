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

PersistCounter::PersistCounter(Type value)
    : mValue(value)
{
}

ByteBuffer::iterator PersistCounter::read(ByteBuffer::iterator &it)
{
    auto nextIt = it;

    mValue = 0;
    for (unsigned char ix = 0; ix < sizeof(Type); ++ix)
    {
        mValue |= BYTE_SET(ix, 0x00, *nextIt++);
    }

    LOGD("read: value=%u", mValue);
    return nextIt;
}

ByteBuffer::iterator PersistCounter::write(ByteBuffer::iterator &it)
{
    auto nextIt = it;

    for (unsigned char ix = 0; ix < sizeof(Type); ++ix)
    {
        *nextIt++ = NBYTE(ix, mValue);
    }

    LOGD("write: value=%u", mValue);
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
    mValue = value;
}
