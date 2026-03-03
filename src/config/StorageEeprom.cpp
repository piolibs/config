/*
 * Copyright (C) 2026 Dmitry Korobkov <dmitry.korobkov.nn@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#undef LOG_MODULE
#define LOG_MODULE "CFG:E"

#include <Arduino.h>
#include <EEPROM.h>
#include <console.h>

#include "StorageEeprom.h"

using namespace console;
using namespace config;

StorageEeprom::StorageEeprom(unsigned short size)
  : mSize(size)
{
    EEPROM.begin(size);
}

unsigned short StorageEeprom::size()
{
    return mSize;
}

const char StorageEeprom::read(unsigned short index)
{
    return EEPROM.read(index);
}

void StorageEeprom::write(unsigned short index, const char value)
{
    if (EEPROM.read(index) != value) {
        EEPROM.write(index, value);
    }
}

void StorageEeprom::commit()
{
    EEPROM.commit();
}

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
void StorageEeprom::dump(unsigned int limit)
{
    LOGD_ADD("** EEPROM (sz:%u) **", mSize);

    for (size_t ix = 0; (ix < mSize) && (ix < limit); ++ix)
    {
        if ((ix % 16) == 0)
        {
            LOGD_FLUSH();
            LOGD_ADD("%3u: ", ix);
        }
        else if ((ix % 4) == 0)
        {
            LOGD_ADD(" ");
        }

        LOGD_ADD("%02x", EEPROM.read(ix));
    }

    LOGD_FLUSH();
}
#else
void StorageEeprom::dump(unsigned int limit) {}
#endif
