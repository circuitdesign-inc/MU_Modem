/**
 * @file advanced_tx_rx_example.ino
 * @brief MU-Modemライブラリの高度な送受信サンプル
 * @copyright Copyright (c) 2025 CircuitDesign,Inc.
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 *
 * @details
 * このサンプルプログラムは、MU-Modemライブラリを使用して、
 * 非同期でのデータ受信と、エラー処理を含むデータ送信を行う方法を示します。
 * simple_tx_rx.inoの発展形です。
 *
 * 動作:
 * 1. setup()関数でモデムを初期化し、非同期イベントを受信するためのコールバック関数を登録します。
 * 2. loop()関数内で、5秒ごとにカウンター付きのメッセージを送信します。
 * 3. 送信時、TransmitData関数の戻り値を確認し、LBT(Listen Before Talk)で失敗した場合は、少し待ってから再送信を試みます。
 * 4. データを受信するとコールバック関数が呼び出され、受信データをグローバル変数に格納し、受信フラグを立てます。
 * 5. loop()関数は受信フラグを検知し、受信したデータペイロードとRSSI値を整形してシリアルモニタに表示します。
 *
 * このサンプルを実行するには、Arduino互換ボードのSerial1に
 * MUモデムが接続されている必要があります。
 */
#include <MU_Modem.h>

MU_Modem modem;

// --- 受信データ共有用の変数 ---
// コールバック関数とloop()関数で安全にデータをやり取りするためにvolatile修飾子を使用します。

// データ受信フラグ。コールバックでtrueになり、loopで処理後にfalseになる。
volatile bool g_packetReceived = false;

// 受信したペイロードを格納するバッファ。
uint8_t g_receivedPayload[MU_MAX_PAYLOAD_LEN];

// @brief 受信したペイロードの長さ。
uint8_t g_receivedLen = 0;

// @brief 最後に受信したパケットのRSSI値。
int16_t g_lastRssi = 0;


/**
 * @brief モデムからの非同期イベントを処理するコールバック関数
 *
 * データ受信時や非同期コマンドの応答受信時にライブラリから自動的に呼び出されます。
 * @param event イベント情報構造体
 */
void modemCallback(const MU_Modem_Event &event)
{
  // データ受信イベントかを確認
  if (event.type == MU_Modem_Response::DataReceived)
  {
    if (event.error == MU_Modem_Error::Ok)
    {
      Serial.println("\n[コールバック] データを受信しました！");
      // loop()で処理するために、受信データをグローバル変数にコピーしてフラグを立てる
      if (event.payloadLen > 0 && event.payloadLen <= sizeof(g_receivedPayload))
      {
        memcpy(g_receivedPayload, event.pPayload, event.payloadLen);
        g_receivedLen = (uint8_t)event.payloadLen;
        g_lastRssi = (int16_t)event.value; // データ受信時のRSSI値はvalueパラメータで渡される
        g_packetReceived = true;
      }
    }
    else
    {
      Serial.printf("[コールバック] データ受信エラー: %d\n", (int)event.error);
    }
  }
  // 他の非同期応答(RSSI取得など)もここで処理可能
  else
  {
    Serial.printf("[コールバック] 非同期イベント受信 Type: %d, Error: %d, Value: %ld\n", (int)event.type, (int)event.error, event.value);
  }
}

void setup() {
  Serial.begin(115200);

  // シリアルポートが開くまで待機
  while (!Serial);
  Serial.println("\n--- 高度な送受信サンプル ---");

  // モデム用のシリアルポートを初期化
  Serial1.begin(MU_DEFAULT_BAUDRATE);

  // モデムドライバを初期化します。データ受信のためにコールバック関数を指定します。
  // begin()の内部で、RSSI値の自動通知(@SION)などが有効化されます。
  MU_Modem_Error err = modem.begin(Serial1, MU_Modem_FrequencyModel::MHz_429, modemCallback);
  if (err != MU_Modem_Error::Ok)
  {
    Serial.println("MUモデムの初期化に失敗しました。接続を確認してください。処理を停止します。");
    while (true);
  }
  Serial.println("MUモデムの初期化に成功しました。");

  // 各種IDとチャンネル、送信出力を設定します。
  // saveValueをtrueにすると、設定がモデムの不揮発性メモリに保存されます。
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

  // 送信出力を設定 (10mWもしくは1mW)
  // 0x01: 1mW, 0x10: 10mW
  // 小エリアで通信する場合は 1mW に設定すると、多くのシステムで電波資源を有効に共有することができます。
  err = modem.SetPower(0x10, false);
  if (err == MU_Modem_Error::Ok) {
    Serial.println("送信出力を10mWに設定しました。");
  } else {
    Serial.printf("送信出力の設定に失敗しました。エラー: %d\n", (int)err);
  }
}

void loop() {
  // モデムの内部処理(シリアルデータの解析やコールバックの呼び出し)のために、
  // Work()メソッドを常に呼び出す必要があります。
  modem.Work();

  // --- データ受信処理 ---
  if (g_packetReceived)
  {
    Serial.println("\n--- 受信パケット処理 ---");
    Serial.printf("RSSI: %d dBm\n", g_lastRssi);
    Serial.printf("長さ: %u バイト\n", g_receivedLen);
    Serial.print("ペイロード (16進数): ");
    for (int i = 0; i < g_receivedLen; i++) {
      Serial.printf("%02X ", g_receivedPayload[i]);
    }
    Serial.println();
    Serial.print("ペイロード (ASCII): ");
    Serial.write(g_receivedPayload, g_receivedLen);
    Serial.println("\n----------------------------------");

    // 処理が終わったらフラグをリセット
    g_packetReceived = false;
  }

  // --- データ送信処理 (5秒ごと) ---
  static uint32_t lastSendTime = 0;
  static uint8_t counter = 0;
  if (millis() - lastSendTime > 5000)
  {
    lastSendTime = millis();

    char message[32];
    snprintf(message, sizeof(message), "Hello MU! %u", counter++);

    Serial.printf("\n--- パケット送信: \"%s\" ---\n", message);

    const int MAX_RETRIES = 3;
    for (int i = 0; i < MAX_RETRIES; i++)
    {
      // ここでは同期型の TransmitData を使用しています。
      // もし TransmitDataAsync (非同期型) を使用する場合は、
      // 送信完了コールバックまで 'message' バッファが破棄されないよう
      // static修飾子を付けるか、グローバル変数にする必要があります。
      MU_Modem_Error err = modem.TransmitData((const uint8_t*)message, strlen(message));

      if (err == MU_Modem_Error::Ok)
      {
        Serial.println("TransmitDataコマンドの受付に成功しました。");
        break; // 送信成功
      }
      else if (err == MU_Modem_Error::FailLbt)
      {
        Serial.printf("送信失敗 (LBT)。チャンネルがビジーです。リトライします... (%d/%d)\n", i + 1, MAX_RETRIES);
        delay(100); // 少し待ってからリトライ
      }
      else
      {
        Serial.printf("送信失敗。エラー: %d\n", (int)err);
        break; // LBT以外のエラーではリトライしない
      }

      if (i == MAX_RETRIES - 1) {
        Serial.println("リトライ回数の上限に達したため、送信を中止しました。");
      }
    }
  }
}
