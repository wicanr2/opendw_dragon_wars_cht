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

Combatant Combatant::from_player(const CharacterRecord& c) {
  Combatant u;
  u.name = c.name;
  u.is_player = true;
  u.hp = c.health;
  u.max_hp = c.max_health;
  // 命中加值:以等級 + 力量綜合(remake 暫定;原版命中公式藏 bytecode,待逆出)。
  u.attack = static_cast<int>(c.level) + (c.strength / 4);
  // AC:player_record 0x5B armor_class — party.hpp 目前未拆出該欄,以 dexterity 近似。
  u.defense = c.dexterity / 4;
  // 傷害:remake 暫定 1d4 + 力量加值;待逆出武器/傷害骰欄位。
  u.dmg_dice = 1;
  u.dmg_sides = 4;
  u.dmg_bonus = c.strength / 8;
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
  u.attack = static_cast<int>(m.attr[1]);
  u.defense = static_cast<int>(m.attr[2]);
  u.dmg_dice = nz(m.attr[3], 1);
  u.dmg_sides = nz(m.attr[4], 4);
  u.dmg_bonus = static_cast<int>(m.attr[5]);
  u.status = 0;
  return u;
}

AttackResult resolve_attack(Combatant& attacker, Combatant& target,
                            CombatRng& rng) {
  AttackResult r;
  // d20 命中骰(RNG 副作用順序固定:先命中)。
  r.to_hit_roll = 1 + static_cast<int>(rng.below(20));
  r.to_hit_need = 10 + target.defense;
  r.hit = (r.to_hit_roll + attacker.attack) >= r.to_hit_need;
  if (r.hit) {
    r.damage = rng.roll(attacker.dmg_dice, attacker.dmg_sides) +
               attacker.dmg_bonus;
    if (r.damage < 0) r.damage = 0;
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
