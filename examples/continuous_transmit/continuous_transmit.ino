/**
 * @file continuous_transmit.ino
 * @brief MU-Modemライブラリの連続送信機能を使用したサンプル
 * @copyright Copyright (c) 2025 CircuitDesign,Inc.
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 *
 * @details
 * このサンプルプログラムは、CheckCarrierSense() と TransmitDataFireAndForget() を組み合わせて、
 * モデムのダブルバッファと連続送信モードを活用し、高スループットでデータを連続送信する方法を示します。
 *
 * このコードは、送信側・受信側の両方で共通して使用できます。
 * BOARD_ROLE の定義を切り替えて、各局の役割を設定してください。
 *
 * 役割設定:
 * - 送信側: #define BOARD_ROLE ROLE_TRANSMITTER
 * - 受信側: #define BOARD_ROLE ROLE_RECEIVER
 *
 * 動作 (送信側):
 * 1. setup()でモデムを初期化します。
 * 2. loop()内で、10秒ごとに連続送信処理を開始します。
 * 3. CheckCarrierSense()でチャンネルが空いているか確認します。
 * 4. チャンネルが空いていれば、forループで TransmitDataFireAndForget() を5回連続で呼び出し、
 *    5つのパケットを間髪入れずに送信します。
 * 5. チャンネルがビジーだった場合は、送信をスキップします。
 *
 * 動作 (受信側):
 * 1. setup()でモデムを初期化し、受信コールバック関数を登録します。
 * 2. loop()内で modem.Work() を呼び出し続けます。
 * 3. データを受信するとコールバック関数が呼び出され、受信内容がシリアルモニタに表示されます。
 *
 * 連続送信条件について(429MHz帯モデムの場合):
 * *DT応答を確認してから「5ms + (2.08ms * データ数)」以内に次のデータを送れば連続送信モードになります。(429MHz:2.08ms,1216MHz:1.04ms)
 * ここで、TransmitData関数を使用すると、キャリアセンスレスポンスを確認するために50msの待機を行うため、特定のデータ数を下回ると連続送信を維持できません。
 * 一方、TransmitDataFireAndForget()は*DT応答を確認後、キャリアセンスレスポンスを待たずにすぐに制御を返すため、この関数を連続で呼び出すことで、連続送信状態を保持できます。
 * 詳細はデータシートをご確認ください。
 * なお、データバッファのオーバーフローを防止するためにフロー制御端子(RTS,CTS)の接続を推奨します。
 *
 * このサンプルを実行するには、Arduino互換ボードのSerial1に
 * MUモデムが接続されている必要があります。
 */
#include <MU_Modem.h>

// --- 役割設定 ---
#define ROLE_TRANSMITTER 0
#define ROLE_RECEIVER    1

// ★★★ このボードの役割を設定してください ★★★
#define BOARD_ROLE ROLE_TRANSMITTER

// --- 定数定義 ---
const uint8_t COMMON_CHANNEL = 0x07;
const uint8_t COMMON_GROUP_ID = 0x01;
const uint8_t MY_EQUIPMENT_ID = (BOARD_ROLE == ROLE_TRANSMITTER) ? 0x11 : 0x22;
const uint8_t DESTINATION_ID = (BOARD_ROLE == ROLE_TRANSMITTER) ? 0x22 : 0x00; // 送信側は宛先を設定

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
void modemCallback(MU_Modem_Error error, MU_Modem_Response responseType, int32_t value, const uint8_t *pPayload, uint16_t len, const uint8_t* pRouteInfo, uint8_t numRouteNodes)
{
  if (responseType == MU_Modem_Response::DataReceived)
  {
    if (error == MU_Modem_Error::Ok)
    {
      Serial.println("\n--- データ受信 ---");
      Serial.printf("RSSI: %d dBm, 長さ: %u バイト\n", (int16_t)value, len);
      Serial.print("ペイロード (ASCII): ");
      Serial.write(pPayload, len);
      Serial.println("\n--------------------");
    }
    else
    {
      Serial.printf("[コールバック] データ受信エラー: %d\n", (int)error);
    }
  }
}

void setup() {
  Serial.begin(115200);

  // シリアルポートが開くまで待機
  while (!Serial);
  Serial.println("\n--- 連続送信サンプル ---");

#if BOARD_ROLE == ROLE_TRANSMITTER
  Serial.println("役割: 送信側");
#else
  Serial.println("役割: 受信側");
#endif

  // モデム用のシリアルポートを初期化
  Serial1.begin(MU_DEFAULT_BAUDRATE);

  // デバッグ出力を有効にする (任意)
  modem.setDebugStream(&Serial);

  // モデムを初期化 (受信側のみコールバック関数を登録)
  MU_Modem_AsyncCallback callback = (BOARD_ROLE == ROLE_RECEIVER) ? modemCallback : nullptr;
  MU_Modem_Error err = modem.begin(Serial1, MU_Modem_FrequencyModel::MHz_429, callback);
  if (err != MU_Modem_Error::Ok)
  {
    Serial.println("MUモデムの初期化に失敗しました。");
    while (true);
  }
  Serial.println("MUモデムが初期化されました。");

  // --- ボーレートを57,600bpsに変更 ---
  const uint32_t newBaudRate = 57600;
  Serial.printf("ボーレートを %lu bps に変更します...\n", newBaudRate);
  err = modem.SetBaudRate(newBaudRate, false); // EEPROMには保存しない
  if (err == MU_Modem_Error::Ok)
  {
    // モデム側のボーレート変更が成功したら、ホスト側(Arduino)のボーレートも変更する
    Serial1.begin(newBaudRate);
    delay(200); // ボーレート変更後の安定化のために少し待機
    Serial.println("ボーレートの変更に成功しました。");
  }
  else
  {
    Serial.printf("ボーレートの変更に失敗しました。処理を停止します。エラー: %d\n", (int)err);
    while (true);
  }


  // --- 共通設定 ---
  modem.SetChannel(COMMON_CHANNEL, false);
  modem.SetGroupID(COMMON_GROUP_ID, false);
  modem.SetEquipmentID(MY_EQUIPMENT_ID, false);
  modem.SetDestinationID(DESTINATION_ID, false);

  Serial.println("設定完了。動作を開始します。");
}

void loop() {
  // モデムの内部処理（受信データの解析、コールバック呼び出しなど）
  modem.Work();

#if BOARD_ROLE == ROLE_TRANSMITTER
  // --- 送信側のみ定期的に連続送信を実行 ---
  static uint32_t lastSendTime = 0;
  if (millis() - lastSendTime > 10000) // 10秒ごとに送信
  {
    lastSendTime = millis();
    Serial.println("\n>>> 連続送信処理を開始します...");

    // 1. 送信前にキャリアセンスでチャンネルが空いているか確認
    // ※補足:
    // このサンプルでは、送信前に CheckCarrierSense() でチャンネルの空きを確認しています。
    // しかし、その直後に TransmitDataFireAndForget() を呼び出すまでのわずかな時間で
    // 他の無線機が送信を開始すると、TransmitDataFireAndForget() 内部のキャリアセンスで送信が失敗する可能性があります。
    //
    // より確実に連続送信を開始するための方法として、以下のような手順があります。
    // a. 最初に TransmitData() を使って、ある程度大きなデータ塊を送信する。
    //    (データサイズは、モデムが最初のキャリアセンス(最大50ms)を行っている間も送信バッファが空にならない程度が目安です)
    // b. その直後から、間髪入れずに TransmitDataFireAndForget() で後続のデータを次々と送信する。
    Serial.println("1. キャリアセンス実行...");
    MU_Modem_Error cs_err = modem.CheckCarrierSense();

    if (cs_err == MU_Modem_Error::Ok)
    {
      Serial.println("   -> チャンネルは空いています。連続送信を開始します。");
      
      // 2. チャンネルが空いていれば、データを連続で送信
      const int NUM_PACKETS_TO_SEND = 5;
      for (int i = 0; i < NUM_PACKETS_TO_SEND; i++)
      {
        char message[32];
        snprintf(message, sizeof(message), "Cont Pkt #%d", i + 1);

        // `TransmitDataFireAndForget` を使用して、*DT応答のみを待ってすぐに次の処理へ
        MU_Modem_Error tx_err = modem.TransmitDataFireAndForget((const uint8_t*)message, strlen(message));

        if (tx_err == MU_Modem_Error::Ok)
        {
          Serial.printf("   Packet %d: 送信コマンド受付成功 (\"%s\")\n", i + 1, message);
        }
        else
        {
          Serial.printf("   Packet %d: 送信コマンド受付失敗。エラー: %d\n", i + 1, (int)tx_err);
          // エラーが発生したら連続送信を中断
          break;
        }
      }
      Serial.println("<<< 連続送信処理完了。");

    }
    else if (cs_err == MU_Modem_Error::FailLbt)
    {
      Serial.println("   -> チャンネルがビジーのため、送信をスキップします。");
    }
    else
    {
      Serial.printf("   -> キャリアセンス中にエラーが発生しました。エラー: %d\n", (int)cs_err);
    }
  }
#endif
}