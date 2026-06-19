#!/usr/bin/env python3
# gen_shop_stock.py — 產生商店庫存 assets/bundle/shop/stock.json(自包含,執行期不需 DATA1)。
#
# 庫存來源(誠實標示):
#   • grounded(fraterrisus / DATA1):從 bundle/items/items.bin 取「可販售」(price>0)的
#     真實物品(目前僅 Dragon Stone price=250)。標 "grounded": true。
#   • curated(remake 設計):標準裝備(劍/盾/各甲),以 fraterrisus 23B 物品欄 bit 佈局
#     (docs/reverse-engineering/44 §2)逐位元編碼,價格取攻略 docs/walkthrough/38 量級的合理值。標 "grounded": false。
#     商店買賣邏輯 opendw C 未實作,故 curated 部分為 remake 設計,非原版 byte-for-byte。
#
# 用法:python3 gen_shop_stock.py <bundle_dir>
#   讀 <bundle_dir>/items/items.bin,寫 <bundle_dir>/shop/stock.json。
import json
import os
import struct
import sys

# ── 23B 物品欄 bit 編碼(fraterrisus,docs/reverse-engineering/44 §2)──────────────────────────────
# 11 byte header(88 bit,LSB-first across bytes)+ 12 byte 名(高位元終止)。
ITEM_TYPE = {
    "general": 0x00, "shield": 0x01, "full_shield": 0x02, "axe": 0x03,
    "flail": 0x04, "sword": 0x05, "two_hander": 0x06, "mace": 0x07,
    "bow": 0x08, "crossbow": 0x09, "cuir_bouilli": 0x10, "brigandine": 0x11,
    "scale": 0x12, "chain": 0x13, "plate_chain": 0x14, "full_plate": 0x15,
    "helmet": 0x16, "boots": 0x18,
}
# 傷害骰高 3 bit = 骰面(d4..d100),低 3 bit = 骰數-1。
DIE_SIDES = {4: 0, 6: 1, 8: 2, 10: 3, 12: 4, 20: 5, 30: 6, 100: 7}


def set_bits(hdr, lo, hi, val):
    for b in range(lo, hi + 1):
        if val & (1 << (b - lo)):
            hdr[b >> 3] |= (1 << (b & 7))


def encode_price(value):
    """購買價 → bit[32-39] 編碼(指數 3b + 尾數 5b,M×10^E)。取能整除的最緊編碼。"""
    if value <= 0:
        return 0
    for e in range(0, 8):
        p = 10 ** e
        if value % p == 0:
            m = value // p
            if m <= 31:
                return ((e & 7) << 5) | (m & 0x1F)
    # 無法精確編碼 → 退而求其次(尾數夾 31)。
    return 0xFF if value >= 310000000 else (m & 0x1F)


def damage_byte(sides, count):
    return ((DIE_SIDES[sides] & 7) << 5) | ((count - 1) & 7)


def encode_item(name, itype, price, av_mod=0, ac_mod=0, dmg=None):
    hdr = [0] * 11
    # bit[40-44] 低 5 bit 物品類型
    set_bits(hdr, 40, 44, ITEM_TYPE[itype])
    # bit[32-39] 購買價
    set_bits(hdr, 32, 39, encode_price(price))
    # AV 修正:bit[08]=負號,bit[24-27]=值
    if av_mod < 0:
        set_bits(hdr, 8, 8, 1)
    set_bits(hdr, 24, 27, abs(av_mod) & 0xF)
    # AC 修正 bit[28-31]
    set_bits(hdr, 28, 31, ac_mod & 0xF)
    # 主傷害骰 bit[64-71]
    if dmg:
        set_bits(hdr, 64, 71, damage_byte(dmg[1], dmg[0]))
    # 12B 名(高位元終止)
    nm = name[:11]
    namebytes = []
    for i, ch in enumerate(nm):
        b = ord(ch) & 0x7F
        if i < len(nm) - 1:
            b |= 0x80
        namebytes.append(b)
    namebytes += [0] * (12 - len(namebytes))
    rec = bytes(hdr) + bytes(namebytes[:12])
    assert len(rec) == 23, len(rec)
    return rec


def grounded_sellable(bundle):
    """從 items.bin 取 price>0 的真實物品(grounded)。"""
    path = os.path.join(bundle, "items", "items.bin")
    out = []
    if not os.path.exists(path):
        return out
    d = open(path, "rb").read()
    if d[:6] != b"DWITM\0":
        return out
    _ver, cnt = struct.unpack("<HH", d[6:10])
    off = 10

    def bf(q, lo, hi):
        v = 0
        for b in range(lo, hi + 1):
            if q[b >> 3] & (1 << (b & 7)):
                v |= 1 << (b - lo)
        return v

    def price(enc):
        e = (enc >> 5) & 7
        m = enc & 0x1F
        v = m
        for _ in range(e):
            v *= 10
        return v

    def name(rec):
        s = ""
        for i in range(11, 23):
            b = rec[i]
            c = b & 0x7F
            if 0x20 <= c < 0x7F:
                s += chr(c)
            if not (b & 0x80):
                break
        return s

    for _ in range(cnt):
        off += 4  # data1_off
        rec = d[off:off + 23]
        off += 23
        enc = bf(rec, 32, 39)
        if price(enc) > 0:
            out.append((name(rec), rec))
    return out


def main():
    bundle = sys.argv[1] if len(sys.argv) > 1 else "assets/bundle"
    entries = []

    # 1) grounded:items.bin 內可販售真實物品。
    for nm, rec in grounded_sellable(bundle):
        # name_key 用物品自身英文名(i18n items.tsv 可補譯)。
        entries.append({"hex": rec.hex(), "name_key": nm, "grounded": True})

    # 2) curated:標準裝備(remake 設計;價格量級取攻略 docs/walkthrough/38)。
    #    name_key 用 i18n 鍵(shop_item_*),items.tsv 補繁中。
    curated = [
        # (name_key, 英文名, type, price, av_mod, ac_mod, dmg=(count,sides))
        ("shop_dagger",       "Dagger",        "sword",  20,   0, 0, (1, 4)),
        ("shop_short_sword",  "Short Sword",   "sword",  60,   0, 0, (1, 6)),
        ("shop_long_sword",   "Long Sword",    "sword",  150,  0, 0, (1, 8)),
        ("shop_battle_axe",   "Battle Axe",    "axe",    200, -1, 0, (1, 10)),
        ("shop_war_hammer",   "War Hammer",    "mace",   180,  0, 0, (1, 8)),
        ("shop_buckler",      "Buckler",       "shield", 40,   0, 1, None),
        ("shop_kite_shield",  "Kite Shield",   "shield", 120, -1, 2, None),
        ("shop_leather_armor","Leather Armor", "cuir_bouilli", 50, 0, 1, None),
        ("shop_chain_mail",   "Chain Mail",    "chain",  300, -2, 3, None),
        ("shop_helmet",       "Iron Helmet",   "helmet", 30,   0, 1, None),
    ]
    for name_key, en, itype, price, av, ac, dmg in curated:
        rec = encode_item(en, itype, price, av_mod=av, ac_mod=ac, dmg=dmg)
        entries.append({"hex": rec.hex(), "name_key": name_key, "grounded": False})

    doc = {
        "format": "opendw-shop/1",
        "source": "grounded items from bundle/items/items.bin (fraterrisus/DATA1) + curated standard equipment (remake design)",
        "spec": "docs/reverse-engineering/44_DATA_FORMATS_AND_MECHANICS.md §2 (fraterrisus Equipment Format); buy/sell logic = remake design (opendw C 未實作商店)",
        "slot_bytes": 23,
        "note": "grounded=true: real DATA1 item byte-for-byte. grounded=false: remake curated equipment with cleanly encoded prices (M x 10^E).",
        "count": len(entries),
        "items": entries,
    }
    out_dir = os.path.join(bundle, "shop")
    os.makedirs(out_dir, exist_ok=True)
    out_path = os.path.join(out_dir, "stock.json")
    with open(out_path, "w") as f:
        json.dump(doc, f, indent=2, ensure_ascii=False)
        f.write("\n")
    print(f"gen_shop_stock: wrote {len(entries)} entries -> {out_path}")


if __name__ == "__main__":
    main()
