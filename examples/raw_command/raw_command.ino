/**
 * @file raw_command_example.ino
 * @brief MU-ModemライブラリのSendRawCommand関数を使用したサンプル
 * @copyright Copyright (c) 2025 CircuitDesign,Inc.
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 *
 * @details
 * このサンプルプログラムは、MU-Modemライブラリに標準で実装されていない
 * 任意のコマンドをモデムに送信し、その応答を受信する方法を示します。
 *
 * 動作:
 * 1. setup()関数でモデムを初期化します。
 * 2. loop()関数内で、5秒ごとにバージョン情報を取得するコマンド「@VR\r\n」を
 *    SendRawCommand()関数を使って送信します。
 * 3. コマンドの送信と応答の受信が成功したかを確認し、結果をシリアルモニタに表示します。
 *
 * このサンプルを実行するには、Arduino互換ボードのSerial1に
 * MUモデムが接続されている必要があります。
 */
#include <MU_Modem.h>

MU_Modem modem;

void setup() {
  Serial.begin(115200);

  
  while (!Serial);
  Serial.println("SendRawCommand関数サンプル");

  // モデム用のシリアルポートを初期化
  Serial1.begin(MU_DEFAULT_BAUDRATE);

  // モデムドライバを初期化します。
  // このサンプルでは受信は行わないため、コールバック関数は指定しません。
  MU_Modem_Error err = modem.begin(Serial1, MU_Modem_FrequencyModel::MHz_429);
  if (err != MU_Modem_Error::Ok)
  {
    Serial.println("MUモデムの初期化に失敗しました。接続を確認してください。");
    while (true);
  }
  Serial.println("MUモデムが初期化されました。");
}

void loop() {
  // モデムの内部処理のために、Work()メソッドを定期的に呼び出す必要があります。
  modem.Work();

  // 5秒ごとに@VRコマンドを送信
  static uint32_t lastSendTime = 0;
  if (millis() - lastSendTime > 5000) {
    lastSendTime = millis();

    Serial.println("\n--- @VRコマンドを送信します ---");

    // モデムからのレスポンスを格納するためのバッファ
    char responseBuffer[64];

    // @VRコマンドを送信し、応答を待ちます。
    // コマンドの末尾には必ず "\r\n" を付ける必要があります。
    const char* command = "@VR\r\n";
    MU_Modem_Error err = modem.SendRawCommand(command, responseBuffer, sizeof(responseBuffer));

    // 結果をシリアルモニタに表示
    if (err == MU_Modem_Error::Ok) {
      Serial.print("成功！ 応答: ");
      Serial.println(responseBuffer);
    } else if (err == MU_Modem_Error::Fail) {
      Serial.println("失敗。タイムアウトまたは解析エラーが発生しました。");
    } else if (err == MU_Modem_Error::BufferTooSmall) {
      Serial.println("失敗。応答がバッファに対して長すぎます。");
    } else {
      Serial.print("エラーが発生しました: ");
      Serial.println((int)err);
    }
  }
}