// extract_ui_pieces — 從 DRAGON.COM 萃取遊戲內 UI chrome(石磚邊框 + Dragon Wars
// logo + pillar)的 ui_pieces 資料,寫成自包含 bundle 資源。
//
// Oracle 對齊(唯讀):opendw src/lib/ui.c ui_load() @785、draw_ui_piece() @546。
//   ui_pieces 偏移表位於 com 0x6AE0,共 UI_PIECE_COUNT(0x2B = 43)筆 little-endian
//   16-bit 偏移(86 bytes)。每筆偏移指向該片的 4-byte header,之後緊接 nibble bitmap:
//     header: width(u8) height(u8) offset_delta(u8) y_pos(u8)
//     data:   width*height bytes,每 byte = 2 像素(hi nibble = 左像素,lo = 右像素;
//             各 nibble = DOS 16 色盤索引)。draw 時 x 起點 = offset_delta*4,
//             y 起點 = y_pos,每列 framebuffer stride = 320(每片自帶 (x,y) 定位)。
//
// 重要:本 remake 使用的 DRAGON.COM = v1.1(56,673 bytes,md5 3aa427d4…)。
//   v1.0(55,217 bytes)在 com 0x6758 / 0x6AE0 的版面不同(0x6AE0 已是像素資料而非
//   偏移表),既有 viewport/minimap bundle(vp0.bin…)也是用 v1.1 對拍 byte-for-byte。
//   故 ui_pieces 同樣須自 v1.1 萃取,才與既有 viewport 資產同源。
//   (extract 會校驗:第一筆偏移必須 == 表尾 0x6B36,否則判定版本不符並 abort。)
//
// 用法:extract_ui_pieces <dragon.com> <out_dir>
//   產出:<out_dir>/viewport/ui_pieces.bin   (固定格式,見下)
//        <out_dir>/viewport/ui_pieces.manifest.json
//
// ui_pieces.bin 格式(little-endian,自包含,執行期不需 DRAGON.COM):
//   magic    "DWUIP\0"  (6 bytes)
//   version  u16 = 1
//   count    u16  (= 43)
//   repeated count 次:
//     width        u8
//     height       u8
//     offset_delta u8   (x 起點 = offset_delta*4 像素)
//     y_pos        u8   (y 起點掃描線)
//     data_len     u16  (= width*height)
//     data[data_len]    nibble bitmap,byte-for-byte 同 DRAGON.COM
//
// 本格式把每片 header(含定位)與 nibble 資料一起保存;data 段對拍 DRAGON.COM
// 對應 byte range 應 byte-for-byte 相等(manifest 記每片 com offset 供核對)。

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace {

constexpr std::size_t kComOrg = 0x100;       // DOS COM 載入起點;檔案 offset = com − 0x100
constexpr std::size_t kTableCom = 0x6AE0;    // ui_pieces 偏移表 com 位址
constexpr std::size_t kCount = 0x2B;         // UI_PIECE_COUNT = 43
constexpr std::size_t kTableEndCom = kTableCom + kCount * 2;  // = 0x6B36(= 第一筆偏移)

void put_u16(std::vector<std::uint8_t>& v, std::uint16_t x) {
  v.push_back(static_cast<std::uint8_t>(x & 0xFF));
  v.push_back(static_cast<std::uint8_t>((x >> 8) & 0xFF));
}

std::vector<std::uint8_t> read_all(const std::filesystem::path& p) {
  std::vector<std::uint8_t> buf;
  std::FILE* f = std::fopen(p.string().c_str(), "rb");
  if (!f) return buf;
  std::fseek(f, 0, SEEK_END);
  long n = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  if (n > 0) {
    buf.resize(static_cast<std::size_t>(n));
    if (std::fread(buf.data(), 1, buf.size(), f) != buf.size()) buf.clear();
  }
  std::fclose(f);
  return buf;
}

// com_extract 等價:從 COM 影像取 [com_off−0x100, +sz)。回傳是否成功。
bool com_slice(const std::vector<std::uint8_t>& com, std::size_t com_off,
               std::size_t sz, const std::uint8_t** out) {
  if (com_off < kComOrg) return false;
  std::size_t fo = com_off - kComOrg;
  if (fo + sz > com.size()) return false;
  *out = com.data() + fo;
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 3) {
    std::fprintf(stderr, "usage: %s <dragon.com> <out_dir>\n", argv[0]);
    return 2;
  }
  const std::filesystem::path com_path = argv[1];
  const std::filesystem::path out_dir = argv[2];

  std::vector<std::uint8_t> com = read_all(com_path);
  if (com.empty()) {
    std::fprintf(stderr, "extract_ui_pieces: cannot read %s\n", argv[1]);
    return 1;
  }

  // 偏移表(43 * 2 bytes)。
  const std::uint8_t* tbl = nullptr;
  if (!com_slice(com, kTableCom, kCount * 2, &tbl)) {
    std::fprintf(stderr, "extract_ui_pieces: offset table OOB (com too small?)\n");
    return 1;
  }
  std::vector<std::uint16_t> offs(kCount);
  for (std::size_t i = 0; i < kCount; ++i) {
    offs[i] = static_cast<std::uint16_t>(tbl[i * 2] | (tbl[i * 2 + 1] << 8));
  }

  // 版本校驗:v1.1 的第一筆偏移恰好 == 表尾(資料緊接表後)。
  // v1.0 此處是 0x9999(像素資料),會在這裡被擋下。
  if (offs[0] != kTableEndCom) {
    std::fprintf(stderr,
                 "extract_ui_pieces: offset[0]=0x%04X != table_end 0x%04zX.\n"
                 "  Wrong DRAGON.COM version? Need v1.1 (56673 bytes, "
                 "md5 3aa427d46a36985d09533efbd36010c0).\n",
                 offs[0], kTableEndCom);
    return 1;
  }

  struct Piece {
    std::uint8_t w, h, dx, y;
    std::uint16_t com_off;       // header 在 COM 的 com 位址
    const std::uint8_t* data;    // nibble bitmap 指標(指進 com)
    std::size_t data_len;
  };
  std::vector<Piece> pieces(kCount);
  for (std::size_t i = 0; i < kCount; ++i) {
    const std::uint8_t* hdr = nullptr;
    if (!com_slice(com, offs[i], 4, &hdr)) {
      std::fprintf(stderr, "extract_ui_pieces: piece %zu header OOB @0x%04X\n", i,
                   offs[i]);
      return 1;
    }
    Piece& p = pieces[i];
    p.w = hdr[0];
    p.h = hdr[1];
    p.dx = hdr[2];
    p.y = hdr[3];
    p.com_off = offs[i];
    p.data_len = static_cast<std::size_t>(p.w) * p.h;
    if (p.data_len > 0) {
      if (!com_slice(com, offs[i] + 4, p.data_len, &p.data)) {
        std::fprintf(stderr, "extract_ui_pieces: piece %zu data OOB @0x%04X sz=%zu\n",
                     i, offs[i] + 4, p.data_len);
        return 1;
      }
    } else {
      p.data = nullptr;
    }
  }

  // 組 blob。
  std::vector<std::uint8_t> blob;
  const char magic[6] = {'D', 'W', 'U', 'I', 'P', '\0'};
  blob.insert(blob.end(), magic, magic + 6);
  put_u16(blob, 1);
  put_u16(blob, static_cast<std::uint16_t>(kCount));
  for (const Piece& p : pieces) {
    blob.push_back(p.w);
    blob.push_back(p.h);
    blob.push_back(p.dx);
    blob.push_back(p.y);
    put_u16(blob, static_cast<std::uint16_t>(p.data_len));
    if (p.data_len > 0) blob.insert(blob.end(), p.data, p.data + p.data_len);
  }

  std::error_code ec;
  std::filesystem::create_directories(out_dir / "viewport", ec);
  const auto bin_path = out_dir / "viewport" / "ui_pieces.bin";
  if (std::FILE* f = std::fopen(bin_path.string().c_str(), "wb")) {
    std::fwrite(blob.data(), 1, blob.size(), f);
    std::fclose(f);
  } else {
    std::fprintf(stderr, "extract_ui_pieces: cannot write %s\n",
                 bin_path.string().c_str());
    return 1;
  }

  // manifest(人讀;含逐片 geometry + com offset,供 byte-for-byte 核對)。
  std::string mf = "{\n  \"format\": \"opendw-ui_pieces/1\",\n";
  mf += "  \"source\": \"DRAGON.COM v1.1 (56673 bytes) com 0x6AE0 (UI_PIECE_COUNT=0x2B)\",\n";
  mf += "  \"oracle\": \"opendw src/lib/ui.c ui_load()@785 + draw_ui_piece()@546\",\n";
  mf += "  \"nibble_format\": \"byte=2px (hi=left,lo=right); palette=DOS16; x=dx*4; y=y_pos; stride=320\",\n";
  mf += "  \"count\": " + std::to_string(kCount) + ",\n";
  mf += "  \"pieces\": [\n";
  for (std::size_t i = 0; i < pieces.size(); ++i) {
    const Piece& p = pieces[i];
    char buf[256];
    std::snprintf(buf, sizeof(buf),
                  "    { \"index\": %zu, \"com_off\": \"0x%04X\", \"w\": %u, \"h\": %u, "
                  "\"dx\": %u, \"x_px\": %u, \"y\": %u, \"data_len\": %zu }",
                  i, p.com_off, p.w, p.h, p.dx, p.dx * 4u, p.y, p.data_len);
    mf += buf;
    mf += (i + 1 < pieces.size()) ? ",\n" : "\n";
  }
  mf += "  ]\n}\n";
  const auto mf_path = out_dir / "viewport" / "ui_pieces.manifest.json";
  if (std::FILE* f = std::fopen(mf_path.string().c_str(), "wb")) {
    std::fwrite(mf.data(), 1, mf.size(), f);
    std::fclose(f);
  }

  std::printf("extract_ui_pieces: wrote %zu pieces -> %s (%zu bytes)\n", kCount,
              bin_path.string().c_str(), blob.size());
  for (std::size_t i = 0; i < pieces.size(); ++i) {
    const Piece& p = pieces[i];
    std::printf("  %2zu  com=0x%04X  %3ux%-3u  x=%3u y=%3u  len=%zu\n", i, p.com_off,
                p.w, p.h, p.dx * 4u, p.y, p.data_len);
  }
  return 0;
}
