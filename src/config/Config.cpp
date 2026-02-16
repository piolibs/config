/*
 * Copyright (C) 2026 Dmitry Korobkov <dmitry.korobkov.nn@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <string>
#include <console.h>

#include "Config.h"

using namespace console;
using namespace config;

Config::Config()
    : mParameters() {

    add(new ConfigParameter<char>(0, 0xA7));
}

Config& Config::write(ByteBuffer& buffer) {

    ByteBuffer::iterator it = buffer.begin();

    for (auto & [ key, parameter]: mParameters) {

        if (!it.isValid()) {
            LOG("Write failed, invalid iterator");
            break;
        }

        it = parameter->write(it);
    }

    buffer.commit();

    return *this;
}

Config& Config::read(ByteBuffer& buffer) {

    ByteBuffer::iterator it = buffer.begin();

    for (auto & [ key, parameter]: mParameters) {

        if (!it.isValid()) {
            break;
        }

        it = parameter->read(it);
    }

    return *this;
}
