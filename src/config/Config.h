/*
 * Copyright (C) 2026 Dmitry Korobkov <dmitry.korobkov.nn@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once
#pragma push_macro("LOG_MODULE")

#undef LOG_MODULE
#define LOG_MODULE "CFG"

#include <map>
#include <memory>
#include <Arduino.h>
#include <console.h>
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
    enum ID : unsigned char;

    static Config& getInstance() {
        static Config config;
        return config;
    }

    //--- Add/Remove parameters ---
    template<typename T>
    Config& add(unsigned char id, const T& value);

    template <typename T>
    Config& add(unsigned char id, T&& value);

    bool remove(unsigned char id);

    //--- Check existence ---
    bool hasValue(unsigned char id) const;

    //--- Find and read values ---
    template<typename T>
    T* find(unsigned char id) const;

    template<typename T>
    T& value(unsigned char id);

    template<typename T>
    const T& value(unsigned char id) const;

    template<typename T>
    T getOr(unsigned char id, const T& defaultValue) const;

    template<typename T>
    T getOr(unsigned char id, T&& defaultValue) const;

    //--- Modify values ---
    template<typename T>
    bool set(unsigned char id, const T& value);

    //--- Serialization ---
    Config& read(ByteBuffer& buffer);
    Config& write(ByteBuffer& buffer);

private:
    std::map<unsigned char, std::shared_ptr<ConfigParameterBase>> mParameters;
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
Config& Config::add(unsigned char id, const T &value)
{
    return add(id, T(value));
}

template <typename T>
Config& Config::add(unsigned char id, T &&value)
{
    LOGV("%s", __func__);

    auto it = mParameters.find(id);
    if (it != mParameters.end())
    {
        LOGD("%s: id=%u found, update existing value", __func__, id);
        auto ptr = std::static_pointer_cast<ConfigParameter<std::remove_reference_t<T>>>(it->second);
        ptr->set(std::forward<T>(value));
    }
    else
    {
        LOGD("%s: id=%u not found, create new value", __func__, id);
        auto ptr = std::make_shared<ConfigParameter<std::remove_reference_t<T>>>(id, std::forward<T>(value));
        mParameters.insert({id, ptr});
    }

    return *this;
}

template <typename T>
T* Config::find(unsigned char id) const
{
    LOGV("%s", __func__);

    auto it = mParameters.find(id);
    if (it == mParameters.end())
        return nullptr;

    auto ptr = std::static_pointer_cast<ConfigParameter<T>>(it->second);
    return ptr ? &(ptr->value()) : nullptr;
}

template <typename T>
bool Config::set(unsigned char id, const T &value)
{
    LOGV("%s", __func__);
    T *ptr = find<T>(id);
    if (ptr != nullptr)
    {
        LOGD("%s: id=%u found", __func__, id);
        *ptr = value;
        return true;
    }

    LOGD("%s: id=%u not found", __func__, id);
    return false;
}

template <typename T>
T& Config::value(unsigned char id)
{
    LOGV("%s", __func__);
    auto it = mParameters.find(id);
    if (it == mParameters.end())
    {
        LOGE("%s: id=%u not found", __func__, id);
        assert(false);
    }
    return std::static_pointer_cast<ConfigParameter<T>>(it->second)->value();
}

template <typename T>
const T& Config::value(unsigned char id) const
{
    LOGV("%s", __func__);
    auto it = mParameters.find(id);
    if (it == mParameters.end())
    {
        LOGE("%s: id=%u not found", __func__, id);
        assert(false);
    }
    return std::static_pointer_cast<ConfigParameter<T>>(it->second)->value();
}

template <typename T>
T Config::getOr(unsigned char id, const T& defaultValue) const
{
    LOGV("%s (const T&)", __func__);
    T *ptr = find<T>(id);
    return ptr ? *ptr : defaultValue;
}

template <typename T>
T Config::getOr(unsigned char id, T &&defaultValue) const
{
    LOGV("%s (T&&)", __func__);
    T *ptr = find<T>(id);
    return ptr ? *ptr : std::move(defaultValue);
}

} // namespace

#pragma pop_macro("LOG_MODULE")
