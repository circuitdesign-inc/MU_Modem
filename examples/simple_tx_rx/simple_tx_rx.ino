/**
 * @file simple_tx_rx.ino
 * @brief MU-Modemライブラリの基本的な送受信サンプル
 * @copyright Copyright (c) 2026 CircuitDesign,Inc.
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 *
 * @details
 * このサンプルプログラムは、MU-Modemライブラリを使用して、モデムの初期化、
 * 各種IDとチャンネルの設定、そして定期的なデータ送信とポーリングによるデータ受信を行う基本的な方法を示します。
 *
 * 動作:
 * 1. `setup()` 関数でモデムを初期化し、チャンネル、グループID、目的局ID、自機IDを設定します。
 * 2. `loop()` 関数内で、5秒ごとに "Hello MU!" というメッセージを送信します。
 * 3. 同時に、`modem.Work()` を呼び出した後、`modem.HasPacket()` で受信データの有無を確認します。
 * データがある場合は `modem.GetPacket()` で内容を取得しシリアルモニタに表示します。
 * (処理後、`modem.DeletePacket()` でバッファを解放します)
 *
 * このサンプルを実行するには、Arduino互換ボードのSerial1に
 * MUモデムが接続されている必要があります。
 */
#include <MU_Modem.h>

MU_Modem modem;

void setup()
{
  Serial.begin(115200);

  // シリアルポートが開くまで待機
  while (!Serial)
    ;

  // モデム用のシリアルポートを初期化
  Serial1.begin(MU_DEFAULT_BAUDRATE);

  // モデムを初期化します。
  // 第2引数で周波数モデルを指定します。
  // MU_Modem_FrequencyModel::MHz_429 または MU_Modem_FrequencyModel::MHz_1216
  MU_Modem_Error err = modem.begin(Serial1, MU_Modem_FrequencyModel::MHz_429);
  if (err != MU_Modem_Error::Ok)
  {
    Serial.println("MUモデムの初期化に失敗しました。接続を確認してください。");
    while (true)
      ;
  }
  Serial.println("MUモデムが初期化されました。");

  // 受信データを格納するためのバッファを設定します
  static uint8_t rxBuffer[MU_MAX_PAYLOAD_LEN];
  modem.setPacketBuffer(rxBuffer, sizeof(rxBuffer));

  // 各種IDとチャンネルを設定します。
  // 第2引数(saveValue)をtrueにすると、設定がモデムの不揮発性メモリに保存されます。
  // このサンプルでは、電源を入れ直すと元に戻るようにfalseに設定しています。

  // チャンネルを設定 (例: 429MHz帯のチャンネル7)
  uint8_t channel = 0x07;
  if (modem.SetChannel(channel, false) != MU_Modem_Error::Ok)
  {
    Serial.println("チャンネルの設定に失敗しました。");
  }
  Serial.print("チャンネルを ");
  Serial.print(channel, HEX);
  Serial.println(" に設定しました。");

  // グループIDを設定
  uint8_t groupID = 0x01;
  if (modem.SetGroupID(groupID, false) != MU_Modem_Error::Ok)
  {
    Serial.println("グループIDの設定に失敗しました。");
  }
  Serial.print("グループIDを ");
  Serial.print(groupID, HEX);
  Serial.println(" に設定しました。");

  // 目的局IDを設定 (0x00はブロードキャスト)
  uint8_t destID = 0x00;
  if (modem.SetDestinationID(destID, false) != MU_Modem_Error::Ok)
  {
    Serial.println("目的局IDの設定に失敗しました。");
  }
  Serial.print("目的局IDを ");
  Serial.print(destID, HEX);
  Serial.println(" に設定しました。");

  // 自機IDを設定
  uint8_t equipID = 0x02;
  if (modem.SetEquipmentID(equipID, false) != MU_Modem_Error::Ok)
  {
    Serial.println("自機IDの設定に失敗しました。");
  }
  Serial.print("自機IDを ");
  Serial.print(equipID, HEX);
  Serial.println(" に設定しました。");
}

void loop()
{
  // モデムの内部処理（受信データの解析など）のために、Work()を常に呼び出します。
  modem.Work();

  // --- 受信処理 ---
  if (modem.HasPacket())
  {
    const uint8_t *pPayload;
    uint8_t len;

    // データの実体を取得
    if (modem.GetPacket(&pPayload, &len) == MU_Modem_Error::Ok)
    {
      Serial.print("パケット受信 (");
      Serial.print(len);
      Serial.print(" バイト): ");

      // 受信データを文字列として表示
      for (int i = 0; i < len; i++)
      {
        Serial.write(pPayload[i]);
      }
      Serial.println();
    }

    // 重要: データ処理が終わったら必ずパケットを破棄して、次の受信ができるようにしてください。
    modem.DeletePacket();
  }

  // --- 送信処理 (例: 5秒ごとに "Hello MU!" を送信) ---
  static uint32_t lastSendTime = 0;
  if (millis() - lastSendTime > 5000)
  {
    lastSendTime = millis();
    const char *message = "Hello MU!";
    Serial.print("メッセージを送信中: ");
    Serial.println(message);
    modem.TransmitData((const uint8_t *)message, strlen(message));
  }
}