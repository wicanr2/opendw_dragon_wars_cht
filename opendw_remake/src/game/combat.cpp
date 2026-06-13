// combat — 結算核心實作。對齊狀態見 combat.hpp 檔頭。
#include "game/combat.hpp"

#include <cstdio>

namespace dw::game {

namespace {
std::uint16_t rd_u16(const std::uint8_t* p) {
  return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}
}  // namespace

std::vector<MonsterRecord> MonsterTable::parse(
    const std::vector<std::uint8_t>& blob) {
  std::vector<MonsterRecord> out;
  // header: magic[6] "DWMON\0", version u16, count u16
  if (blob.size() < 10) return out;
  if (!(blob[0] == 'D' && blob[1] == 'W' && blob[2] == 'M' && blob[3] == 'O' &&
        blob[4] == 'N' && blob[5] == 0)) {
    return out;
  }
  std::uint16_t version = rd_u16(&blob[6]);
  if (version != 1) return out;
  std::uint16_t count = rd_u16(&blob[8]);
  std::size_t off = 10;
  for (std::uint16_t i = 0; i < count; ++i) {
    if (off + 21 + 1 > blob.size()) break;
    MonsterRecord m;
    for (int k = 0; k < 21; ++k) m.attr[k] = blob[off + k];
    off += 21;
    std::uint8_t nlen = blob[off++];
    if (off + nlen > blob.size()) break;
    m.name.assign(reinterpret_cast<const char*>(&blob[off]), nlen);
    off += nlen;
    out.push_back(std::move(m));
  }
  return out;
}

std::vector<MonsterRecord> MonsterTable::load(
    const std::filesystem::path& bundle_dir) {
  const auto path = bundle_dir / "monsters" / "monsters.bin";
  std::FILE* f = std::fopen(path.string().c_str(), "rb");
  if (!f) return {};
  std::fseek(f, 0, SEEK_END);
  long len = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  std::vector<std::uint8_t> blob(len > 0 ? static_cast<std::size_t>(len) : 0);
  if (!blob.empty()) {
    if (std::fread(blob.data(), 1, blob.size(), f) != blob.size()) {
      std::fclose(f);
      return {};
    }
  }
  std::fclose(f);
  return parse(blob);
}

int str_damage_bonus(int strength) {
  // SDA:「STR 愈大傷害愈大」,確切曲線未記。暫用每 4 點 STR +1(類 D&D)。待 DOS 校準。
  return strength / 4;
}

Combatant Combatant::from_player(const CharacterRecord& c) {
  Combatant u;
  u.name = c.name;
  u.is_player = true;
  // HP=Stun(SDA):戰鬥傷害作用於 STUN → 載入 STUN 為耐打值。
  u.hp = c.stun;
  u.max_hp = c.max_stun;
  // AV/DV/AC:依 fraterrisus 欄位 + SDA 公式(stored 為 0 時回退 DEX/4;見 party.cpp)。
  u.av = c.effective_av();
  u.dv = c.effective_dv();
  u.ac = c.effective_ac();
  // 傷害骰:解碼主武器主傷害骰(fraterrisus);無武器 → 徒手回退。
  EquipItem w = c.main_weapon();
  if (w.present && w.primary_dmg.valid()) {
    u.dmg_dice = w.primary_dmg.count;
    u.dmg_sides = w.primary_dmg.sides;
  } else {
    u.dmg_dice = kUnarmedDice;
    u.dmg_sides = kUnarmedSides;
  }
  u.dmg_bonus = str_damage_bonus(c.strength);  // STR 傷害修正
  u.status = c.status;
  return u;
}

Combatant Combatant::from_monster(const MonsterRecord& m) {
  Combatant u;
  u.name = m.name;
  u.is_player = false;
  // ── remake 暫定 stat 對映(非 oracle 真值;見 combat.hpp 註解)──
  // opendw 未解出 21 bytes 逐欄語意。為驅動確定性切片,暫以固定 byte 位置投影。
  // 一旦逆出真實欄位,只需改此函式即可,blob 格式不動(21 bytes 全保留)。
  // 假設(待驗證):attr[0]=HP, attr[1]=attack, attr[2]=defense/AC,
  //               attr[3]=dmg_dice, attr[4]=dmg_sides, attr[5]=dmg_bonus。
  auto nz = [](std::uint8_t v, int dflt) { return v ? static_cast<int>(v) : dflt; };
  u.max_hp = nz(m.attr[0], 4);
  u.hp = u.max_hp;
  u.av = static_cast<int>(m.attr[1]);          // 暫定:命中
  u.dv = static_cast<int>(m.attr[2]);          // 暫定:閃避
  u.ac = static_cast<int>(m.attr[6]);          // 暫定:護甲(0 = 無減傷)
  u.dmg_dice = nz(m.attr[3], 1);
  u.dmg_sides = nz(m.attr[4], 4);
  u.dmg_bonus = static_cast<int>(m.attr[5]);
  u.status = 0;
  return u;
}

AttackResult resolve_attack(Combatant& attacker, Combatant& target,
                            CombatRng& rng) {
  AttackResult r;
  // 命中:2dN 小骰(暫定;待 DOS 校準,見 combat.hpp ToHit 參數)。
  // hit ⇔ roll + attacker.av >= kToHitBase + target.dv。
  // RNG 副作用順序固定:先擲命中,命中才擲傷害 → 可重現。
  r.to_hit_roll = rng.roll(kToHitDiceCount, kToHitDie);
  r.to_hit_need = kToHitBase + target.dv;
  r.hit = (r.to_hit_roll + attacker.av) >= r.to_hit_need;
  if (r.hit) {
    // 傷害:武器主傷害骰 + STR 修正 − 目標 AC(AC 先扣;SDA)。
    int raw_dmg = rng.roll(attacker.dmg_dice, attacker.dmg_sides) +
                  attacker.dmg_bonus;
    r.damage = raw_dmg - target.ac;  // AC 先從物理傷害扣除
    if (r.damage < 0) r.damage = 0;
    // 作用於 STUN(HP=Stun);STUN≤0 → 死亡。
    target.hp -= r.damage;
    if (target.hp <= 0) {
      target.hp = 0;
      target.status |= 0x01;  // dead(對照 player_record 0x4C bit0)
      r.target_died = true;
    }
  }
  r.target_hp_after = target.hp;
  return r;
}

}  // namespace dw::game
