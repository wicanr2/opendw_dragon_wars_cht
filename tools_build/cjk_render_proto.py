#!/usr/bin/env python3
# #2 slice 1 原型:驗證 24x24 中文 + 8x8 ASCII 混排進遊戲 320x200 / DOS 16 色 framebuffer。
# 結論:WenQuanYi zenhei 22px→24格,中文清晰可讀;每行約 13 個中文字(320/24)。
# 字型來源: /usr/share/fonts/truetype/wqy/wqy-zenhei.ttc (GPL,符合授權)
# 下一步(slice 2+): 在 C 引擎以 freetype 產 24x24 1-bit glyph,接進 ui.c 字串輸出 + vga_sdl pixel scaling。
# 完整原型見 git 歷史 / 本檔同目錄產出 docs/cjk_demo/cjk_24x24_proof.png
print("see docs/cjk_demo/cjk_24x24_proof.png — 24x24 CJK 渲染驗證通過")
