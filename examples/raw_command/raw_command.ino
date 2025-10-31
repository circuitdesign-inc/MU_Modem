/**
 * @file raw_command_example.ino
 * @brief MU-ModemライブラリのSendRawCommand関数を使用したサンプル
 *
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
  // デバッグ用のシリアルを開始
  Serial.begin(115200);
  while (!Serial);
  Serial.println("Raw Command Example");

  // MUモデムとの通信にSerial1を使用します。
  // モデム用のシリアルを開始
  Serial1.begin(MU_DEFAULT_BAUDRATE);

  // モデムドライバを初期化します。
  // このサンプルでは受信は行わないため、コールバック関数は指定しません。
  MU_Modem_Error err = modem.begin(Serial1, MU_Modem_FrequencyModel::MHz_429);
  if (err != MU_Modem_Error::Ok)
  {
    Serial.println("MU Modem initialization failed. Please check the connection.");
    while (true);
  }
  Serial.println("MU Modem initialized.");
}

void loop() {
  // モデムの内部処理のために、Work()メソッドを定期的に呼び出す必要があります。
  modem.Work();

  // 5秒ごとに@VRコマンドを送信
  static uint32_t lastSendTime = 0;
  if (millis() - lastSendTime > 5000) {
    lastSendTime = millis();

    Serial.println("\n--- Sending @VR command ---");

    // モデムからのレスポンスを格納するためのバッファ
    char responseBuffer[64];

    // @VRコマンドを送信し、応答を待ちます。
    // コマンドの末尾には必ず "\r\n" を付ける必要があります。
    const char* command = "@VR\r\n";
    MU_Modem_Error err = modem.SendRawCommand(command, responseBuffer, sizeof(responseBuffer));

    // 結果をシリアルモニタに表示
    if (err == MU_Modem_Error::Ok) {
      Serial.print("Success! Response: ");
      Serial.println(responseBuffer);
    } else if (err == MU_Modem_Error::Fail) {
      Serial.println("Failed. Timeout or parse error occurred.");
    } else if (err == MU_Modem_Error::BufferTooSmall) {
      Serial.println("Failed. Response was too long for the buffer.");
    } else {
      Serial.print("An error occurred: ");
      Serial.println((int)err);
    }
  }
}