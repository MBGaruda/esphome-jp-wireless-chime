# ESPHome JP Wireless Chime Receiver

Home Assistant 用の ESPHome external component。
ESP32 と 315MHz RF 受信モジュールを使用して、日本の無線チャイム信号を受信します。

このコンポーネントは日本の無線ドアチャイム製品の RF 信号を解析し、
Home Assistant イベントとして通知します。

---

## 対応プロトコル

現在対応しているプロトコル：

* REVEX X シリーズ
* OHM 07 シリーズ

---

## 機能

* 315MHz ASK/OOK RF 信号受信
* REVEX / OHM RF プロトコル解析
* 24bit RF データ抽出
* Home Assistant イベント送信
* 同一信号の重複抑止（5秒）

---

## インストール

### external_components を使用

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

## 使い方

### 基本的な設定例

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

### パラメータ説明

* **pin**:
  RF 受信モジュールの DATA ピンを接続する GPIO 番号

---

## ハードウェア

### 必要なもの

* ESP32
* 315MHz ASK/OOK RF受信モジュール

### 動作確認済み受信モジュール

* MX-RM-5V
* SYN470R系
* RXB6
* WL101-341

### 接続例

```text
RF Receiver → ESP32

DATA → GPIO27
VCC  → 3.3V
GND  → GND
```

---

## Home Assistant イベント

受信時に以下イベントを送信します。

```text
esphome.jp_wireless_chime_raw_received
```

---

## イベントデータ

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

## パラメータ説明

### protocol_version

イベント仕様バージョン。

現在は固定で `"1"`。

### source

イベント送信元。

現在は固定で `"esp32_rf_receiver"`。

### protocol_hint

受信した RF プロトコル。

* `revex_x`
* `ohm_07`

### bit_count

受信したビット数。

現在は固定で `"24"`。

### bits

24bit の生ビット列。

### raw_hex

bits を HEX 化した値。

Home Assistant 統合側では通常この値をキーとして扱います。

### sync_us

検出した同期パルス長（マイクロ秒）。

### received_at_ms

ESP32 起動後の millis() 値。

---

## 重複抑止

多くの RF 送信機は、1回のボタン押下で同じ信号を複数回送信します。

このコンポーネントでは：

```text
protocol_hint + raw_hex
```

が同一の場合、5秒間イベント送信を抑止します。

---

## Home Assistant 統合との連携

この component は：

* RF受信
* RF解析
* raw_hex生成

までを担当します。

以下は Home Assistant 側の `jp_wireless_chime` 統合で行う想定です。

* チャンネル解釈
* 音色解釈
* デバイス化
* UI表示
* オートメーション連携

---

## 自動化の例

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

## 動作確認環境

* Home Assistant 最新版
* ESPHome 2026.4.x
* ESP32 DevKit
* REVEX Xシリーズ
* OHM-07シリーズ

---

## 開発状況

この component は開発中です。

現在は以下機能を実装済みです。

* REVEX X 受信
* OHM-07 受信
* Home Assistant イベント送信

---

## 免責事項

このプロジェクトは REVEX、OHM、ESPHome、Home Assistant とは何ら関係はありません。
