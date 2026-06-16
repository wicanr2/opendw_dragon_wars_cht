// verify_party_ops — 次要指令(刪除 / 改名 / 重排 / 物品轉移 / 丟棄)確定性 PASS/FAIL(ctest)。
//
// 全部 deterministic、無外部資產(用 chargen 序列化 + 手工 raw record 建測資)。
// 涵蓋手冊明列次要指令(真值層級:remake 設計 grounded 手冊):
//  1. 改名 R(手冊 147):rename → name 欄與 raw[0..11] 高位元終止編碼同步、round-trip 一致。
//  2. 刪除 D(手冊 147):remove → 隊伍縮短、其餘成員順序與內容不變。
//  3. 重排 O(手冊 / CONTROLS):move → 指定成員移到新位置、其餘順移。
//  4. 物品丟棄(手冊 Item / Discard):discard_item → 該格清空(present=false)、其餘格不變。
//  5. 物品轉移(手冊 Item / Transfer):transfer_item → 整 23B 搬到目標第一個空格、來源清空、
//     裝備位元/名隨之搬移;目標滿則失敗不變。
//  + round-trip:操作後 raw_records → from_raw_records 解析回欄位一致(存檔相容)。
#include "game/chargen.hpp"
#include "game/party.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace dw::game;

namespace {
int g_fail = 0;
void check(bool cond, const char* what) {
  std::printf("  [%s] %s\n", cond ? "PASS" : "FAIL", what);
  if (!cond) g_fail++;
}

std::array<std::uint8_t, 512> make_raw(const char* name) {
  DraftCharacter d;
  d.name = name;
  return d.serialize();
}

// 在 raw 第 slot 格寫一件物品(equipped bit0 + type@byte5 + 名)。對齊 equipment.cpp。
void put_item(std::array<std::uint8_t, 512>& raw, int slot, bool equipped,
              std::uint8_t type, const char* name) {
  const int base = CharacterRecord::kInventoryBase + slot * CharacterRecord::kItemStride;
  for (int i = 0; i < CharacterRecord::kItemStride; ++i) raw[base + i] = 0;
  // byte0:bit0 equipped + bit3-7 charges。固定給 charges=1,確保 header 非全 0 →
  //   parse_item present=true(即使 type=0 的一般物品也算「有物品」)。
  raw[base + 0] = static_cast<std::uint8_t>((equipped ? 0x01 : 0) | (1 << 3));
  raw[base + 5] = static_cast<std::uint8_t>(type & 0x1F);
  int n = 0; while (name[n]) ++n;
  for (int i = 0; i < n && i < 12; ++i) {
    std::uint8_t b = static_cast<std::uint8_t>(name[i] & 0x7F);
    if (i + 1 < n) b |= 0x80;
    raw[base + 11 + i] = b;
  }
}

int present_count(const CharacterRecord& c) {
  int k = 0;
  for (int s = 0; s < CharacterRecord::kInventorySlots; ++s)
    if (c.item_at(s).present) ++k;
  return k;
}
}  // namespace

int main() {
  std::printf("verify_party_ops\n");

  // ── 1. 改名 R ───────────────────────────────────────────────────────────
  {
    Party p = Party::from_raw_records({make_raw("Aaa"), make_raw("Bbb")});
    bool ok = p.rename(0, "Zorro");
    check(ok, "rename(0) returns true");
    check(p.at(0).name == "Zorro", "name field updated");
    check(p.at(1).name == "Bbb", "other member name unchanged");
    // round-trip:raw → 重新解析 → 名一致。
    Party p2 = Party::from_raw_records(p.raw_records());
    check(p2.at(0).name == "Zorro", "rename round-trips through raw[]");
    check(!p.rename(9, "X"), "rename out-of-range -> false");
    check(!p.rename(0, ""), "rename empty -> false");
  }

  // ── 2. 刪除 D ───────────────────────────────────────────────────────────
  {
    Party p = Party::from_raw_records({make_raw("A"), make_raw("B"), make_raw("C")});
    bool ok = p.remove(1);
    check(ok, "remove(1) returns true");
    check(p.size() == 2, "party shrinks to 2");
    check(p.at(0).name == "A" && p.at(1).name == "C", "remaining order A,C");
    check(!p.remove(5), "remove out-of-range -> false");
  }

  // ── 3. 重排 O ───────────────────────────────────────────────────────────
  {
    Party p = Party::from_raw_records({make_raw("A"), make_raw("B"),
                                       make_raw("C"), make_raw("D")});
    // 把第 3 名(D)移到首位 → D,A,B,C
    bool ok = p.move(3, 0);
    check(ok, "move(3,0) returns true");
    check(p.at(0).name == "D" && p.at(1).name == "A" &&
          p.at(2).name == "B" && p.at(3).name == "C", "order becomes D,A,B,C");
    // 再把首位(D)移到末位 → A,B,C,D
    p.move(0, 3);
    check(p.at(0).name == "A" && p.at(3).name == "D", "order restored A,B,C,D");
    check(!p.move(0, 0), "move to same pos -> false");
    check(!p.move(9, 0), "move out-of-range -> false");
  }

  // ── 4. 物品丟棄 ─────────────────────────────────────────────────────────
  {
    auto ra = make_raw("Holder");
    put_item(ra, 0, false, 0x05, "Sword");      // slot0 劍
    put_item(ra, 1, false, 0x00, "Potion");     // slot1 一般
    Party p = Party::from_raw_records({ra});
    check(present_count(p.at(0)) == 2, "holder starts with 2 items");
    bool ok = p.discard_item(0, 0);
    check(ok, "discard slot0 returns true");
    check(!p.at(0).item_at(0).present, "slot0 now empty");
    check(p.at(0).item_at(1).present && p.at(0).item_at(1).name == "Potion",
          "slot1 untouched");
    check(present_count(p.at(0)) == 1, "holder has 1 item left");
    check(!p.discard_item(0, 0), "discard empty slot -> false");
    check(!p.discard_item(0, 99), "discard out-of-range -> false");
  }

  // ── 5. 物品轉移 ─────────────────────────────────────────────────────────
  {
    auto ra = make_raw("Giver");
    put_item(ra, 0, true, 0x05, "Blade");        // equipped 劍
    auto rb = make_raw("Taker");                  // 空背包
    Party p = Party::from_raw_records({ra, rb});
    bool ok = p.transfer_item(0, 0, 1);
    check(ok, "transfer giver slot0 -> taker returns true");
    check(!p.at(0).item_at(0).present, "giver slot0 cleared");
    // 目標第一個空格 = slot0。
    auto moved = p.at(1).item_at(0);
    check(moved.present && moved.name == "Blade", "taker received Blade in slot0");
    check(moved.equipped, "equipped bit preserved on transfer");
    check(present_count(p.at(0)) == 0 && present_count(p.at(1)) == 1,
          "item count moved giver->taker");
    // round-trip 一致。
    Party p2 = Party::from_raw_records(p.raw_records());
    check(p2.at(1).item_at(0).name == "Blade", "transfer round-trips through raw[]");
    // 失敗路徑。
    check(!p.transfer_item(0, 0, 1), "transfer empty source -> false");
    check(!p.transfer_item(1, 0, 1), "transfer to self -> false");
    check(!p.transfer_item(1, 0, 9), "transfer out-of-range -> false");
    // 目標滿:把 Taker 所有可用格塞滿(slot 12 base=512 超出 512B record → 不可用,
    //   與 item_at/transfer_item 的容納守門一致),再從另一人轉 → 失敗。
    auto rc = make_raw("Full");
    for (int s = 0; s < CharacterRecord::kInventorySlots; ++s) {
      int base = CharacterRecord::kInventoryBase + s * CharacterRecord::kItemStride;
      if (base + CharacterRecord::kItemStride > 512) continue;   // 跳過放不下的格
      put_item(rc, s, false, 0x01, "X");
    }
    auto rd = make_raw("Donor");
    put_item(rd, 0, false, 0x00, "Y");
    Party q = Party::from_raw_records({rd, rc});
    check(!q.transfer_item(0, 0, 1), "transfer to full pack -> false");
  }

  std::printf("%s (fails=%d)\n", g_fail ? "FAIL" : "PASS", g_fail);
  return g_fail ? 1 : 0;
}
