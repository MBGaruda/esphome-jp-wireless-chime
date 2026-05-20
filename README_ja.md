# JP Wireless Chime Receiver for ESPHome

日本のワイヤレスチャイム信号を ESP32 + ESPHome で受信し、Home Assistant イベントとして通知する ESPHome External Component です。

このコンポーネントは以下のプロトコルに対応しています。

- REVEX X シリーズ
- REVEX XP シリーズ
- OHM-07 シリーズ (品番が07で始まるワイヤレスチャイムシリーズ)

受信した信号は Home Assistant のイベントとして発火され、`ha-jp-wireless-chime` などの統合で利用できます。

関連リポジトリ：

- ha-jp-wireless-chime
  https://github.com/MBGaruda/ha-jp-wireless-chime

- esphome-jp-wireless-chime
  https://github.com/MBGaruda/esphome-jp-wireless-chime

---

## 対応プロトコル

### REVEX X

- 24bit RF フレーム
- `protocol_hint: revex_x`

### REVEX XP

- 34bit RF フレーム
- XP拡張音色対応
- `protocol_hint: revex_xp`

### OHM-07

- 24bit RF フレーム
- `protocol_hint: ohm_07`

---

## 動作仕様

### イベント発火

受信した RF 信号は Home Assistant イベントとして発火されます。

イベント名：

```text
esphome.jp_wireless_chime_raw_received
```

イベント例：

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

## イベントデータ

| 項目 | 内容 |
|---|---|
| source | ESPHome の `esphome.name` |
| protocol_version | イベント仕様バージョン |
| protocol_hint | `revex_x` / `revex_xp` / `ohm_07` |
| bit_count | 受信ビット数 |
| bits | 生ビット列 |
| raw_hex | デコード済みHEX |
| sync_us | 検出した同期パルス長 |
| received_at_ms | ESP32起動後の受信時刻(ms) |

---

## 重複イベント抑止

ワイヤレスチャイム送信機は、1回の押下で複数フレームを送信する場合があります。

このコンポーネントでは、同一プロトコルのイベントを一定時間抑止します。

仕様：

- 同一 `protocol_hint` のイベントを抑止
- 同一信号を受信するたびにタイマーを更新
- RF信号が途切れてから 2000ms 後に再発火可能

これにより：

- 長時間送信型チャイム
- 同一押下中の誤デコード
- ノイズによる別HEX生成

などによる二重発火を防止します。

---

## インストール

### External Components

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/MBGaruda/esphome-jp-wireless-chime
```

---

## ESPHome設定例

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

## GPIO接続例

一般的な 315MHz ASK/OOK RF Receiver Module を使用します。

| RF Receiver | ESP32 |
|---|---|
| VCC | 3.3V |
| GND | GND |
| DATA | GPIO27 |

---

## Home Assistantでの確認

Home Assistant の開発ツールからイベントを監視できます。

```text
開発ツール
→ イベント
→ イベントを購読

esphome.jp_wireless_chime_raw_received
```

---

## ログレベル

通常運用：

```yaml
logger:
  level: INFO
```

デバッグ：

```yaml
logger:
  level: DEBUG
```

---

## 受信例

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

## 注意事項

- 本コンポーネントは 315MHz 日本向けワイヤレスチャイムを対象としています
- RF受信モジュールの性能やノイズ環境により受信精度は変化します
- ESPHome 2026.4 系で動作確認しています