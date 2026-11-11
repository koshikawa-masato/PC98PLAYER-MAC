# Mac版の開発引き継ぎ

更新日：2026-11-11（日本時間）。最新のユーザー指示を優先し、変更前に `git status` とremoteを確認してください。

## 新しい環境での開始

```sh
git clone https://github.com/koshikawa-masato/PC98PLAYER-MAC.git
cd PC98PLAYER-MAC
./tools/fetch_ymfm.sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64'
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

CMakeとXcode Command Line Toolsが必要です。生成物は `build/PC98PLAYER.app`。ゲームデータ・セーブ・署名証明書・公証認証は含まれません。署名・公証して配布する手順は `docs/` と `tools/` を参照し、秘密情報をコミットしないでください。

## 現在の内容

- 本家Ver.261107を取り込んだmainに、Apple側の起動パス変換とタイトル識別用の調査ファイルを統合します。
- `apple/StartupPath.h` はAppleフロントエンドからコアへ渡す実行ファイルの区切りを変換します。Windows用共有コアへ変換処理を追加しない方針を維持します。
- `research/identify_pc98_title.py` と `research/pc9800cg-ratings.json` はファイルサイズ・SHA-256による作品識別と、名前からの候補検索の研究用です。公式レーティングの完全なDBではなく、不明な項目を確定済みと扱わないでください。ゲーム本体やCG画像は含みません。
- 調査データのJSON構造とハッシュ一致／不一致の確認は実施済みです。タイトル識別をMacアプリUIへ組み込んだという意味ではありません。
- この統合だけでは新しい署名済みリリースを作成・公証・公開しません。

## iOS版との区別

現在のRetroDOS Player iOS実装は別リポジトリです。

```sh
git clone https://github.com/koshikawa-masato/PC98PLAYER-iOS.git
```

iOS版の `LOCAL_HANDOFF.md` を読み、公開版0.2.4（ビルド51）とmainの差分、フィードバック受付、全画面一時停止の未解決報告を確認してください。このMacリポジトリ内の古い `ios/` を最新iOS版として使わないでください。
