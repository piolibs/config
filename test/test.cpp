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

enum CusromID
{
    ID1 = Config::ID::CUSTOM_START + 1,
    ID2,
    ID3,
    ID4,
    ID5,
    ID6,
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
        .add<PersistCounter>(ID6, PersistCounter(4))
        .write(eeprom);

    (void)config;
}

void test_basic(void)
{
    TEST_LOG("--- TEST CASE: %s ---", __func__);

    Config &config = Config::getInstance();

    {
        auto actual = config.valueOr<char>(ID1, -1);

        TEST_LOG("id=%u, value<char>=%u", ID1, actual);
        TEST_ASSERT_EQUAL(1, actual);
    }

    {
        auto actual = config.valueOr<int>(ID2, -1);

        TEST_LOG("id=%u, value<int>=%d", ID2, actual);
        TEST_ASSERT_EQUAL(2, actual);
    }

    {
        auto actual = config.valueOr<std::string>(ID3, "");

        TEST_LOG("id=%u, value<string>=%s", ID3, actual.c_str());
        TEST_ASSERT_EQUAL_STRING("test", actual.c_str());
    }

    {
        auto actual = config.valueOr<std::vector<int>>(ID4, std::vector<int>());

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
        auto actual = config.valueOr<IPAddress>(ID5, IPAddress());

        TEST_LOG("id=%u, address=%s", ID5, actual.toString().c_str());
        TEST_ASSERT_EQUAL_STRING("1.2.3.4", actual.toString().c_str());
    }

    {
        TEST_ASSERT_TRUE(config.hasValue(ID6));
        auto actual = config.value<PersistCounter>(ID6);

        TEST_LOG("id=%u, counter=%u", ID6, actual);
        TEST_ASSERT_EQUAL(0, actual);
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
        auto actual = config.valueOr<char>(ID1, -1);

        TEST_LOG("id=%u, value<char>=%u", ID1, actual);
        TEST_ASSERT_EQUAL(1, actual);
    }

    {
        char defaultValue = -1;
        auto actual = config.valueOr<char>(ID1, defaultValue);

        TEST_LOG("id=%u, value<char>=%u", ID1, actual);
        TEST_ASSERT_EQUAL(1, actual);
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
    RUN_TEST(test_remove);
    UNITY_END();
}

void loop()
{
    delay(1000);
}