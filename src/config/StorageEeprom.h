/*
 * Copyright (C) 2026 Dmitry Korobkov <dmitry.korobkov.nn@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include <console.h>

#include "ByteBuffer.h"

namespace config {

class StorageEeprom : public ByteBuffer {
public:
    StorageEeprom(unsigned short size);
    ~StorageEeprom() = default;

    const char read(unsigned short index);
    void write(unsigned short index, const char value);

    void commit();
    unsigned short size();

    void dump(unsigned int limit);

private:
    unsigned short mSize;
};

} // namespace
