/*
 * Copyright (C) 2026 Dmitry Korobkov <dmitry.korobkov.nn@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <Arduino.h>
#include <unity.h>

#include <console.h>
#include <config.h>

using namespace config;

enum CustomID : unsigned char
{
    ID1 = Config::ID::CUSTOM_START + 1,
    ID2, ID3, ID4, ID5, ID6, ID7,
    ID_MAX
};

void setUp()
{
    TEST_LOG("setup fixture");

    config::StorageEeprom eeprom = config::StorageEeprom(512);
    eeprom.dump(64);

    Config &config = Config::getInstance()
        .add<char>(ID1, 1)
        .add<int>(ID2, 2)
        .add<std::string>(ID3, "test")
        .add<std::vector<int>>(ID4, {1, 2, 3})
        .add<IPAddress>(ID5, IPAddress(1, 2, 3, 4))
        .add<PersistCounter>(ID6, PersistCounter(5))
        .add<float>(ID7, 6.0)
        .write(eeprom);

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

    config::StorageEeprom eeprom = config::StorageEeprom(512);
    Config &config = Config::getInstance();

    {
        auto id = ID1;
        using type = char;

        TEST_ASSERT_TRUE(config.hasValue(id));

        config.value<type>(id) = 10;
        config.write(eeprom);
        TEST_ASSERT_EQUAL(10, config.getOr<type>(id, 0.0));

        config.set<type>(id, 20);
        TEST_ASSERT_EQUAL(20, config.getOr<type>(id, 0.0));

        config.read(eeprom);
        TEST_ASSERT_EQUAL(10, config.getOr<type>(id, 0.0));
    }

    {
        auto id = ID2;
        using type = int;

        TEST_ASSERT_TRUE(config.hasValue(id));

        config.value<type>(id) = 10;
        config.write(eeprom);
        TEST_ASSERT_EQUAL(10, config.getOr<type>(id, 0.0));

        config.set<type>(id, 20);
        TEST_ASSERT_EQUAL(20, config.getOr<type>(id, 0.0));

        config.read(eeprom);
        TEST_ASSERT_EQUAL(10, config.getOr<type>(id, 0.0));
    }

    {
        auto id = ID3;
        using type = std::string;

        TEST_ASSERT_TRUE(config.hasValue(id));

        config.value<type>(id) = "10";
        config.write(eeprom);
        TEST_ASSERT_EQUAL_STRING("10", config.getOr<type>(id, "").c_str());

        config.set<type>(id, "20");
        TEST_ASSERT_EQUAL_STRING("20", config.getOr<type>(id, "").c_str());

        config.read(eeprom);
        TEST_ASSERT_EQUAL_STRING("10", config.getOr<type>(id, "").c_str());
    }

    {
        auto id = ID4;
        using type = std::vector<int>;

        TEST_ASSERT_TRUE(config.hasValue(id));

        config.value<type>(id) = {10};
        config.write(eeprom);
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

        config.read(eeprom);
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
        config.write(eeprom);
        {
            auto actual = config.getOr<IPAddress>(ID5, IPAddress());
            TEST_ASSERT_EQUAL_STRING("10.2.3.4", actual.toString().c_str());
        }

        config.set<type>(id, IPAddress(20, 2, 3, 4));
        {
            auto actual = config.getOr<IPAddress>(ID5, IPAddress());
            TEST_ASSERT_EQUAL_STRING("20.2.3.4", actual.toString().c_str());
        }

        config.read(eeprom);
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
        config.write(eeprom);
        TEST_ASSERT_EQUAL(10, config.getOr<type>(id, 0));

        config.set<type>(id, 20);
        TEST_ASSERT_EQUAL(20, config.getOr<type>(id, 0));

        config.read(eeprom);
        TEST_ASSERT_EQUAL(10, config.getOr<type>(id, 0));
    }

    {
        TEST_ASSERT_TRUE(config.hasValue(ID7));

        config.value<float>(ID7) = 10.0;
        config.write(eeprom);
        TEST_ASSERT_EQUAL(10.0, config.getOr<float>(ID7, 0.0));

        config.set<float>(ID7, 20.0);
        TEST_ASSERT_EQUAL(20.0, config.getOr<float>(ID7, 0.0));

        config.read(eeprom);
        TEST_ASSERT_EQUAL(10.0, config.getOr<float>(ID7, 0.0));
    }
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
    RUN_TEST(test_remove);
    UNITY_END();
}

void loop()
{
    delay(1000);
}