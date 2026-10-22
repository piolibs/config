/*
 * Copyright (C) 2026 Dmitry Korobkov <dmitry.korobkov.nn@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace config {

/*
 * A range of NOR flash split into sectors. Erase sets a whole sector to 0xFF,
 * write can only clear bits. Offsets are relative to the start of the range,
 * offsets and sizes are multiples of 4.
 */
class Flash {
public:
    virtual ~Flash() = default;

    virtual uint32_t sectorSize() const = 0;
    virtual uint16_t sectorCount() const = 0;

    virtual bool read(uint32_t offset, uint32_t *data, size_t size) = 0;
    virtual bool write(uint32_t offset, const uint32_t *data, size_t size) = 0;
    virtual bool erase(uint16_t sector) = 0;
};

#if defined(ESP8266)

/*
 * SPI flash of the ESP8266. The range must be sector aligned and unused by
 * the sketch, OTA or a filesystem, e.g. the FS area when no FS is mounted:
 *
 *   #include <flash_hal.h>
 *   config::FlashEsp flash(FS_PHYS_ADDR, FS_PHYS_SIZE);
 */
class FlashEsp : public Flash {
public:
    FlashEsp(uint32_t address, uint32_t size);

    uint32_t sectorSize() const override;
    uint16_t sectorCount() const override;

    bool read(uint32_t offset, uint32_t *data, size_t size) override;
    bool write(uint32_t offset, const uint32_t *data, size_t size) override;
    bool erase(uint16_t sector) override;

private:
    uint32_t mAddress;
    uint16_t mSectorCount;
};

#endif // ESP8266

} // namespace
