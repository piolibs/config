/*
 * Copyright (C) 2026 Dmitry Korobkov <dmitry.korobkov.nn@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#undef LOG_MODULE
#define LOG_MODULE "CFG:F"

#include "Flash.h"

#if defined(ESP8266)

#include <Arduino.h>
#include <console.h>

using namespace config;

FlashEsp::FlashEsp(uint32_t address, uint32_t size)
  : mAddress(address),
    mSectorCount(size / SPI_FLASH_SEC_SIZE)
{}

uint32_t FlashEsp::sectorSize() const
{
    return SPI_FLASH_SEC_SIZE;
}

uint16_t FlashEsp::sectorCount() const
{
    return mSectorCount;
}

bool FlashEsp::read(uint32_t offset, uint32_t *data, size_t size)
{
    return ESP.flashRead(mAddress + offset, data, size);
}

bool FlashEsp::write(uint32_t offset, const uint32_t *data, size_t size)
{
    if (!ESP.flashWrite(mAddress + offset, data, size))
    {
        LOGE("write failed: offset=%u, size=%u", offset, size);
        return false;
    }
    return true;
}

bool FlashEsp::erase(uint16_t sector)
{
    if (!ESP.flashEraseSector(mAddress / SPI_FLASH_SEC_SIZE + sector))
    {
        LOGE("erase failed: sector=%u", sector);
        return false;
    }
    return true;
}

#endif // ESP8266
