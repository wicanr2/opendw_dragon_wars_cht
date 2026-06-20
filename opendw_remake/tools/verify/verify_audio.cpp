// verify_audio — 音效子系統確定性 PASS/FAIL(headless,SDL_AUDIODRIVER=dummy)。
//
// 驗:
//   1. 音效子系統 init OK(實體 / dummy / 靜音三種皆 is_open()==true,絕不因無裝置失敗)。
//   2. op_90 dispatch:func_5060 索引 → 正確 audio::SoundId(0..10 對映,>=11 拒絕)。
//   3. grounded 真值對映:door_open/wall_bump/effect_88 的 dx/bx == opendw 反組譯值。
//   4. 透過 VM op_90 sound sink 派發索引正確(VM↔audio 解耦路徑通)。
//   5. play() 在開啟下回報合法(靜音時為 no-op 但不崩),close() 後乾淨退出。
//
// 真值層級:索引→語意對映 + dx/bx 常數 = opendw 真值;Hz/ms 換算與方波合成 = remake 設計
//   (見 sound.hpp 檔頭)。本測試不驗實際波形,只驗派發 / 對映 / 生命週期(headless)。
#include "audio/sound.hpp"
#include "vm/interpreter.hpp"
#include "vm/vm_state.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <vector>

using namespace dw;

// 寫一個極簡 mono / 16-bit / 22050Hz WAV(供音樂頻道測試;~0.2s 方波音)。
static bool write_test_wav(const std::string& path, int ms = 200, int freq = 330) {
  const int rate = 22050;
  const int n = rate * ms / 1000;
  std::vector<std::int16_t> s((std::size_t)n);
  for (int i = 0; i < n; ++i) s[(std::size_t)i] = ((i * freq / rate) % 2) ? 8000 : -8000;
  std::uint32_t data_bytes = (std::uint32_t)(s.size() * 2);
  std::uint32_t riff = 36 + data_bytes;
  std::FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) return false;
  auto w32 = [&](std::uint32_t v) { std::fputc(v & 0xFF, f); std::fputc((v >> 8) & 0xFF, f);
                                     std::fputc((v >> 16) & 0xFF, f); std::fputc((v >> 24) & 0xFF, f); };
  auto w16 = [&](std::uint16_t v) { std::fputc(v & 0xFF, f); std::fputc((v >> 8) & 0xFF, f); };
  std::fwrite("RIFF", 1, 4, f); w32(riff); std::fwrite("WAVE", 1, 4, f);
  std::fwrite("fmt ", 1, 4, f); w32(16); w16(1); w16(1); w32(rate); w32(rate * 2); w16(2); w16(16);
  std::fwrite("data", 1, 4, f); w32(data_bytes);
  std::fwrite(s.data(), 2, s.size(), f);
  std::fclose(f);
  return true;
}

static int g_fail = 0;
#define CHECK(cond, msg)                                                  \
  do {                                                                    \
    if (!(cond)) { std::printf("FAIL: %s\n", msg); ++g_fail; }            \
    else { std::printf("ok: %s\n", msg); }                               \
  } while (0)

int main(int argc, char** argv) {
  // dummy audio driver(headless / CI:不需真實裝置)。
  ::setenv("SDL_AUDIODRIVER", "dummy", 1);
  ::setenv("SDL_VIDEODRIVER", "dummy", 1);

  // argv[1] = bundle 目錄(載 <bundle>/audio 真實 PCM 取樣);未給則只驗方波路徑。
  const std::string audio_dir = (argc > 1) ? std::string(argv[1]) + "/audio" : std::string();

  // 1) init OK(非靜音:dummy driver 下仍應成功開啟或自動退靜音,皆 is_open）。
  {
    audio::Sound snd;
    bool ok = snd.open(/*muted=*/false);
    CHECK(ok && snd.is_open(), "音效子系統 init OK(dummy driver)");
    // play 不論靜音與否都不可崩。
    snd.play(audio::SoundId::DoorOpen);
    snd.play(audio::SoundId::WallBump);
    snd.close();
    CHECK(!snd.is_open(), "close() 後乾淨退出");
  }

  // 2) 靜音模式 init OK 且 play 為合法 no-op。
  {
    audio::Sound snd;
    bool ok = snd.open(/*muted=*/true);
    CHECK(ok && snd.is_open() && snd.is_muted(), "靜音模式 init OK");
    bool played = snd.play(audio::SoundId::Hit);
    CHECK(!played, "靜音時 play() 為合法 no-op(回報未出聲)");
    snd.close();
  }

  // 3) op_90 dispatch 索引 → SoundId 對映。
  {
    audio::SoundId id;
    CHECK(audio::dispatch_index_to_sound(0, id) && id == audio::SoundId::EffectB2, "idx0 → EffectB2");
    CHECK(audio::dispatch_index_to_sound(1, id) && id == audio::SoundId::Effect88, "idx1 → Effect88");
    CHECK(audio::dispatch_index_to_sound(2, id) && id == audio::SoundId::DoorOpen, "idx2 → DoorOpen");
    CHECK(audio::dispatch_index_to_sound(3, id) && id == audio::SoundId::WallBump, "idx3 → WallBump");
    CHECK(audio::dispatch_index_to_sound(10, id) && id == audio::SoundId::Pcm6, "idx10 → Pcm6");
    CHECK(!audio::dispatch_index_to_sound(11, id), "idx11 超出 func_5060 範圍 → 拒絕");
    CHECK(!audio::dispatch_index_to_sound(-1, id), "idx-1 非法 → 拒絕");
  }

  // 4) grounded 真值對映(opendw 反組譯 dx/bx)。
  {
    auto door = audio::params_of(audio::SoundId::DoorOpen);
    CHECK(door.grounded && door.oracle_dx == 0x28 && door.oracle_bx == 0x400,
          "DoorOpen grounded dx=0x28 bx=0x400(真值)");
    auto wall = audio::params_of(audio::SoundId::WallBump);
    CHECK(wall.grounded && wall.oracle_dx == 0xC8 && wall.oracle_bx == 0x800,
          "WallBump grounded dx=0xC8 bx=0x800(真值)");
    auto e88 = audio::params_of(audio::SoundId::Effect88);
    CHECK(e88.grounded && e88.oracle_dx == 0xF0 && e88.oracle_bx == 0x200,
          "Effect88 grounded dx=0xF0 bx=0x200(真值)");
    // 頻率關係:bx 越大音越低(door 1024 → wall 2048 應更低)。
    CHECK(wall.freq_hz < door.freq_hz && door.freq_hz < e88.freq_hz,
          "頻率隨 bx 反比:wall < door < effect_88");
    // remake 設計值(非 grounded)。
    auto hit = audio::params_of(audio::SoundId::Hit);
    CHECK(!hit.grounded, "Hit 為 remake 設計值(非 oracle 真值)");
  }

  // 5) VM op_90 sound sink 派發(VM↔audio 解耦):op_90 operand → sink 索引。
  {
    vm::VmState st;
    // 手工塞一段 bytecode:op_90 0x02(door_open)、op_90 0x03(wall_bump)、op_5A(halt)。
    st.script = {0x90, 0x02, 0x90, 0x03, 0x5A};

    std::vector<int> dispatched;
    audio::Sound snd;
    snd.open(/*muted=*/true);  // 靜音:測派發路徑,不需真出聲

    vm::Interpreter ip(st);
    ip.set_sound_sink([&](int idx) {
      dispatched.push_back(idx);
      audio::SoundId id;
      if (audio::dispatch_index_to_sound(idx, id)) snd.play(id);
    });
    ip.run();

    CHECK(dispatched.size() == 2 && dispatched[0] == 2 && dispatched[1] == 3,
          "VM op_90 → sound sink 派發索引正確(2=door, 3=wall)");
    snd.close();
  }

  // 6) 真實 PCM 取樣載入 + 播放(R8;需 bundle/audio 路徑)。
  if (!audio_dir.empty()) {
    audio::Sound snd;
    bool ok = snd.open(/*muted=*/false, audio_dir);
    CHECK(ok && snd.is_open(), "音效子系統 init OK(載 bundle/audio 真實取樣)");
    // 對映到真實取樣的 SoundId 應有樣本可播(來源:Amiga data5/6、X68000 DW.SND)。
    CHECK(snd.has_sample(audio::SoundId::DoorOpen), "DoorOpen 有真實 PCM 取樣(amiga_data5)");
    CHECK(snd.has_sample(audio::SoundId::WallBump), "WallBump 有真實 PCM 取樣(amiga_data6)");
    CHECK(snd.has_sample(audio::SoundId::Cast),     "Cast 有真實 PCM 取樣(x68k_dwsnd)");
    CHECK(snd.has_sample(audio::SoundId::Pcm0),     "Pcm0(op_90 idx4)有真實 PCM 取樣");
    // 無對映者退回方波(無樣本)。
    CHECK(!snd.has_sample(audio::SoundId::EffectB2), "EffectB2 無取樣(退回方波)");
    CHECK(!snd.has_sample(audio::SoundId::Effect88), "Effect88 無取樣(grounded 方波)");
    // play() 在 dummy device 下對取樣 / 方波皆回報已派發,且不崩。
    CHECK(snd.play(audio::SoundId::DoorOpen), "play(DoorOpen) 派發真實取樣");
    CHECK(snd.play(audio::SoundId::EffectB2), "play(EffectB2) 派發方波(退回)");
    snd.close();
    CHECK(!snd.is_open(), "載取樣後 close() 乾淨退出");
  } else {
    std::printf("(skip PCM 取樣測試:未給 bundle 路徑)\n");
  }

  // 7) 背景音樂頻道:合成測試曲 → 驗載入 / 切曲 / idempotent / 缺檔 no-op。
  {
    const std::string mdir = "/tmp/dwr_music_test";
    ::mkdir(mdir.c_str(), 0777);
    ::mkdir((mdir + "/music").c_str(), 0777);
    bool wrote = write_test_wav(mdir + "/music/title.wav");
    CHECK(wrote, "合成測試音樂 title.wav");
    // 只放 title.wav(game/combat/end 缺)→ 驗載入有/無皆正確。
    audio::Sound snd;
    bool ok = snd.open(/*muted=*/false, mdir);
    CHECK(ok && snd.is_open(), "音樂頻道:Sound init OK(載 music/)");
    CHECK(snd.has_music(audio::MusicId::Title), "title.wav 已載入(has_music)");
    CHECK(!snd.has_music(audio::MusicId::Game), "game.wav 缺檔 → 未載入(no-op)");
    CHECK(snd.play_music(audio::MusicId::Title), "play_music(Title) 啟動(有資料)");
    CHECK(snd.play_music(audio::MusicId::Title), "play_music(Title) 再呼叫 idempotent(同曲)");
    CHECK(!snd.play_music(audio::MusicId::Game), "play_music(Game) 缺檔 → 合法 no-op(不崩)");
    snd.stop_music();   // 停止不崩
    CHECK(true, "stop_music() 不崩");
    snd.close();
    // 靜音模式:music 為 no-op。
    audio::Sound m;
    m.open(/*muted=*/true, mdir);
    CHECK(!m.play_music(audio::MusicId::Title), "靜音模式 play_music 為合法 no-op");
    m.close();
    std::remove((mdir + "/music/title.wav").c_str());
  }

  if (g_fail == 0) {
    std::printf("\nAUDIO PASS\n");
    return 0;
  }
  std::printf("\nAUDIO FAIL (%d)\n", g_fail);
  return 1;
}
