# SD-Duck

REAPER向け軽量ビジュアル・ダッキング VST3 プラグイン (Windows 64-bit)

![UI Preview Placeholder](docs/preview.png)

## 概要

`SD-Duck` は、YTPMV / 音MAD / EDM 制作におけるダッキング処理を極限まで効率化・視覚化するために設計された VST3 プラグインです。
REAPER標準の `ReaComp` や既存のLFO系プラグインが抱えていた「シーク時の位相ズレ」「長いキック（ガバキック等）で音量が戻らない問題」「サイドチェインルーティングの確認しづらさ」を単一プラグインでスマートに解決します。

## 主な特徴

- **3つのトリガーモード**:
  - `Tempo Sync`: DAWのタイムライン（`ppqPosition`）に完全同期。再生位置のランダムシークやBPM変更マーカーがあってもタイミングズレが起きません。
  - `Audio Sidechain`: 外部サイドチェイン入力（`ch 3/4`）によるダッキング。
  - `MIDI Trigger`: トラック上のMIDIノートオン（Any Noteまたは特定ノート番号）でダッキングを実行。
- **ガバキック対策（One-Shot トランジェント検出）**:
  - 音MADやハードコアで頻出する長いキックでも、立ち上がりのみ検知して指定時間（Hold/Release）で確実に音量を復帰。
- **低域限定ダッキング（Split-Band Ducking）**:
  - 位相フラットな 4次Linkwitz-Riley（LR4 / 24dB/oct）フィルタにより、キックと被るベースの低音域のみをダッキングし、アタック感を損ないません。
- **Mid成分限定ダッキング（Target: Mid Only）**:
  - センター定位の音量のみを抑制し、Side成分（ステレオの広がり）を維持します。
- **差分ビジュアル波形モニター**:
  - 処理前の音量から「削り取られたエリア」を減衰量に応じたダイナミックカラー（シアン→アンバー→レッド）で可視化。
- **REAPERルーティング診断インジケーター**:
  - サイドチェイン入力の信号状態（`SC: ACTIVE` / `SC: NO SIGNAL`）を常時監視・表示。
- **ゼロレイテンシ＆クリックノイズ防止**:
  - PDC = 0 samples。ゲイン遷移には $\tau = 1.0\text{ ms}$ のワンポールスムージングフィルタを適用。

---

## REAPERでのサイドチェイン（Audioモード）設定手順

```text
[キックトラック (Track 1)]
       │
       └─── [ROUTE (センド)] ───> Audio 1/2 => 3/4 ───┐
                                                       ▼
[ベース/オケトラック (Track 2)]                    [SD-Duck (VST3)]
       │                                         ┌──────────────┐
       └─────── 通常のトラック音声 (ch 1/2) ────>│ Main In (1/2)│
                                                 │ SC In   (3/4)│
                                                 └──────────────┘
```

1. キックトラックの `ROUTE` ボタンをクリックします。
2. **「新しいセンドを追加... (Add new send...)」** から、ダッキングをかけたいベースやオケのトラックを選択します。
3. 送信先のオーディオチャンネルを **`Audio: 1/2 => 3/4`** に設定します。
4. ベース/オケトラックに `SD-Duck` をインサートし、モードを `AUDIO` に切り替えます。
5. トップバーの右側に `SC: ACTIVE (-XX.X dB)` と緑色で表示されれば接続完了です！
   - ※接続が確認できない場合は `[MON]` ボタンをONにして、キックの音が聞こえるかテストしてください。

---

## インストール手順 (Windows 11 / 10 64-bit)

1. [Releases](../../releases) から最新の `SD-Duck-v*.*.*-win64-vst3.zip` をダウンロードします。
2. ZIPを解凍し、中にある `SD-Duck.vst3` フォルダを以下のディレクトリに配置します:
   ```text
   C:\Program Files\Common Files\VST3\
   ```
3. REAPERを起動（または設定の `Plugins -> VST -> Rescan` を実行）すると、FXブラウザに `SD-Duck (SagyouDarui)` が追加されます。

---

## ローカルでのビルド手順

### 必須要件
- Windows 10 / 11 (x64)
- CMake 3.22 以降
- Visual Studio 2022（C++デスクトップ開発ワークロード）または Build Tools
- Git

### ビルドコマンド
```powershell
# 1. リポジトリをクローン
git clone https://github.com/your-username/sd-duck-vst3.git
cd sd-duck-vst3

# 2. CMakeの構成 (JUCE 8が自動取得されます)
cmake -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release

# 3. ビルド実行
cmake --build build --config Release --target SD-Duck_VST3
```
ビルド完了後、`build/SD-Duck_artefacts/Release/VST3/SD-Duck.vst3` に成果物が生成されます。

---

## ライセンス

本ソフトウェアは [GNU General Public License v3.0 (GPLv3)](LICENSE) の下で公開されています。
JUCE 8 Framework を使用しています。
