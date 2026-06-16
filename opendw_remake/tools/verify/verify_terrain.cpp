// verify_terrain — 探索互動(門/密門/陷阱/地形法術)機制確定性自測(不需 SDL,印 PASS/FAIL)。
//
// 真值層級:機制為 remake 設計(opendw 主遊戲 K 開門 / 陷阱 / 探索施法未反編;見 docs/57)。
//   本測試驗證 remake 機制邏輯(walkability 閘、門狀態轉移、陷阱解除/感知、地形法術效果、
//   TerrainState 序列化 round-trip)的確定性與正確性,非對拍 oracle。
//
// 涵蓋:
//   1) terrain_walkable:關門/鎖門/密門/石牆未開時擋路;陷阱可走;旗標設妥後可走。
//   2) apply_terrain_spell:Disarm Trap / Sense Traps / Soften Stone 效果與 NoEffect。
//   3) TerrainState 序列化 → 反序列化 round-trip,旗標逐格一致。
#include <cstdint>
#include <cstdio>

#include "game/terrain.hpp"
#include "game/terrain_state.hpp"

using namespace dw::game;

namespace {
int g_fail = 0;
void check(bool cond, const char* what) {
  std::printf("  [%s] %s\n", cond ? "ok" : "FAIL", what);
  if (!cond) ++g_fail;
}
}  // namespace

int main() {
  const int A = 7;  // 任意 area id

  std::printf("== 1) terrain_walkable 閘 ==\n");
  {
    TerrainState ts;
    // 一般地面(非保留段)→ 本模組不攔。
    check(terrain_walkable(ts, A, 1, 1, 0x01), "floor(0x01) walkable");
    // 關閉的門未開 → 擋。
    check(!terrain_walkable(ts, A, 2, 2, TT_DoorClosed), "closed door blocks until opened");
    ts.set(A, 2, 2, TF_DoorOpen);
    check(terrain_walkable(ts, A, 2, 2, TT_DoorClosed), "opened door walkable");
    // 鎖門:同樣以 DoorOpen 旗標解。
    check(!terrain_walkable(ts, A, 3, 3, TT_DoorLocked), "locked door blocks until opened");
    ts.set(A, 3, 3, TF_DoorOpen);
    check(terrain_walkable(ts, A, 3, 3, TT_DoorLocked), "unlocked door walkable");
    // 密門:像牆,SecretBroken 才可走。
    check(!terrain_walkable(ts, A, 4, 4, TT_SecretDoor), "secret door blocks (looks like wall)");
    ts.set(A, 4, 4, TF_SecretBroken);
    check(terrain_walkable(ts, A, 4, 4, TT_SecretDoor), "broken secret door walkable");
    // 石牆:Soften Stone(SecretBroken)才可走。
    check(!terrain_walkable(ts, A, 5, 5, TT_Stone), "stone wall blocks until softened");
    ts.set(A, 5, 5, TF_SecretBroken);
    check(terrain_walkable(ts, A, 5, 5, TT_Stone), "softened stone walkable");
    // 陷阱:一律可走(踩格才結算)。
    check(terrain_walkable(ts, A, 6, 6, TT_Trap), "trap tile always walkable");
  }

  std::printf("== 2) apply_terrain_spell ==\n");
  {
    // Disarm Trap:前方是陷阱 → 解除;再施一次 → NoEffect。
    TerrainState ts;
    auto r = apply_terrain_spell(ts, SP_DisarmTrap, A, /*fwd*/2, 2, TT_Trap,
                                 /*cur*/1, 1, 0x01);
    check(r == TerrainSpellResult::TrapDisarmed, "Disarm Trap disarms forward trap");
    check(ts.has(A, 2, 2, TF_TrapDisarmed), "trap (2,2) flagged disarmed");
    r = apply_terrain_spell(ts, SP_DisarmTrap, A, 2, 2, TT_Trap, 1, 1, 0x01);
    check(r == TerrainSpellResult::NoEffect, "Disarm Trap on already-disarmed = NoEffect");
    // Disarm Trap:前方非陷阱、當前格是陷阱 → 解除當前格。
    TerrainState ts2;
    r = apply_terrain_spell(ts2, SP_DisarmTrap, A, 2, 2, 0x01, 1, 1, TT_Trap);
    check(r == TerrainSpellResult::TrapDisarmed && ts2.has(A, 1, 1, TF_TrapDisarmed),
          "Disarm Trap falls back to current-cell trap");

    // Sense Traps:標記可見。
    TerrainState ts3;
    r = apply_terrain_spell(ts3, SP_SenseTraps, A, 2, 2, TT_Trap, 1, 1, 0x01);
    check(r == TerrainSpellResult::TrapsSensed && ts3.has(A, 2, 2, TF_TrapSensed),
          "Sense Traps marks forward trap sensed");
    r = apply_terrain_spell(ts3, SP_SenseTraps, A, 2, 2, 0x01, 1, 1, 0x01);
    check(r == TerrainSpellResult::NoEffect, "Sense Traps with no trap = NoEffect");

    // Soften Stone:前方石牆 → 軟化(SecretBroken);其後可走。
    TerrainState ts4;
    r = apply_terrain_spell(ts4, SP_SoftenStone, A, 3, 3, TT_Stone, 1, 1, 0x01);
    check(r == TerrainSpellResult::StoneSoftened && ts4.has(A, 3, 3, TF_SecretBroken),
          "Soften Stone softens forward stone");
    check(terrain_walkable(ts4, A, 3, 3, TT_Stone), "softened stone now walkable");
    r = apply_terrain_spell(ts4, SP_SoftenStone, A, 3, 3, 0x01, 1, 1, 0x01);
    check(r == TerrainSpellResult::NoEffect, "Soften Stone with no stone = NoEffect");

    // Create Wall(0x21):在前方可走的一般地面格放石牆 → 變不可走;牆上施放 = NoEffect。
    TerrainState ts5;
    r = apply_terrain_spell(ts5, SP_CreateWall, A, 4, 4, 0x01 /*floor*/, 1, 1, 0x01);
    check(r == TerrainSpellResult::WallCreated && ts5.has(A, 4, 4, TF_WallPlaced),
          "Create Wall places wall on forward floor");
    check(!terrain_walkable(ts5, A, 4, 4, 0x01), "placed wall blocks a normally-walkable floor");
    r = apply_terrain_spell(ts5, SP_CreateWall, A, 4, 4, 0x01, 1, 1, 0x01);
    check(r == TerrainSpellResult::NoEffect, "Create Wall on already-walled cell = NoEffect");
    r = apply_terrain_spell(ts5, SP_CreateWall, A, 5, 5, 0x00 /*wall*/, 1, 1, 0x01);
    check(r == TerrainSpellResult::NoEffect, "Create Wall on existing wall tile = NoEffect");

    // Mage Light(0x05):party 層級光源 → 回報 LightLit(無格作用)。
    r = apply_terrain_spell(ts5, SP_MageLight, A, 4, 4, 0x01, 1, 1, 0x01);
    check(r == TerrainSpellResult::LightLit, "Mage Light reports LightLit (party-wide)");

    // 非地形法術 id → NotTerrain。
    r = apply_terrain_spell(ts4, 0x07 /*Elvar's Fire*/, A, 3, 3, TT_Stone, 1, 1, 0x01);
    check(r == TerrainSpellResult::NotTerrain, "combat spell = NotTerrain");
    check(is_terrain_spell(SP_DisarmTrap) && is_terrain_spell(SP_SoftenStone) &&
              is_terrain_spell(SP_SenseTraps) && is_terrain_spell(SP_CreateWall) &&
              is_terrain_spell(SP_MageLight) && !is_terrain_spell(0x07),
          "is_terrain_spell classifies correctly (incl. Create Wall / Mage Light)");
  }

  std::printf("== 3) TerrainState 序列化 round-trip ==\n");
  {
    TerrainState ts;
    ts.set(1, 10, 20, TF_DoorOpen);
    ts.set(1, 10, 20, TF_TrapSensed);  // 同格多旗標
    ts.set(5, 0, 0, TF_SecretBroken);
    ts.set(31, 7, 3, TF_TrapDisarmed | TF_TrapSprung);
    auto blob = ts.serialize();
    TerrainState ts2;
    bool ok = ts2.deserialize(blob.data(), blob.size());
    check(ok, "deserialize ok");
    check(ts2.get(1, 10, 20) == (TF_DoorOpen | TF_TrapSensed), "(1,10,20) flags match");
    check(ts2.has(5, 0, 0, TF_SecretBroken), "(5,0,0) secret broken");
    check(ts2.get(31, 7, 3) == (TF_TrapDisarmed | TF_TrapSprung), "(31,7,3) flags match");
    check(ts2.get(1, 99, 99) == 0, "unset cell = 0");
    // 再序列化 → byte-for-byte 一致(確定性)。
    auto blob2 = ts2.serialize();
    check(blob == blob2, "re-serialize byte-for-byte equal");
    // 截斷檔 → 反序列化拒絕。
    TerrainState ts3;
    check(blob.size() < 2 || !ts3.deserialize(blob.data(), 3), "truncated blob rejected");
  }

  std::printf(g_fail ? "\nFAIL: %d check(s) failed\n" : "\nPASS: all terrain checks ok\n",
              g_fail);
  return g_fail ? 1 : 0;
}
