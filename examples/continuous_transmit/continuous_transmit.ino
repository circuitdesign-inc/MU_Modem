/**
 * @file continuous_transmit.ino
 * @brief MU-Modemライブラリの連続送信機能を使用したサンプル
 *
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
 * 連続送信条件について:
 * データシートには、*DT応答を確認してから「5ms + (2.08ms * データ数)」以内に次のデータを送れば
 * 連続送信モードになる、と記載されています。
 * TransmitDataFireAndForget()は*DT応答を確認後すぐに制御を返すため、この関数を単純に
 * 連続で呼び出すことで、この条件は自然と満たされ、モデムの性能を最大限に引き出すことができます。
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
 * @brief モデムからの非同期イベントを処理するコールバック関数 (受信側で使用)
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
  while (!Serial);
  Serial.println("\n--- 連続送信サンプル ---");

#if BOARD_ROLE == ROLE_TRANSMITTER
  Serial.println("役割: 送信側");
#else
  Serial.println("役割: 受信側");
#endif

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
    Serial1.updateBaudRate(newBaudRate);
    delay(200); // ボーレート変更後の安定化のために少し待機
    Serial.println("ボーレートの変更に成功しました。");
  }
  else
  {
    Serial.printf("ボーレートの変更に失敗しました。エラー: %d\n", (int)err);
    // エラーが発生しても処理を続行するが、通信は失敗する可能性が高い
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
        snprintf(message, sizeof(message), "Continuous Packet #%d", i + 1);

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
        // この関数はすぐに返ってくるため、delayは不要。最速で次のループを実行する。
      }
      Serial.println("<<< 連続送信処理完了。");

    }
    else if (cs_err == MU_Modem_Error::FailLbt)
    {
      Serial.println("   -> チャンネルがビジーのため、今回の送信はスキップします。");
    }
    else
    {
      Serial.printf("   -> キャリアセンス中にエラーが発生しました。エラー: %d\n", (int)cs_err);
    }
  }
#endif
}