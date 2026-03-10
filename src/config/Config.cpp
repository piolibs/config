/*
 * Copyright (C) 2026 Dmitry Korobkov <dmitry.korobkov.nn@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#undef LOG_MODULE
#define LOG_MODULE "CFG"

#include <string>
#include <console.h>

#include "Config.h"

using namespace console;

namespace config {

Config::Config() : mParameters()
{
    add<char>(UNDEFINED, 0xA7);
}

bool Config::hasValue(unsigned char id) const
{
    LOGV("%s", __func__);
    auto it = mParameters.find(id);
    return (it == mParameters.end()) ? false : true;
}

bool Config::remove(unsigned char id)
{
    LOGV("%s", __func__);
    return mParameters.erase(id) > 0;
}

Config& Config::write(ByteBuffer& buffer)
{
    LOGV("%s", __func__);
    ByteBuffer::iterator it = buffer.begin();

    for (auto & [ key, parameter]: mParameters)
    {
        if (!it.isValid())
        {
            LOG("Write failed, invalid iterator");
            break;
        }

        it = parameter->write(it);
    }

    buffer.commit();

    return *this;
}

Config& Config::read(ByteBuffer& buffer)
{
    LOGV("%s", __func__);
    ByteBuffer::iterator it = buffer.begin();

    for (auto & [ key, parameter]: mParameters)
    {
        if (!it.isValid())
        {
            break;
        }

        it = parameter->read(it);
    }

    return *this;
}

// --------------------------------------------------------

Config& getInstance()
{
    return Config::getInstance();
}

} // namespace