// terrain — 門 / 密門 / 陷阱 / 石牆障礙 的 tile 型約定 + 互動規則(remake 設計)。
//
// Deep module:對外露 (1) tile 型常數、(2) 「該格現在可不可走」判定(綜合
// Level tile + TerrainState 旗標)、(3) 互動結果列舉。內部隱藏旗標組合邏輯。
//
// ── 真值層級(務必誠實,見 docs/gameplay/57_DOORS_TRAPS_TERRAIN.md)─────────────────────────────────────
//   逆向深掘(2026-06-16):可識別到「面向前方牆型 byte」(engine.c refresh_viewport
//   5663-5673:前方中央格牆 nibble → data_56C6[nibble+0xF] → gs[0x26]),其低 nibble
//   hilo 是逐關牆型碼,地牢類部分牆 nibble hilo!=0 與主牆區隔(門牆候選)。**但**
//   hilo 重載(area34 整間實驗室內牆 hilo=3 是主牆非門)、全 40 關牆 nibble 只選固定
//   5 個 sprite tag、無專屬門 tag → K 開門「開/鎖/阻擋/開後可走」語意完全在消費
//   gs[0x26] 的未反編主迴圈,靜態無法乾淨逆出;連 move walkability(tile!=0)亦然。
//   故以下 tile 型→語意為 **remake 設計約定**(grounded 手冊 K=開門/破密門、
//   Disarm/Sense Trap/Soften Stone 法術),非 oracle 真值。
//
//   2026-06-17 更新:**真實 .lvl 陷阱格已由路徑 B 識別**(`real_terrain.hpp`:逐事件格
//   跑 VM,傷害訊息語意類 → 原版真座標,3 關 111 格)。本檔 0x30..0x34 改為 remake 測試關
//   保留約定(真實 .lvl 不含),與真陷阱機制相容並存(真陷阱優先)。門真實格仍受阻
//   (牆 sprite 無專屬門 tag;見 docs/gameplay/57_DOORS_TRAPS_TERRAIN.md §路徑 A + render_fp_cell 證據)。
// ──────────────────────────────────────────────────────────────────────
#pragma once

#include <cstdint>

#include "game/terrain_state.hpp"

namespace dw::game {

// remake tile 型保留段(word_11C8 值):門 / 密門 / 陷阱 / 石牆。
//   0=void/牆、1=地面、其餘原為事件格;0x30..0x34 為本機制保留約定。
enum TerrainTile : std::uint8_t {
  TT_DoorClosed = 0x30,  // 關閉的門:未開時擋路,K 面向它 → 開啟後可走
  TT_DoorLocked = 0x31,  // 鎖住的門:需 Lockpick;K + 技能足夠 → 開啟
  TT_SecretDoor = 0x32,  // 牆中密門:移動視為牆,K 面向它 → 粉碎後可走
  TT_Trap       = 0x33,  // 陷阱格:可走但踩中觸發傷害(未解除/未觸發時)
  TT_Stone      = 0x34,  // 石牆障礙:視為牆,Soften Stone 軟化後可走
};

// 某 tile 型是否屬本機制的「門/密門/石牆/陷阱」保留段。
inline bool is_terrain_tile(std::uint8_t t) {
  return t >= TT_DoorClosed && t <= TT_Stone;
}

// 綜合判定:站在 area 的某格、其 tile 型為 t、考量 TerrainState 旗標後,
// 該格現在可不可走。
//   • 非保留段:交回呼叫端用一般 tile!=0 判定(回 true 表示「本模組不攔」)。
//   • 關門/鎖門:DoorOpen 才可走。
//   • 密門/石牆:SecretBroken 才可走。
//   • 陷阱:一律可走(踩格才結算傷害)。
// 回傳:true = 可走(或本模組不攔);false = 被門/密門/石牆擋住。
inline bool terrain_walkable(const TerrainState& ts, int area, int x, int y,
                             std::uint8_t t) {
  // Create Wall(remake 設計):在原本可走的格放石牆 → 變不可走(對稱 Soften Stone)。
  //   TF_WallPlaced 優先於 tile 型判定(連一般地面格也擋)。離關重載即消失(狀態不寫 .lvl)。
  if (ts.has(area, x, y, TF_WallPlaced)) return false;
  switch (t) {
    case TT_DoorClosed:
    case TT_DoorLocked:
      return ts.has(area, x, y, TF_DoorOpen);
    case TT_SecretDoor:
    case TT_Stone:
      return ts.has(area, x, y, TF_SecretBroken);
    case TT_Trap:
      return true;
    default:
      return true;  // 非保留段:不攔(由一般 walkable 判定)
  }
}

// 地形法術(戰鬥外)effect id(對齊 spells.cpp:0x14 感知陷阱、0x36 解除陷阱、
// 0x22 軟化石頭)。
enum TerrainSpellId : std::uint8_t {
  SP_MageLight   = 0x05,  // Mage Light:法師魔光(黑暗中視物)→ 點亮探索光源狀態(party 層級)
  SP_SenseTraps  = 0x14,  // Sense Traps:標記當前 area 已知陷阱格可見
  SP_CreateWall  = 0x21,  // Create Wall:在面向前方格放石牆障礙 → 變不可走(對稱 Soften Stone)
  SP_SoftenStone = 0x22,  // Soften Stone:軟化面向前方石牆障礙 → 可走
  SP_DisarmTrap  = 0x36,  // Disarm Trap:解除面向前方或當前格陷阱
};

// 地形法術結果語意(供訊息提示)。
enum class TerrainSpellResult : std::uint8_t {
  NotTerrain,    // 非地形法術(交回呼叫端做戰鬥/工具結算或忽略)
  TrapsSensed,   // Sense Traps 已對前方/當前陷阱標記可見
  TrapDisarmed,  // Disarm Trap 已解除一個陷阱
  StoneSoftened, // Soften Stone 已軟化前方石牆
  WallCreated,   // Create Wall 已在前方格放置石牆障礙
  LightLit,      // Mage Light 已點亮探索光源(party 層級;由呼叫端維護光源旗標)
  NoEffect,      // 是地形法術但目標處無對應地形(無效果)
};

// 套用地形法術。對「面向前方格」(fx,fy)+「當前格」(cx,cy)作用。
//   • Sense Traps:若前方/當前是陷阱格(tile==TT_Trap)→ 標 TF_TrapSensed。
//   • Disarm Trap:若前方/當前是未解除陷阱 → 標 TF_TrapDisarmed。
//   • Soften Stone:若前方是石牆(TT_Stone)未軟化 → 標 TF_SecretBroken(可走)。
// fwd_tile / cur_tile = Level::tile(前方)/(當前)。回傳結果語意。
//   fwd_real_trap / cur_real_trap:前方/當前格是否為「真實陷阱格」(路徑 B 識別,原版
//     座標;見 real_terrain.hpp)。為 true 時等同 0x33 陷阱(供 Sense/Disarm 對真陷阱生效)。
inline TerrainSpellResult apply_terrain_spell(TerrainState& ts, std::uint8_t spell_id,
                                              int area, int fx, int fy,
                                              std::uint8_t fwd_tile, int cx, int cy,
                                              std::uint8_t cur_tile,
                                              bool fwd_real_trap = false,
                                              bool cur_real_trap = false) {
  const bool fwd_trap = (fwd_tile == TT_Trap) || fwd_real_trap;
  const bool cur_trap = (cur_tile == TT_Trap) || cur_real_trap;
  switch (spell_id) {
    case SP_SenseTraps: {
      bool any = false;
      if (fwd_trap) { ts.set(area, fx, fy, TF_TrapSensed); any = true; }
      if (cur_trap) { ts.set(area, cx, cy, TF_TrapSensed); any = true; }
      return any ? TerrainSpellResult::TrapsSensed : TerrainSpellResult::NoEffect;
    }
    case SP_DisarmTrap: {
      // 前方優先,其次當前格。
      if (fwd_trap && !ts.has(area, fx, fy, TF_TrapDisarmed)) {
        ts.set(area, fx, fy, TF_TrapDisarmed);
        return TerrainSpellResult::TrapDisarmed;
      }
      if (cur_trap && !ts.has(area, cx, cy, TF_TrapDisarmed)) {
        ts.set(area, cx, cy, TF_TrapDisarmed);
        return TerrainSpellResult::TrapDisarmed;
      }
      return TerrainSpellResult::NoEffect;
    }
    case SP_SoftenStone: {
      if (fwd_tile == TT_Stone && !ts.has(area, fx, fy, TF_SecretBroken)) {
        ts.set(area, fx, fy, TF_SecretBroken);
        return TerrainSpellResult::StoneSoftened;
      }
      return TerrainSpellResult::NoEffect;
    }
    case SP_CreateWall: {
      // 在面向前方格放石牆障礙(對稱 Soften Stone)。手冊:在你和敵人之間建立一道障礙。
      //   remake 設計:只要前方格本來可走(非牆 fwd_tile!=0)且尚未放牆,即標 TF_WallPlaced。
      //   不可在牆(fwd_tile==0)上放(無意義);保留段門/密門/陷阱格亦不疊放(語意衝突)。
      if (fwd_tile != 0 && !is_terrain_tile(fwd_tile) &&
          !ts.has(area, fx, fy, TF_WallPlaced)) {
        ts.set(area, fx, fy, TF_WallPlaced);
        return TerrainSpellResult::WallCreated;
      }
      return TerrainSpellResult::NoEffect;
    }
    case SP_MageLight:
      // 法師魔光:party 層級光源(無格作用)。本函式僅回報「應點亮」,實際光源旗標由
      //   呼叫端(探索層)維護(對齊 Sense/Soften 之外的 party-wide 狀態)。
      return TerrainSpellResult::LightLit;
    default:
      return TerrainSpellResult::NotTerrain;
  }
}

// 某法術是否為「戰鬥外可用的地形法術」。
inline bool is_terrain_spell(std::uint8_t spell_id) {
  return spell_id == SP_SenseTraps || spell_id == SP_SoftenStone ||
         spell_id == SP_DisarmTrap || spell_id == SP_CreateWall ||
         spell_id == SP_MageLight;
}

// K 互動(對面向前方格)的結果語意。
enum class DoorAction : std::uint8_t {
  None,         // 前方非門/密門/石牆(無動作)
  Opened,       // 關閉的門 → 已開啟
  Unlocked,     // 鎖門 + Lockpick 足夠 → 已開啟
  LockedNeedPick,  // 鎖門但 Lockpick 不足 → 提示「鎖住的」
  SecretBroken, // 密門 → 已粉碎揭示
  StoneBlocked, // 石牆障礙:K 無法破(需 Soften Stone 法術)
  AlreadyOpen,  // 已開/已破,無需再動作
};

}  // namespace dw::game
