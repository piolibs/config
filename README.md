# config

Typed configuration parameters for ESP8266, persisted to flash with as few
sector erases as possible.

Parameters are kept in memory and written to an append-only journal in raw
flash. A write appends only the parameters that changed since the last
read/write, so a sector is erased once per sector-full of changes instead of on
every write, and the erases are spread over a ring of sectors.

## Usage

```cpp
#include <flash_hal.h>
#include <config.h>

using namespace config;

constexpr unsigned char COUNTER = Config::ID::CUSTOM_START + 1;
constexpr unsigned char NAME = Config::ID::CUSTOM_START + 2;

// Any sector-aligned range unused by the sketch, OTA or a filesystem,
// e.g. the FS area when no filesystem is mounted
FlashEsp flash(FS_PHYS_ADDR, FS_PHYS_SIZE);
Journal journal(flash);

void setup()
{
    auto &config = Config::getInstance()
        .add<PersistCounter>(COUNTER, 0)   // defaults
        .add<std::string>(NAME, "device");

    config.read(journal);                  // false on the first run

    config.set<PersistCounter>(COUNTER, config.getOr<PersistCounter>(COUNTER, 0) + 1);
    config.write(journal);                 // appends the COUNTER record only
}
```

Call `read()` once at startup before the first `write()`: it restores the
values and remembers what is stored, so unchanged parameters are not written
again.

### Parameter types

| C++ type | Notes |
|---|---|
| `char`, `int`, `float` | |
| `std::string` | up to 255 characters |
| `std::vector<T>` | integral `T`, up to 255 elements, 259 bytes serialized (e.g. 63 `int`) |
| `IPAddress` | |
| `PersistCounter` | unsigned counter |

A parameter is accessed with the type it was added with: `find()` and `getOr()`
return `nullptr` / the default for another type, `value()` asserts. `add()` with
an existing id and another type replaces the parameter.

### Results

- `read()` returns `false` if the flash holds no journal yet; parameters without
  a stored value keep their current values.
- `write()` returns `false` if a parameter is too big or the flash failed;
  the previously stored values stay intact.

## Storage

```
Sector:  [header][record][record]...[0xFF ...]
Header:  magic "CNFG", sequence, crc32(magic, sequence)
Record:  size | ~size << 16, payload padded to 4 bytes, crc32(payload)
Payload: id, type, value, crc8(value)
```

- Records are appended to the active sector without erasing it. On reading, the
  last record of a parameter wins.
- When the active sector is full, a snapshot of all parameters is written into
  the next sector of the ring. Its header is written last, so the sector with
  the newest valid header is the active one.
- A power loss while appending leaves a record with a bad CRC, which is
  skipped; a torn record header makes the next write start a new sector.
  A power loss during a snapshot leaves the previous sector active.
- At least 2 sectors are required. With 4 KB sectors a snapshot of a few small
  parameters plus ~245 counter updates fit into one sector.
- Records with unknown ids or another type are skipped, so parameters can be
  added, removed or retyped between firmware versions.

Version 2.0.0 replaced `StorageEeprom` with `Journal`; data stored by 1.x is
not read.

## Tests

Tests run on an ESP8266 connected over USB. Most of them use a RAM flash
emulation; `test_esp_flash` writes to the first 2 sectors of the FS area.

```
pio test -e board-esp8266
pio test -e board-esp8266 --without-testing
pio run -e board-esp8266 -t monitor
```
