# bundle/audio/music — 背景音樂(Amiga MANIACS of NOISE,UADE 渲染)

remake 的背景音樂來自 **Amiga 版《火龍之戰》的 `.tune` 檔**(片頭寫 "Music by MANIACS of
NOISE / 24-04-90")。引擎端的循環播放系統已就緒(`src/audio/sound.cpp` 的 music 頻道 +
`main.cpp` 依遊戲狀態切曲);**只要把渲染好的 WAV 放進本目錄,音樂即會自動播放**:

| 檔名 | 對應狀態 | 來源 `.tune` |
|---|---|---|
| `title.wav`  | 標題 / 選單 / 建角(`S_TITLE`/`S_MENU`/`S_CREATE`) | `title.tune` |
| `game.wav`   | 探索 / 地圖(`S_GAME`/`S_MAP`) | `game.tune` |
| `combat.wav` | 戰鬥(`S_COMBAT`) | `combat.tune` |
| `end.wav`    | 結局(`S_ENDING`) | `end.tune` |

**規格**:mono / 16-bit signed PCM / 22050 Hz(對齊 `sound.cpp` 混音輸出率,runtime 不需
resampler)。缺檔的曲目自動靜默 no-op(不影響執行 / ctest)。

## 為什麼 WAV 不入庫(預設)/ 自行渲染

`.tune` 是 **68000 機械碼播放器 + 內嵌曲目資料**的自訂格式,不能直接當音檔。標準作法是用
**UADE**(Unix Amiga Delitracker Emulator)模擬其播放器、輸出 WAV —— **不需 Kickstart ROM**。
本專案的開發沙箱網路受限(程式碼託管站被擋),無法在此抓取 UADE;故渲染步驟留給有網路的環境
執行。原始 `.tune` 與渲染後音檔屬 MANIACS of NOISE / Interplay 著作,**本 repo 不入庫**
(與 `DRAGON.COM`/`DATA*` 等原始素材一致);請自備 Amiga 版合法副本後依下方渲染。

## 渲染步驟(在有網路的機器上)

```sh
# 1) 取得 UADE(任一)
#    ★ 實測可用:官網 release tarball(gitlab/github 抓取常被 Cloudflare/auth 擋,官網最穩):
#       curl -sSLO https://zakalwe.fi/uade/uade2/uade-2.13.tar.bz2
#       tar xjf uade-2.13.tar.bz2 && cd uade-2.13 && ./configure --prefix=$PWD/inst && make && make install
#       # uade123 在 src/frontends/uade123/uade123;資料在 inst/share/uade2、uadecore 在 inst/lib/uade2
#    源碼(git):https://gitlab.com/heikkiorsila/uade
#    (部分發行版有 uade123 套件,可直接裝)

# 2) 取出 .tune(從 Amiga 版,素材不入庫)
7z e dragonwars_amiga_win.7z "DragonWars/fsuae/Hard Drives/data/*.tune"

# 3) 渲染每曲 → mono 22050 WAV(放進本目錄)
#    -t 為各曲擷取秒數(夠長以含完整一輪;循環由引擎處理)
MUSIC_DIR=opendw_remake/assets/bundle/audio/music
for t in title game combat end; do
  uade123 -t 180 --write-audio=/tmp/$t.raw.wav $t.tune
  ffmpeg -y -i /tmp/$t.raw.wav -ac 1 -ar 22050 -c:a pcm_s16le "$MUSIC_DIR/$t.wav"
done
```

渲染後直接執行 `opendw_remake`,各狀態即循環對應背景音樂;`--mute` 可整體關閉。

> 也可用 `tools_build/render_music.sh`(同上步驟的腳本版)。
