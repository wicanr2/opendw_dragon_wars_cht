# golden_sweep — 全 40 關第一人稱 viewport 像素對拍代表 golden

每個 `<area>.f<facing>.<x>_<y>.vpmem` 是 golden_pixel.c oracle 跑完整
`refresh_viewport` 後 dump 的 `viewport_memory`(10880 bytes,byte-for-byte)。
這是「廣度掃描」的**入庫代表子集**:每關取 1 個取樣格 × 最多 4 朝向
(oracle 接受的朝向),共 154 檔涵蓋全部 40 關。

## 與 Purgatory 4 朝向 golden 的關係

- `maps/golden/`:原本只測 area 1 (Purgatory) 4 朝向的細節 golden
  (`.golden` / `.sel.golden` / `.vpmem`),`verify_fp` / `verify_fov` /
  `verify_compose` 續用,不受本目錄影響。
- `maps/golden_sweep/`(本目錄):全 40 關廣度掃描的代表 `.vpmem`,由
  `verify_fp_sweep verifycases` 對拍,行使 word_mode / neg_x / neg_x_alt /
  flip_y 等較少見 decode 分支。

## 重生與驗證

```sh
# 完整掃描(每關 N 取樣格 × 4 朝向,golden 即時重生於 tmp/fp_sweep_golden):
tools_build/viewport_compose_golden/sweep_run.sh 3

# 只驗證本入庫子集(讀 golden_sweep/,不需重生):
opendw_remake/assets/bundle/maps/golden_sweep/verify_subset.sh
```

## 已知限制(oracle,非 remake bug)

flag&2(wrap-around)地圖(area 0/18/19/22/27/31/34/35/39 等)在 FOV 取樣
觸及地圖邊界時,golden_pixel.c 的 `check_map_boundary_x/y` 走到原版反組譯
未實作的分支(`engine.c` 印 `unimpl` 後 `exit(1)`)。這些 case 視為
**oracle-declined**,不入庫、不計入 PASS 分母。remake 端對該分支目前是
silent clamp,與原版真實 wrap 行為皆未驗證,留待 opendw 該分支補完後再對拍。

## 結果(最近一次全掃,max_pos=3)

- 40 關全覆蓋,oracle-accepted 430/430 PASS(100%)。
- decode 分支命中:quad=5230 word=264 neg_x=225 neg_x_alt=58 flip_y=328。
- oracle-declined(flag&2 wrap unimpl):50 case。
