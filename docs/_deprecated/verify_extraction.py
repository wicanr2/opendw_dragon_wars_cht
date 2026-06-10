#!/usr/bin/env python3
"""
⚠️ 作廢(2026-06-10):本腳本驗的是 `20_ALL_TEXT_FROM_DATA1.txt`(3,926 條)這個
「逐 byte 暴力解 5-bit 壓縮」產生的雜訊基準,邏輯已過時,僅存歷史。
乾淨文字請見 `docs/ALL_TEXT_FROM_SCRIPTS.txt`,正確萃取方法見 `docs/07_REVISED_PLAN.md`。

驗證 DATA1 文字萃取完整性
1. 載入 ALL_TEXT_FROM_DATA1.txt
2. 檢查格式正確性
3. 交叉比對 TRANSLATION.md
4. 輸出驗證報告
"""
import re
import sys
from collections import defaultdict

def load_extracted_text(path):
    """載入萃取的文字"""
    texts = []
    current_section = None
    current_offset = None
    current_text = []
    
    with open(path, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.rstrip('\n')
            
            # Section header
            if line.startswith('# Section '):
                current_section = line
                continue
            
            # Offset header
            m = re.match(r'^\[\s*(\d+)\]\s*(.*)', line)
            if m:
                if current_offset is not None and current_text:
                    texts.append({
                        'section': current_section,
                        'offset': current_offset,
                        'text': ' '.join(current_text)
                    })
                current_offset = int(m.group(1))
                current_text = [m.group(2)] if m.group(2) else []
            elif current_offset is not None and line.strip():
                current_text.append(line)
    
    # Last entry
    if current_offset is not None and current_text:
        texts.append({
            'section': current_section,
            'offset': current_offset,
            'text': ' '.join(current_text)
        })
    
    return texts

def load_translations(path):
    """載入翻譯表（支援 | ID | 英文 | 中文 | 格式）"""
    translations = {}
    
    with open(path, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.rstrip('\n')
            
            # Match translation table rows
            m = re.match(r'^\|\s*(\w+)\s*\|\s*(.+?)\s*\|\s*(.+?)\s*\|', line)
            if m:
                en_text = m.group(2).strip()
                zh_text = m.group(3).strip()
                if en_text and zh_text and zh_text != '—':
                    translations[en_text] = zh_text
    
    return translations

def main():
    print("=" * 60)
    print("DATA1 文字萃取驗證報告")
    print("=" * 60)
    
    # 1. 載入萃取文字
    try:
        texts = load_extracted_text('docs/ALL_TEXT_FROM_DATA1.txt')
        print(f"\n[1] 載入 ALL_TEXT_FROM_DATA1.txt: {len(texts)} 條文字")
    except Exception as e:
        print(f"\n[1] 錯誤：無法載入 ALL_TEXT_FROM_DATA1.txt: {e}")
        return 1
    
    # 2. 檢查格式
    print(f"\n[2] 格式檢查")
    empty_texts = [t for t in texts if not t['text'].strip()]
    print(f"  - 空文字：{len(empty_texts)} 條")
    short_texts = [t for t in texts if len(t['text']) < 3 and t['text'].strip()]
    print(f"  - 短文字（<3 字）：{len(short_texts)} 條")
    
    # 3. 統計 section 分布
    print(f"\n[3] Section 分布")
    section_counts = defaultdict(int)
    for t in texts:
        section = t['section'] if t['section'] else 'Unknown'
        section_counts[section] += 1
    for section, count in sorted(section_counts.items()):
        print(f"  - {section}: {count} 條")
    
    # 4. 載入翻譯表
    print(f"\n[4] 翻譯表檢查")
    try:
        translations = load_translations('docs/TRANSLATION.md')
        print(f"  - TRANSLATION.md: {len(translations)} 條翻譯")
    except Exception as e:
        print(f"  - 錯誤：無法載入 TRANSLATION.md: {e}")
        translations = {}
    
    # 5. 交叉比對
    print(f"\n[5] 交叉比對")
    translated = 0
    untranslated = 0
    for t in texts:
        text = t['text'].strip()
        if text in translations:
            translated += 1
        else:
            untranslated += 1
    
    total = len(texts)
    coverage = (translated / total * 100) if total > 0 else 0
    print(f"  - 已翻譯：{translated} 條")
    print(f"  - 未翻譯：{untranslated} 條")
    print(f"  - 涵蓋率：{coverage:.1f}%")
    
    # 6. 關鍵內容檢查
    print(f"\n[6] 關鍵內容檢查")
    keywords = {
        'Sword': '武器',
        'Armor': '防具',
        'Heal': '治療',
        'Fire': '法術',
        'Dragon': '火龍',
        'Potion': '藥水',
        'Gold': '金幣',
        'Fight': '戰鬥',
        'Game Over': '遊戲結束',
    }
    
    for keyword, desc in keywords.items():
        found = any(keyword.lower() in t['text'].lower() for t in texts)
        status = "✓" if found else "✗"
        print(f"  {status} {desc}（{keyword}）")
    
    # 7. 總結
    print(f"\n[7] 總結")
    print(f"  - 總萃取文字數：{total}")
    print(f"  - 翻譯涵蓋率：{coverage:.1f}%")
    print(f"  - 缺失翻譯：{untranslated} 條")
    
    return 0

if __name__ == '__main__':
    sys.exit(main())
