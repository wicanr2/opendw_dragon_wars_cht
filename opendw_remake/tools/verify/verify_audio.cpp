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

#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace dw;

static int g_fail = 0;
#define CHECK(cond, msg)                                                  \
  do {                                                                    \
    if (!(cond)) { std::printf("FAIL: %s\n", msg); ++g_fail; }            \
    else { std::printf("ok: %s\n", msg); }                               \
  } while (0)

int main() {
  // dummy audio driver(headless / CI:不需真實裝置)。
  ::setenv("SDL_AUDIODRIVER", "dummy", 1);
  ::setenv("SDL_VIDEODRIVER", "dummy", 1);

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

  if (g_fail == 0) {
    std::printf("\nAUDIO PASS\n");
    return 0;
  }
  std::printf("\nAUDIO FAIL (%d)\n", g_fail);
  return 1;
}
