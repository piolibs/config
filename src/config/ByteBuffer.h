/*
 * Copyright (C) 2026 Dmitry Korobkov <dmitry.korobkov.nn@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include <iterator>
#include <memory>
#include <macros.h>

namespace config {

class ByteBuffer
{
public:
    class iterator;
    class reference;

    iterator begin();
    iterator end();

    unsigned short read(const iterator& it, char* value_p, unsigned short length);
    unsigned short write(const iterator& it, const char* value_p, unsigned short length);

    template <typename Type>
    unsigned short write(const iterator& it, const Type value);

    template <typename Type>
    unsigned short read(const iterator& it, Type* value_p);

    template <typename Type>
    Type read(const iterator& it);

public:
    virtual const char read(unsigned short index) = 0;
    virtual void write(unsigned short index, const char value) = 0;

    virtual void commit() = 0;
    virtual unsigned short size() = 0;
};

class ByteBuffer::reference
{
public:
    reference(iterator& it) noexcept;

    reference(const reference&) = delete;
    reference& operator=(const reference&) = delete;

    reference(reference&&) = delete;
    reference& operator=(reference&&) = delete;

    iterator& operator=(const char value) noexcept;
    operator char() const noexcept;

private:
    iterator& mIter;
};

class ByteBuffer::iterator
{
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = char;
    using difference_type = unsigned short;
    using pointer = const char*;
    using reference = ByteBuffer::reference;

    iterator(ByteBuffer* buffer_p, difference_type cursor = 0);

    iterator operator++(int); /* postfix */
    iterator& operator++();   /* prefix */
    iterator& operator+=(const difference_type distance);

    iterator operator+(const difference_type distance);

    bool operator==(const iterator& other) const;
    bool operator!=(const iterator& other) const;

    reference operator*() noexcept;

    bool isValid() const;

public:
    friend reference;

    ByteBuffer* mBuffer_p;
    difference_type mCursor;
};

template <typename Type>
unsigned short ByteBuffer::read(const iterator& it, Type* value_p)
{
    return read(it, reinterpret_cast<char*>(value_p), sizeof(Type));
}

template <typename Type>
Type ByteBuffer::read(const iterator& it)
{
    Type value;
    read(it, reinterpret_cast<char*>(&value), sizeof(Type));
    return value;
}

template <typename Type>
unsigned short ByteBuffer::write(const iterator& it, const Type value)
{
    return write(it, reinterpret_cast<const char*>(&value), sizeof(Type));
}

}  // namespace
