# bundle/audio — 真實 PCM 音效資產

remake 在 PC speaker 風格方波合成之外,額外載入原版平台的真實 8-bit PCM 音效取樣。
本目錄的 WAV 由 `tools_build/audio_extract.py` 從原版素材轉出(素材本身不入庫)。

## 檔案來源(真值層級)

| 檔名 | 來源平台 / 原始檔 | 原始格式(觀測真值) | 長度 |
|---|---|---|---|
| `amiga_data5.wav` | Amiga `data/data5` | 8-bit signed mono PCM | 3.33s |
| `amiga_data6.wav` | Amiga `data/data6` | 8-bit signed mono PCM | 1.42s |
| `x68k_dwsnd.wav`  | X68000 Disk1 `DW.SND` | 8-bit signed mono PCM | 2.00s |

來源檔位元組為 8-bit signed PCM 屬**觀測真值**(波形能量分佈、過零率、主頻分析確認為真實音訊,
非雜訊;data5/data6 主頻約 1.5 kHz、DW.SND 前段強擊主頻約 520 Hz)。各檔皆為**單一連續音效**,
內部無多段 clip 邊界(僅 Amiga 檔開頭有短靜音 lead-in,已去除)。

## remake 設計值(非 oracle 真值)

- **原生取樣率**:原版反組譯未標出來源取樣率。採該平台音效常見回放率為設計值
  (Amiga 11025 Hz、X68000 8000 Hz),轉檔時 resample 到 22050 Hz(對齊 `sound.cpp` 合成輸出率,
  runtime 不需 resampler)。
- **事件 → 樣本對映**:原版無「遊戲事件 ↔ 樣本」對照表。對映由 remake 依音訊特性指定
  (見 `src/audio/sound.cpp` `kSampleMap`),屬 remake 設計,**不謊稱為原版真值**。樣本缺檔時自動
  退回方波合成(door/wall/effect_88 的方波頻率由 opendw 反組譯 dx/bx 推導,見 `sound.hpp`)。

## 背景音樂(引擎就緒,素材待渲染)

- **Amiga 音樂**(`title/game/combat/end.tune`,"Music by MANIACS of NOISE"):68000 機械碼
  播放器 + 內嵌曲目,非 raw PCM。**引擎端循環播放已就緒**(`sound.cpp` music 頻道 + `main.cpp`
  依 state 切 title/game/combat/end);用 **UADE**(不需 Kickstart)渲染成 WAV 放進
  [`music/`](music/README.md) 即自動循環。開發沙箱網路受限抓不到 UADE → 渲染留本機
  (見 `music/README.md` / `tools_build/render_music.sh`)。
- **DOS PC speaker**(`DRAGON.COM 0x5C3B`):反組譯確認為 **PC speaker 音效播放碼**
  (OUT 到 PIT 0x43/0x40),**非背景音樂** —— DOS 版本來就沒有背景音樂。

## 重新產生

```sh
# 素材:從 dragon_wars/dragonwars_amiga_win.7z 解出 data5/data6,
#       從 X68000 Disk1 .DIM(去 256B header)經 tools_build/fat12_extract.py 解出 DW.SND。
docker run --rm --user "$(id -u):$(id -g)" -v <src_pcm>:/src -v <out>:/out \
  python:3.12-slim python3 tools_build/audio_extract.py /src /out
```
