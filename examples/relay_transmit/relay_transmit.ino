/**
 * @file relay_transmit.ino
 * @brief MU-Modemライブラリの中継機能を使用したサンプル
 * @copyright Copyright (c) 2025 CircuitDesign,Inc.
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 *
 * @details
 * このサンプルプログラムは、MU-Modemライブラリを使用して、
 * 送信元局 -> 中継局 -> 宛先局 という1段の中継通信を行う方法を示します。
 * 送信するデータは送信元局からのみ送信され、中継局・宛先局は受信のみ行います。
 *
 * このコードは、送信元・中継・宛先の各局で共通して使用できます。
 * MY_EQUIPMENT_ID の値を変更して、各局の役割を設定してください。
 *
 * 役割設定:
 * - 送信元局: MY_EQUIPMENT_ID = SOURCE_ID (例: 0x01)
 * - 中継局:   MY_EQUIPMENT_ID = RELAY_ID  (例: 0x02)
 * - 宛先局:   MY_EQUIPMENT_ID = DEST_ID   (例: 0x03)
 *
 * 動作:
 * 1. setup()関数でモデムを初期化し、共通のチャンネル、グループIDを設定します。
 * 2. 各局固有の機器ID (MY_EQUIPMENT_ID) を設定します。
 * 3. 送信元局の場合のみ、`SetRouteInfo()` を使用して中継ルート [中継局ID, 宛先局ID] を
 * モデムのルートレジスタに設定します。
 * 4. `loop()` 関数内で、常に `modem.Work()` を呼び出してモデムの処理を行います。
 * 5. データを受信するとコールバック関数が呼び出され、受信内容をシリアルモニタに表示します。(中継局も宛先局もデータを受信します)
 * 6. 送信元局の場合のみ、10秒ごとにカウンター付きのメッセージを送信します。
 *    - このサンプルでは、`TransmitData()` の第3引数に `true` を指定し、モデムのルートレジスタに設定された情報を使って中継送信します。
 *    - 別のアプローチとして、`TransmitDataWithRoute()` を使用して、送信時にルート情報を直接指定することも可能です。
 *
 * このサンプルを実行するには、Arduino互換ボードのSerial1に
 * MUモデムが接続されている必要があります。
 */
#include <MU_Modem.h>

// --- 定数定義 ---
const uint8_t COMMON_CHANNEL = 0x07; // 3局共通のチャンネル
const uint8_t COMMON_GROUP_ID = 0x01; // 3局共通のグループID

const uint8_t SOURCE_ID = 0x01;  // 送信元局の機器ID
const uint8_t RELAY_ID  = 0x02;  // 中継局の機器ID
const uint8_t DEST_ID   = 0x03;  // 宛先局の機器ID

// ★★★ このボードの役割に応じてIDを設定してください ★★★
const uint8_t MY_EQUIPMENT_ID = SOURCE_ID; // 例: 送信元局として動作させる場合
// const uint8_t MY_EQUIPMENT_ID = RELAY_ID; // 例: 中継局として動作させる場合
// const uint8_t MY_EQUIPMENT_ID = DEST_ID;  // 例: 宛先局として動作させる場合

MU_Modem modem;

// --- 受信データ共有用の変数 (コールバックとloop間) ---
volatile bool g_packetReceived = false;
uint8_t g_receivedPayload[MU_MAX_PAYLOAD_LEN];
uint8_t g_receivedLen = 0;
int16_t g_lastRssi = 0;

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
void modemCallback(MU_Modem_Error error, MU_Modem_Response responseType, int32_t value, const uint8_t *pPayload, uint16_t len, const uint8_t* pRouteInfo, uint8_t numRouteNodes)
{
  if (responseType == MU_Modem_Response::DataReceived)
  {
    if (error == MU_Modem_Error::Ok)
    {
      Serial.println("\n[コールバック] データを受信しました！");
      if (len > 0 && len <= sizeof(g_receivedPayload))
      {
        memcpy(g_receivedPayload, pPayload, len);
        g_receivedLen = len;
        g_lastRssi = value;
        g_packetReceived = true;
      }
    }
    else
    {
      Serial.printf("[コールバック] データ受信エラー: %d\n", (int)error);
    }
  }
  else
  {
    Serial.printf("[コールバック] 非同期イベント受信 Type: %d, Error: %d, Value: %ld\n", (int)responseType, (int)error, value);
  }
}

void setup() {
  Serial.begin(115200);

  // シリアルポートが開くまで待機
  while (!Serial);

  Serial.println("\n--- 中継通信サンプル ---");
  Serial.print("この局のID: 0x"); Serial.println(MY_EQUIPMENT_ID, HEX);

  // モデム用のシリアルポートを初期化
  Serial1.begin(MU_DEFAULT_BAUDRATE);

  // モデムを初期化 (コールバック関数を登録)
  MU_Modem_Error err = modem.begin(Serial1, MU_Modem_FrequencyModel::MHz_429, modemCallback);
  if (err != MU_Modem_Error::Ok)
  {
    Serial.println("MUモデムの初期化に失敗しました。");
    while (true);
  }
  Serial.println("MUモデムが初期化されました。");

  // --- 共通設定 ---
  Serial.println("共通設定中...");
  // チャンネルを設定
  if (modem.SetChannel(COMMON_CHANNEL, false) != MU_Modem_Error::Ok) {
    Serial.println("チャンネルの設定に失敗しました。");
  }
  Serial.print("チャンネルを 0x"); Serial.print(COMMON_CHANNEL, HEX); Serial.println(" に設定しました。");

  // グループIDを設定
  if (modem.SetGroupID(COMMON_GROUP_ID, false) != MU_Modem_Error::Ok) {
    Serial.println("グループIDの設定に失敗しました。");
  }
  Serial.print("グループIDを 0x"); Serial.print(COMMON_GROUP_ID, HEX); Serial.println(" に設定しました。");

  // --- 個別設定 ---
  Serial.println("個別設定中...");
  // 自機IDを設定
  if (modem.SetEquipmentID(MY_EQUIPMENT_ID, false) != MU_Modem_Error::Ok) {
    Serial.println("自機IDの設定に失敗しました。");
  }
  Serial.print("自機IDを 0x"); Serial.print(MY_EQUIPMENT_ID, HEX); Serial.println(" に設定しました。");

  // --- 送信元局のみルート情報を設定 ---
  if (MY_EQUIPMENT_ID == SOURCE_ID) {
    Serial.println("送信元局としてルート情報を設定します...");
    // ルート情報: [中継局ID, 宛先局ID]
    uint8_t route[] = {RELAY_ID, DEST_ID};
    // ルートレジスタに設定 (@RT コマンド)
    err = modem.SetRouteInfo(route, sizeof(route), false);
    if (err != MU_Modem_Error::Ok) {
      Serial.println("ルート情報の設定に失敗しました。");
    } else {
      Serial.print("ルート情報を [0x"); Serial.print(RELAY_ID, HEX);
      Serial.print(", 0x"); Serial.print(DEST_ID, HEX); Serial.println("] に設定しました。");

      // ルートレジスタの自動設定(@RRON)を変更します。
      // この設定を有効にすると、受信パケットをもとに、受信側で返信ルートを自動生成し、ルートレジスタに書き込みます。
      // ルート設定コマンドで設定したルート情報も、データパケットを受ける度に上書きされるので注意してください
      if (modem.SetAutoReplyRoute(false,false) != MU_Modem_Error::Ok) {
        Serial.println("ルート使用設定(@RRON)に失敗しました。");
      }
    }
     // 送信元局は目的局IDをダミー(e.g., 0x00)にしておきます (ルート情報を使うためDIは無視されます)
     if (modem.SetDestinationID(0x00, false) != MU_Modem_Error::Ok) {
         Serial.println("ダミー目的局IDの設定に失敗しました。");
     }
  } else {
    // 中継局・宛先局はルート設定不要
    // (中継局は受信したパケットのルート情報を見て自動中継する)
    // (宛先局もDIは関係なく、ルート情報で自分が終点になっていれば受信する)
     if (modem.SetDestinationID(0x00, false) != MU_Modem_Error::Ok) {
         Serial.println("ダミー目的局IDの設定に失敗しました。");
     }
  }

  Serial.println("設定完了。動作を開始します。");
}

void loop() {
  // モデムの内部処理（受信データの解析、コールバック呼び出しなど）のために、Work()を常に呼び出します。
  modem.Work();

  // --- 受信データの処理 ---
  if (g_packetReceived)
  {
    Serial.println("\n--- 受信パケット ---");
    Serial.printf("RSSI: %d dBm\n", g_lastRssi);
    Serial.printf("長さ: %u バイト\n", g_receivedLen);
    Serial.print("ペイロード (16進数): ");
    for (int i = 0; i < g_receivedLen; i++) {
      Serial.printf("%02X ", g_receivedPayload[i]);
    }
    Serial.println();
    Serial.print("ペイロード (ASCII): ");
    Serial.write(g_receivedPayload, g_receivedLen);
    Serial.println("\n--------------------");

    // フラグをリセットして次の受信に備える
    g_packetReceived = false;
  }

  // --- 送信元局のみ定期的にデータを送信 ---
  if (MY_EQUIPMENT_ID == SOURCE_ID) {
    static uint32_t lastSendTime = 0;
    static uint32_t counter = 0;
    if (millis() - lastSendTime > 10000) // 10秒ごとに送信
    {
      lastSendTime = millis();

      char message[40];
      snprintf(message, sizeof(message), "Message via Relay! Count: %lu", counter++);

      Serial.printf("\n--- メッセージ送信: \"%s\" ---\n", message);

      const int MAX_RETRIES = 3;
      for (int i = 0; i < MAX_RETRIES; i++)
      {
        // TransmitDataの第3引数に true を指定して、
        // SetRouteInfoで設定済みのルートレジスタ情報を使用するよう指示します。
        // TransmitDataWithRoute() を使用して、送信時にルート情報を直接指定することも可能です。
        MU_Modem_Error err = modem.TransmitData((const uint8_t*)message, strlen(message), true);

        if (err == MU_Modem_Error::Ok)
        {
          Serial.println("送信コマンド受付成功。");
          break; // 送信成功
        }
        else if (err == MU_Modem_Error::FailLbt)
        {
          Serial.printf("送信失敗 (LBT)。リトライします... (%d/%d)\n", i + 1, MAX_RETRIES);
          delay(100); // 少し待ってリトライ
        }
        else
        {
          Serial.printf("送信失敗。エラー: %d\n", (int)err);
          break; // LBT以外のエラー
        }
        if (i == MAX_RETRIES - 1) {
             Serial.println("リトライ上限到達。送信中止。");
         }
      }
    }
  }

}