// trace — 逐指令執行追蹤,供差異測試對拍 opendw。
#pragma once
#include <cstdint>
#include <cstdio>
#include <vector>

namespace dw::vm {

struct TraceRec {
  std::size_t pc;       // 執行此 opcode 前的 pc
  std::uint8_t op;      // opcode
  std::uint16_t r2, r4, flags;
  std::uint8_t mode;
};

// 收集執行軌跡;之後與 opendw 的同格式輸出 diff(第一個分歧 = remake bug)。
class Trace {
public:
  void record(const TraceRec& r) { recs_.push_back(r); }
  const std::vector<TraceRec>& records() const { return recs_; }
  void dump(std::FILE* f) const {
    for (const auto& r : recs_)
      std::fprintf(f, "%04zx op=%02x r2=%04x r4=%04x fl=%04x m=%02x\n",
                   r.pc, r.op, r.r2, r.r4, r.flags, r.mode);
  }
private:
  std::vector<TraceRec> recs_;
};

}  // namespace dw::vm
