# SDL2 實作計畫：取代 DOS 未實作功能

## 一、DOS 設定選單 (`0x627-0963`) → SDL2 設定系統

### 1.1 原始功能分析

原始的 `check_config()` 在 DOS 模式下提供：
- 顯示模式選擇：CGA RGB、CGA Composite、Tandy 16色、EGA 16色、VGA/MCGA 16色
- 滑鼠開關：Mouse On / Mouse Off
- 儲存設定到 dragon.com

### 1.2 SDL2 取代方案

由於 SDL2 已支援所有現代顯示模式，不需要 CGA/EGA 選擇。
但仍需要一個 **遊戲內設定選單**：

```
┌─────────────────────────────────────────┐
│         Dragon Wars 設定                │
├─────────────────────────────────────────┤
│  視窗大小: [640×480] [800×600] [全螢幕] │
│  縮放比例: [1x] [2x] [3x] [4x]          │
│  音效:     [開] [關]                    │
│  音樂:     [開] [關]                    │
│  按鍵設定: [設定]                       │
│                                        │
│  [確定]  [取消]  [預設值]               │
└─────────────────────────────────────────┘
```

### 1.3 實作方式

**新增檔案**：`src/lib/config.h`, `src/lib/config.c`

```c
// config.h
#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

struct game_config {
    int window_width;
    int window_height;
    int scale_factor;
    int fullscreen;
    int sound_enabled;
    int music_enabled;
    // Key bindings
    int key_up;
    int key_down;
    int key_left;
    int key_right;
    int key_action;
    int key_cancel;
};

extern struct game_config config;

int config_load(const char *path);
int config_save(const char *path);
void config_set_defaults(void);
void config_apply_sdl(void);

#endif
```

**與現有程式碼整合**：
- `fe/vga_sdl.c` 的 `WIN_WIDTH`, `WIN_HEIGHT` 改為讀取 `config`
- `fe/main.c` 在啟動時呼叫 `config_load()` 和 `config_apply_sdl()`

### 1.4 按鍵綁定

SDL2 使用 `SDL_Keycode` 而非 DOS scan code：

| 按鍵 | SDL Keycode | 功能 |
|------|-------------|------|
| ↑ | `SDLK_UP` | 上移 |
| ↓ | `SDLK_DOWN` | 下移 |
| ← | `SDLK_LEFT` | 左移 |
| → | `SDLK_RIGHT` | 右移 |
| Enter | `SDLK_RETURN` | 確認 |
| ESC | `SDLK_ESCAPE` | 取消 |
| Space | `SDLK_SPACE` | 空白鍵 |

**注意**：原始遊戲使用 `0x88` (Left), `0x8A` (Down), `0x8B` (Up), `0x8D` (Enter), `0x95` (Right), `0x9B` (ESC) 等自定義 keycode。
需要在 `vga_sdl.c` 的 `get_key_from_buffer()` 中做轉換。

---

## 二、PC Speaker 音樂 (`0x5C3B-0x5D1D`) → SDL2 Audio

### 2.1 原始功能分析

原始的 PIT 音樂系統：
- 使用 8253/54 PIT 計時器（port 40h, 42h, 43h）
- 設定頻率 = `1193182 / frequency`
- 通過 port 61h bit 0-1 控制 PC speaker 開關
- 音樂資料儲存在 `5A9Dh` 和 `5C1Dh`（每個音符 48 bytes）
- 4 個聲道（`byte_5A72` = 4）

### 2.2 SDL2 取代方案

**新增檔案**：`src/lib/audio.h`, `src/lib/audio.c`

```c
// audio.h
#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>
#include <SDL.h>

struct audio_channel {
    int active;
    uint16_t frequency;
    uint16_t duration;
    uint8_t  volume;
};

int audio_init(void);
void audio_shutdown(void);
void audio_play_note(int channel, uint16_t frequency, uint16_t duration);
void audio_stop_note(int channel);
void audio_set_volume(int channel, uint8_t volume);
void audio_update(void);

// Music playback
int music_load_from_resource(int resource_index);
void music_play(void);
void music_stop(void);
void music_update(void);

#endif
```

### 2.3 音生成

使用 SDL2 的 `SDL_OpenAudioDevice()` 生成方波：

```c
// audio.c
static void audio_callback(void *userdata, Uint8 *stream, int len) {
    // Generate square wave at current frequency
    static uint32_t phase = 0;
    Sint16 *buffer = (Sint16 *)stream;
    int samples = len / 2;
    
    for (int i = 0; i < samples; i++) {
        // Square wave: high or low based on phase
        buffer[i] = (phase % current_period < current_period / 2) ? 
                    8000 : -8000;
        phase++;
    }
}
```

### 2.4 音樂資料格式

原始音樂資料格式（每個聲道 48 bytes）：
```
Offset 0x00: uint16_t  ; 是否播放 (0 = 停止)
Offset 0x02: uint16_t  ; 頻率相關
Offset 0x04: uint16_t  ; 持續時間
...
```

**注意**：需要從 dragon.com 提取音樂資料結構，可能需要調整格式以相容 SDL2。

### 2.5 與現有程式碼整合

- `sub_5C3B` (init_pit_timer) → `audio_init()`
- `sub_5C7B` (pit_timer_isr) → `audio_update()` (在遊戲主迴圈中呼叫)
- `sub_5CB6` (play_music_frame) → `music_update()`
- `sub_5D1D` (play_music_note) → `audio_play_note()`
- `sub_5ECA` (init_music_state) → `music_load_from_resource()`

---

## 三、未實作的 Opcode → SDL2 實作

### 3.1 分類

| 類別 | 數量 | 說明 |
|------|------|------|
| 圖形模式相關 | ~15 | CGA/EGA/Tandy 特定功能，SDL2 不需要 |
| 音效相關 | ~10 | PC speaker 音效，改用 SDL2 audio |
| 檔案 I/O | ~5 | DOS INT 21h 呼叫，改用 stdio |
| 記憶體管理 | ~5 | DOS 記憶體配置，改用 malloc |
| 遊戲邏輯 | ~30 | 需要從 dragon.asm 分析 |
| 未知功能 | ~20 | 需要進一步調查 |

### 3.2 圖形模式相關（可忽略）

這些 opcode 用於 CGA/EGA/Tandy 的特定顯示模式，SDL2 不需要：

```c
// 這些可以實作為空函式
static void op_unused_02(void) { /* CGA specific, NOP in SDL2 */ }
static void op_unused_1B(void) { /* EGA specific, NOP in SDL2 */ }
static void op_unused_1E(void) { /* Tandy specific, NOP in SDL2 */ }
// ... etc
```

### 3.3 音效相關（改用 SDL2 Audio）

| Opcode | 原始功能 | SDL2 取代 |
|--------|----------|-----------|
| `op_90` | 播放音效 | `audio_play_note()` |
| `sub_5080` | 開門音效 | `audio_play_sound(SOUND_DOOR)` |
| `sub_5088` | 未知音效 | `audio_play_sound(SOUND_UNKNOWN)` |
| `sub_5090` | 撞牆音效 | `audio_play_sound(SOUND_BUMP)` |
| `sub_50B2` | 未知音效 | `audio_play_sound(SOUND_UNKNOWN2)` |

### 3.4 需要進一步調查的 Opcode

以下 opcode 需要從 dragon.asm 分析其功能：

| Opcode | 位址 | 猜測功能 |
|--------|------|----------|
| `op_02` | `0x3B1F` | 可能與載入資源有關 |
| `op_1B` | `0x3D73` | 可能與遊戲狀態有關 |
| `op_1E` | `0x01B2` | 可能與腳本執行有關 |
| `op_29` | `0x3E1B` | 可能與 UI 有關 |
| `op_2C` | `0x3E4C` | 可能與輸入有關 |
| `op_37` | `0x3FAD` | 可能與音樂有關 |
| `op_46` | `0x40C1` | 可能與角色有關 |
| `op_64-6B` | 未知 | 需要調查 |
| `op_6E-70` | 未知 | 需要調查 |
| `op_79` | `0x47FA` | 需要調查 |
| `op_7E` | `0x4845` | 需要調查 |
| `op_8E-8F` | 未知 | 需要調查 |
| `op_9C` | 未知 | 需要調查 |
| `op_9F` | 未知 | 需要調查 |
| `op_A0-FF` | 未知 | 需要調查 |

---

## 四、實作優先順序

### Phase 1：SDL2 設定系統（1-2 天）

1. 建立 `config.h` / `config.c`
2. 修改 `vga_sdl.c` 使用 config
3. 修改 `main.c` 載入 config
4. 加入命令列參數 `--config <path>`

### Phase 2：SDL2 Audio 系統（2-3 天）

1. 建立 `audio.h` / `audio.c`
2. 實作音訊回調
3. 實作音樂播放
4. 整合到遊戲主迴圈

### Phase 3：按鍵映射（1 天）

1. 建立 `keymap.h` / `keymap.c`
2. 修改 `vga_sdl.c` 的 `get_key_from_buffer()`
3. 支援自訂按鍵

### Phase 4：未實作 Opcode 調查（3-5 天）

1. 逐一分析 dragon.asm 中的未實作 opcode
2. 分類為：可忽略 / 需要實作 / 需要調查
3. 實作遊戲邏輯相關的 opcode
4. 音效相關 opcode 改用 SDL2 Audio

### Phase 5：測試與修正（2-3 天）

1. 測試所有功能
2. 修正相容性問題
3. 確保 Windows/macOS/Linux 都能執行

---

## 五、技術細節

### 5.1 SDL2 Audio 初始化

```c
int audio_init(void) {
    SDL_AudioSpec want, have;
    
    SDL_zero(want);
    want.freq = 44100;
    want.format = AUDIO_S16LSB;
    want.channels = 1;
    want.samples = 1024;
    want.callback = audio_callback;
    
    audio_device = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (audio_device == 0) {
        fprintf(stderr, "Failed to open audio: %s\n", SDL_GetError());
        return -1;
    }
    
    SDL_PauseAudioDevice(audio_device, 0); // Start playing
    return 0;
}
```

### 5.2 音樂播放

```c
void music_update(void) {
    static uint32_t last_tick = 0;
    uint32_t current_tick = SDL_GetTicks();
    
    if (current_tick - last_tick >= 55) { // ~18.2 Hz
        last_tick = current_tick;
        
        // Update music channels
        for (int i = 0; i < 4; i++) {
            if (channels[i].active) {
                channels[i].duration--;
                if (channels[i].duration == 0) {
                    channels[i].active = 0;
                }
            }
        }
    }
}
```

### 5.3 按鍵映射

```c
static const struct {
    int dragon_key;
    SDL_Keycode sdl_key;
} keymap[] = {
    { 0x8B, SDLK_UP },      // Up
    { 0x8A, SDLK_DOWN },    // Down
    { 0x88, SDLK_LEFT },    // Left
    { 0x95, SDLK_RIGHT },   // Right
    { 0x8D, SDLK_RETURN },  // Enter
    { 0x9B, SDLK_ESCAPE },  // Escape
    { 0xA0, SDLK_SPACE },   // Space
    { 0xB1, SDLK_1 },       // 1
    { 0xB2, SDLK_2 },       // 2
    { 0xB3, SDLK_3 },       // 3
    { 0xB4, SDLK_4 },       // 4
    { 0xB5, SDLK_5 },       // 5
    { 0xB6, SDLK_6 },       // 6
    { 0xB7, SDLK_7 },       // 7
    { 0xB8, SDLK_8 },       // 8
    { 0xB9, SDLK_9 },       // 9
    { 0xCE, SDLK_n },       // N (No)
    { 0xD9, SDLK_y },       // Y (Yes)
    { 0xCA, SDLK_j },       // J (Left)
    { 0xCB, SDLK_k },       // K (Down)
    { 0xCC, SDLK_l },       // L (Right)
    { 0xC1, SDLK_a },       // A (Up)
    { 0xC9, SDLK_i },       // I (Up)
    { 0xDA, SDLK_z },       // Z (Down)
    { 0xAF, SDLK_SLASH },   // / (blocked)
    { 0xDC, SDLK_BACKSLASH }, // \ (blocked)
    { 0, 0 } // terminator
};
```

---

## 六、檔案結構

```
src/
├── fe/
│   ├── main.c           # 程式進入點（修改：載入 config）
│   ├── vga_sdl.c        # SDL2 顯示（修改：使用 config）
│   └── vga.h            # VGA 介面
├── lib/
│   ├── config.h         # 新增：設定系統
│   ├── config.c         # 新增：設定系統實作
│   ├── audio.h          # 新增：音訊系統
│   ├── audio.c          # 新增：音訊系統實作
│   ├── keymap.h         # 新增：按鍵映射
│   ├── keymap.c         # 新增：按鍵映射實作
│   ├── engine.c         # 修改：未實作 opcode
│   └── ui.c             # 修改：設定選單
└── tools/
    └── extract_music.cpp # 新增：從 dragon.com 提取音樂資料
```

---

## 七、測試計畫

### 7.1 設定系統測試

- [ ] 載入/儲存設定
- [ ] 命令列參數
- [ ] 視窗大小調整
- [ ] 全螢幕切換

### 7.2 音訊系統測試

- [ ] 播放單一音符
- [ ] 播放音樂
- [ ] 音效播放
- [ ] 音量控制

### 7.3 按鍵映射測試

- [ ] 所有方向鍵
- [ ] 確認/取消鍵
- [ ] 數字鍵
- [ ] 自訂按鍵

### 7.4 遊戲邏輯測試

- [ ] 未實作 opcode 不崩潰
- [ ] 遊戲流程正常
- [ ] 音效/音樂播放正常
