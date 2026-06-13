// verify_fp_sweep — 全 40 關第一人稱 viewport 像素對拍「廣度掃描」。
//
// 從只測 Purgatory 4 朝向擴展為:對每關掃出若干可走格 (tile==1) × 4 朝向,
// 跑 remake render_first_person 產生 viewport_memory (10880B),memcmp 對拍
// 由 golden_pixel.c 產生的 .vpmem golden,統計 PASS/FAIL 與 decode 分支覆蓋。
//
// 位置挑選必須在「本工具」與「oracle 驅動腳本 (sweep_run.sh)」之間完全一致,
// 故本工具提供 `list` 模式輸出 case 清單 (facing x y),由腳本餵給 golden_pixel.c。
//
// case 命名慣例:<base>.f<facing>.<x>_<y>.vpmem (沿用 verify_fp)。
//
// 用法:
//   verify_fp_sweep list   <level.lvl> <max_pos>
//       → 印出該關的 case 清單 (每行 "facing x y"),供 oracle 生成 golden。
//   verify_fp_sweep verify <level.lvl> <max_pos> <golden_dir> <components_dir>
//       → 對每 case 跑 remake + 對拍 golden,輸出 PASS/FAIL + 分支統計。
//
// 位置挑選 (pick_positions):掃整張地圖的 tile==1 可走格,依固定步長均勻取樣,
// 盡量涵蓋面牆 / 面通道 / 不同區域,deterministic (同一關每次結果相同)。
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "render/viewport.hpp"
#include "render/viewport_compose.hpp"
#include "resource/level.hpp"

namespace {

struct Pos {
  int x, y;
};

bool read_file(const std::string& path, std::vector<std::uint8_t>& out) {
  std::FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) return false;
  std::fseek(f, 0, SEEK_END);
  long sz = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  if (sz <= 0) {
    std::fclose(f);
    return false;
  }
  out.resize((std::size_t)sz);
  bool ok = std::fread(out.data(), 1, (std::size_t)sz, f) == (std::size_t)sz;
  std::fclose(f);
  return ok;
}

// 等步長 deterministic 取樣 (含頭尾),並去重。
std::vector<Pos> spread_sample(const std::vector<Pos>& pool, int n) {
  std::vector<Pos> out;
  if (pool.empty() || n <= 0) return out;
  if ((int)pool.size() <= n) return pool;
  const long denom = (n - 1 > 0) ? (n - 1) : 1;
  for (int i = 0; i < n; ++i) {
    long idx = (long)i * (long)(pool.size() - 1) / denom;
    out.push_back(pool[(std::size_t)idx]);
  }
  return out;
}

// 從一關地圖挑選 max_pos 個取樣格 × 4 朝向,deterministic 均勻取樣。
//
// 觀察:Level::tile()==1 在多數關卡不是可靠的「可走」predicate
// (.lvl tile 位元語意尚在校正,見 maps/README.md);Dilmun / Depths of Nisir /
// Tars Ruins 等關 tile==1 僅 0~1 格。為了「全 40 關都有足夠廣度、涵蓋
// 面牆 / 面通道 / 不同區域」,挑選策略:
//   1. 優先取 tile==1 的格 (語意上的地面)。
//   2. 不足 max_pos 時,以 tile!=0 (任何非 void 格) 的均勻取樣補足。
//   3. 仍不足,再以全部 in-bounds 格補足 (保證每關都有 case)。
// 每層均做等步長取樣以涵蓋地圖各角落;像素對拍只要求 oracle 與 remake 在
// 同一 (x,y) 算出一致 viewport_memory,座標本身不必真的可走。
std::vector<Pos> pick_positions(const dw::res::Level& lvl, int max_pos) {
  std::vector<Pos> walk, nonzero, anyc;
  for (int y = 0; y < lvl.h; ++y) {
    for (int x = 0; x < lvl.w; ++x) {
      std::uint8_t t = lvl.tile(x, y);
      anyc.push_back({x, y});
      if (t != 0) nonzero.push_back({x, y});
      if (t == 1) walk.push_back({x, y});
    }
  }
  std::vector<Pos> out = spread_sample(walk, max_pos);
  auto add_unique = [&](const std::vector<Pos>& pool) {
    if ((int)out.size() >= max_pos) return;
    for (const auto& p : spread_sample(pool, max_pos)) {
      bool dup = false;
      for (const auto& q : out)
        if (q.x == p.x && q.y == p.y) { dup = true; break; }
      if (!dup) out.push_back(p);
      if ((int)out.size() >= max_pos) break;
    }
  };
  add_unique(nonzero);
  add_unique(anyc);
  return out;
}

std::string basename_noext(const std::string& path) {
  std::string base = path;
  auto slash = base.find_last_of('/');
  if (slash != std::string::npos) base = base.substr(slash + 1);
  auto dot = base.find_last_of('.');
  if (dot != std::string::npos) base = base.substr(0, dot);
  return base;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 4) {
    std::fprintf(stderr,
                 "usage:\n"
                 "  %s list   <level.lvl> <max_pos>\n"
                 "  %s verify <level.lvl> <max_pos> <golden_dir> <components_dir>\n",
                 argv[0], argv[0]);
    return 2;
  }
  const std::string mode = argv[1];
  const std::string lvl_path = argv[2];
  const int max_pos = std::atoi(argv[3]);

  auto lvl = dw::res::Level::load_file(lvl_path);
  if (!lvl) {
    std::fprintf(stderr, "load level failed: %s\n", lvl_path.c_str());
    return 1;
  }

  std::vector<Pos> positions = pick_positions(*lvl, max_pos);

  if (mode == "list") {
    // 每行 "facing x y";4 朝向。
    for (const auto& p : positions)
      for (int f = 0; f < 4; ++f) std::printf("%d %d %d\n", f, p.x, p.y);
    return 0;
  }

  // 待對拍的 case 清單。verify 模式 = pick_positions × 4 朝向;
  // verifycases 模式 = 從 casefile 讀 (facing x y),供驅動腳本排除
  // oracle 無法生成 golden 的 case (flag&2 wrap 邊界未實作)。
  struct VCase { int facing, x, y; };
  std::vector<VCase> vcases;
  std::string gdir, cdir;

  if (mode == "verify") {
    if (argc < 6) {
      std::fprintf(stderr, "verify needs <golden_dir> <components_dir>\n");
      return 2;
    }
    gdir = argv[4];
    cdir = argv[5];
    for (const auto& p : positions)
      for (int f = 0; f < 4; ++f) vcases.push_back({f, p.x, p.y});
  } else if (mode == "verifycases") {
    // verifycases <level.lvl> <max_pos(ignored)> <golden_dir> <components_dir> <casefile>
    if (argc < 7) {
      std::fprintf(stderr,
                   "verifycases needs <golden_dir> <components_dir> <casefile>\n");
      return 2;
    }
    gdir = argv[4];
    cdir = argv[5];
    std::FILE* cf = std::fopen(argv[6], "r");
    if (!cf) {
      std::fprintf(stderr, "open casefile failed: %s\n", argv[6]);
      return 2;
    }
    int f, x, y;
    while (std::fscanf(cf, "%d %d %d", &f, &x, &y) == 3) vcases.push_back({f, x, y});
    std::fclose(cf);
  } else {
    std::fprintf(stderr, "unknown mode: %s\n", mode.c_str());
    return 2;
  }

  const std::string base = basename_noext(lvl_path);
  dw::render::ComponentStore comps(cdir);

  int fails = 0, total = 0, missing_gold = 0, missing_comp = 0;
  // 分支覆蓋率 (本關)。
  dw::render::decode_branch_counters().reset();

  {
    for (const auto& vc : vcases) {
      const int facing = vc.facing;
      const Pos p{vc.x, vc.y};
      ++total;
      char gpath[1024];
      std::snprintf(gpath, sizeof(gpath), "%s/%s.f%d.%d_%d.vpmem", gdir.c_str(),
                    base.c_str(), facing, p.x, p.y);
      std::vector<std::uint8_t> gold;
      if (!read_file(gpath, gold)) {
        std::printf("[%s f%d (%d,%d)] FAIL — 無法讀 golden %s\n", base.c_str(),
                    facing, p.x, p.y, gpath);
        ++fails;
        ++missing_gold;
        continue;
      }

      dw::render::ViewportDecoder dec;
      dw::render::render_first_person(*lvl, p.x, p.y, facing, dec, comps);

      if (gold.size() != dec.mem.size()) {
        std::printf("[%s f%d (%d,%d)] FAIL — size mismatch gold=%zu got=%zu\n",
                    base.c_str(), facing, p.x, p.y, gold.size(), dec.mem.size());
        ++fails;
        continue;
      }

      int first_diff = -1, ndiff = 0;
      for (std::size_t i = 0; i < gold.size(); ++i) {
        if (gold[i] != dec.mem[i]) {
          if (first_diff < 0) first_diff = (int)i;
          ++ndiff;
        }
      }
      if (ndiff != 0) {
        ++fails;
        std::printf(
            "[%s f%d (%d,%d)] FAIL — %d/%zu bytes differ, first@%d gold=%02X got=%02X\n",
            base.c_str(), facing, p.x, p.y, ndiff, gold.size(), first_diff,
            gold[first_diff], dec.mem[first_diff]);
      }
    }
  }

  const auto& bc = dw::render::decode_branch_counters();
  std::printf(
      "[%s] %d/%d PASS  (cases=%d positions=%zu missing_gold=%d)  "
      "branches: quad=%ld word=%ld neg_x=%ld neg_x_alt=%ld flip_y=%ld\n",
      base.c_str(), total - fails, total, total, positions.size(), missing_gold,
      bc.process_quadrant, bc.word_mode, bc.neg_x, bc.neg_x_alt, bc.flip_y);
  (void)missing_comp;
  return fails == 0 ? 0 : 1;
}
