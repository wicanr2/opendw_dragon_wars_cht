#!/bin/sh
# render_music.sh — 把 Amiga《火龍之戰》MANIACS of NOISE 的 .tune 渲染成 remake 背景音樂 WAV。
#
# 需求(在有網路 / 已裝工具的機器上跑):
#   - UADE(uade123):https://gitlab.com/heikkiorsila/uade(源碼 ./configure && make && make install)
#   - ffmpeg
#
# 用法: render_music.sh <含 *.tune 的目錄> [<輸出目錄,預設 bundle/audio/music>]
#   .tune 取得:7z e dragonwars_amiga_win.7z "DragonWars/fsuae/Hard Drives/data/*.tune"
#
# 輸出:title/game/combat/end .wav(mono / 16-bit / 22050 Hz;對齊 sound.cpp 混音規格)。
# 引擎端循環播放系統已就緒(sound.cpp music 頻道 + main.cpp 依 state 切曲);放好即播。
set -eu

TUNE_DIR="${1:?用法: render_music.sh <tune 目錄> [out 目錄]}"
OUT_DIR="${2:-opendw_remake/assets/bundle/audio/music}"
SECS="${MUSIC_SECS:-180}"   # 各曲擷取秒數(夠長含完整一輪;循環由引擎處理)

command -v uade123 >/dev/null 2>&1 || { echo "缺 uade123(見 $OUT_DIR/README.md)"; exit 1; }
command -v ffmpeg  >/dev/null 2>&1 || { echo "缺 ffmpeg"; exit 1; }
mkdir -p "$OUT_DIR"

for t in title game combat end; do
  src="$TUNE_DIR/$t.tune"
  [ -f "$src" ] || { echo "略過 $t(找不到 $src)"; continue; }
  echo "渲染 $t.tune → $OUT_DIR/$t.wav"
  uade123 -t "$SECS" --write-audio="/tmp/dwmusic_$t.raw.wav" "$src"
  ffmpeg -y -loglevel error -i "/tmp/dwmusic_$t.raw.wav" -ac 1 -ar 22050 -c:a pcm_s16le "$OUT_DIR/$t.wav"
  rm -f "/tmp/dwmusic_$t.raw.wav"
done
echo "完成。直接執行 opendw_remake 即依狀態循環背景音樂(--mute 可關)。"
