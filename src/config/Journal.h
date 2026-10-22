/*
 * Copyright (C) 2026 Dmitry Korobkov <dmitry.korobkov.nn@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include <cstdint>
#include <vector>

#include "Flash.h"

namespace config {

/*
 * Append-only log of records kept in a ring of flash sectors.
 *
 * Records are appended to the active sector without erasing it, so a sector
 * is erased only once per sector-full of changes. When the active sector is
 * full, the caller writes a snapshot of all values into the next sector of
 * the ring (beginSnapshot, append..., endSnapshot), which spreads the wear
 * over all sectors.
 *
 * Sector:  [header][record][record]...[0xFF...]
 * Header:  magic, sequence, crc32(magic, sequence)
 * Record:  size | ~size << 16, payload padded to 4 bytes, crc32(payload)
 *
 * The snapshot header is written last, so an interrupted snapshot leaves the
 * previous sector active. A record torn by a power loss fails its CRC and is
 * skipped; a torn record header ends the log and forces the next write to
 * start a new sector.
 */
class Journal {
public:
    explicit Journal(Flash &flash);

    // Find the active sector and rewind to its first record.
    // Returns false if the flash holds no journal yet.
    bool open();

    // Read the next valid record, returns false at the end of the log
    bool next(std::vector<char> &payload);

    // Append a record, returns false if the current sector can't take it
    bool append(const char *data, size_t size);

    // Start a snapshot in the next sector of the ring
    bool beginSnapshot();
    // Make the snapshot the active sector
    bool endSnapshot();

    // Bytes a record with the given payload size takes in flash
    static uint32_t recordLength(size_t size);
    // Space for records in a sector
    uint32_t capacity() const;

private:
    // Record length in flash, 0 for free space, -1 for a broken header
    int32_t readRecord(uint32_t offset, std::vector<char> *payload);

    uint32_t address(uint32_t offset) const;

private:
    static constexpr uint32_t MAGIC = 0x47464E43;  // "CNFG"
    static constexpr uint32_t HEADER_SIZE = 3 * sizeof(uint32_t);

    Flash &mFlash;
    bool mOpened;

    int32_t mActive;       // active sector, -1 if none
    uint32_t mSequence;    // sequence number of the active sector

    int32_t mSector;       // sector taking appends (active or snapshot)
    uint32_t mWriteOffset;
    uint32_t mReadOffset;
    bool mWritable;        // false after a broken record header
};

} // namespace
