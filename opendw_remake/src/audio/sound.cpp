// sound — 音效子系統實作(SDL2 audio + PC speaker 風格方波合成)。
// 介面與真值層級說明見 sound.hpp 檔頭。
#include "audio/sound.hpp"

#include <SDL.h>

#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <vector>

namespace dw::audio {

namespace {

// 合成輸出規格(remake 設計;PC speaker 為單聲道方波,取 22050 Hz 已足夠)。
constexpr int kSampleRate = 22050;
constexpr int kChannels   = 1;
constexpr float kAmplitude = 0.18f;   // 方波振幅(避免過大;-1..1 範圍)

// func_5060 真值 dx/bx → remake Hz/ms 換算(grounded 音效語意,非逐 tick 真值)。
//
// 換算公式(remake 設計):
//   原版 PC speaker 半週期由 bx 控制(bx 越大音越低)。取一個基準把 bx 映成可聽頻率:
//     freq_hz = round( kPitConst / bx )。挑 kPitConst 使 door_open(bx=0x400=1024)
//     落在約 440 Hz(A4),wall_bump(bx=0x800=2048)約 220 Hz(低沉撞牆),
//     effect_88(bx=0x200=512)約 880 Hz(尖銳)。即 kPitConst ≈ 1024*440 = 450560。
//   音長 dur_ms = dx 線性映射:door_open dx=0x28(40)→ 約 80ms(短促),
//     wall_bump dx=0xC8(200)→ 約 240ms,effect_88 dx=0xF0(240)→ 約 280ms。
//     取 dur_ms = round(dx * kMsPerDx),kMsPerDx ≈ 1.2。
constexpr double kPitConst = 450560.0;
constexpr double kMsPerDx  = 1.2;

int hz_from_bx(std::uint16_t bx) {
  if (bx == 0) return 0;
  return (int)std::lround(kPitConst / (double)bx);
}
int ms_from_dx(std::uint16_t dx) {
  return (int)std::lround((double)dx * kMsPerDx);
}

// 每個 SoundId 的來源參數(oracle dx/bx;grounded 旗標)。
struct Entry { std::uint16_t dx, bx; bool grounded; int freq, dur; };

// 由 oracle dx/bx 推導 freq/dur(grounded);無真值者直接給 remake 設計 freq/dur。
Entry make_grounded(std::uint16_t dx, std::uint16_t bx) {
  return Entry{dx, bx, true, hz_from_bx(bx), ms_from_dx(dx)};
}
Entry make_remake(int freq, int dur) {
  return Entry{0, 0, false, freq, dur};
}

const Entry& entry_of(SoundId id) {
  static const Entry table[(int)SoundId::Count] = {
    /* EffectB2 */ make_remake(660, 60),                 // oracle 未實作 → 泛用嗶聲(remake)
    /* Effect88 */ make_grounded(0xF0, 0x200),           // 真值:dx=240 bx=512
    /* DoorOpen */ make_grounded(0x28, 0x400),           // 真值:dx=40  bx=1024
    /* WallBump */ make_grounded(0xC8, 0x800),           // 真值:dx=200 bx=2048
    /* Pcm0 */ make_remake(523, 90),                     // PCM(oracle 未播放)→ remake 方波近似
    /* Pcm1 */ make_remake(587, 90),
    /* Pcm2 */ make_remake(659, 90),
    /* Pcm3 */ make_remake(698, 90),
    /* Pcm4 */ make_remake(784, 90),
    /* Pcm5 */ make_remake(880, 90),
    /* Pcm6 */ make_remake(988, 90),
    /* Hit  */ make_remake(150, 70),                     // 命中:低沉短擊(remake)
    /* Cast */ make_remake(990, 160),                    // 施法:高頻較長(remake)
  };
  return table[(int)id];
}

// 一個進行中的方波發聲(SDL callback 線程消費)。
struct Voice {
  double phase = 0.0;       // 0..1 方波相位
  double phase_inc = 0.0;   // 每樣本相位增量 = freq / sampleRate
  int remaining = 0;        // 剩餘樣本數
  bool active = false;
};

// 全域混音狀態:單一 audio device 服務整個程式(音效屬全域副作用)。
constexpr int kMaxVoices = 8;
std::mutex g_mtx;
Voice g_voices[kMaxVoices];

void audio_callback(void* /*userdata*/, std::uint8_t* stream, int len) {
  float* out = reinterpret_cast<float*>(stream);
  int frames = len / (int)sizeof(float) / kChannels;
  std::lock_guard<std::mutex> lk(g_mtx);
  for (int i = 0; i < frames; ++i) {
    float sample = 0.0f;
    for (auto& v : g_voices) {
      if (!v.active) continue;
      // 方波:相位前半 +amp,後半 -amp。
      sample += (v.phase < 0.5 ? kAmplitude : -kAmplitude);
      v.phase += v.phase_inc;
      if (v.phase >= 1.0) v.phase -= 1.0;
      if (--v.remaining <= 0) v.active = false;
    }
    // 軟性夾限,避免多聲疊加爆音。
    if (sample > 1.0f) sample = 1.0f;
    if (sample < -1.0f) sample = -1.0f;
    for (int c = 0; c < kChannels; ++c) out[i * kChannels + c] = sample;
  }
}

void start_voice(int freq, int dur_ms) {
  if (freq <= 0 || dur_ms <= 0) return;
  std::lock_guard<std::mutex> lk(g_mtx);
  for (auto& v : g_voices) {
    if (v.active) continue;
    v.phase = 0.0;
    v.phase_inc = (double)freq / (double)kSampleRate;
    v.remaining = (int)((long long)kSampleRate * dur_ms / 1000);
    v.active = true;
    return;
  }
  // 聲部已滿 → 丟棄(PC speaker 本來也只有單音;這裡寬容到 8 聲)。
}

}  // namespace

bool dispatch_index_to_sound(int idx, SoundId& out) {
  if (idx < 0 || idx >= kNumDispatchSounds) return false;
  out = (SoundId)idx;  // EffectB2..Pcm6 與 func_5060 索引一致
  return true;
}

SoundParams params_of(SoundId id) {
  const Entry& e = entry_of(id);
  return SoundParams{e.dx, e.bx, e.freq, e.dur, e.grounded};
}

Sound::~Sound() { close(); }

bool Sound::open(bool muted) {
  if (opened_) return true;
  muted_ = muted;

  // 靜音模式:不碰 SDL audio 子系統(headless / CI 不依賴裝置)。仍算初始化成功。
  if (muted_) {
    opened_ = true;
    return true;
  }

  if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
    // 無 audio 子系統(例如某些 CI)→ 退回靜音,不視為失敗。
    std::fprintf(stderr, "[audio] SDL_INIT_AUDIO 失敗(%s):退回靜音模式\n", SDL_GetError());
    muted_ = true;
    opened_ = true;
    return true;
  }

  SDL_AudioSpec want;
  std::memset(&want, 0, sizeof(want));
  want.freq = kSampleRate;
  want.format = AUDIO_F32SYS;
  want.channels = (Uint8)kChannels;
  want.samples = 512;
  want.callback = audio_callback;

  SDL_AudioSpec have;
  SDL_AudioDeviceID dev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
  if (dev == 0) {
    // dummy driver 或無實體裝置:多數情況 SDL 仍會給一個有效裝置;失敗則退靜音。
    std::fprintf(stderr, "[audio] SDL_OpenAudioDevice 失敗(%s):退回靜音模式\n", SDL_GetError());
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    muted_ = true;
    opened_ = true;
    return true;
  }

  dev_handle_ = reinterpret_cast<void*>((std::uintptr_t)dev);
  SDL_PauseAudioDevice(dev, 0);  // 開始播放(callback 被 SDL 線程驅動)
  opened_ = true;
  return true;
}

void Sound::close() {
  if (!opened_) return;
  if (dev_handle_) {
    SDL_AudioDeviceID dev = (SDL_AudioDeviceID)(std::uintptr_t)dev_handle_;
    SDL_CloseAudioDevice(dev);
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    dev_handle_ = nullptr;
  }
  // 清空聲部。
  {
    std::lock_guard<std::mutex> lk(g_mtx);
    for (auto& v : g_voices) v.active = false;
  }
  opened_ = false;
}

bool Sound::play(SoundId id) {
  if (!opened_) return false;
  if ((int)id >= (int)SoundId::Count) return false;
  if (muted_ || dev_handle_ == nullptr) return false;  // 合法 no-op(已派發但不出聲)
  const Entry& e = entry_of(id);
  start_voice(e.freq, e.dur);
  return true;
}

}  // namespace dw::audio
