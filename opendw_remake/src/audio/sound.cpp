// sound — 音效子系統實作(SDL2 audio + PC speaker 風格方波合成)。
// 介面與真值層級說明見 sound.hpp 檔頭。
#include "audio/sound.hpp"

#include <SDL.h>

#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
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

// 一個進行中的 PCM 取樣播放(SDL callback 線程消費)。指向全域 sample bank 的 float 緩衝。
//   SFX 化:原始樣本是 1.4–3.3s 的持續音,當門/撞牆/命中等事件音太長 → 只播前段 [0,end)
//   並在尾端淡出(避免 click)。end 由 play() 依事件設(remake 設計;保留完整 WAV 不改檔)。
struct SampleVoice {
  const std::vector<float>* data = nullptr;  // 取樣資料(22050Hz mono float -1..1)
  std::size_t pos = 0;
  std::size_t end = 0;                        // 播放上限(≤ data->size();SFX 截短)
  bool active = false;
};

// 尾端淡出長度(~12ms @22050;避免截斷 click)。
constexpr std::size_t kFadeSamples = 256;

// 各事件的取樣播放上限(ms;SFX 化,remake 設計)。0 = 不截(整段播)。
int sample_cap_ms(SoundId id) {
  switch (id) {
    case SoundId::WallBump: return 320;   // 撞牆:短促衝擊
    case SoundId::Hit:      return 350;   // 命中:短促打擊
    case SoundId::DoorOpen: return 500;   // 開門:略長
    case SoundId::Cast:     return 700;   // 施法:強擊 + 短衰減尾
    default:                return 600;   // op_90 PCM:泛用 0.6s
  }
}

// ── 真實 PCM 取樣 bank(remake 載入原版平台音效;見 bundle/audio/README.md)──
//
// 對映(remake 設計;來源檔/格式為觀測真值,事件↔樣本對映非 oracle 真值):
//   DoorOpen → amiga_data5.wav(較長、較亮,作開門/移動感)
//   WallBump → amiga_data6.wav(較短,作撞牆衝擊)
//   Hit      → amiga_data6.wav(短促衝擊,複用撞牆樣本)
//   Cast     → x68k_dwsnd.wav(前段強擊 + 衰減尾,作施法)
//   Pcm0..6  → x68k_dwsnd.wav(op_90 idx>=4 的泛用真實 PCM;原版未播放)
//   EffectB2/Effect88 → 無對映(退回方波;effect_88 為 grounded dx/bx)
struct SampleMapEntry { SoundId id; const char* file; };
const SampleMapEntry kSampleMap[] = {
    {SoundId::DoorOpen, "amiga_data5.wav"},
    {SoundId::WallBump, "amiga_data6.wav"},
    {SoundId::Hit,      "amiga_data6.wav"},
    {SoundId::Cast,     "x68k_dwsnd.wav"},
    {SoundId::Pcm0,     "x68k_dwsnd.wav"},
    {SoundId::Pcm1,     "x68k_dwsnd.wav"},
    {SoundId::Pcm2,     "x68k_dwsnd.wav"},
    {SoundId::Pcm3,     "x68k_dwsnd.wav"},
    {SoundId::Pcm4,     "x68k_dwsnd.wav"},
    {SoundId::Pcm5,     "x68k_dwsnd.wav"},
    {SoundId::Pcm6,     "x68k_dwsnd.wav"},
};

// 全域混音狀態:單一 audio device 服務整個程式(音效屬全域副作用)。
constexpr int kMaxVoices = 8;
constexpr int kMaxSampleVoices = 4;
constexpr float kSampleGain = 0.7f;   // PCM 取樣播放增益(避免過大;原始已 8-bit)
std::mutex g_mtx;
Voice g_voices[kMaxVoices];
SampleVoice g_sample_voices[kMaxSampleVoices];

// 每個 SoundId 的取樣資料(空 = 無樣本,該 id 退回方波)。共享於整個程式。
std::shared_ptr<std::vector<float>> g_samples[(int)SoundId::Count];

// ── 背景音樂(循環)──
constexpr float kMusicGain = 0.32f;   // 背景音樂增益(壓在 SFX 之下,不蓋過音效)
std::shared_ptr<std::vector<float>> g_music[(int)MusicId::Count];  // 各曲 PCM(mono 22050 float)
int g_music_active = 0;        // 當前曲目(MusicId;0=None);g_mtx 保護
std::size_t g_music_pos = 0;   // 當前曲播放位置;g_mtx 保護

// MusicId → 檔名(music/ 子目錄;None 無檔)。
const char* music_file(MusicId id) {
  switch (id) {
    case MusicId::Title:  return "title.wav";
    case MusicId::Game:   return "game.wav";
    case MusicId::Combat: return "combat.wav";
    case MusicId::End:    return "end.wav";
    default:              return nullptr;
  }
}

void audio_callback(void* /*userdata*/, std::uint8_t* stream, int len) {
  float* out = reinterpret_cast<float*>(stream);
  int frames = len / (int)sizeof(float) / kChannels;
  std::lock_guard<std::mutex> lk(g_mtx);
  // 當前背景音樂(循環);active=0 或無資料 → 不混。
  const std::vector<float>* music =
      (g_music_active > 0 && g_music_active < (int)MusicId::Count && g_music[g_music_active])
          ? g_music[g_music_active].get()
          : nullptr;
  for (int i = 0; i < frames; ++i) {
    float sample = 0.0f;
    // 背景音樂:循環讀出(到尾回 0),壓在 SFX 之下。
    if (music && !music->empty()) {
      sample += (*music)[g_music_pos] * kMusicGain;
      if (++g_music_pos >= music->size()) g_music_pos = 0;
    }
    for (auto& v : g_voices) {
      if (!v.active) continue;
      // 方波:相位前半 +amp,後半 -amp。
      sample += (v.phase < 0.5 ? kAmplitude : -kAmplitude);
      v.phase += v.phase_inc;
      if (v.phase >= 1.0) v.phase -= 1.0;
      if (--v.remaining <= 0) v.active = false;
    }
    // PCM 取樣:逐樣本讀出(取樣率已對齊 kSampleRate,不需 runtime resample);播到 end 截止,
    //   尾端淡出避免 click。
    for (auto& sv : g_sample_voices) {
      if (!sv.active || sv.data == nullptr) continue;
      if (sv.pos >= sv.end) { sv.active = false; continue; }
      float v = (*sv.data)[sv.pos] * kSampleGain;
      std::size_t left = sv.end - sv.pos;
      if (left < kFadeSamples) v *= (float)left / (float)kFadeSamples;  // 線性淡出
      sample += v;
      if (++sv.pos >= sv.end) sv.active = false;
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

// 啟動一個 PCM 取樣聲部(指向共享 sample bank)。聲部滿 → 覆蓋最舊(最大 pos)。
//   end = 播放上限樣本數(SFX 截短;尾端由 callback 淡出)。
void start_sample(const std::shared_ptr<std::vector<float>>& data, std::size_t end) {
  if (!data || data->empty()) return;
  if (end == 0 || end > data->size()) end = data->size();
  std::lock_guard<std::mutex> lk(g_mtx);
  SampleVoice* slot = nullptr;
  for (auto& sv : g_sample_voices) {
    if (!sv.active) { slot = &sv; break; }
  }
  if (slot == nullptr) {
    // 全忙 → 挑進度最深(最接近結束)的覆蓋。
    std::size_t best = 0;
    for (auto& sv : g_sample_voices) {
      if (sv.pos >= best) { best = sv.pos; slot = &sv; }
    }
  }
  slot->data = data.get();
  slot->pos = 0;
  slot->end = end;
  slot->active = true;
}

// 極簡 WAV 載入器:只接受 mono / 16-bit signed PCM / kSampleRate(22050)。
//   不支援的格式 / 讀檔失敗 → 回 nullptr(該音效退回方波,不視為錯誤)。
//   (assets 由 tools_build/audio_extract.py 產生,保證上述規格;此處仍做完整檢查。)
std::shared_ptr<std::vector<float>> load_wav_mono16(const std::string& path) {
  FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) return nullptr;
  auto fail = [&]() -> std::shared_ptr<std::vector<float>> { std::fclose(f); return nullptr; };

  std::uint8_t hdr[12];
  if (std::fread(hdr, 1, 12, f) != 12) return fail();
  if (std::memcmp(hdr, "RIFF", 4) != 0 || std::memcmp(hdr + 8, "WAVE", 4) != 0) return fail();

  std::uint16_t fmt = 0, channels = 0, bits = 0;
  std::uint32_t rate = 0;
  std::vector<float> samples;
  bool have_fmt = false, have_data = false;

  // 逐 chunk 掃描(fmt / data;其餘略過)。
  for (;;) {
    std::uint8_t ch[8];
    if (std::fread(ch, 1, 8, f) != 8) break;
    std::uint32_t sz = ch[4] | (ch[5] << 8) | (ch[6] << 16) | ((std::uint32_t)ch[7] << 24);
    if (std::memcmp(ch, "fmt ", 4) == 0) {
      std::uint8_t fb[16];
      std::uint32_t n = sz < 16 ? sz : 16;
      if (std::fread(fb, 1, n, f) != n) return fail();
      fmt = fb[0] | (fb[1] << 8);
      channels = fb[2] | (fb[3] << 8);
      rate = fb[4] | (fb[5] << 8) | (fb[6] << 16) | ((std::uint32_t)fb[7] << 24);
      bits = fb[14] | (fb[15] << 8);
      if (sz > n) std::fseek(f, (long)(sz - n), SEEK_CUR);
      have_fmt = true;
    } else if (std::memcmp(ch, "data", 4) == 0) {
      if (!have_fmt || fmt != 1 || channels != 1 || bits != 16 ||
          rate != (std::uint32_t)kSampleRate) {
        return fail();  // 非預期格式 → 放棄此檔(退回方波)
      }
      std::size_t count = sz / 2;
      samples.reserve(count);
      std::vector<std::uint8_t> raw(sz);
      if (std::fread(raw.data(), 1, sz, f) != sz) return fail();
      for (std::size_t i = 0; i + 1 < sz; i += 2) {
        std::int16_t v = (std::int16_t)(raw[i] | (raw[i + 1] << 8));
        samples.push_back((float)v / 32768.0f);
      }
      have_data = true;
      break;
    } else {
      // 略過未知 chunk(含 word 對齊)。
      std::fseek(f, (long)(sz + (sz & 1)), SEEK_CUR);
    }
  }
  std::fclose(f);
  if (!have_data || samples.empty()) return nullptr;
  return std::make_shared<std::vector<float>>(std::move(samples));
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

bool Sound::open(bool muted, const std::string& audio_dir) {
  if (opened_) return true;
  muted_ = muted;

  // 靜音模式:不碰 SDL audio 子系統,也不載樣本(play 為 no-op,不需要)。仍算初始化成功。
  if (muted_) {
    opened_ = true;
    return true;
  }

  // 載入真實 PCM 取樣(缺檔不視為失敗;同檔只載一次共享)。
  if (!audio_dir.empty()) {
    for (const auto& m : kSampleMap) {
      int slot = (int)m.id;
      if (g_samples[slot]) continue;  // 已載(本次或前次 open)
      // 同檔去重:先找已載入同名檔的 SoundId 共享。
      std::shared_ptr<std::vector<float>> shared;
      for (const auto& m2 : kSampleMap) {
        if (g_samples[(int)m2.id] && std::strcmp(m2.file, m.file) == 0) {
          shared = g_samples[(int)m2.id];
          break;
        }
      }
      if (!shared) shared = load_wav_mono16(audio_dir + "/" + m.file);
      g_samples[slot] = shared;  // 可能為 nullptr(缺檔 → 退回方波)
    }
    // 背景音樂曲目(audio_dir/music/*.wav;缺檔該曲靜默)。
    for (int id = 1; id < (int)MusicId::Count; ++id) {
      if (g_music[id]) continue;  // 已載
      const char* fn = music_file((MusicId)id);
      if (fn) g_music[id] = load_wav_mono16(audio_dir + "/music/" + fn);
    }
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
  // 清空聲部(方波 + PCM 取樣)。樣本資料保留於 g_samples,下次 open() 不必重載。
  {
    std::lock_guard<std::mutex> lk(g_mtx);
    for (auto& v : g_voices) v.active = false;
    for (auto& sv : g_sample_voices) { sv.active = false; sv.data = nullptr; }
  }
  opened_ = false;
}

bool Sound::has_sample(SoundId id) const {
  if ((int)id >= (int)SoundId::Count) return false;
  return g_samples[(int)id] && !g_samples[(int)id]->empty();
}

void Sound::reset_caches() {
  std::lock_guard<std::mutex> lk(g_mtx);
  for (auto& s : g_samples) s.reset();
  for (auto& m : g_music) m.reset();
  g_music_active = 0;
  g_music_pos = 0;
}

bool Sound::has_music(MusicId id) const {
  if ((int)id <= 0 || (int)id >= (int)MusicId::Count) return false;
  return g_music[(int)id] && !g_music[(int)id]->empty();
}

bool Sound::play_music(MusicId id) {
  if (!opened_ || muted_ || dev_handle_ == nullptr) return false;  // 合法 no-op
  int want = (int)id;
  if (want < 0 || want >= (int)MusicId::Count) want = 0;
  std::lock_guard<std::mutex> lk(g_mtx);
  if (want == g_music_active) return has_music(id);  // 同曲 → 不重啟
  g_music_active = want;
  g_music_pos = 0;
  return want > 0 && g_music[want] && !g_music[want]->empty();
}

bool Sound::play(SoundId id) {
  if (!opened_) return false;
  if ((int)id >= (int)SoundId::Count) return false;
  if (muted_ || dev_handle_ == nullptr) return false;  // 合法 no-op(已派發但不出聲)
  // 有真實 PCM 取樣 → 播放取樣(依事件截短成 SFX);否則退回方波合成。
  if (g_samples[(int)id] && !g_samples[(int)id]->empty()) {
    int cap_ms = sample_cap_ms(id);
    std::size_t end = cap_ms > 0 ? (std::size_t)((long long)kSampleRate * cap_ms / 1000) : 0;
    start_sample(g_samples[(int)id], end);
    return true;
  }
  const Entry& e = entry_of(id);
  start_voice(e.freq, e.dur);
  return true;
}

}  // namespace dw::audio
