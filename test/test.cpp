/*
 * Copyright (C) 2026 Dmitry Korobkov <dmitry.korobkov.nn@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <algorithm>
#include <cstring>
#include <vector>

#include <Arduino.h>
#include <coredecls.h>
#include <flash_hal.h>
#include <unity.h>

#include <console.h>
#include <config.h>

using namespace config;

enum CustomID : unsigned char
{
    ID1 = Config::ID::CUSTOM_START + 1,
    ID2, ID3, ID4, ID5, ID6, ID7, ID8, ID9,
    ID_EXTRA, ID_BIG1, ID_BIG2,
    ID_MAX
};

// NOR flash emulated in RAM: erase sets 0xFF, write only clears bits.
// Small sectors make the journal switch sectors after a few records.
class MemoryFlash : public Flash
{
public:
    static constexpr uint32_t SECTOR_SIZE = 512;

    explicit MemoryFlash(uint16_t sectors)
        : mData(sectors * SECTOR_SIZE, 0xFF), mErases(sectors, 0) {}

    uint32_t sectorSize() const override { return SECTOR_SIZE; }
    uint16_t sectorCount() const override { return mErases.size(); }

    bool read(uint32_t offset, uint32_t *data, size_t size) override
    {
        if ((offset % 4) || (size % 4) || (offset + size > mData.size()))
            return false;

        memcpy(data, &mData[offset], size);
        return true;
    }

    bool write(uint32_t offset, const uint32_t *data, size_t size) override
    {
        if ((offset % 4) || (size % 4) || (offset + size > mData.size()))
            return false;

        ++mWrites;

        // Power loss: only the first mBudget bytes get programmed
        size_t count = (mBudget < 0) ? size : std::min<size_t>(size, mBudget);
        if (mBudget >= 0)
            mBudget -= count;

        auto bytes = reinterpret_cast<const uint8_t *>(data);
        for (size_t ix = 0; ix < count; ++ix)
            mData[offset + ix] &= bytes[ix];

        return count == size;
    }

    bool erase(uint16_t sector) override
    {
        if ((sector >= mErases.size()) || mEraseFails)
            return false;

        std::fill_n(&mData[sector * SECTOR_SIZE], SECTOR_SIZE, 0xFF);
        ++mErases[sector];
        return true;
    }

    unsigned erases() const
    {
        unsigned total = 0;
        for (auto count : mErases)
            total += count;
        return total;
    }

    std::vector<uint8_t> mData;
    std::vector<unsigned> mErases;
    unsigned mWrites = 0;
    long mBudget = -1;  // bytes left before the power loss, -1 if unlimited
    bool mEraseFails = false;
};

// Returns the offset of a serialized parameter (id, type), or -1 if not found
int findParameter(const MemoryFlash &flash, unsigned char id, ConfigParameterType type)
{
    for (size_t ix = 0; ix + 1 < flash.mData.size(); ++ix)
    {
        if ((flash.mData[ix] == id) &&
            (flash.mData[ix + 1] == static_cast<uint8_t>(type)))
        {
            return ix;
        }
    }
    return -1;
}

// Rewrite the sequence number in a sector header
void setSequence(MemoryFlash &flash, uint16_t sector, uint32_t sequence)
{
    uint32_t header[3];
    memcpy(header, &flash.mData[sector * MemoryFlash::SECTOR_SIZE], sizeof(header));
    header[1] = sequence;
    header[2] = crc32(header, 2 * sizeof(uint32_t));
    memcpy(&flash.mData[sector * MemoryFlash::SECTOR_SIZE], header, sizeof(header));
}

// Offset of the first free word after the records of a sector
uint32_t usedSize(const MemoryFlash &flash, uint16_t sector)
{
    for (uint32_t offset = 12; offset < MemoryFlash::SECTOR_SIZE; offset += 4)
    {
        uint32_t word;
        memcpy(&word, &flash.mData[sector * MemoryFlash::SECTOR_SIZE + offset], sizeof(word));
        if (word == 0xFFFFFFFF)
            return offset;
    }
    return MemoryFlash::SECTOR_SIZE;
}

// Read the values back like after a reboot
bool reboot(Flash &flash)
{
    Journal journal(flash);
    return Config::getInstance().read(journal);
}

void setUp()
{
    TEST_LOG("setup fixture");

    Config::getInstance().remove(ID_EXTRA);
    Config::getInstance().remove(ID_BIG1);
    Config::getInstance().remove(ID_BIG2);

    Config &config = Config::getInstance()
        .add<char>(ID1, 1)
        .add<int>(ID2, 2)
        .add<std::string>(ID3, "test")
        .add<std::vector<int>>(ID4, {1, 2, 3})
        .add<IPAddress>(ID5, IPAddress(1, 2, 3, 4))
        .add<PersistCounter>(ID6, PersistCounter(5))
        .add<float>(ID7, 6.0)
        .add<int>(ID8, 8)
        .add<PersistCounter>(ID9, PersistCounter(9));

    (void)config;
}

void test_basic(void)
{
    TEST_LOG("--- TEST CASE: %s ---", __func__);

    Config &config = Config::getInstance();

    {
        auto actual = config.getOr<char>(ID1, -1);

        TEST_LOG("id=%u, value<char>=%u", ID1, actual);
        TEST_ASSERT_EQUAL(1, actual);
    }

    {
        auto actual = config.getOr<int>(ID2, -1);

        TEST_LOG("id=%u, value<int>=%d", ID2, actual);
        TEST_ASSERT_EQUAL(2, actual);
    }

    {
        auto actual = config.getOr<std::string>(ID3, "");

        TEST_LOG("id=%u, value<string>=%s", ID3, actual.c_str());
        TEST_ASSERT_EQUAL_STRING("test", actual.c_str());
    }

    {
        auto actual = config.getOr<std::vector<int>>(ID4, std::vector<int>());

        TEST_LOG("id=%u, value<vector>::size=%u", ID4, actual.size());
        TEST_ASSERT_EQUAL(actual.size(), 3);

        for (size_t ix = 0; ix < actual.size(); ++ix)
        {
            TEST_LOG("id=%u, value<vector>[%u]=%u", ID4, ix, actual[ix]);
        }
        int expected[] = {1, 2, 3};
        TEST_ASSERT_EQUAL_INT_ARRAY(expected, actual.data(), actual.size());
    }

    {
        auto actual = config.getOr<IPAddress>(ID5, IPAddress());

        TEST_LOG("id=%u, address=%s", ID5, actual.toString().c_str());
        TEST_ASSERT_EQUAL_STRING("1.2.3.4", actual.toString().c_str());
    }

    {
        TEST_ASSERT_TRUE(config.hasValue(ID6));
        auto actual = config.value<PersistCounter>(ID6);

        TEST_LOG("id=%u, counter=%u", ID6, actual);
        TEST_ASSERT_EQUAL(5, actual);
    }

    {
        auto actual = config.getOr<float>(ID7, -1.0);

        TEST_LOG("id=%u, value<float>=%f", ID7, actual);
        TEST_ASSERT_EQUAL(6.0, actual);
    }
}

void test_find(void)
{
    TEST_LOG("--- CASE: %s ---", __func__);

    Config &config = Config::getInstance();

    TEST_ASSERT_NOT_NULL(config.find<char>(ID1));
    TEST_ASSERT_NULL(config.find<char>(ID_MAX));
}

void test_has_value(void)
{
    TEST_LOG("--- CASE: %s ---", __func__);

    Config &config = Config::getInstance();

    {
        TEST_ASSERT_TRUE(config.hasValue(ID1));

        auto actual = config.value<char>(ID1);

        TEST_LOG("id=%u, value<char>=%u", ID1, actual);
        TEST_ASSERT_EQUAL(1, actual);
    }

    {
        auto actual = config.getOr<char>(ID1, -1);

        TEST_LOG("id=%u, value<char>=%u", ID1, actual);
        TEST_ASSERT_EQUAL(1, actual);
    }

    {
        char defaultValue = -1;
        auto actual = config.getOr<char>(ID1, defaultValue);

        TEST_LOG("id=%u, value<char>=%u", ID1, actual);
        TEST_ASSERT_EQUAL(1, actual);
    }
}

void test_modify(void)
{
    TEST_LOG("--- TEST CASE: %s ---", __func__);

    Config &config = Config::getInstance();

    {
        auto actual = config.getOr<float>(ID7, 0.0);
        TEST_ASSERT_NOT_EQUAL(0.0, actual);

        auto value = actual * 2;
        TEST_LOG("id=%u, value<float>: %.1f -> %.1f", ID7, actual, value);

        TEST_ASSERT_TRUE(config.hasValue(ID7));
        config.value<float>(ID7) = value;

        TEST_ASSERT_EQUAL(value, config.getOr<float>(ID7, 0.0));
    }
}

void test_read(void)
{
    TEST_LOG("--- TEST CASE: %s ---", __func__);

    MemoryFlash flash(4);
    Journal journal(flash);
    Config &config = Config::getInstance();

    TEST_ASSERT_FALSE(config.read(journal));

    {
        auto id = ID1;
        using type = char;

        TEST_ASSERT_TRUE(config.hasValue(id));

        config.value<type>(id) = 10;
        TEST_ASSERT_TRUE(config.write(journal));
        TEST_ASSERT_EQUAL(10, config.getOr<type>(id, 0.0));

        config.set<type>(id, 20);
        TEST_ASSERT_EQUAL(20, config.getOr<type>(id, 0.0));

        TEST_ASSERT_TRUE(config.read(journal));
        TEST_ASSERT_EQUAL(10, config.getOr<type>(id, 0.0));
    }

    {
        auto id = ID2;
        using type = int;

        TEST_ASSERT_TRUE(config.hasValue(id));

        config.value<type>(id) = 10;
        TEST_ASSERT_TRUE(config.write(journal));
        TEST_ASSERT_EQUAL(10, config.getOr<type>(id, 0.0));

        config.set<type>(id, 20);
        TEST_ASSERT_EQUAL(20, config.getOr<type>(id, 0.0));

        TEST_ASSERT_TRUE(config.read(journal));
        TEST_ASSERT_EQUAL(10, config.getOr<type>(id, 0.0));
    }

    {
        auto id = ID3;
        using type = std::string;

        TEST_ASSERT_TRUE(config.hasValue(id));

        config.value<type>(id) = "10";
        TEST_ASSERT_TRUE(config.write(journal));
        TEST_ASSERT_EQUAL_STRING("10", config.getOr<type>(id, "").c_str());

        config.set<type>(id, "20");
        TEST_ASSERT_EQUAL_STRING("20", config.getOr<type>(id, "").c_str());

        TEST_ASSERT_TRUE(config.read(journal));
        TEST_ASSERT_EQUAL_STRING("10", config.getOr<type>(id, "").c_str());
    }

    {
        auto id = ID4;
        using type = std::vector<int>;

        TEST_ASSERT_TRUE(config.hasValue(id));

        config.value<type>(id) = {10};
        TEST_ASSERT_TRUE(config.write(journal));
        {
            auto actual = config.value<type>(id);
            int expected[] = {10};
            TEST_ASSERT_EQUAL_INT_ARRAY(expected, actual.data(), actual.size());
        }

        config.set<type>(id, {20});
        {
            auto actual = config.value<type>(id);
            int expected[] = {20};
            TEST_ASSERT_EQUAL_INT_ARRAY(expected, actual.data(), actual.size());
        }

        TEST_ASSERT_TRUE(config.read(journal));
        {
            auto actual = config.value<type>(id);
            int expected[] = {10};
            TEST_ASSERT_EQUAL_INT_ARRAY(expected, actual.data(), actual.size());
        }
    }

    {
        auto id = ID5;
        using type = IPAddress;

        TEST_ASSERT_TRUE(config.hasValue(id));

        config.value<type>(id) = IPAddress(10, 2, 3, 4);
        TEST_ASSERT_TRUE(config.write(journal));
        {
            auto actual = config.getOr<IPAddress>(ID5, IPAddress());
            TEST_ASSERT_EQUAL_STRING("10.2.3.4", actual.toString().c_str());
        }

        config.set<type>(id, IPAddress(20, 2, 3, 4));
        {
            auto actual = config.getOr<IPAddress>(ID5, IPAddress());
            TEST_ASSERT_EQUAL_STRING("20.2.3.4", actual.toString().c_str());
        }

        TEST_ASSERT_TRUE(config.read(journal));
        {
            auto actual = config.getOr<IPAddress>(ID5, IPAddress());
            TEST_ASSERT_EQUAL_STRING("10.2.3.4", actual.toString().c_str());
        }
    }

    {
        auto id = ID6;
        using type = PersistCounter;

        TEST_ASSERT_TRUE(config.hasValue(id));

        config.value<type>(id) = 10;
        TEST_ASSERT_TRUE(config.write(journal));
        TEST_ASSERT_EQUAL(10, config.getOr<type>(id, 0));

        config.set<type>(id, 20);
        TEST_ASSERT_EQUAL(20, config.getOr<type>(id, 0));

        TEST_ASSERT_TRUE(config.read(journal));
        TEST_ASSERT_EQUAL(10, config.getOr<type>(id, 0));
    }

    {
        TEST_ASSERT_TRUE(config.hasValue(ID7));

        config.value<float>(ID7) = 10.0;
        TEST_ASSERT_TRUE(config.write(journal));
        TEST_ASSERT_EQUAL(10.0, config.getOr<float>(ID7, 0.0));

        config.set<float>(ID7, 20.0);
        TEST_ASSERT_EQUAL(20.0, config.getOr<float>(ID7, 0.0));

        TEST_ASSERT_TRUE(config.read(journal));
        TEST_ASSERT_EQUAL(10.0, config.getOr<float>(ID7, 0.0));
    }
}

void test_float_fraction(void)
{
    TEST_LOG("--- TEST CASE: %s ---", __func__);

    MemoryFlash flash(4);
    Journal journal(flash);
    Config &config = Config::getInstance();

    // A fractional value must survive a write/read round trip
    config.set<float>(ID7, 0.25);
    TEST_ASSERT_TRUE(config.write(journal));

    config.set<float>(ID7, 1.0);
    TEST_ASSERT_TRUE(reboot(flash));

    auto actual = config.getOr<float>(ID7, 0.0);
    TEST_LOG("id=%u, value<float>=%f", ID7, actual);
    TEST_ASSERT_EQUAL_FLOAT(0.25, actual);
}

void test_counter_value(void)
{
    TEST_LOG("--- TEST CASE: %s ---", __func__);

    MemoryFlash flash(4);
    Journal journal(flash);
    Config &config = Config::getInstance();

    // Assigning a plain number keeps the counter usable, any byte value is valid
    config.set<PersistCounter>(ID9, 0x180);
    TEST_ASSERT_TRUE(config.write(journal));

    config.set<PersistCounter>(ID9, 1);
    TEST_ASSERT_TRUE(reboot(flash));

    unsigned int actual = config.value<PersistCounter>(ID9);
    TEST_LOG("id=%u, counter=0x%X", ID9, actual);
    TEST_ASSERT_EQUAL_HEX32(0x180, actual);
}

void test_unchanged_write(void)
{
    TEST_LOG("--- TEST CASE: %s ---", __func__);

    MemoryFlash flash(4);
    Journal journal(flash);
    Config &config = Config::getInstance();

    TEST_ASSERT_FALSE(config.read(journal));
    TEST_ASSERT_TRUE(config.write(journal));

    unsigned writes = flash.mWrites;
    unsigned erases = flash.erases();

    // Nothing changed: no flash access at all
    TEST_ASSERT_TRUE(config.write(journal));
    TEST_ASSERT_EQUAL(writes, flash.mWrites);
    TEST_ASSERT_EQUAL(erases, flash.erases());

    // Also after a reboot
    Journal rebooted(flash);
    TEST_ASSERT_TRUE(config.read(rebooted));
    TEST_ASSERT_TRUE(config.write(rebooted));
    TEST_ASSERT_EQUAL(writes, flash.mWrites);
}

void test_changed_only(void)
{
    TEST_LOG("--- TEST CASE: %s ---", __func__);

    MemoryFlash flash(4);
    Journal journal(flash);
    Config &config = Config::getInstance();

    TEST_ASSERT_FALSE(config.read(journal));
    TEST_ASSERT_TRUE(config.write(journal));

    unsigned writes = flash.mWrites;
    unsigned erases = flash.erases();

    // One changed parameter: one record appended, nothing erased
    config.set<PersistCounter>(ID6, 100);
    TEST_ASSERT_TRUE(config.write(journal));

    TEST_ASSERT_EQUAL(writes + 1, flash.mWrites);
    TEST_ASSERT_EQUAL(erases, flash.erases());

    config.set<PersistCounter>(ID6, 0);
    TEST_ASSERT_TRUE(reboot(flash));
    TEST_ASSERT_EQUAL(100, config.getOr<PersistCounter>(ID6, 0));
}

void test_rotation(void)
{
    TEST_LOG("--- TEST CASE: %s ---", __func__);

    constexpr unsigned COUNT = 200;

    MemoryFlash flash(4);
    Journal journal(flash);
    Config &config = Config::getInstance();

    TEST_ASSERT_FALSE(config.read(journal));

    unsigned snapshot = 0;
    for (unsigned ix = 1; ix <= COUNT; ++ix)
    {
        config.set<PersistCounter>(ID6, ix);
        TEST_ASSERT_TRUE(config.write(journal));

        if (ix == 1)
            snapshot = usedSize(flash, 0);
    }

    // A sector is erased once per sector-full of records, not per write:
    // a snapshot, then 16-byte counter records up to the end of the sector
    unsigned records = (MemoryFlash::SECTOR_SIZE - snapshot) / Journal::recordLength(7);
    unsigned expected = (COUNT - 1) / (records + 1) + 1;

    TEST_LOG("writes=%u, snapshot=%u, records=%u, erases=%u", COUNT, snapshot, records, flash.erases());
    TEST_ASSERT_EQUAL(expected, flash.erases());

    // The wear is spread over the ring
    auto minmax = std::minmax_element(flash.mErases.begin(), flash.mErases.end());
    TEST_ASSERT_GREATER_THAN(0, *minmax.first);
    TEST_ASSERT_LESS_OR_EQUAL(1, *minmax.second - *minmax.first);

    config.set<PersistCounter>(ID6, 0);
    TEST_ASSERT_TRUE(reboot(flash));
    TEST_ASSERT_EQUAL(COUNT, config.getOr<PersistCounter>(ID6, 0));
    TEST_ASSERT_EQUAL_STRING("test", config.getOr<std::string>(ID3, "").c_str());
}

void test_power_loss_record(void)
{
    TEST_LOG("--- TEST CASE: %s ---", __func__);

    // Torn payload (header programmed) and torn header
    for (long budget : {6L, 2L})
    {
        MemoryFlash flash(4);
        Journal journal(flash);
        Config &config = Config::getInstance();

        config.set<PersistCounter>(ID6, 10);
        TEST_ASSERT_FALSE(config.read(journal));
        TEST_ASSERT_TRUE(config.write(journal));

        // Power is lost while the record is written
        config.set<PersistCounter>(ID6, 20);
        flash.mBudget = budget;
        TEST_ASSERT_FALSE(config.write(journal));
        flash.mBudget = -1;

        // The previous value survives
        TEST_ASSERT_TRUE(reboot(flash));
        TEST_ASSERT_EQUAL(10, config.getOr<PersistCounter>(ID6, 0));

        // and the journal stays writable
        Journal rebooted(flash);
        TEST_ASSERT_TRUE(config.read(rebooted));
        config.set<PersistCounter>(ID6, 30);
        TEST_ASSERT_TRUE(config.write(rebooted));

        TEST_ASSERT_TRUE(reboot(flash));
        TEST_ASSERT_EQUAL(30, config.getOr<PersistCounter>(ID6, 0));
    }
}

void test_power_loss_snapshot(void)
{
    TEST_LOG("--- TEST CASE: %s ---", __func__);

    MemoryFlash flash(4);
    Journal journal(flash);
    Config &config = Config::getInstance();

    config.set<PersistCounter>(ID6, 10);
    TEST_ASSERT_FALSE(config.read(journal));
    TEST_ASSERT_TRUE(config.write(journal));

    // A valid counter record: id, type, value, CRC8 of the value
    PersistCounter::Type value = 20;
    char record[] = {static_cast<char>(ID6), static_cast<char>(ConfigParameterType::COUNTER), 0, 0, 0, 0, 0};
    memcpy(&record[2], &value, sizeof(value));
    record[6] = checksum::Checksum(checksum::Checksum::CRC8)
                    .calculate(reinterpret_cast<const char *>(&value), sizeof(value));

    // Power is lost after a snapshot record, before the snapshot header
    TEST_ASSERT_TRUE(journal.beginSnapshot());
    TEST_ASSERT_TRUE(journal.append(record, sizeof(record)));

    // The unfinished snapshot is ignored
    TEST_ASSERT_TRUE(reboot(flash));
    TEST_ASSERT_EQUAL(10, config.getOr<PersistCounter>(ID6, 0));

    // Control: the same snapshot is used once its header is written
    TEST_ASSERT_TRUE(journal.endSnapshot());
    TEST_ASSERT_TRUE(reboot(flash));
    TEST_ASSERT_EQUAL(20, config.getOr<PersistCounter>(ID6, 0));
}

void test_corrupted_record(void)
{
    TEST_LOG("--- TEST CASE: %s ---", __func__);

    MemoryFlash flash(4);
    Journal journal(flash);
    Config &config = Config::getInstance();

    TEST_ASSERT_FALSE(config.read(journal));
    TEST_ASSERT_TRUE(config.write(journal));

    // Damage the value of ID2 only
    int offset = findParameter(flash, ID2, ConfigParameterType::NUMBER);
    TEST_ASSERT_NOT_EQUAL(-1, offset);
    flash.mData[offset + 2] ^= 0x55;

    config.set<int>(ID2, 0);
    config.set<std::string>(ID3, "changed");
    config.set<int>(ID8, 0);

    TEST_ASSERT_TRUE(reboot(flash));

    // The damaged record is skipped
    TEST_ASSERT_EQUAL(0, config.getOr<int>(ID2, -1));

    // Records after the damaged one are still restored
    TEST_ASSERT_EQUAL_STRING("test", config.getOr<std::string>(ID3, "").c_str());
    TEST_ASSERT_EQUAL(8, config.getOr<int>(ID8, -1));
}

void test_garbage_flash(void)
{
    TEST_LOG("--- TEST CASE: %s ---", __func__);

    // Flash never used by the journal, e.g. leftovers of a filesystem
    MemoryFlash flash(4);
    uint32_t seed = 12345;
    for (auto &byte : flash.mData)
    {
        seed = seed * 1103515245 + 12345;
        byte = seed >> 16;
    }

    Journal journal(flash);
    Config &config = Config::getInstance();

    TEST_ASSERT_FALSE(config.read(journal));

    config.set<int>(ID8, 88);
    TEST_ASSERT_TRUE(config.write(journal));

    config.set<int>(ID8, 0);
    TEST_ASSERT_TRUE(reboot(flash));
    TEST_ASSERT_EQUAL(88, config.getOr<int>(ID8, 0));
}

void test_type_changed(void)
{
    TEST_LOG("--- TEST CASE: %s ---", __func__);

    MemoryFlash flash(4);
    Journal journal(flash);
    Config &config = Config::getInstance();

    TEST_ASSERT_FALSE(config.read(journal));
    config.set<int>(ID8, 123);
    TEST_ASSERT_TRUE(config.write(journal));

    // A new firmware declares the same id with another type
    config.add<float>(ID8, 1.5);
    TEST_ASSERT_NULL(config.find<int>(ID8));

    // The stored int is skipped, the default is kept
    TEST_ASSERT_TRUE(reboot(flash));
    TEST_ASSERT_EQUAL_FLOAT(1.5, config.getOr<float>(ID8, 0.0));

    // and the new type is stored
    Journal rebooted(flash);
    TEST_ASSERT_TRUE(config.read(rebooted));
    config.set<float>(ID8, 2.5);
    TEST_ASSERT_TRUE(config.write(rebooted));

    config.set<float>(ID8, 0.0);
    TEST_ASSERT_TRUE(reboot(flash));
    TEST_ASSERT_EQUAL_FLOAT(2.5, config.getOr<float>(ID8, 0.0));
}

void test_unknown_id(void)
{
    TEST_LOG("--- TEST CASE: %s ---", __func__);

    MemoryFlash flash(4);
    Journal journal(flash);
    Config &config = Config::getInstance();

    config.add<int>(ID_EXTRA, 5);
    TEST_ASSERT_FALSE(config.read(journal));
    TEST_ASSERT_TRUE(config.write(journal));

    // A new firmware doesn't know ID_EXTRA anymore
    config.remove(ID_EXTRA);
    config.set<std::string>(ID3, "changed");

    TEST_ASSERT_TRUE(reboot(flash));
    TEST_ASSERT_FALSE(config.hasValue(ID_EXTRA));
    TEST_ASSERT_EQUAL_STRING("test", config.getOr<std::string>(ID3, "").c_str());
}

void test_parameter_size(void)
{
    TEST_LOG("--- TEST CASE: %s ---", __func__);

    MemoryFlash flash(4);
    Journal journal(flash);
    Config &config = Config::getInstance();

    TEST_ASSERT_FALSE(config.read(journal));

    // The longest string fits
    config.add<std::string>(ID_EXTRA, std::string(255, 'x'));
    TEST_ASSERT_TRUE(config.write(journal));

    config.set<std::string>(ID_EXTRA, "");
    TEST_ASSERT_TRUE(reboot(flash));
    TEST_ASSERT_EQUAL(255, config.getOr<std::string>(ID_EXTRA, "").length());

    // A longer one is rejected, other parameters are still stored
    Journal rebooted(flash);
    TEST_ASSERT_TRUE(config.read(rebooted));

    config.set<std::string>(ID_EXTRA, std::string(256, 'x'));
    config.set<int>(ID8, 77);
    TEST_ASSERT_FALSE(config.write(rebooted));

    config.set<int>(ID8, 0);
    TEST_ASSERT_TRUE(reboot(flash));
    TEST_ASSERT_EQUAL(77, config.getOr<int>(ID8, 0));
    TEST_ASSERT_EQUAL(255, config.getOr<std::string>(ID_EXTRA, "").length());
}

void test_snapshot_too_big(void)
{
    TEST_LOG("--- TEST CASE: %s ---", __func__);

    MemoryFlash flash(4);
    Journal journal(flash);
    Config &config = Config::getInstance();

    TEST_ASSERT_FALSE(config.read(journal));
    TEST_ASSERT_TRUE(config.write(journal));

    // All parameters together don't fit into a sector anymore
    config.add<std::string>(ID_BIG1, std::string(240, 'a'));
    config.add<std::string>(ID_BIG2, std::string(240, 'b'));

    unsigned erases = flash.erases();

    // Every write fails, but none of them erases a sector
    for (int ix = 0; ix < 3; ++ix)
    {
        TEST_ASSERT_FALSE(config.write(journal));
    }
    TEST_ASSERT_EQUAL(erases, flash.erases());

    // The stored values are intact
    config.set<std::string>(ID3, "changed");
    TEST_ASSERT_TRUE(reboot(flash));
    TEST_ASSERT_EQUAL_STRING("test", config.getOr<std::string>(ID3, "").c_str());
}

void test_erase_failure(void)
{
    TEST_LOG("--- TEST CASE: %s ---", __func__);

    MemoryFlash flash(4);
    Journal journal(flash);
    Config &config = Config::getInstance();

    TEST_ASSERT_FALSE(config.read(journal));
    TEST_ASSERT_TRUE(config.write(journal));

    // Fill the sector until a snapshot is needed and its erase fails
    flash.mEraseFails = true;

    unsigned value = 0;
    while (true)
    {
        TEST_ASSERT_LESS_THAN(100, value);

        config.set<PersistCounter>(ID6, ++value);
        if (!config.write(journal))
            break;
    }

    flash.mEraseFails = false;

    // The last value that was written survives
    TEST_ASSERT_TRUE(reboot(flash));
    TEST_ASSERT_EQUAL(value - 1, config.getOr<PersistCounter>(ID6, 0));

    // and the journal recovers once the flash works again
    Journal rebooted(flash);
    TEST_ASSERT_TRUE(config.read(rebooted));
    config.set<PersistCounter>(ID6, 1000);
    TEST_ASSERT_TRUE(config.write(rebooted));

    TEST_ASSERT_TRUE(reboot(flash));
    TEST_ASSERT_EQUAL(1000, config.getOr<PersistCounter>(ID6, 0));
}

void test_sequence_wrap(void)
{
    TEST_LOG("--- TEST CASE: %s ---", __func__);

    MemoryFlash flash(4);
    Journal journal(flash);
    Config &config = Config::getInstance();

    TEST_ASSERT_FALSE(config.read(journal));

    // Write until the journal moves to sector 1
    int value = 0;
    while (flash.mErases[1] == 0)
    {
        TEST_ASSERT_LESS_THAN(100, value);

        config.set<int>(ID8, ++value);
        TEST_ASSERT_TRUE(config.write(journal));
    }

    // Sector 0 holds older values, sector 1 is newer across the wrap
    setSequence(flash, 0, 0xFFFFFFFF);
    setSequence(flash, 1, 0);

    config.set<int>(ID8, -1);
    TEST_ASSERT_TRUE(reboot(flash));
    TEST_ASSERT_EQUAL(value, config.getOr<int>(ID8, 0));
}

void test_esp_flash(void)
{
    TEST_LOG("--- TEST CASE: %s ---", __func__);

    // First two sectors of the FS area, no filesystem is used by the test
    TEST_ASSERT_GREATER_OR_EQUAL(2 * SPI_FLASH_SEC_SIZE, FS_PHYS_SIZE);
    FlashEsp flash(FS_PHYS_ADDR, 2 * SPI_FLASH_SEC_SIZE);

    Journal journal(flash);
    Config &config = Config::getInstance();

    config.read(journal);

    unsigned value = config.getOr<PersistCounter>(ID6, 0) + 1;
    config.set<PersistCounter>(ID6, value);
    TEST_ASSERT_TRUE(config.write(journal));

    config.set<PersistCounter>(ID6, 0);
    TEST_ASSERT_TRUE(reboot(flash));
    TEST_ASSERT_EQUAL(value, config.getOr<PersistCounter>(ID6, 0));
}

void test_find_type_mismatch(void)
{
    TEST_LOG("--- CASE: %s ---", __func__);

    Config &config = Config::getInstance();

    // Accessing a parameter as a different type must not succeed
    TEST_ASSERT_NULL(config.find<float>(ID2));
    TEST_ASSERT_NULL(config.find<int>(ID7));
}

void test_remove(void)
{
    TEST_LOG("--- CASE: %s ---", __func__);

    Config &config = Config::getInstance();

    {
        TEST_ASSERT_TRUE(config.hasValue(ID1));
        config.remove(ID1);
        TEST_ASSERT_FALSE(config.hasValue(ID1));
    }
}

void setup()
{
    Serial.begin(74880);
    delay(1000);

    Serial.println("\n\n");
    Serial.flush();

    UNITY_BEGIN();
    RUN_TEST(test_basic);
    RUN_TEST(test_find);
    RUN_TEST(test_has_value);
    RUN_TEST(test_modify);
    RUN_TEST(test_read);
    RUN_TEST(test_float_fraction);
    RUN_TEST(test_counter_value);
    RUN_TEST(test_unchanged_write);
    RUN_TEST(test_changed_only);
    RUN_TEST(test_rotation);
    RUN_TEST(test_power_loss_record);
    RUN_TEST(test_power_loss_snapshot);
    RUN_TEST(test_corrupted_record);
    RUN_TEST(test_garbage_flash);
    RUN_TEST(test_type_changed);
    RUN_TEST(test_unknown_id);
    RUN_TEST(test_parameter_size);
    RUN_TEST(test_snapshot_too_big);
    RUN_TEST(test_erase_failure);
    RUN_TEST(test_sequence_wrap);
    RUN_TEST(test_esp_flash);
    RUN_TEST(test_find_type_mismatch);
    RUN_TEST(test_remove);
    UNITY_END();
}

void loop()
{
    delay(1000);
}