# Fliqlo for RenkuOS

[English](README.md) | **日本語** | [한국어](README.ko.md)

[Fliqlo](https://fliqlo.com/) を手本にした Haiku/RenkuOS 用のフリップ時計スクリーン
セーバーです。黒い背景に暗いカードが 2 枚並び、時と分を縦長の太い数字で表示します。
値が変わるたびにカードの上半分が下へめくれます。32 ビット x86 (x86_gcc2)、x86_64、
arm64 で動作します。

![RenkuOS arm64 で全画面表示した Fliqlo](docs/screenshot.png)

## pkgman でインストール

| Haiku | リポジトリの追加 |
| --- | --- |
| 32 ビット x86 (x86_gcc2) | `pkgman add-repo https://pkgman.rainygirl.com/x86_gcc2` |
| x86_64 | `pkgman add-repo https://pkgman.rainygirl.com/x86_64` |
| arm64 | `pkgman add-repo http://pkgman.rainygirl.com/arm64` |

```sh
pkgman install fliqlo
```

`pkgman add-repo` が `Operation not supported` で失敗する場合、そのイメージの
ネットワークキットには TLS がありません。`https://` の代わりに `http://` を
使ってください。

arm64 ではパッケージが `libscreensaver.so` と **ScreenSaver** 環境設定も
インストールします。RENKU arm64 イメージは最小ビルドで、どちらも含まれていません。

## 使い方

1. **Deskbar -> Preferences -> ScreenSaver** を開きます。
2. 一覧から **Fliqlo** を選びます。プレビューに時計が表示されます。
3. 右側でオプションを設定します。
   - **24-hour clock** -- オフで `4 50` と隅の AM/PM、オンで `16 50`。
   - **Size** -- カードが画面に占める大きさ。
   - **Brightness** -- カードと数字の明るさ。
4. **Test** で全画面表示を確認します。マウスを動かすかキーを押すと終わります。

![ScreenSaver 環境設定の Fliqlo のオプション](docs/preferences.png)
![56 から 57 へめくれるカード](docs/flip.png)

Fliqlo の数字は Helvetica Condensed Bold ですが、このフォントは配布できません。
そのためパッケージは Helvetica を手本にした無料フォント TeX Gyre Heros Condensed
Bold (GUST Font License) をインストールします。フォントがない場合は Noto Sans
Display Bold で表示します。

## ビルド

Haiku で `make install` を実行すると、アドオンをビルドしてフォントと一緒に
`~/config/non-packaged` へコピーします。32 ビット x86 ハイブリッドでは既定の
`g++` (gcc2) を使ってください。そこの screen_blanker は gcc2 のアドオンしか
読み込みません。

Docker のある作業用マシンでは `tools/cross-build.sh all` で `dist/x86/` (gcc2)、
`dist/x86_64/`、`dist/arm64/` をクロスビルドします。pkgman-repo のレシピがそれを
パッケージにします。

## 無関係であること

Haiku 向けに独自に実装したものです。Fliqlo の制作者が作ったものでも承認した
ものでもなく、関係もありません。Fliqlo のコードや画像は使っていません。

## AI の利用について

Fliqlo for RenkuOS は AI コーディングツール (Anthropic の Claude) の支援を受けて
開発しました。

## ライセンス

コードは MIT。`fonts/texgyreheroscn-bold.otf` (TeX Gyre Heros、B. Jackowski、
J. M. Nowacki) は GUST Font License (`fonts/GUST-FONT-LICENSE.txt`) です。
