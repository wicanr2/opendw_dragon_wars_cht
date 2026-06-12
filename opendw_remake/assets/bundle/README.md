# OpenDW Remake — Asset Bundle

remake 執行期讀此 bundle,**不需原始 DATA1 / DATA2 / dragon.com**(字型 chr_table 仍取自 dragon.com,後續可一併入 bundle)。
由 `tools_build/build_bundle.sh` 一鍵從 DATA1/DATA2 萃取(對拍驗證,見 docs/adr/0001)。

| 目錄 | 內容 | 編輯方式 |
|------|------|----------|
| `scripts/N.bin` | 解壓後 script bytecode(來源 DATA1) | VM 直接執行 |
| `strings/N.tsv` | 內嵌對話(offset→英文) | 改 TSV 即翻譯;遊戲內經 i18n 出中文 |
| `sprites/*.spr` | 執行期 indexed sprite(來源 DATA1 **與 DATA2**) | 改對應 `.png` → 轉回 `.spr` |
| `sprites/manifest.json` | sprite ID→檔案/平台/尺寸 | 換檔即換圖(未來 X68000/PC-9801) |

驗證:`scripts` 與 DATA1 byte-for-byte 一致;DATA2-only sprite(如 152 guard)在無 data1/data2 環境可從 bundle 渲染 → 已脫離兩者。
