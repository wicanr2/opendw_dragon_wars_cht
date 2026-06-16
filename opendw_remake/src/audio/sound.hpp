// sound — 音效子系統(PC speaker 風格方波合成)。
//
// 對照 opendw 的音效 dispatch 結構(engine.c):
//   op_sound_effect(op_90 @0x49E7)讀 1 byte operand(音效編號 idx),
//   呼叫 dispatch_sound_effect(idx) → func_5060[idx]() 函式表:
//     idx 0  0x5060 play_sound_effect_B2   (oracle 未實作)
//     idx 1  0x5062 play_sound_effect_88   dx=0xF0 bx=0x200
//     idx 2  0x5064 play_sound_door_open   dx=0x28 bx=0x400
//     idx 3  0x5066 play_sound_wall_bump   dx=0xC8 bx=0x800
//     idx 4+ 0x5068.. snd_pcm_resource     (從資源載 PCM,oracle 也未播放)
//   原版用 PC speaker(8253/8254 PIT timer channel 2 方波)發聲;dx 為迴圈次數
//   (音長),bx 為與頻率相關的延遲常數(bx 越大 → 半週期越長 → 音越低)。
//
// 真值層級(誠實標示):
//   - func_5060 表的「索引 → 語意對映」與每個音效的 dx/bx 常數 = opendw 反組譯真值。
//   - 把 dx/bx 換算成實際 Hz/ms 的公式、方波合成本身的取樣參數
//     = remake 設計(grounded 音效語意);DOS 原版逐 tick 的精確波形未逐位元逆出。
//   - PCM 音效(snd_pcm_resource,idx>=4)opendw 本身未實作播放,remake 暫以
//     方波近似(標 remake 設計),不謊稱為原版 PCM 真值。
//
// Deep module:對外只有 open/play/close/is_open + SoundId 列舉;
//   內部隱藏 SDL2 audio device、方波合成、ring buffer、靜音旗標。
//   無 SDL audio 裝置 / 靜音 / dummy driver 時,play() 仍為合法 no-op(回報已派發),
//   絕不導致 init 失敗或卡住(headless / CI 不依賴音效裝置)。
#pragma once
#include <cstdint>

namespace dw::audio {

// 音效語意 id(對映 opendw func_5060 表 + remake 補充)。
// 數值與 op_90 operand(func_5060 索引)一一對應,供 VM dispatch 直接轉換。
enum class SoundId : std::uint8_t {
  EffectB2  = 0,  // 0x5060 play_sound_effect_B2(oracle 未實作;remake 以泛用嗶聲近似)
  Effect88  = 1,  // 0x5062 play_sound_effect_88   dx=0xF0 bx=0x200
  DoorOpen  = 2,  // 0x5064 play_sound_door_open   dx=0x28 bx=0x400
  WallBump  = 3,  // 0x5066 play_sound_wall_bump   dx=0xC8 bx=0x800
  Pcm0      = 4,  // 0x5068 snd_pcm_resource(idx 4..10;PCM,oracle 未播放)
  Pcm1      = 5,
  Pcm2      = 6,
  Pcm3      = 7,
  Pcm4      = 8,
  Pcm5      = 9,
  Pcm6      = 10,
  // ── remake 補充事件音(非 op_90 dispatch;由戰鬥/施法等直接呼叫)──
  Hit       = 11, // 命中(戰鬥近戰擊中);remake 設計方波
  Cast      = 12, // 施法;remake 設計方波(上行掃頻感)
  Count
};

// op_90 dispatch 表大小(對照 NUM_FUNCS:func_5060 11 個項目)。
inline constexpr int kNumDispatchSounds = 11;

// op_90 operand(func_5060 索引)→ SoundId。超出 dispatch 範圍回傳 false。
bool dispatch_index_to_sound(int idx, SoundId& out);

// 取某音效的 grounded 參數(供測試 / 診斷檢視真值對映)。
// 回傳 true 表示該 id 有 opendw 反組譯出的 dx/bx 真值;false 表示 remake 設計值。
struct SoundParams {
  std::uint16_t oracle_dx;   // 原版迴圈次數(音長);無真值時為 0
  std::uint16_t oracle_bx;   // 原版頻率相關延遲常數;無真值時為 0
  int freq_hz;               // remake 合成頻率(Hz)
  int dur_ms;                // remake 合成時長(ms)
  bool grounded;             // dx/bx 是否為 opendw 真值
};
SoundParams params_of(SoundId id);

// 音效引擎:RAII 開啟 SDL2 audio,合成方波寫入裝置。
class Sound {
public:
  Sound() = default;
  ~Sound();
  Sound(const Sound&) = delete;
  Sound& operator=(const Sound&) = delete;

  // 開啟音效裝置。muted=true 或無可用 audio 裝置 → 進靜音模式(play 仍合法 no-op)。
  // 回傳 true 表示「子系統初始化成功」(含靜音模式也算成功;絕不因無裝置而失敗)。
  bool open(bool muted = false);
  void close();
  bool is_open() const { return opened_; }
  bool is_muted() const { return muted_; }

  // 播放一個音效(非阻塞;混入輸出 ring buffer)。
  // 靜音 / 未開啟時為合法 no-op。回傳是否實際送進合成佇列(供測試斷言派發路徑)。
  bool play(SoundId id);

private:
  void* dev_handle_ = nullptr;   // SDL_AudioDeviceID(以 void* 隱藏 SDL 型別)
  bool opened_ = false;
  bool muted_ = false;
};

}  // namespace dw::audio
