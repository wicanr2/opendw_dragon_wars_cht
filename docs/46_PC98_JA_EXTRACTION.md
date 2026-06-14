# 46. 日本語版テキスト抽出（X68000 版《ドラゴンウォーズ》）

本書は、日本語版《ドラゴンウォーズ》(Dragon Wars) のディスクイメージから原文の日本語イベントテキストを抽出し、remake の `assets/i18n/ja/` に補完した作業記録。目的は **日本語プレイヤーに英語フォールバックではなく原版の日本語**を見せること。

## 0. 重要な訂正：PC-98 ではなく X68000

タスク当初は「PC-98 日本語版」と想定していたが、ディスクイメージを解析した結果、**実体は Sharp X68000 版**であることが判明した。

| 想定 | 実体 |
|---|---|
| PC-98（x86 / NEC） | **X68000（Motorola 68000 / Sharp）** |
| Starcraft 発行 | Starcraft / **Hudson soft**（ブートに `Hudson soft 2.00`） |
| FDI/HDM/D88 | **.DIM**（DiskImage 形式） |

根拠:
- ブートセクタが 68000 機械語（`6000`=BRA, `4ffa`/`43fa`=LEA PC-rel, `4e4f`=TRAP）。x86 ではない。
- ディスク1（起動 Kidou）に **Human68k OS** のシステムファイル: `HUMAN.SYS`, `USKCG.SYS`(ユーザ定義フォント), `COMMAND.X`, `CONFIG.SYS`。
- 本体実行ファイルは `DRAGON.X`（X68000 の `.X` 実行形式、284 KB）。

「PC-98 / Starcraft / Kidou」というラベルはファイル名由来の誤称。中身は X68000 だが、**日本語版テキストの出典としては正当**（同じ日本ローカライズ）。

## 1. ディスク形式と解凍フロー

```
zip → .DIM → (256B ヘッダ除去) → raw 2HD イメージ → Human68k FAT12 → 個別ファイル
```

### DIM 形式
- サイズ 1,261,824 = **256B ヘッダ + 1,261,568B 本体**。
- ヘッダ byte0 = `0x00`（メディア種別 2HD 1.23MB）、以降トラック存在テーブル。
- 本体 = 77 トラック × 2 ヘッド × 8 セクタ × **1024B/セクタ** = 1,261,568B。

ヘッダ除去:
```bash
dd if=disk.dim of=disk.raw bs=256 skip=1
```

### Human68k FAT12 レイアウト（経験的に確定）
| 領域 | オフセット | 備考 |
|---|---|---|
| セクタサイズ | 1024B | |
| ブート | 0x000–0x400 | 1 セクタ、68k ブートコード |
| FAT ×2 | 0x400–0x1400 | 各 2 セクタ、メディア記述子 `fe ff ff`（FAT12） |
| ルートディレクトリ | 0x1400–0x1800 | 1 セクタ、32 エントリ |
| データ領域 | 0x1800– | クラスタ2 起点、1 クラスタ = 1 セクタ |

ディレクトリエントリは MS-DOS 互換 32B（name[8]+ext[3]+attr[1]+予約[10]+time[2]+date[2]+start_cluster[2]+size[4]、リトルエンディアン）。

抽出ツール: `tools_build/fat12_extract.py`（Docker 内 `python:3.12-slim` で実行。本ツールは作業用、原ゲームファイルは入庫しない）。

### 抽出ファイル一覧
| ディスク | 主なファイル |
|---|---|
| 1（Kidou/起動） | `DRAGON.X`(284KB 本体), `COMMAND.X`, `HUMAN.SYS`, `USKCG.SYS`, `VDATA1/2`, `DW.SND` |
| 2（Disk A） | `MAP`, `MONS`, `ITEM`, **`SPECIAL`(159KB)**, `PROG1-3.PKH`, `SUBTTL.PKH`, `PIC.PIX` |
| 3（Disk B） | `TITLE.PKH`, `3D1-4.PKH`, `END1-5.PKH`, `ICON.PIX`, `MON.PIX`, `ID1/2` |

`.PKH` = 圧縮（高エントロピー、未解凍）。`.PIX` = 画像。

## 2. テキスト符号化と発見

- **符号化: Shift-JIS（CP932）、非圧縮**。
- イベント/物語テキストは **`SPECIAL`(159,034B)** に集中。SJIS 先頭バイト密度 ≈ 49%（77,215/159,034）。
- もう一つの主要文字列表は **`DRAGON.X` 内 ~0x369e4 以降**: 呪文名 / スキル / 能力値 / UI ラベル / 戦闘メッセージ / 店メニュー が連続した SJIS テーブルで格納（既存 `ja/combat.tsv`・`ja/chars.tsv` の語と一致を確認）。

SJIS 文字列抽出は「先頭 0x81–0x9F/0xE0–0xFC + 後続 0x40–0x7E/0x80–0xFC」の連続を走査し `cp932` でデコード（オフセット付き）。

## 3. 英↔日 対照表（events.tsv、全 13 件）

remake の `events.tsv` キー（= 英語原文、パーガトリー序盤イベント 0/13）に対し、`SPECIAL` 内の対応日本語を **イベント列の位置 + 意味**で直接対位。**信頼度: 高**（同一シーケンス・固有名詞一致）。

| # | 英語キー（要約） | `SPECIAL` offset | 日本語 |
|---|---|---|---|
| 1 | Stripped of all possessions… Namtar | 0x27d2 | 地獄から来た怪物ナムターの命令により、君たちは財産も富も全てはぎ取られ、裸で無防備のままパーガトリーの貧民街に投げ込まれた。 |
| 2 | stone walls … monument | 0x29da | パーガトリーの石壁が、損なわれた一生と破壊された夢の記念碑のように立ちはだかる。 |
| 3 | smell the sea … border on the harbor | 0x2679 | 海の臭いがする。この辺りの壁は港に隣接しているにちがいない。 |
| 4 | breeze … sickly stench | 0x38d4 | 港の方から、はき気を催すような匂いの風が吹いてくる。 |
| 5 | chorus of voices from the west | 0x32a7 | 西から歓声がどっとわき上がった。 |
| 6 | lusty shouts from the east | 0x323d | 東から群衆の元気な叫び声が聞こえる。 |
| 7 | bloodthirsty howls from the north | 0x3267 | 北側の壁の向こうから、群衆の血に飢えたわめき声が聞こえる。 |
| 8 | sea is cold and rough … good swimmer | 0x32d4 | 海は冷たく荒れている。泳ぎの達者な者だけがここから逃れるチャンスがある。 |
| 9 | gap in the city wall … harbor | 0x28c6 | 市の壁に割れ目がある。はるか下方に、君たちがこの恐ろしい島に入るとき通った港の水面が見える。 |
| 10 | Is freedom … worth a long dive | 0x2923 | パーガトリーからの脱出は、ひょっとしたら浅瀬かもしれない海に高い所から飛び込み、それから死にもの狂いで港を泳ぎ抜くだけの価値があることだろうか。 |
| 11 | Ahead lay odd waters | 0x52ea | 前方に何か変わった水たまりがある。 |
| 12 | crowd grows wild … more victims | 0x5312 | 群衆はもっと犠牲者が見れると興奮している。 |
| 13 | You feel strangely energized! | 0x5aa7 | この水たまりに入ると、不思議にも魔力がみなぎって来るのを感じた！ |

対位の確証（#11/#13）: `SPECIAL` 0x52ea「変わった水たまり」と 0x5aa7「水たまりに入ると魔力がみなぎる」が **水たまり** を共有し、英語版の "odd waters → energized" の隣接イベント対と一致。直後に 0x5aed「何も起きない。」(Nothing happens) も存在。

### 補完先
`assets/i18n/ja/events.tsv`（新規、13/13）。英語キーは `zh-TW/events.tsv` と完全一致（末尾空白を含むキー #9 も保持）。

## 4. 検証

1. `verify_i18n`（既存ツール）: `ja/events.tsv` が **0/13 → 13/13**。畸形行 0、fallback 契約 OK、**PASS**。
2. フォント字形: 新規日本語に漢字/かな計 162 字、うち 78 字が既存 atlas に未収録だった。`tools_build/gen_cjk_atlas_from_i18n.sh`（Docker + wqy-zenhei）で **全 i18n + bundle 段落 + VM 文字列の union** から `assets/fonts/cjk24.atlas` を再生成。
   - 旧 1653 字形 → 新 **1790 字形**（旧字形は全保持、欠落 0）。
   - `ja/events.tsv` の全文字を被覆（欠字 0）。
3. レンダリング目視: atlas から日本語 3 行をビットマップ描画し、漢字+ひらがな+カタカナが正しく表示されることを確認。
4. アプリ: `main.cpp` は `--locale ja` で `assets/i18n/ja/events.tsv` を自動 merge（既存ロジック、変更なし）。

## 5. 未確認・保留（人手確認待ち）

品質優先（寧缺勿濫）のため、確証のある対位のみ補完した。以下は保留:

### combat.tsv モンスター名（**解決済み：23 件補完、2026-06-14**）

当初「`MONS` 内の名前は二進/符号化で SJIS 抽出不可」と保留していたが、符号化を解明し全 25 レコード（ユニーク 23 キー）を補完した。

#### 符号化の解明：ニブルスワップ SJIS

`MONS`（および X68000 版データファイルの日本語テキスト全般）は **Shift-JIS だが、各バイトのニブル（上位 4bit と下位 4bit）が入れ替わっている**。先頭の SJIS スキャンが `裹` パディングと化け文字しか拾えなかったのはこのため。

復号:
```python
def nibswap(b): return bytes(((x << 4) | (x >> 4)) & 0xFF for x in b)
text = nibswap(raw_mons).decode("cp932")
```
発見の決め手: `MONS` 0x162 の生バイト `38 27 38 55 38 d6 …` をニブルスワップすると `83 72 83 55 83 6d …` = `ビザノープルの地下`（=遭遇地名、クリーンな SJIS）。

> 注: 半角カタカナ（単一バイト 0xA1–0xDF）も使われる（例 index 17/21 = `ﾛｯｸ･ｽﾊﾟｲﾀﾞｰ`）。NFKC で全角化して TSV に格納。

#### モンスター名テーブル

`MONS` 内、オフセット **0x211F から 0x3C（60）バイト間隔**の固定長配列。**DOS res31 の並び順と完全に 1:1 一致**（英語キーとの対位はこの配列位置で確定）。全 25 レコード:

| idx | offset | EN（DOS res31） | JA（X68000、全角化） | JA→EN 逆引き |
|---|---|---|---|---|
| 0 | 0x211F | Robber | 強盗 | robber ✓ |
| 1 | 0x215B | King's Guard | 王の衛兵 | king's guard ✓ |
| 2 | 0x2197 | Soldier | 兵士 | soldier ✓ |
| 3 | 0x21D3 | Bandit | 追いはぎ | highwayman ✓ |
| 4 | 0x220F | Pikeman | 槍兵 | pikeman ✓ |
| 5 | 0x224B | Loon | 狂人 | madman ✓ |
| 6 | 0x2287 | Fanatic | 狂信者 | fanatic ✓ |
| 7 | 0x22C3 | Yonderboy | チンピラ | punk/hoodlum ✓ |
| 8 | 0x22FF | Born Loser | 流浪者 | wanderer/drifter ~ |
| 9 | 0x233B | Unjustly Accused | 不浄の霊魂 | unclean spirit ~（日本語は意訳） |
| 10 | 0x2377 | Innocent Man | 殉教者 | martyr ~（日本語は意訳） |
| 11 | 0x23B3 | Giant Spider | 大グモ | giant spider ✓ |
| 12 | 0x23EF | Wild Dog | 野犬 | wild dog ✓ |
| 13 | 0x242B | Spider | クモ | spider ✓ |
| 14 | 0x2467 | Cannibal | 人食い | man-eater ✓ |
| 15 | 0x24A3 | Big Dog | 大きな犬 | big dog ✓ |
| 16 | 0x24DF | Wild hound | 狂った猟犬 | mad hound ✓ |
| 17 | 0x251B | Rock Spider | ロック・スパイダー | rock spider ✓ |
| 18 | 0x2557 | Spider | クモ | spider ✓ |
| 19 | 0x2593 | Wolf | 狼 | wolf ✓ |
| 20 | 0x25CF | Jail Keeper | 番人 | keeper/watchman ✓ |
| 21 | 0x260B | Rock Spider | ロック・スパイダー | rock spider ✓ |
| 22 | 0x2647 | Drunk | 飲んだくれ | drunkard ✓ |
| 23 | 0x2683 | Humbaba | ハンババ | Humbaba ✓ |
| 24 | 0x26BF | Gladiator | 剣闘士 | gladiator ✓ |

ユニーク英語キー 23 件すべて補完。重複キー（Spider×2 → クモ、Rock Spider×2 → ロック・スパイダー）は値が一致し矛盾なし。**信頼度: 高**（実機日本語版から抽出 + 配列位置 1:1 + JA→EN 逆引き整合）。idx 8〜10 は日本語版の意訳（流浪者/不浄の霊魂/殉教者）だが、これも実機原文なので採用。

#### 戦闘動詞（hit/miss/damage/slain）は引き続き保留

英語版では原子語だが、X68000 版は文テンプレート（`…を攻撃し…回命中して…のダメージを与えた！…を倒した！`、`DRAGON.X` 0x38xxx 帯）で、1:1 の単語対応が無い。憶測対訳は避け、英語フォールバックのまま（`ja/combat.tsv` 48/52、残り 4 = これら動詞）。

#### 補完先・ツール
- `assets/i18n/ja/combat.tsv`: モンスター名 23 件追加（既存の暫定訳 盗賊→強盗 / オオカミ→狼 / フンババ→ハンババ も実機原文に修正）。
- `assets/fonts/cjk24.atlas`: 新字形 13 個分を追加再生成（1790→1878 字形、旧字形全保持）。`tools_build/gen_cjk_atlas_from_i18n.sh`。
- 抽出手順: `tools_build/fat12_extract.py` で `MONS` を取り出し → 上記 nibswap で復号 → 0x211F/0x3C 配列を読む。

### menu.tsv
`ja/menu.tsv` は 13/14（既存。`"Map  -  Esc: bac"` 1 件のみ欠、本タスク範囲外）。

## 6. 変更ファイル

| ファイル | 種別 | 内容 |
|---|---|---|
| `assets/i18n/ja/events.tsv` | 新規 | 日本語イベント 13 件（信頼度 高） |
| `assets/fonts/cjk24.atlas` | 更新 | 1653→1790 字形（日本語字形追加、旧字形保持） |
| `tools_build/fat12_extract.py` | 新規 | Human68k FAT12 抽出ツール（Docker） |
| `tools_build/gen_cjk_atlas_from_i18n.sh` | 新規 | i18n 全文字から atlas 再生成（Docker + wqy-zenhei） |
| `docs/46_PC98_JA_EXTRACTION.md` | 新規 | 本書 |

原ゲームファイル（X68000 / DOS）は **入庫しない**。
