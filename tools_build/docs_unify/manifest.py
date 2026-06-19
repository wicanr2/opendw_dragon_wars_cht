# -*- coding: utf-8 -*-
"""docs 統一搬移映射表。

old (相對 repo root) -> new (相對 repo root)。
連結重寫器用這份表把舊路徑映射到新路徑。
"""

# 目錄級搬移:old_prefix -> new_prefix (整棵子樹,保留內部結構)
DIR_MOVES = {
    # ---- 根 docs 資產目錄 ----
    "docs/softworld_images": "docs/walkthrough/softworld_images",
    "docs/scene_pictures": "docs/walkthrough/scene_pictures",
    "docs/chinese_manual_images": "docs/manual/chinese_manual_images",
    "docs/en_manual_images": "docs/manual/en_manual_images",
    "docs/chargen_screens": "docs/media/chargen_screens",
    "docs/combat_screens": "docs/media/combat_screens",
    "docs/dos_playtest": "docs/media/dos_playtest",
    "docs/layout_compare": "docs/media/layout_compare",
    "docs/monster_sprites": "docs/media/monster_sprites",
    "docs/cjk_demo": "docs/media/cjk_demo",
    "docs/ui_chrome_demo": "docs/media/ui_chrome_demo",
    "docs/assessment": "docs/assessment/pm_scale_screens",  # 5 張 scale 比較圖
    # ---- remake docs 子樹 ----
    "opendw_remake/docs/media": "docs/media/remake",
    "opendw_remake/docs/audit/dos_compare": "docs/assessment/dos_compare",
}

# 單檔搬移:old_path -> new_path
FILE_MOVES = {
    # ===== reverse-engineering =====
    "docs/01_PLAN.md": "docs/reverse-engineering/01_PLAN.md",
    "docs/02_ANALYSIS.md": "docs/reverse-engineering/02_ANALYSIS.md",
    "docs/03_REVIEW_REPORT.md": "docs/reverse-engineering/03_REVIEW_REPORT.md",
    "docs/21_DATA1_RESOURCE_INDEX.md": "docs/reverse-engineering/21_DATA1_RESOURCE_INDEX.md",
    "docs/22_DATA1_SECTION_DETAILS.md": "docs/reverse-engineering/22_DATA1_SECTION_DETAILS.md",
    "docs/23_SOURCE_CODE_MAP.md": "docs/reverse-engineering/23_SOURCE_CODE_MAP.md",
    "docs/24_SCRIPT_TEXT_MAPPING.md": "docs/reverse-engineering/24_SCRIPT_TEXT_MAPPING.md",
    "docs/25_OPCODE_INTERPRETATION.md": "docs/reverse-engineering/25_OPCODE_INTERPRETATION.md",
    "docs/26_MONSTERS_AND_SPRITES.md": "docs/reverse-engineering/26_MONSTERS_AND_SPRITES.md",
    "docs/40_ORIGINAL_DOCS_SUMMARY.md": "docs/reverse-engineering/40_ORIGINAL_DOCS_SUMMARY.md",
    "docs/42_COMBAT_BYTECODE.md": "docs/reverse-engineering/42_COMBAT_BYTECODE.md",
    "docs/42_WHY_DATA1_DATA2.md": "docs/reverse-engineering/42_WHY_DATA1_DATA2.md",
    "docs/43_DOS_PLAYTEST.md": "docs/reverse-engineering/43_DOS_PLAYTEST.md",
    "docs/44_DATA_FORMATS_AND_MECHANICS.md": "docs/reverse-engineering/44_DATA_FORMATS_AND_MECHANICS.md",
    "docs/46_PC98_JA_EXTRACTION.md": "docs/reverse-engineering/46_PC98_JA_EXTRACTION.md",
    "docs/51_WORLDMAP_AREA_SWITCH_RE.md": "docs/reverse-engineering/51_WORLDMAP_AREA_SWITCH_RE.md",
    "docs/52_MAINLINE_EVENT_STRINGS.md": "docs/reverse-engineering/52_MAINLINE_EVENT_STRINGS.md",
    "docs/OPCODE_REFERENCE.md": "docs/reverse-engineering/OPCODE_REFERENCE.md",
    "docs/dragon.asm": "docs/reverse-engineering/dragon.asm",
    "docs/ALL_TEXT_FROM_SCRIPTS.txt": "docs/reverse-engineering/ALL_TEXT_FROM_SCRIPTS.txt",

    # ===== walkthrough =====
    "docs/35_SOFTWORLD_25.md": "docs/walkthrough/35_SOFTWORLD_25.md",
    "docs/36_SOFTWORLD_26.md": "docs/walkthrough/36_SOFTWORLD_26.md",
    "docs/37_SOFTWORLD_27.md": "docs/walkthrough/37_SOFTWORLD_27.md",
    "docs/38_SOFTWORLD_WALKTHROUGH.md": "docs/walkthrough/38_SOFTWORLD_WALKTHROUGH.md",
    "docs/39_SOFTWORLD_FULLTEXT_AND_MAPS.md": "docs/walkthrough/39_SOFTWORLD_FULLTEXT_AND_MAPS.md",

    # ===== gameplay =====
    "opendw_remake/docs/gameplay/56_PLAYABLE_ENDING_CHAIN.md": "docs/gameplay/56_PLAYABLE_ENDING_CHAIN.md",
    "opendw_remake/docs/gameplay/57_DOORS_TRAPS_TERRAIN.md": "docs/gameplay/57_DOORS_TRAPS_TERRAIN.md",
    "opendw_remake/docs/gameplay/58_MAGIC_REFERENCE.md": "docs/gameplay/58_MAGIC_REFERENCE.md",
    "opendw_remake/docs/gameplay/59_SKILL_CHECK_TRIGGERS.md": "docs/gameplay/59_SKILL_CHECK_TRIGGERS.md",
    "docs/54_WORLDMAP_REACHABILITY_AUDIT.md": "docs/gameplay/54_WORLDMAP_REACHABILITY_AUDIT.md",
    "docs/55_MAINLINE_QUEST_GATE_AND_ENDGAME.md": "docs/gameplay/55_MAINLINE_QUEST_GATE_AND_ENDGAME.md",

    # ===== engine =====
    "opendw_remake/docs/engine/CONTROLS.md": "docs/engine/CONTROLS.md",
    "opendw_remake/docs/engine/REWRITE_READINESS.md": "docs/engine/REWRITE_READINESS.md",
    "opendw_remake/docs/engine/VIEWPORT.md": "docs/engine/VIEWPORT.md",
    "opendw_remake/docs/engine/VIEWPORT_COMPOSE.md": "docs/engine/VIEWPORT_COMPOSE.md",
    "docs/05_SDL2_IMPLEMENTATION.md": "docs/engine/05_SDL2_IMPLEMENTATION.md",
    "docs/06_IMPLEMENTATION_PLAN.md": "docs/engine/06_IMPLEMENTATION_PLAN.md",
    "docs/07_REVISED_PLAN.md": "docs/engine/07_REVISED_PLAN.md",
    "docs/08_READ_PARAGRAPH_FEATURE.md": "docs/engine/08_READ_PARAGRAPH_FEATURE.md",
    "docs/50_BUILD.md": "docs/engine/50_BUILD.md",
    "docs/51_TEST_PLAN.md": "docs/engine/51_TEST_PLAN.md",

    # ===== reference =====
    "opendw_remake/docs/reference/61_MULTIVERSION_ASSETS.md": "docs/reference/61_MULTIVERSION_ASSETS.md",
    "opendw_remake/docs/reference/62_VGA256_THEME.md": "docs/reference/62_VGA256_THEME.md",
    "opendw_remake/docs/reference/63_OPENDW_VS_REMAKE_ARCH.md": "docs/reference/63_OPENDW_VS_REMAKE_ARCH.md",
    "opendw_remake/docs/reference/64_X68K_PKH_LESSONS.md": "docs/reference/64_X68K_PKH_LESSONS.md",
    "opendw_remake/docs/reference/66_BARDS_TALE_LINEAGE.md": "docs/reference/66_BARDS_TALE_LINEAGE.md",

    # ===== assessment =====
    "docs/00_DOC_AUDIT.md": "docs/assessment/00_DOC_AUDIT.md",
    "docs/41_TECHNICAL_DEBT.md": "docs/assessment/41_TECHNICAL_DEBT.md",
    "docs/47_REMAKE_ASSESSMENT.md": "docs/assessment/47_REMAKE_ASSESSMENT.md",
    "docs/48_COMPLETABILITY_ROADMAP.md": "docs/assessment/48_COMPLETABILITY_ROADMAP.md",
    "docs/49_GAP_AUDIT.md": "docs/assessment/49_GAP_AUDIT.md",
    "docs/57_PM_REVIEW.md": "docs/assessment/57_PM_REVIEW.md",
    "docs/58_DOC_AUDIT_AND_DRIFT.md": "docs/assessment/58_DOC_AUDIT_AND_DRIFT.md",
    "docs/59_LAYOUT_FIDELITY.md": "docs/assessment/59_LAYOUT_FIDELITY.md",
    "docs/60_SKILL.md": "docs/assessment/60_SKILL.md",
    "docs/61_SHOP_AND_RECRUIT.md": "docs/assessment/61_SHOP_AND_RECRUIT.md",
    "opendw_remake/docs/audit/60_DOS_VS_REMAKE_VISUAL.md": "docs/assessment/60_DOS_VS_REMAKE_VISUAL.md",
    "opendw_remake/docs/audit/65_GAME_EVALUATION.md": "docs/assessment/65_GAME_EVALUATION.md",

    # ===== translation =====
    "docs/10_TRANSLATION.md": "docs/translation/10_TRANSLATION.md",
    "docs/11_TRANSLATION_DIALOGUE.md": "docs/translation/11_TRANSLATION_DIALOGUE.md",
    "docs/12_TRANSLATION_ITEMS.md": "docs/translation/12_TRANSLATION_ITEMS.md",
    "docs/13_TRANSLATION_SKILLS.md": "docs/translation/13_TRANSLATION_SKILLS.md",
    "docs/14_TRANSLATION_MONSTERS.md": "docs/translation/14_TRANSLATION_MONSTERS.md",
    "docs/15_TRANSLATION_DRAFT.md": "docs/translation/15_TRANSLATION_DRAFT.md",
    "docs/53_EVENTS_TRANSLATION_REVIEW.md": "docs/translation/53_EVENTS_TRANSLATION_REVIEW.md",

    # ===== manual =====
    "docs/32_EN_MANUAL_TEXT.md": "docs/manual/32_EN_MANUAL_TEXT.md",
    "docs/33_MANUAL_TRANSCRIPTION.md": "docs/manual/33_MANUAL_TRANSCRIPTION.md",
    "docs/34_READ_PARAGRAPHS.md": "docs/manual/34_READ_PARAGRAPHS.md",
    "docs/Dragon-Wars_Manual_DOS_EN.pdf": "docs/manual/Dragon-Wars_Manual_DOS_EN.pdf",
    "docs/manual_files.txt": "docs/manual/manual_files.txt",
    "docs/en_page_1_thumb.b64": "docs/manual/en_page_1_thumb.b64",
    "docs/en_page_2_thumb.b64": "docs/manual/en_page_2_thumb.b64",
    "docs/en_page_3_thumb.b64": "docs/manual/en_page_3_thumb.b64",
    "docs/en_page_4_thumb.b64": "docs/manual/en_page_4_thumb.b64",
    "docs/en_page_5_thumb.b64": "docs/manual/en_page_5_thumb.b64",
    "docs/en_page_6_thumb.b64": "docs/manual/en_page_6_thumb.b64",
    "docs/珍066-火龍之戰.rar": "docs/manual/珍066-火龍之戰.rar",

    # ===== adr (合併) =====
    "opendw_remake/docs/adr/0002-two-layer-cjk-rendering.md": "docs/adr/0002-two-layer-cjk-rendering.md",
}

# 不搬移(留原位):docs/README.md(改寫成總表)、docs/99_INDEX.md、docs/adr/0001、
# docs/media/README.md(+3 gif)、docs/media/(已是 media)、docs/_deprecated/*。
