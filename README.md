# MU_Modem

サーキットデザイン社製MUシリーズ無線モデムを制御するためのArduinoライブラリです。

**本ライブラリは、動作保証、並びに個別のサポートは行っておりません。 自己責任の上、各ソフトウェアライセンスを尊重のうえご利用ください。<br>ご質問やバグのご報告はGitHubのIssueをご利用ください。**

## 概要

このライブラリは、Arduinoでサーキットデザイン社製MUシリーズ無線モデム（MU-3-429及びMU-3-1216、MU-4-429）を制御するためのインターフェースを提供します。<br>
シリアルコマンドインターフェースを介して、データの送受信やモデムの設定を簡単に行うことができます。

MUシリーズについてはこちらをご参照ください。<br>
https://www.circuitdesign.jp/technical/about-mu-series/

![MU-3-429](https://www.circuitdesign.jp/wp/wp-content/uploads/2019/08/mu-3-429-ph005-300x178.png)

## 対応ハードウェア

*   Circuit Design MU-3-429
*   Circuit Design MU-3-1216
*   Circuit Design MU-4-429  

## API
API等は以下ドキュメントをご参照ください。<br>
https://circuitdesign-inc.github.io/MU_Modem/

## インストール

1.  GitHubリポジトリの **[Releases](https://github.com/circuitdesign-inc/MU_Modem/releases)** ページから、最新バージョンの **`MU_Modem-XXXX.zip`** をダウンロードします。
    - **重要**: GitHubが自動生成する `Source code (zip)` にはサブモジュールが含まれていません。必ず **`MU_Modem-` で始まるZIPファイル** をダウンロードして使用してください。
2.  Arduino IDEで、`スケッチ` > `ライブラリをインクルード` > `.ZIP形式のライブラリをインストール...` に移動します。
3.  ダウンロードしたZIPファイルを選択します。

> [!NOTE]
> 本ライブラリはコードの一部にサブモジュール（`SerialModemBase`）を使用しています。
> 開発のためにリポジトリをクローンする場合は、以下のコマンドを使用してサブモジュールを含めて取得してください：
> ```bash
> git clone --recursive https://github.com/circuitdesign-inc/MU_Modem.git
> ```

## 基本的な使い方

### ハードウェアのセットアップ

MUモデムをArduinoなどのマイクロコントローラと接続するための基本的なセットアップ方法です。
詳細は各モデムのデータシートを必ずご確認ください。

#### 必須の接続

最低限、以下の5つの端子を接続する必要があります。

| MU端子 | 接続先 (Arduino) | 説明                                                                                                                                |
| :----: | :--------------: | :---------------------------------------------------------------------------------------------------------------------------------- |
| `VCC`  |   3.0V ~ 5.0V    | 安定化された電源を接続します。Arduinoの`5V`または`3.3V`ピンに接続します。                                                           |
| `GND`  |      `GND`       | グランドを接続します。                                                                                                              |
| `TXD`  |  Arduinoの `RX`  | MUモデムの送信(TXD)をArduinoの受信(RX)に接続します。コントロール電圧はVCCに依存します。                                             |
| `RXD`  |  Arduinoの `TX`  | MUモデムの受信(RXD)をArduinoの送信(TX)に接続します。コントロール電圧はVCCに依存します。                                             |
| `CTS`  |      `GND`       | MUモデムのハードウェアフロー制御用の入力です。HIGHの時はビジーと判断しTXDからデータを送信しません。（内部でプルアップされています） |

> [!IMPORTANT]
> *   **ロジックレベル**: MUモデムのロジックレベルは電源電圧(VCC)に依存します。ArduinoとMUモデムの電源電圧を合わせてください（例: どちらも5V、またはどちらも3.3V）。電圧が異なる場合は、ロジックレベル変換IC等が必要です。
> *   **未使用端子**: 使用しない端子はデータシートの指示に従い、オープン（未接続）にしてください。

#### オプションの接続

必要に応じて、以下の端子も接続、拡張できます。

|  MU端子  |     接続先 (Arduino)      | 説明                                                                      |
| :------: | :-----------------------: | :------------------------------------------------------------------------ |
|  `RTS`   | Arduinoのデジタル入力ピン | ハードウェアフロー制御用の出力です。モデムがデータ受信可能かを示します。  |
|  `CTS`   | Arduinoのデジタル出力ピン | ハードウェアフロー制御用の入力です。モデムにデータ送信を許可/禁止します。 |
| `TX-LED` |            LED            | データ送信中に点灯するLEDを接続できます。電流制限抵抗が必要です。         |
| `RX-LED` |            LED            | データ受信中に点灯するLEDを接続できます。電流制限抵抗が必要です。         |

**注意**: 本ライブラリはハードウェアフロー制御(`RTS`/`CTS`)を標準では使用しません。使用する場合は、ご自身で制御ロジックを実装する必要があります。


### プログラム
以下は、MUモデムを初期化し、データを受信し、5秒ごとにメッセージを送信する基本的なサンプルコードです。
詳細なサンプルはexsamplesフォルダをご確認ください。

```cpp
#include <MU_Modem.h>

// MUモデムのインスタンスを作成
MU_Modem modem;

// データ受信時に呼び出されるコールバック関数
void modemCallback(const MU_Modem_Event &event)
{
  if (event.type == MU_Modem_Response::DataReceived && event.error == MU_Modem_Error::Ok)
  {
    Serial.println("\n--- データ受信 ---");
    Serial.printf("RSSI: %d dBm, 長さ: %u バイト\n", (int16_t)event.value, event.payloadLen);
    Serial.print("ペイロード: ");
    Serial.write(event.pPayload, event.payloadLen);
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

## デバッグ
platformioを使用している場合、ライブラリのデバッグ出力を有効にすることができます。

デバッグ機能を有効にするには、platformio.iniに以下のビルドフラグ追加してください:
```
build_flags = -D ENABLE_SERIAL_MODEM_DEBUG
```

また、デバッグの出力先を以下の関数で指定する必要があります。
```cpp
// デバッグの出力先を設定
modem.setDebugStream(&Serial);
```

## 非同期送信時の重要な注意点

`TransmitDataAsync()` を使用する場合、以下の点に十分注意してください。

- **バッファの有効性**: この関数は内部でデータのコピーを行わず、渡されたポインタをそのまま保持します。そのため、送信が実際に完了するまで（コールバックが呼び出されるまで）、提供したバッファの内容が書き換えられたり、スコープから外れて破棄されたりしてはいけません。
- **推奨される対策**:
    - 送信データには `static` 変数またはグローバル変数を使用してください。
    - コールバックで `TxComplete` または `TxFailed` を受け取るまで、そのバッファを再利用しないでください。

```cpp
// 悪い例（NG）
void sendData() {
    uint8_t data[] = {1, 2, 3}; 
    modem.TransmitDataAsync(data, 3);
} // 関数を抜けると data が破棄され、不正なデータが送信される可能性がある

// 良い例（OK）
void sendData() {
    static uint8_t data[] = {1, 2, 3}; // 静的バッファ
    modem.TransmitDataAsync(data, 3);
}
```

## License

このライブラリはMITライセンスの下でリリースされています。
