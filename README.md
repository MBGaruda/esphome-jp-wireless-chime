# JP Wireless Chime Receiver for ESPHome

An ESPHome External Component that receives Japanese wireless chime RF signals using ESP32 + ESPHome and forwards them as Home Assistant events.

This component currently supports the following protocols:

- REVEX X Series
- REVEX XP Series
- OHM-07 Series (wireless chime series with model numbers starting with 07)

Received signals are published as Home Assistant events and can be used with integrations such as `ha-jp-wireless-chime`.

Related repositories:

- ha-jp-wireless-chime  
  https://github.com/MBGaruda/ha-jp-wireless-chime

- esphome-jp-wireless-chime  
  https://github.com/MBGaruda/esphome-jp-wireless-chime

---

## Supported Protocols

### REVEX X

- 24bit RF frame
- `protocol_hint: revex_x`

### REVEX XP

- 34bit RF frame
- Supports XP extended melodies
- `protocol_hint: revex_xp`

### OHM-07

- 24bit RF frame
- `protocol_hint: ohm_07`

---

## Operation

### Event Publishing

Received RF signals are published as Home Assistant events.

Event name:

```text
esphome.jp_wireless_chime_raw_received
```

Example event:

```yaml
event_type: esphome.jp_wireless_chime_raw_received

data:
  source: wireless-chime-rx
  protocol_version: "1"
  protocol_hint: revex_x
  bit_count: "24"
  bits: "110101111111111100000001"
  raw_hex: D7FF01
  sync_us: "4535"
  received_at_ms: "237082"
```

---

## Event Data

| Field | Description |
|---|---|
| source | ESPHome `esphome.name` |
| protocol_version | Event specification version |
| protocol_hint | `revex_x` / `revex_xp` / `ohm_07` |
| bit_count | Received bit count |
| bits | Raw bit string |
| raw_hex | Decoded hexadecimal value |
| sync_us | Detected sync pulse length |
| received_at_ms | Milliseconds since ESP32 boot |

---

## Duplicate Event Suppression

Wireless chime transmitters may send multiple RF frames during a single button press.

This component suppresses duplicate events for the same protocol.

Behavior:

- Suppresses repeated events per `protocol_hint`
- Refreshes the suppression timer whenever the same signal is received
- Allows the next event only after RF transmission has stopped for 2000ms

This helps prevent duplicate events caused by:

- Long-duration transmitters
- Misdecoded frames during continuous transmission
- Noise-generated alternative HEX values

---

## Installation

### External Components

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/MBGaruda/esphome-jp-wireless-chime
```

---

## ESPHome Configuration Example

```yaml
esphome:
  name: wireless-chime-rx
  friendly_name: Wireless Chime RX

esp32:
  board: esp32dev

logger:
  level: INFO

api:

ota:

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

external_components:
  - source:
      type: git
      url: https://github.com/MBGaruda/esphome-jp-wireless-chime

jp_wireless_chime_receiver:
  pin: GPIO27
```

---

## GPIO Wiring Example

Uses a standard 315MHz ASK/OOK RF receiver module.

| RF Receiver | ESP32 |
|---|---|
| VCC | 3.3V |
| GND | GND |
| DATA | GPIO27 |

---

## Monitoring Events in Home Assistant

Events can be monitored from the Home Assistant Developer Tools.

```text
Developer Tools
→ Events
→ Listen to events

esphome.jp_wireless_chime_raw_received
```

---

## Logger Levels

Normal operation:

```yaml
logger:
  level: INFO
```

Debug mode:

```yaml
logger:
  level: DEBUG
```

---

## Receive Examples

### REVEX X

```yaml
protocol_hint: revex_x
bit_count: "24"
raw_hex: D7FF01
```

### REVEX XP

```yaml
protocol_hint: revex_xp
bit_count: "34"
raw_hex: D7FF5050
```

### OHM-07

```yaml
protocol_hint: ohm_07
bit_count: "24"
raw_hex: 104F44
```

---

## Notes

- This component targets Japanese 315MHz wireless chime systems
- Reception quality may vary depending on the RF receiver module and noise environment
- Tested with ESPHome 2026.4 series