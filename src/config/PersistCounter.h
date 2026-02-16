/*
 * Copyright (C) 2026 Dmitry Korobkov <dmitry.korobkov.nn@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include <Arduino.h>

#include "ByteBuffer.h"

namespace config
{
    class PersistCounter
    {
    public:
        using Type = unsigned int;
        static_assert(std::is_unsigned_v<Type>, "Must be unsigned!");

        explicit PersistCounter(unsigned char size);
        ~PersistCounter() = default;

        PersistCounter(const PersistCounter &) = default;

        operator Type() const;
        PersistCounter &operator=(const Type &value);

        PersistCounter &operator++();
        PersistCounter &operator+=(Type value);

        ByteBuffer::iterator read(ByteBuffer::iterator &it);
        ByteBuffer::iterator write(ByteBuffer::iterator &it);

        const Type& get();
        void set(Type value);

    private:
        static constexpr char FLAG = 0x80;
        static constexpr Type MASK = (Type)(-1UL) >> 2;

    private:
        Type mValue;

        unsigned char mSize;
        unsigned char mSlot;
    };

} // namespace
