/**
 * @file simple_tx_rx.ino
 * @brief MU-Modemライブラリの基本的な送受信サンプル
 * @copyright Copyright (c) 2025 CircuitDesign,Inc.
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 *
 * @details
 * このサンプルプログラムは、MU-Modemライブラリを使用して、モデムの初期化、
 * 各種IDとチャンネルの設定、そして定期的なデータ送信と非同期のデータ受信を
 * 行う基本的な方法を示します。
 *
 * 動作:
 * 1. `setup()` 関数でモデムを初期化し、チャンネル、グループID、目的局ID、自機IDを設定します。
 * 2. `loop()` 関数内で、5秒ごとに "Hello MU!" というメッセージを送信します。
 * 3. 同時に、`modem.Work()` を通じてモデムがデータを受信したかを常に確認し、
 *    受信した場合はコールバック関数 `mu_callback` が呼び出され、その内容がシリアルモニタに表示されます。
 *
 * このサンプルを実行するには、Arduino互換ボードのSerial1に
 * MUモデムが接続されている必要があります。
 */
#include <MU_Modem.h>

MU_Modem modem;

/**
 * @brief モデムからの非同期イベントを処理するコールバック関数
 *
 * データ受信時や非同期コマンドの応答受信時にライブラリから自動的に呼び出されます。
 * @param error エラーコード
 * @param responseType 応答の種類
 * @param value 応答に含まれる数値（RSSIなど）
 * @param pPayload 受信したデータのペイロードへのポインタ
 * @param len ペイロードの長さ (バイト単位)
 * @param pRouteInfo 受信パケットに含まれるルート情報へのポインタ
 * @param numRouteNodes ルート情報のノード数
 */
void mu_callback(MU_Modem_Error error, MU_Modem_Response responseType, int32_t value, const uint8_t *pPayload, uint16_t len, const uint8_t* pRouteInfo, uint8_t numRouteNodes)
{
  // データ受信応答の場合のみ処理
  if (responseType == MU_Modem_Response::DataReceived)
  {
    Serial.print("パケット受信 (");
    Serial.print(len);
    Serial.print(" バイト, RSSI: ");
    Serial.print(value); // RSSI値
    Serial.print(" dBm): ");

    // 受信データを文字列として表示
    for (int i = 0; i < len; i++) {
      Serial.write(pPayload[i]);
    }
    Serial.println();
  }
}

void setup() {
  Serial.begin(115200);
  
  // シリアルポートが開くまで待機
  while (!Serial);

  // モデム用のシリアルポートを初期化
  Serial1.begin(MU_DEFAULT_BAUDRATE);

  // モデムを初期化します。
  // 第2引数で周波数モデルを、第3引数でコールバック関数を指定します。
  // MU_Modem_FrequencyModel::MHz_429 または MU_Modem_FrequencyModel::MHz_1216
  MU_Modem_Error err = modem.begin(Serial1, MU_Modem_FrequencyModel::MHz_429, mu_callback);
  if (err != MU_Modem_Error::Ok)
  {
    Serial.println("MUモデムの初期化に失敗しました。接続を確認してください。");
    while (true);
  }
  Serial.println("MUモデムが初期化されました。");

  // 各種IDとチャンネルを設定します。
  // 第2引数(saveValue)をtrueにすると、設定がモデムの不揮発性メモリに保存されます。
  // このサンプルでは、電源を入れ直すと元に戻るようにfalseに設定しています。

  // チャンネルを設定 (例: 429MHz帯のチャンネル7)
  uint8_t channel = 0x07;
  if (modem.SetChannel(channel, false) != MU_Modem_Error::Ok) {
    Serial.println("チャンネルの設定に失敗しました。");
  }
  Serial.print("チャンネルを "); Serial.print(channel, HEX); Serial.println(" に設定しました。");

  // グループIDを設定
  uint8_t groupID = 0x01;
  if (modem.SetGroupID(groupID, false) != MU_Modem_Error::Ok) {
    Serial.println("グループIDの設定に失敗しました。");
  }
  Serial.print("グループIDを "); Serial.print(groupID, HEX); Serial.println(" に設定しました。");

  // 目的局IDを設定 (0x00はブロードキャスト)
  uint8_t destID = 0x00;
  if (modem.SetDestinationID(destID, false) != MU_Modem_Error::Ok) {
    Serial.println("目的局IDの設定に失敗しました。");
  }
  Serial.print("目的局IDを "); Serial.print(destID, HEX); Serial.println(" に設定しました。");

  // 自機IDを設定
  uint8_t equipID = 0x02;
  if (modem.SetEquipmentID(equipID, false) != MU_Modem_Error::Ok) {
    Serial.println("自機IDの設定に失敗しました。");
  }
  Serial.print("自機IDを "); Serial.print(equipID, HEX); Serial.println(" に設定しました。");
}

void loop() {
  // モデムの内部処理（受信データの解析、コールバック呼び出しなど）のために、Work()を常に呼び出します。
  modem.Work();

  // 例: 5秒ごとに "Hello MU!" を送信
  static uint32_t lastSendTime = 0;
  if (millis() - lastSendTime > 5000) {
    lastSendTime = millis();
    const char* message = "Hello MU!";
    Serial.print("メッセージを送信中: ");
    Serial.println(message);
    modem.TransmitData((const uint8_t*)message, strlen(message));
  }
}
