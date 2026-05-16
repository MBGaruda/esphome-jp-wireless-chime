# ESPHome JP Wireless Chime Receiver

ESPHome external component for Home Assistant.
Receives Japanese wireless chime RF signals using ESP32 and 315MHz RF receiver modules.

This component decodes RF signals from Japanese wireless door chime products and sends them as Home Assistant events.

---

## Supported Protocols

Currently supported protocols:

* REVEX X Series
* OHM 07 Series

---

## Features

* 315MHz ASK/OOK RF signal reception
* REVEX / OHM RF protocol decoding
* 24bit RF data extraction
* Home Assistant event dispatch
* Duplicate signal suppression (5 seconds)

---

## Installation

### Using external_components

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/MBGaruda/esphome-jp-wireless-chime
      ref: main
    components:
      - jp_wireless_chime_receiver
```

---

## Usage

### Basic Configuration Example

```yaml
esphome:
  name: wireless-chime-rx

esp32:
  board: esp32dev
  framework:
    type: arduino

logger:

api:
  homeassistant_services: true

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

external_components:
  - source:
      type: git
      url: https://github.com/MBGaruda/esphome-jp-wireless-chime
      ref: main
    components:
      - jp_wireless_chime_receiver

jp_wireless_chime_receiver:
  pin: GPIO27
```

### Parameters

* **pin**:
  GPIO connected to the RF receiver DATA pin

---

## Hardware

### Required Components

* ESP32
* 315MHz ASK/OOK RF receiver module

### Tested RF Receiver Modules

* MX-RM-5V
* SYN470R series
* RXB6
* WL101-341

### Wiring Example

```text
RF Receiver → ESP32

DATA → GPIO27
VCC  → 3.3V
GND  → GND
```

---

## Home Assistant Events

The following event is fired when a signal is received:

```text
esphome.jp_wireless_chime_raw_received
```

---

## Event Data

### REVEX X

```yaml
event_type: esphome.jp_wireless_chime_raw_received

data:
  protocol_version: "1"
  source: esp32_rf_receiver
  protocol_hint: revex_x
  bit_count: "24"
  bits: "110101111111111100000001"
  raw_hex: "D7FF01"
  sync_us: "4535"
  received_at_ms: "46262"
```

### OHM-07

```yaml
event_type: esphome.jp_wireless_chime_raw_received

data:
  protocol_version: "1"
  source: esp32_rf_receiver
  protocol_hint: ohm_07
  bit_count: "24"
  bits: "010101010101111101000100"
  raw_hex: "555F44"
  sync_us: "4156"
  received_at_ms: "123456"
```

---

## Parameter Details

### protocol_version

Event protocol version.

Currently fixed to `"1"`.

### source

Event source identifier.

Currently fixed to `"esp32_rf_receiver"`.

### protocol_hint

Detected RF protocol.

* `revex_x`
* `ohm_07`

### bit_count

Received bit length.

Currently fixed to `"24"`.

### bits

Raw 24bit binary string.

### raw_hex

HEX representation of `bits`.

This value is typically used as the primary key in the Home Assistant integration.

### sync_us

Detected synchronization pulse length in microseconds.

### received_at_ms

ESP32 uptime in milliseconds using `millis()`.

---

## Duplicate Suppression

Most RF transmitters send the same signal multiple times per button press.

This component suppresses duplicate events for 5 seconds when:

```text
protocol_hint + raw_hex
```

matches a previously received signal.

---

## Integration with Home Assistant

This component is responsible for:

* RF reception
* RF decoding
* raw_hex generation

The following responsibilities are expected to be handled by the Home Assistant `jp_wireless_chime` integration:

* Channel decoding
* Melody decoding
* Device/entity management
* UI integration
* Automation handling

---

## Automation Example

```yaml
automation:
  - alias: "Wireless Chime Debug"
    trigger:
      - platform: event
        event_type: esphome.jp_wireless_chime_raw_received

    action:
      - service: system_log.write
        data:
          message: >
            {{ trigger.event.data.protocol_hint }}
            {{ trigger.event.data.raw_hex }}
          level: info
```

---

## Tested Environment

* Latest Home Assistant
* ESPHome 2026.4.x
* ESP32 DevKit
* REVEX X Series
* OHM-07 Series

---

## Development Status

This component is currently under development.

Implemented features:

* REVEX X reception
* OHM-07 reception
* Home Assistant event dispatch

---

## Disclaimer

This project is not affiliated with REVEX, OHM, ESPHome, or Home Assistant.
