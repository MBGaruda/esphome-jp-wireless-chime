#include "jp_wireless_chime_receiver.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <map>
#include <string>

namespace esphome {
namespace jp_wireless_chime_receiver {

static const char *const TAG = "jp_wireless_chime_receiver";

static constexpr uint8_t PROTOCOL_VERSION = 1;

static constexpr uint16_t RING_SIZE = 2048;
static constexpr uint16_t MIN_EDGE_US = 100;

static constexpr uint8_t REVEX_X_BITS = 24;
static constexpr uint8_t REVEX_XP_BITS = 34;
static constexpr uint8_t OHM_BITS = 24;
static constexpr uint8_t MAX_BITS = 34;

static constexpr uint32_t SAME_EVENT_IDLE_MS = 2000;
static constexpr uint32_t HA_SEND_INTERVAL_MS = 300;

static const char *const HA_EVENT_NAME =
    "esphome.jp_wireless_chime_raw_received";

struct Pulse {
  uint16_t duration;
  uint8_t level;
};

struct DecoderState {
  bool active;
  uint8_t bit_count;
  char pending;
  char bits[MAX_BITS + 1];
  uint16_t sync_us;
};

struct PendingEvent {
  bool pending;
  std::string protocol;
  std::string bits;
  std::string raw_hex;
  uint8_t bit_count;
  uint16_t sync_us;
  uint32_t received_at_ms;
};

struct RecentEvent {
  std::string key;
  uint32_t last_seen_ms;
};

static volatile Pulse ring[RING_SIZE];
static volatile uint16_t write_index = 0;
static volatile uint16_t read_index = 0;

static GPIOPin *isr_pin = nullptr;

static DecoderState revex_state{false, 0, 0, "", 0};
static DecoderState ohm_state{false, 0, 0, "", 0};

static PendingEvent pending_event{false, "", "", "", 0, 0, 0};

static RecentEvent recent_events[8];
static uint8_t recent_event_pos = 0;

static uint32_t last_ha_send_ms = 0;

static bool in_range(uint16_t v, uint16_t min_v, uint16_t max_v) {
  return v >= min_v && v <= max_v;
}

static bool is_revex_sync(uint16_t us) {
  return in_range(us, 4300, 4900);
}

static bool is_ohm_sync(uint16_t us) {
  return in_range(us, 4000, 4300);
}

static char revex_class(uint16_t us) {
  if (in_range(us, 200, 390)) return 'S';
  if (in_range(us, 700, 1000)) return 'L';
  return '?';
}

static char ohm_class(uint16_t us) {
  if (in_range(us, 150, 320)) return 'A';
  if (in_range(us, 540, 720)) return 'B';
  return '?';
}

static void reset_decoder(DecoderState &st) {
  st.active = false;
  st.bit_count = 0;
  st.pending = 0;
  st.bits[0] = '\0';
  st.sync_us = 0;
}

static void start_decoder(DecoderState &st, uint16_t sync_us) {
  st.active = true;
  st.bit_count = 0;
  st.pending = 0;
  st.bits[0] = '\0';
  st.sync_us = sync_us;
}

static std::string bits_to_hex(const char *bits, uint8_t bit_count) {
  std::string hex;
  uint8_t full_nibbles = bit_count / 4;

  for (uint8_t i = 0; i < full_nibbles * 4; i += 4) {
    uint8_t v = 0;

    for (uint8_t j = 0; j < 4; j++) {
      v <<= 1;
      if (bits[i + j] == '1') {
        v |= 1;
      }
    }

    hex.push_back(v < 10 ? char('0' + v) : char('A' + (v - 10)));
  }

  return hex;
}

static std::string make_suppress_key(
    const std::string &protocol,
    const std::string &raw_hex) {
  return protocol;
}

static bool should_queue_event(
    const std::string &protocol,
    const std::string &raw_hex) {
  std::string key = make_suppress_key(protocol, raw_hex);
  uint32_t now = millis();

  for (auto &recent : recent_events) {
    if (recent.key == key) {
      uint32_t elapsed = now - recent.last_seen_ms;

      recent.last_seen_ms = now;

      if (elapsed < SAME_EVENT_IDLE_MS) {
        ESP_LOGD(TAG,
                 "Duplicate suppressed: key=%s protocol=%s raw_hex=%s elapsed=%u",
                 key.c_str(),
                 protocol.c_str(),
                 raw_hex.c_str(),
                 elapsed);
        return false;
      }

      return true;
    }
  }

  recent_events[recent_event_pos].key = key;
  recent_events[recent_event_pos].last_seen_ms = now;
  recent_event_pos = (recent_event_pos + 1) % 8;

  return true;
}

static void queue_event(
    const char *protocol,
    const char *bits,
    uint8_t bit_count,
    const std::string &raw_hex,
    uint16_t sync_us) {
  if (!should_queue_event(protocol, raw_hex)) {
    return;
  }

  pending_event.pending = true;
  pending_event.protocol = protocol;
  pending_event.bits = bits;
  pending_event.raw_hex = raw_hex;
  pending_event.bit_count = bit_count;
  pending_event.sync_us = sync_us;
  pending_event.received_at_ms = millis();

  ESP_LOGI(TAG,
           "Queued event: protocol=%s bit_count=%u raw_hex=%s",
           protocol,
           bit_count,
           raw_hex.c_str());
}

static bool revex_xp_has_valid_terminator(const DecoderState &st) {
  return
      st.bit_count == REVEX_XP_BITS &&
      st.bits[32] == '0' &&
      st.bits[33] == '0';
}

static void log_revex_discard(
    const char *reason,
    const DecoderState &st) {
  char tmp[MAX_BITS + 1];

  uint8_t n = st.bit_count;
  if (n > MAX_BITS) {
    n = MAX_BITS;
  }

  for (uint8_t i = 0; i < n; i++) {
    tmp[i] = st.bits[i];
  }

  tmp[n] = '\0';

  ESP_LOGD(TAG,
           "REVEX discard: reason=%s bit_count=%u pending=%c bits=%s",
           reason,
           st.bit_count,
           st.pending == 0 ? '-' : st.pending,
           tmp);
}

static void finalize_revex_frame(
    DecoderState &st,
    const char *reason) {
  if (!st.active) {
    return;
  }

  if (st.bit_count == REVEX_XP_BITS &&
      revex_xp_has_valid_terminator(st)) {
    st.bits[REVEX_XP_BITS] = '\0';

    std::string raw_hex = bits_to_hex(st.bits, 32);

    ESP_LOGD(TAG,
             "REVEX XP detected: bits=%s raw_hex=%s reason=%s",
             st.bits,
             raw_hex.c_str(),
             reason);

    queue_event(
        "revex_xp",
        st.bits,
        REVEX_XP_BITS,
        raw_hex,
        st.sync_us);

    reset_decoder(st);
    return;
  }

  if (st.bit_count == REVEX_X_BITS) {
    st.bits[REVEX_X_BITS] = '\0';

    std::string raw_hex = bits_to_hex(st.bits, REVEX_X_BITS);

    ESP_LOGD(TAG,
             "REVEX X detected: bits=%s raw_hex=%s reason=%s",
             st.bits,
             raw_hex.c_str(),
             reason);

    queue_event(
        "revex_x",
        st.bits,
        REVEX_X_BITS,
        raw_hex,
        st.sync_us);

    reset_decoder(st);
    return;
  }

  log_revex_discard(reason, st);
  reset_decoder(st);
}

static void IRAM_ATTR gpio_intr() {
  static uint32_t last_edge_time = 0;

  uint32_t now = micros();
  uint32_t diff = now - last_edge_time;
  last_edge_time = now;

  if (diff < MIN_EDGE_US) {
    return;
  }

  uint16_t next = (write_index + 1) % RING_SIZE;

  if (next == read_index) {
    return;
  }

  ring[write_index].duration =
      diff > 65535 ? 65535 : diff;

  ring[write_index].level =
      isr_pin != nullptr
          ? isr_pin->digital_read()
          : 0;

  write_index = next;
}

void JPWirelessChimeReceiver::setup() {
  this->pin_->setup();

  isr_pin = this->pin_;

  attachInterrupt(
      digitalPinToInterrupt(this->pin_number_),
      gpio_intr,
      CHANGE);

  ESP_LOGI(TAG, "JP Wireless Chime Receiver started");
  ESP_LOGI(TAG, "protocol_version=%u", PROTOCOL_VERSION);
  ESP_LOGI(TAG, "ha_event=%s", HA_EVENT_NAME);
  ESP_LOGI(TAG, "ohm_07 suppress_mode=protocol");
}

void JPWirelessChimeReceiver::loop() {
  if (pending_event.pending) {
    uint32_t now = millis();

    if (now - last_ha_send_ms >= HA_SEND_INTERVAL_MS) {
      std::map<std::string, std::string> data;

      data["protocol_version"] = "1";
      data["source"] = App.get_name();
      data["protocol_hint"] = pending_event.protocol;
      data["bit_count"] = std::to_string(pending_event.bit_count);
      data["bits"] = pending_event.bits;
      data["raw_hex"] = pending_event.raw_hex;
      data["sync_us"] = std::to_string(pending_event.sync_us);
      data["received_at_ms"] = std::to_string(pending_event.received_at_ms);

      this->fire_homeassistant_event(
          HA_EVENT_NAME,
          data);

      ESP_LOGI(TAG,
               "HA event sent: source=%s protocol=%s bit_count=%u raw_hex=%s",
               App.get_name().c_str(),
               pending_event.protocol.c_str(),
               pending_event.bit_count,
               pending_event.raw_hex.c_str());

      pending_event.pending = false;
      last_ha_send_ms = now;
    }
  }

  while (read_index != write_index) {
    Pulse p;

    noInterrupts();

    p.duration = ring[read_index].duration;
    p.level = ring[read_index].level;
    read_index = (read_index + 1) % RING_SIZE;

    interrupts();

    // REVEX X / REVEX XP
    if (is_revex_sync(p.duration)) {
      finalize_revex_frame(
          revex_state,
          "next_sync");

      start_decoder(
          revex_state,
          p.duration);

    } else if (revex_state.active) {
      char c = revex_class(p.duration);

      if (c == '?') {
        ESP_LOGD(TAG,
                 "REVEX invalid pulse=%u",
                 p.duration);

        finalize_revex_frame(
            revex_state,
            "invalid_pulse");

      } else if (revex_state.pending == 0) {
        revex_state.pending = c;

      } else {
        char a = revex_state.pending;
        char b = c;

        revex_state.pending = 0;

        if (a == 'L' && b == 'S') {
          if (revex_state.bit_count < MAX_BITS) {
            revex_state.bits[revex_state.bit_count++] = '1';
          } else {
            finalize_revex_frame(
                revex_state,
                "overflow");
          }

        } else if (a == 'S' && b == 'L') {
          if (revex_state.bit_count < MAX_BITS) {
            revex_state.bits[revex_state.bit_count++] = '0';
          } else {
            finalize_revex_frame(
                revex_state,
                "overflow");
          }

        } else {
          ESP_LOGD(TAG,
                   "REVEX invalid pair=%c%c",
                   a,
                   b);

          finalize_revex_frame(
              revex_state,
              "invalid_pair");
        }

        if (revex_state.active &&
            revex_state.bit_count == REVEX_XP_BITS) {
          finalize_revex_frame(
              revex_state,
              "xp_34bit");
        }
      }
    }

    // OHM-07
    if (is_ohm_sync(p.duration)) {
      start_decoder(
          ohm_state,
          p.duration);

    } else if (ohm_state.active) {
      char c = ohm_class(p.duration);

      if (c == '?') {
        ESP_LOGD(TAG,
                 "OHM invalid pulse=%u bit_count=%u",
                 p.duration,
                 ohm_state.bit_count);

        reset_decoder(ohm_state);

      } else if (ohm_state.pending == 0) {
        ohm_state.pending = c;

      } else {
        char a = ohm_state.pending;
        char b = c;

        ohm_state.pending = 0;

        if (a == 'A' && b == 'B') {
          if (ohm_state.bit_count < OHM_BITS) {
            ohm_state.bits[ohm_state.bit_count++] = '0';
          } else {
            reset_decoder(ohm_state);
          }

        } else if (a == 'B' && b == 'A') {
          if (ohm_state.bit_count < OHM_BITS) {
            ohm_state.bits[ohm_state.bit_count++] = '1';
          } else {
            reset_decoder(ohm_state);
          }

        } else {
          ESP_LOGD(TAG,
                   "OHM invalid pair=%c%c bit_count=%u",
                   a,
                   b,
                   ohm_state.bit_count);

          reset_decoder(ohm_state);
        }

        if (ohm_state.active &&
            ohm_state.bit_count == OHM_BITS) {
          ohm_state.bits[OHM_BITS] = '\0';

          std::string raw_hex =
              bits_to_hex(
                  ohm_state.bits,
                  OHM_BITS);

          ESP_LOGD(TAG,
                   "OHM-07 detected: bits=%s raw_hex=%s",
                   ohm_state.bits,
                   raw_hex.c_str());

          queue_event(
              "ohm_07",
              ohm_state.bits,
              OHM_BITS,
              raw_hex,
              ohm_state.sync_us);

          reset_decoder(ohm_state);
        }
      }
    }
  }
}

}  // namespace jp_wireless_chime_receiver
}  // namespace esphome