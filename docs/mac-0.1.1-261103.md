# Mac 0.1.1 — 本家 Ver.261103-2 同期

2026-11-03、本家 [Imaha486/PC98PLAYER の Ver.261103-2](https://github.com/Imaha486/PC98PLAYER/commit/48d088e) を取り込みました。Mac版のバンドルバージョンは **0.1.1 (2)** です。

## 内容

- 本家のCPU待機ループ、GDC同期待ち、EMSページ切り替え、AVSDRV状態待ちの高速化を反映。
- CD音声MP3の途中再生に使う頭出し索引、AVSDRVのゼロ除算対策を反映。
- PC-9821の機種識別情報と上位メモリの512KB VRAM窓を反映。
- BIOS ROM不要の共通コアを維持。Mac固有のフォント、音声、ウインドウ表示、ローカル／NAS選択、保存用オーバーレイ、VFキー対応を維持。
- Windows用UI／ゲームパッドの変更は履歴とソースを取り込みますが、MacのUIは引き続きCocoaによる実装です。

## 成果物

`out/distribution/PC98PLAYER-0.1.1/PC98PLAYER.app`

`out/distribution/PC98PLAYER-0.1.1-macOS-universal-signed-not-notarized.zip`

ZIPと同じ場所に `.sha256` を付けたチェックサムがあります。**macOS 11以降、x86_64／arm64を1本の実行ファイルに収録**しています。ゲーム、BIOS ROM、商用フォントは同梱しません。

Developer ID署名、Hardened Runtime、タイムスタンプを付与し、ZIPを展開したアプリについて両アーキテクチャの署名とDeveloper IDの要件を検証済みです。公証は現在のXcodeにアカウントがなく `exportArchive No Accounts` で停止しました。以前の一時保存先のXcodeも開発ツール一式が残っていないため使用できません。**この成果物は未公証です。** 公証済みの配布版としては扱いません。公開済み0.1.0のZIPは置き換えていません。

## 確認結果

- Universal Releaseビルド成功、`lipo` で `x86_64 arm64` を確認。
- arm64でCTest **4/4成功**。
- x86_64でもRosetta経由で同じ4項目を実行し成功。Intel実機そのものによる検証ではありません。
- CPU／DOS／VRAM／音声生成／入力／ステート、元データ保護、16色・256色・480ライン・上位VRAM窓の回帰テストを実施。
- NASの独立したテスト用コピーで、前回と同じ17タイトルを再確認。全タイトルが予定の2,400〜18,000フレームまで異常終了せず実行。
- ランス4.1／4.2はロゴの先の256色画像と会話、ランスIV・AmbivalenZは冒頭の物語画面、闘神都市IIは冒頭の文章進行を確認。
- ランス4.1はx86_64でも4,200フレーム実行し、arm64と最終画像がピクセル単位で一致。
- 署名した配布用アプリのCocoaウインドウ起動も確認。

17タイトルはランスIII、ランスIV、ランス4.1、ランス4.2、闘神都市、闘神都市II、AmbivalenZ、あゆみちゃん物語、DALK、D.P.S. SG／set2／set3、宇宙快盗ファニーBee、学園KING、ぷろすちゅーでんとG、Dr.STOP!、ALICEの館3です。

起動と冒頭の短時間確認であり、全編の進行・すべての選択肢・長時間プレイは未確認です。音声はPCMの生成確認で、全タイトルの聴感確認は含みません。詳細な結果とゲーム画像はGit管理外の `out/upstream-261103-verification/` に保管しています。

## 再ビルド

```sh
./build_mac.sh
# 保存済みDeveloper IDとXcode認証で署名・公証する場合
./tools/package_mac.sh
# 署名のみを作る場合
./tools/package_mac.sh --sign-only
```

公証にはXcodeのApple Accountへのログイン、または保存済みnotarytoolプロファイルが必要です。認証情報はソースに記録しません。
