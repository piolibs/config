/*
 * Copyright (C) 2026 Dmitry Korobkov <dmitry.korobkov.nn@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include <map>
#include <memory>
#include <Arduino.h>
#include <checksum.h>

#include "ConfigParameter.h"
#include "ByteBuffer.h"
#include "StorageEeprom.h"

namespace config {

class Config {
private:
    Config();
    ~Config() = default;

    Config(Config const&) = delete;
    Config& operator= (Config const&) = delete;

public:
    static Config& getInstance() {
        static Config config;
        return config;
    }

    template<typename T>
    Config& add(ConfigParameter<T>* parameter_p);

    template<typename T>
    Config& add(unsigned char id, const T &value);

    template<typename T>
    T& get(uint8_t id);

    template<typename T>
    bool set(uint8_t id, const T& value);

    Config& read(ByteBuffer& buffer);
    Config& write(ByteBuffer& buffer);

public:
    enum ID : unsigned char;

private:
    std::map<uint8_t, std::shared_ptr<ConfigParameterBase>> mParameters;
};

enum Config::ID : unsigned char
{
    UNDEFINED = 0,
    WIFI_AP_NAME,
    WIFI_AP_PASSWORD,
    SETUP_AP_NAME,
    SETUP_AP_PASSWORD,
    SETUP_AP_ADDRESS,
    SETUP_AP_GATEWAY,
    SETUP_AP_NW_MASK,
    WIFI_AP_BSSID,
    WIFI_AP_CHANNEL,
    LOCAL_IP_ADDRESS,
    SENSOR_A,
    SENSOR_B,
    REPORT_INTERVAL,
    SKIP_EMPTY_REPORT,
    CONFIG_MAX_ID,

    CUSTOM_MASK = 0x80,
    CUSTOM_START = CUSTOM_MASK,
};

template<typename T>
Config& Config::add(ConfigParameter<T>* parameter_p) {

    mParameters.insert(std::pair<uint8_t, std::shared_ptr<ConfigParameterBase>>(
        parameter_p->getId(),
        std::make_shared<ConfigParameter<T>>(*parameter_p)));

    return *this;
}

template<typename T>
Config& Config::add(unsigned char id, const T &value) {

    add<T>(new ConfigParameter<T>(id, value));
    return *this;
}

template<typename T>
T& Config::get(uint8_t id) {

    for (auto & [ _, parameter]: mParameters) {

        if (parameter->getId() == id) {
            LOG("get: found %u", id);
            return std::static_pointer_cast<ConfigParameter<T>>(parameter)->get();
        }
    }

    LOG("get: not found %u", id);
    return std::shared_ptr<ConfigParameter<T>>(new ConfigParameter<T>())->get();
}

template<typename T>
bool Config::set(uint8_t id, const T& value)
{
    for (auto & [ _, parameter]: mParameters)
    {
        if (parameter->getId() == id)
        {
            LOG("set: found: %u", id);
            *std::static_pointer_cast<ConfigParameter<T>>(parameter) = value;
            return true;
        }
    }
    LOG("set: not found %u", id);
    return false;
}

} // namespace
