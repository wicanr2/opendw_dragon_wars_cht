// progression — 實作。grounded 來源 vs remake 設計見 progression.hpp 檔頭。
#include "game/progression.hpp"

#include <algorithm>

namespace dw::game {

using namespace progression;

namespace {

// raw 16-bit little-endian 寫入(對拍 chargen wr16 / party rd16)。
void wr16(std::array<std::uint8_t, 512>& r, int off, int v) {
  if (v < 0) v = 0;
  if (v > 0xFFFF) v = 0xFFFF;
  r[off] = static_cast<std::uint8_t>(v & 0xFF);
  r[off + 1] = static_cast<std::uint8_t>((v >> 8) & 0xFF);
}

// 把屬性 cur=max 同步寫回 raw(STR[12,13]/DEX[14,15]/INT[16,17]/SPI[18,19])。
void sync_attr_raw(CharacterRecord& c, int attr_target) {
  switch (attr_target) {
    case kAttrStr: c.raw[0x0C] = c.strength;  c.raw[0x0D] = c.max_strength;  break;
    case kAttrDex: c.raw[0x0E] = c.dexterity; c.raw[0x0F] = c.max_dexterity; break;
    case kAttrInt: c.raw[0x10] = c.intel;     c.raw[0x11] = c.max_intel;     break;
    case kAttrSpi: c.raw[0x12] = c.spirit;    c.raw[0x13] = c.max_spirit;    break;
    default: break;
  }
}

}  // namespace

int xp_threshold_for_next(int current_level) {
  int lv = current_level <= 0 ? 1 : current_level;
  return kXpPerLevelFactor * lv;
}

int available_ap(const CharacterRecord& c) { return c.raw[59]; }

LevelUpResult check_level_up(CharacterRecord& c) {
  LevelUpResult res;
  res.new_level = static_cast<int>(c.level);

  // 可一次連升(若 XP 一次撈很多)。每級消耗門檻、給 AP、長 STR/HP/STUN。
  // 安全護欄:最多連升 100 級(防呆;正常永不觸及)。
  for (int guard = 0; guard < 100; ++guard) {
    int cur_level = static_cast<int>(c.level);
    int need = xp_threshold_for_next(cur_level);
    if (static_cast<int>(c.xp) < need) break;

    // 消耗門檻(XP[80] 為 1 byte;消耗制 → 永落 [0,255],存檔 round-trip 安全)。
    int new_xp = static_cast<int>(c.xp) - need;
    if (new_xp < 0) new_xp = 0;
    c.xp = static_cast<std::uint8_t>(new_xp);
    c.raw[80] = c.xp;

    // 等級 +1([79] 1 byte,夾 255)。
    int new_level = cur_level + 1;
    if (new_level > 255) new_level = 255;
    c.level = static_cast<std::uint16_t>(new_level);
    c.raw[0x4F] = static_cast<std::uint8_t>(new_level);

    // AP +2([59])。
    int ap = available_ap(c) + kApPerLevel;
    if (ap > 255) ap = 255;
    c.raw[59] = static_cast<std::uint8_t>(ap);

    // STR +1(cur=max;[手冊] 每級提高力量值)。夾 in-game 上限。
    int str_gain = 0;
    if (static_cast<int>(c.max_strength) < kAttrMaxInGame) {
      c.strength = static_cast<std::uint8_t>(std::min<int>(c.strength + kStrGainPerLevel, kAttrMaxInGame));
      c.max_strength = static_cast<std::uint8_t>(std::min<int>(c.max_strength + kStrGainPerLevel, kAttrMaxInGame));
      str_gain = kStrGainPerLevel;
      sync_attr_raw(c, kAttrStr);
    }

    // HP/STUN +(2 + STR/4)(cur=max 同加;[手冊] 隨等級提高,公式 remake)。
    int gain = kHpGainBase + static_cast<int>(c.max_strength) / 4;
    int hp_new_max = std::min<int>(static_cast<int>(c.max_health) + gain, 0xFFFF);
    int hp_gain = hp_new_max - static_cast<int>(c.max_health);
    c.max_health = static_cast<std::uint16_t>(hp_new_max);
    c.health = static_cast<std::uint16_t>(std::min<int>(static_cast<int>(c.health) + hp_gain, hp_new_max));
    wr16(c.raw, 0x14, c.health); wr16(c.raw, 0x16, c.max_health);

    int st_new_max = std::min<int>(static_cast<int>(c.max_stun) + gain, 0xFFFF);
    int st_gain = st_new_max - static_cast<int>(c.max_stun);
    c.max_stun = static_cast<std::uint16_t>(st_new_max);
    c.stun = static_cast<std::uint16_t>(std::min<int>(static_cast<int>(c.stun) + st_gain, st_new_max));
    wr16(c.raw, 0x18, c.stun); wr16(c.raw, 0x1A, c.max_stun);

    res.levels_gained += 1;
    res.ap_gained     += kApPerLevel;
    res.hp_gained     += hp_gain;
    res.stun_gained   += st_gain;
    res.str_gained    += str_gain;
    res.new_level      = new_level;
  }
  return res;
}

// ── X 配點 ────────────────────────────────────────────────────────────────
bool spend_ap_on_attr(CharacterRecord& c, int attr_target) {
  if (attr_target < 0 || attr_target >= kAttrTargetCount) return false;
  if (available_ap(c) < 1) return false;
  std::uint8_t* cur = nullptr; std::uint8_t* mx = nullptr;
  switch (attr_target) {
    case kAttrStr: cur = &c.strength;  mx = &c.max_strength;  break;
    case kAttrDex: cur = &c.dexterity; mx = &c.max_dexterity; break;
    case kAttrInt: cur = &c.intel;     mx = &c.max_intel;     break;
    case kAttrSpi: cur = &c.spirit;    mx = &c.max_spirit;    break;
    default: return false;
  }
  if (static_cast<int>(*mx) >= kAttrMaxInGame) return false;  // 已達上限
  *cur = static_cast<std::uint8_t>(*cur + 1);
  *mx  = static_cast<std::uint8_t>(*mx + 1);
  sync_attr_raw(c, attr_target);
  c.raw[59] = static_cast<std::uint8_t>(available_ap(c) - 1);  // 扣 1 AP
  return true;
}

bool spend_ap_on_skill(CharacterRecord& c, int skill_index) {
  if (skill_index < 0 || skill_index >= 27) return false;
  if (available_ap(c) < 1) return false;
  if (static_cast<int>(c.skills[skill_index]) >= kSkillMax) return false;
  c.skills[skill_index] = static_cast<std::uint8_t>(c.skills[skill_index] + 1);
  c.raw[32 + skill_index] = c.skills[skill_index];           // skills[32-58]
  c.raw[59] = static_cast<std::uint8_t>(available_ap(c) - 1);
  return true;
}

// ── 使用物品 U ──────────────────────────────────────────────────────────────
UseItemResult use_item(CharacterRecord& c, int slot) {
  UseItemResult res;
  if (slot < 0 || slot >= CharacterRecord::kInventorySlots) return res;
  ItemInstance it = c.item_at(slot);
  if (!it.present) return res;

  const int base = CharacterRecord::kInventoryBase + slot * CharacterRecord::kItemStride;

  if (it.restores_power()) {
    // 回 Power = magic_lo 數量(夾到 max_power)。
    int amount = it.magic_lo();
    int newp = std::min<int>(static_cast<int>(c.power) + amount, static_cast<int>(c.max_power));
    res.power_restored = newp - static_cast<int>(c.power);
    c.power = static_cast<std::uint16_t>(newp);
    wr16(c.raw, 0x1C, c.power);
    res.restored_power = true;
    res.ok = true;
  } else if (it.teaches_spell()) {
    // 習得法術:設 spells bitfield 對應 bit(spell id 在 magic_lo)。
    int sid = it.magic_lo();
    if (sid >= 0 && sid < 64) {
      int byte_i = sid / 8, bit_i = sid % 8;
      c.spells[byte_i] = static_cast<std::uint8_t>(c.spells[byte_i] | (1 << bit_i));
      c.raw[60 + byte_i] = c.spells[byte_i];                 // spells[60-67]
      res.taught_spell = true; res.taught_spell_id = sid;
      res.ok = true;
    }
  } else if (it.casts_spell()) {
    // Use 時施法:回傳 spell id 供呼叫端實際結算(本模組不重算法術)。
    res.casts_spell = true; res.cast_spell_id = it.cast_spell_id();
    res.ok = true;
  }

  if (!res.ok) return res;  // 無可用效果 → 不消耗

  // 消耗 1 充能;歸 0(或本就無充能的消耗品)→ 整格清空(23B 全 0)。
  // charges 在 bit[03-07](raw header byte 0 的 bit3..7)。
  int charges = it.charges;
  bool consume = (charges <= 1);  // 1 或 0 → 用後移除
  if (consume) {
    for (int i = 0; i < CharacterRecord::kItemStride && base + i < 512; ++i)
      c.raw[base + i] = 0;
    res.consumed = true;
  } else {
    // 扣 1 充能:重寫 bit[03-07]。bit[03-07] 落在 header byte 0 的 bit3..7。
    int newc = charges - 1;
    std::uint8_t b0 = c.raw[base];
    b0 = static_cast<std::uint8_t>((b0 & 0x07) | ((newc & 0x1F) << 3));
    c.raw[base] = b0;
  }
  return res;
}

// ── 裝備穿脫 ──────────────────────────────────────────────────────────────
namespace {
// 翻 / 設第 slot 格的 bit[00](已裝備)。bit[00] = header byte 0 的 bit0。
void set_equipped_bit(CharacterRecord& c, int slot, bool on) {
  const int base = CharacterRecord::kInventoryBase + slot * CharacterRecord::kItemStride;
  if (base >= 512) return;
  if (on) c.raw[base] |= 0x01;
  else    c.raw[base] &= ~0x01;
}
}  // namespace

EquipResult toggle_equip(CharacterRecord& c, int slot) {
  EquipResult res;
  if (slot < 0 || slot >= CharacterRecord::kInventorySlots) return res;
  ItemInstance it = c.item_at(slot);
  if (!it.present) return res;
  res.ok = true;

  bool target_on = !it.equipped;  // 切換
  if (target_on) {
    // 互斥:裝武器時卸其他武器;裝身甲時卸其他身甲(remake 設計)。
    bool this_weapon = is_weapon(it.type);
    bool this_armor  = is_armor(it.type);
    if (this_weapon || this_armor) {
      for (int s = 0; s < CharacterRecord::kInventorySlots; ++s) {
        if (s == slot) continue;
        ItemInstance o = c.item_at(s);
        if (!o.present || !o.equipped) continue;
        if ((this_weapon && is_weapon(o.type)) || (this_armor && is_armor(o.type)))
          set_equipped_bit(c, s, false);
      }
    }
  }
  set_equipped_bit(c, slot, target_on);
  res.now_equipped = target_on;
  return res;
}

// ── 技能檢定 ──────────────────────────────────────────────────────────────
SkillCheckResult skill_check(int skill_level, int difficulty, CombatRng& rng) {
  SkillCheckResult r;
  r.roll = static_cast<int>(rng.below(kSkillCheckDie)) + 1;  // d20:1..20
  r.total = r.roll + skill_level;
  r.success = r.total >= difficulty;
  return r;
}

SkillCheckResult try_lockpick(const CharacterRecord& c, int difficulty, CombatRng& rng) {
  return skill_check(static_cast<int>(c.skills[kLockpick]), difficulty, rng);
}

int bandage(const CharacterRecord& healer, CharacterRecord& target, CombatRng& rng) {
  if (target.status & 0x01) return 0;          // 死亡不可包紮
  int skill = static_cast<int>(healer.skills[kBandage]);
  if (skill <= 0) return 0;                    // 無包紮技能 → 無效
  int range = static_cast<int>(healer.strength) / 4 + 1;  // [1..range]
  int per = rng.roll(1, range);                // 1..range
  int heal = skill * per;
  int newhp = std::min<int>(static_cast<int>(target.health) + heal, static_cast<int>(target.max_health));
  int applied = newhp - static_cast<int>(target.health);
  target.health = static_cast<std::uint16_t>(newhp);
  wr16(target.raw, 0x14, target.health);       // 同步 raw HP cur
  return applied;
}

}  // namespace dw::game
