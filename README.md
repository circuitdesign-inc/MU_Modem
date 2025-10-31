# MU_Modem

サーキットデザイン社製MUシリーズ無線モデムを制御するためのArduinoライブラリです。

## 概要

このライブラリは、Arduinoでサーキットデザイン社製MUシリーズ無線モデム（例：MU-3-429及びMU-3-1216、MU-4-429）を制御するためのインターフェースを提供します。
シリアルコマンドインターフェースを介して、データの送受信やモデムの設定を簡単に行うことができます。

## 対応ハードウェア

*   Circuit Design MU-3-429
*   Circuit Design MU-3-1216
*   Circuit Design MU-4-429

## インストール

1.  GitHubリポジトリから最新のリリースをダウンロードします。
2.  Arduino IDEで、`スケッチ` > `ライブラリをインクルード` > `.ZIP形式のライブラリをインストール...` に移動します。
3.  ダウンロードしたZIPファイルを選択します。

## 基本的な使い方

以下は、MUモデムを初期化し、データを受信し、5秒ごとにメッセージを送信する基本的なサンプルコードです。

```cpp
#include <MU_Modem.h>

// MUモデムのインスタンスを作成
MU_Modem modem;

// データ受信時に呼び出されるコールバック関数
void modemCallback(MU_Modem_Error error, MU_Modem_Response responseType, int32_t value, const uint8_t *pPayload, uint16_t len, const uint8_t* pRouteInfo, uint8_t numRouteNodes)
{
  if (responseType == MU_Modem_Response::DataReceived && error == MU_Modem_Error::Ok)
  {
    Serial.println("\n--- データ受信 ---");
    Serial.printf("RSSI: %d dBm, 長さ: %u バイト\n", (int16_t)value, len);
    Serial.print("ペイロード: ");
    Serial.write(pPayload, len);
    Serial.println("\n--------------------");
  }
}

void setup() {
  // PCとの通信用シリアルを開始
  Serial.begin(115200);
  while (!Serial);
  Serial.println("MU Modem Basic Example");

  // MUモデムとの通信用シリアル(Serial1など)を開始
  // デフォルトのボーレートは19200bps
  Serial1.begin(MU_DEFAULT_BAUDRATE);

  // モデムを初期化
  // 第1引数: 通信に使うStreamオブジェクト
  // 第2引数: 使用するモデムの周波数モデル(MHz_429:MU-3-429とMU-4-429, MHz_1216:MU-3-1216に対応)
  // 第3引数: データ受信時に呼び出すコールバック関数 (受信しない場合はnullptr)
  MU_Modem_Error err = modem.begin(Serial1, MU_Modem_FrequencyModel::MHz_429, modemCallback);

  if (err != MU_Modem_Error::Ok) {
    Serial.println("MUモデムの初期化に失敗しました。");
    while (1);
  }
  Serial.println("MUモデムの初期化が完了しました。");

  // モデムの各種設定 (任意)
  modem.SetChannel(0x07, false);      // チャンネルを7に設定
  modem.SetEquipmentID(0x01, false);  // 自局IDを1に設定
}

void loop() {
  // この関数をループ内で常に呼び出す必要があります。
  // 受信データの解析やコールバックの呼び出しを行います。
  modem.Work();

  // 5秒ごとにメッセージを送信する例
  static uint32_t lastSendTime = 0;
  if (millis() - lastSendTime > 5000) {
    lastSendTime = millis();
    
    const char* message = "Hello, MU-Modem!";
    Serial.printf("'%s' を送信します...\n", message);
    
    // データ送信 (目的局IDは事前にSetDestinationID()で設定)
    MU_Modem_Error tx_err = modem.TransmitData((const uint8_t*)message, strlen(message));
    
    if (tx_err == MU_Modem_Error::Ok) {
      Serial.println("送信に成功しました。");
    } else {
      Serial.printf("送信に失敗しました。エラー: %d\n", (int)tx_err);
    }
  }
}
```

## API

For a detailed API reference, please see the comments in the `MU_Modem.h` header file.

## License

This library is released under the MIT License.
