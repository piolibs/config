/*
 * Copyright (C) 2026 Dmitry Korobkov <dmitry.korobkov.nn@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <algorithm>
#include <memory>
#include <console.h>
#include <macros.h>

#include "ByteBuffer.h"

using namespace console;
using namespace config;

// Class ByteBuffer

ByteBuffer::iterator ByteBuffer::begin()
{
    return ByteBuffer::iterator(this, 0);
}

ByteBuffer::iterator ByteBuffer::end()
{
    return ByteBuffer::iterator(this, size());
}

unsigned short ByteBuffer::read(const iterator& it, char* value_p, unsigned short length)
{
    iterator retIt = it;

    for (unsigned short ix = 0; ix < length; ++ix)
    {
        value_p[ix] = *retIt++;
    }

    return length;
}

unsigned short ByteBuffer::write(const iterator& it, const char* value_p, unsigned short length)
{
    iterator retIt = it;

    for (unsigned short ix = 0; ix < length; ++ix)
    {
        *retIt++ = value_p[ix];
    }

    return length;
}

// Class ByteBuffer::reference

ByteBuffer::reference::reference(iterator& it) : mIter(it) {}

ByteBuffer::iterator& ByteBuffer::reference::operator=(const char value)
{
    if (mIter != mIter.mBuffer_p->end())
    {
        mIter.mBuffer_p->write(mIter.mCursor, value);
    }

    return mIter;
}

ByteBuffer::reference::operator char() const
{
    if (mIter != mIter.mBuffer_p->end())
    {
        return mIter.mBuffer_p->read(mIter.mCursor);
    }

    return (-1);
}

// Class ByteBuffer::iterator

ByteBuffer::iterator::iterator(ByteBuffer* buffer_p, difference_type cursor)
    : mBuffer_p(buffer_p), mCursor(cursor)
{}

ByteBuffer::iterator ByteBuffer::iterator::operator++(int)
{
    return ByteBuffer::iterator(mBuffer_p, mCursor++);
}

ByteBuffer::iterator& ByteBuffer::iterator::operator++()
{
    mCursor += (*this != mBuffer_p->end()) ? 1 : 0;
    return *this;
}

ByteBuffer::iterator& ByteBuffer::iterator::operator+=(const difference_type distance)
{
    auto end = mBuffer_p->end();
    mCursor = std::min<short>(mCursor + distance, end.mCursor);

    return *this;
}

ByteBuffer::iterator ByteBuffer::iterator::operator+(const difference_type distance)
{
    auto end = mBuffer_p->end();
    auto cursor = std::min<short>(mCursor + distance, end.mCursor);

    return iterator(mBuffer_p, cursor);
}

bool ByteBuffer::iterator::operator==(const ByteBuffer::iterator& other) const
{
    return this->mCursor == other.mCursor;
}

bool ByteBuffer::iterator::operator!=(const ByteBuffer::iterator& other) const
{
    return !(*this == other);
}

ByteBuffer::reference ByteBuffer::iterator::operator*()
{
    return ByteBuffer::reference(*this);
}

bool ByteBuffer::iterator::isValid() const
{
    return (*this != mBuffer_p->end());
}
