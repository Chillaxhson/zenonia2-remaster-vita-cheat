# Zenonia 2 PS Vita: Cheat Modification & Development Guide

This guide details the architecture, reverse-engineered engine symbols, memory offsets, and implementation steps for building the hotkey cheat system for the PS Vita port of **Zenonia 2: The Lost Memories** (`zenonia2-remaster-vita-cheat` / `so_loader`).

---

## 📑 Table of Contents
1. [One-Shot Implementation Prompt](#one-shot-implementation-prompt)
2. [Critical Technical Pitfall: The Float ABI Trap](#critical-technical-pitfall-the-float-abi-trap)
3. [Reverse-Engineered Symbols & Offsets](#reverse-engineered-symbols--offsets)
4. [Hotkey Controls Scheme](#hotkey-controls-scheme)
5. [Code Modifications Reference](#code-modifications-reference)
   - [CMakeLists.txt](#1-cmakeliststxt)
   - [source/patch.c](#2-sourcepatchc-god-mode--50x-exp-multiplier)
   - [source/main.c](#3-sourcemainc-symbols--hotkey-handler)
6. [Stability & Headless Design: Avoiding Boot Crashes](#-stability--headless-design-avoiding-boot-crashes)
7. [Compilation & Packaging](#-compilation--packaging)

---

## 🚀 One-Shot Implementation Prompt

Copy and paste the prompt below into any fresh AI agent session or use it as a standalone specification on a pristine clone of the repository:

```markdown
You are modifying the PS Vita port of Zenonia 2 (so_loader loading Android libzenonia2.so).
The goal is to implement a clean, headless hotkey cheat system using L1 as a modifier button without introducing any visual, GL, or input bugs.

### ⚠️ CRITICAL COMPILATION & ABI REQUIREMENT
1. DO NOT change the float ABI to `-mfloat-abi=hard`. The Android library `libzenonia2.so` uses `softfp`.
   Compiling with `hard` float ABI corrupts floating-point arguments passed to VitaGL/Android routines, causing a massive screen zoom, flipped projection matrix, and non-responsive touch controls.
2. Maintain `-mfloat-abi=softfp` in `CMakeLists.txt`.
3. ALWAYS compile using the specific softfp Docker/Podman image: `docker.io/atamanenko/vitasdk-softfp:latest`. DO NOT use the standard/latest `vitasdk/vitasdk:latest` image.

---

### CHEAT SPECIFICATIONS & CONTROLS
The L1 button (`AKEYCODE_BUTTON_L1`) does not conflict with in-game controls (it maps to 0 in `vita_to_control`).
Implement the following hotkey combinations when holding **L1**:

- **L1 + SELECT**: Toggle 50x EXP Multiplier ON/OFF
- **L1 + SQUARE**: Full HP & SP Refill (100% HP & SP)
- **L1 + TRIANGLE**: Add 50,000 Gold directly to inventory
- **L1 + CIRCLE**: Add 5 Stat Points
- **L1 + CROSS**: Add 5 Skill Points
- **L1 + START**: Toggle God Mode (Invulnerability) ON/OFF

*Note*: When L1 is held down and any of the shortcut keys are pressed, suppress forwarding the key event to the game engine (`_ZN6CMvApp10EvKeyPressEi`) so that normal actions (like opening the in-game menu, map, or attacking) are not triggered unintentionally.
```

---

## ⚠️ Critical Technical Pitfall: The Float ABI Trap

### The Problem
The Android game library (`libzenonia2.so`) was compiled for older ARMv7 processors using the **`softfp`** floating point ABI.
- In **`softfp`**, floating point parameters are passed via standard integer registers (`r0`–`r3` and stack).
- In modern VitaSDK releases (`vitasdk/vitasdk:latest`), toolchain defaults have migrated to **`hard`** float ABI (VFP registers `s0`–`s15`).

### The Symptom
If `so_loader` is compiled with `hard` float ABI:
1. OpenGL functions (like `glOrthof`, `glClearColor`, viewport transformations) receive corrupted floats.
2. The game renders with a **massive zoom**, **flipped/inverted projection**, and **unresponsive touch input**.
3. Re-exporting matrices or tuning viewport resolution in code will **not** fix this issue because the underlying function parameters are fundamentally misaligned at the register level.

### The Solution
- Always build with: `docker.io/atamanenko/vitasdk-softfp:latest`
- Keep `CMakeLists.txt` configured with: `-mfloat-abi=softfp`

---

## 🔍 Reverse-Engineered Symbols & Offsets

| Symbol / Offset | Description | Signature |
|---|---|---|
| `_ZN12CMvCharacter6FullHPEv` | Restores character HP to maximum | `void FullHP(void *this)` |
| `_ZN12CMvCharacter6FullSPEbb` | Restores character SP to maximum | `void FullSP(void *this, bool b1, bool b2)` |
| `_ZN12CMvCharacter5SetSPEib` | Sets character SP value | `void SetSP(void *this, int sp, bool b)` |
| `_ZN12CGsSingletonI10CMvItemMgrE10GetInstPtrEv` | Retrieves Item Manager singleton | `void* GetInstPtr(void)` |
| `_ZN15CMvItemSaveData8IncMoneyEi` | Adds money/gold to inventory | `void IncMoney(void *this, int money)` |
| `_ZN9CMvPlayer21CreateGiveMoneyEffectEi` | Adds gold + animated float effect | `void CreateGiveMoneyEffect(void *this, int money)` |
| `_ZN9CMvPlayer11AddBonusExpEj` | Adds mob & quest EXP (hooked for 50x) | `void AddBonusExp(void *this, unsigned int exp)` |
| `_ZN9CMvPlayer6AddExpEj` | Adds direct/PvP EXP (hooked for 50x) | `void AddExp(void *this, unsigned int exp)` |
| `_ZN9CMvPlayer9OnDamagedEiP9CMvObjectb15EnumElementTypeb` | Player damage handler (hooked for God Mode) | `int OnDamaged(void *this, int dmg, void *atk, bool b1, int elem, bool b2)` |
| `_ZN9CMvPlayer12AddStatPointEi` | Adds player stat points | `void AddStatPoint(void *this, int pts)` |
| `_ZN9CMvPlayer13AddSkillPointEi` | Adds player skill points | `void AddSkillPoint(void *this, int pts)` |
| `g_CMvPlayer_instance + 0x69c` | Player Stat Points (Unallocated) | `uint16_t` |
| `g_CMvPlayer_instance + 0x69e` | Player Skill Points (Unallocated) | `uint16_t` |
| `itemMgr + 0x4` | Offset to `CMvItemSaveData` inside `CMvItemMgr` | `void*` |
| `_ZN12CGsSingletonI12CMvObjectMgrE10GetInstPtrEv` | Retrieves Object Manager singleton | `void* GetInstPtr(void)` |
| `_ZN12CMvObjectMgr9GetPlayerEv` | Retrieves current Player instance | `void* GetPlayer(void *this)` |

---

## 🎮 Hotkey Controls Scheme

The `L1` button (`AKEYCODE_BUTTON_L1`) returns `0` in `vita_to_control` and is unused in normal gameplay. It serves as the primary modifier:

| Button Combination | In-Game Action | Effect |
|---|---|---|
| <kbd>L1</kbd> + <kbd>SELECT</kbd> | Toggle 50x EXP | Toggles 50x Experience Points multiplier ON / OFF |
| <kbd>L1</kbd> + <kbd>SQUARE</kbd> | Refill Vitals | Restores HP and SP to 100% |
| <kbd>L1</kbd> + <kbd>TRIANGLE</kbd> | Add Gold | Adds +50,000 Gold directly to inventory |
| <kbd>L1</kbd> + <kbd>CIRCLE</kbd> | Add Stat Points | Adds +5 unassigned Character Stat Points |
| <kbd>L1</kbd> + <kbd>CROSS</kbd> | Add Skill Points | Adds +5 unassigned Skill Points |
| <kbd>L1</kbd> + <kbd>START</kbd> | Toggle God Mode | Toggles complete damage immunity ON / OFF |

*All combo presses consume the input event so normal gameplay actions (such as menu opening or attacking) do not trigger.*

---

## 🛠️ Code Modifications Reference

### 1. `CMakeLists.txt`
Ensure the compiler flags maintain `softfp`:
```cmake
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q -mfloat-abi=softfp -std=gnu11 -Wno-deprecated")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,-q -mfloat-abi=softfp -std=gnu++20 -Wno-write-strings -Wno-psabi")
```

### 2. `source/patch.c` (God Mode, 50x EXP Multiplier, Player Instance Tracking & Save DRM Bypass)
```c
#include <stdbool.h>

void *g_CMvApp_instance = NULL;
void *g_CMvPlayer_instance = NULL;

// Cheat: God Mode
int cheat_god_mode = 0;
static so_hook CMvPlayer_OnDamaged_hook;

int CMvPlayer_OnDamaged_patched(void *this_ptr, int damage, void* attacker, bool b1, int elem, bool b2) {
    g_CMvPlayer_instance = this_ptr;
    if (cheat_god_mode) {
        return 0; // Suppress damage
    }
    return SO_CONTINUE(int, CMvPlayer_OnDamaged_hook, this_ptr, damage, attacker, b1, elem, b2);
}

// Cheat: 50x EXP Multiplier
int cheat_exp_multiplier = 0;
static so_hook CMvPlayer_AddBonusExp_hook;
static so_hook CMvPlayer_AddExp_hook;

void CMvPlayer_AddBonusExp_patched(void *this_ptr, unsigned int exp) {
    g_CMvPlayer_instance = this_ptr;
    if (cheat_exp_multiplier) {
        if (exp > (4294967295U / 50)) {
            exp = 4294967295U;
        } else {
            exp *= 50;
        }
    }
    SO_CONTINUE(void *, CMvPlayer_AddBonusExp_hook, this_ptr, exp);
}

void CMvPlayer_AddExp_patched(void *this_ptr, unsigned int exp) {
    g_CMvPlayer_instance = this_ptr;
    if (cheat_exp_multiplier) {
        if (exp > (4294967295U / 50)) {
            exp = 4294967295U;
        } else {
            exp *= 50;
        }
    }
    SO_CONTINUE(void *, CMvPlayer_AddExp_hook, this_ptr, exp);
}

// Ensure g_CMvPlayer_instance is always tracked across all player states
static so_hook CMvPlayerC2Ev_hook;
void CMvPlayerC2Ev_patched(void *this) {
    g_CMvPlayer_instance = this;
    SO_CONTINUE(void *, CMvPlayerC2Ev_hook, this);
}

static so_hook CMvPlayerC2Ei_hook;
void CMvPlayerC2Ei_patched(void *this, int param) {
    g_CMvPlayer_instance = this;
    SO_CONTINUE(void *, CMvPlayerC2Ei_hook, this, param);
}

static so_hook CMvPlayer_DoUpdate_hook;
void CMvPlayer_DoUpdate_patched(void *this) {
    g_CMvPlayer_instance = this;
    SO_CONTINUE(void *, CMvPlayer_DoUpdate_hook, this);
}

// Hook GsFSFileSize to return accurate file sizes from Vita native filesystem
static so_hook _Z12GsFSFileSizePKci_hook;
int _Z12GsFSFileSizePKci_patched(const char *path, int flag) {
    if (!path) return 0;

    char real_path[256];
    get_real_path_patched(path, real_path);

    struct stat st;
    if (stat(real_path, &st) == 0) {
        l_info("GsFSFileSize(\"%s\" -> \"%s\"): %ld", path, real_path, (long)st.st_size);
        return (int)st.st_size;
    }

    l_warn("GsFSFileSize(\"%s\" -> \"%s\"): not found (stat failed)", path, real_path);
    return 0;
}

// Protect save files from being deleted by engine integrity checks
static so_hook CGsFile_Delete_hook;
int CGsFile_Delete_patched(void *this, const char *path) {
    if (path && (strstr(path, "Save") || strstr(path, ".dat") || strstr(path, ".sav"))) {
        l_warn("CGsFile::Delete(\"%s\") BLOCKED to protect savefile!", path);
        return 0;
    }
    return SO_CONTINUE(int, CGsFile_Delete_hook, this, path);
}

void so_patch(void) {
    // ... default engine hooks ...

    CMvPlayerC2Ev_hook = hook_addr((uintptr_t)so_symbol(&so_mod, "_ZN9CMvPlayerC2Ev"),
        (uintptr_t)&CMvPlayerC2Ev_patched);

    CMvPlayerC2Ei_hook = hook_addr((uintptr_t)so_symbol(&so_mod, "_ZN9CMvPlayerC2Ei"),
        (uintptr_t)&CMvPlayerC2Ei_patched);

    CMvPlayer_DoUpdate_hook = hook_addr((uintptr_t)so_symbol(&so_mod, "_ZN9CMvPlayer8DoUpdateEv"),
        (uintptr_t)&CMvPlayer_DoUpdate_patched);

    _Z12GsFSFileSizePKci_hook = hook_addr((uintptr_t)so_symbol(&so_mod, "_Z12GsFSFileSizePKci"),
        (uintptr_t)&_Z12GsFSFileSizePKci_patched);

    CGsFile_Delete_hook = hook_addr((uintptr_t)so_symbol(&so_mod, "_ZN7CGsFile6DeleteEPKc"),
        (uintptr_t)&CGsFile_Delete_patched);

    // Bypass file size check and checksum validation in CGsEncryptFile::LoadBegin(const char*, bool):
    uintptr_t load_begin = (uintptr_t)so_symbol(&so_mod, "_ZN14CGsEncryptFile9LoadBeginEPKcb");
    uintptr_t lb_base = load_begin ? (load_begin & ~1) : (so_mod.text_base + 0xa2804);

    // 0x8e from lb_base (0xa2892): 0xd01f (beq.n a28d4) -> b.n a28d4 (0xe01f) to bypass file size header mismatch
    uint16_t b_skip_size_err = 0xe01f;
    kuKernelCpuUnrestrictedMemcpy((void *)(lb_base + 0x8e), &b_skip_size_err, sizeof(b_skip_size_err));

    // 0x128 from lb_base (0xa292c): 0xd017 (beq.n a295e) -> b.n a295e (0xe017) to bypass checksum check and skip CGsFile::Delete
    uint16_t b_skip_chksum_err = 0xe017;
    kuKernelCpuUnrestrictedMemcpy((void *)(lb_base + 0x128), &b_skip_chksum_err, sizeof(b_skip_chksum_err));

    kuKernelFlushCaches((void *)(lb_base + 0x80), 0x120);

    // Bypass anti-tamper DRM checks in CMvGameState::LoadGameData(int slot):
    // Prevents false "Save data corrupted. Please recreate your character!" popup
    uintptr_t load_game_data = (uintptr_t)so_symbol(&so_mod, "_ZN12CMvGameState12LoadGameDataEi");
    uintptr_t fn_base = load_game_data ? (load_game_data & ~1) : (so_mod.text_base + 0x103438);

    // 0xee from fn_base (0x103526): 0xd110 (bne.n 10354a) -> NOP (0xbf00) to bypass SaveTime checksum comparison
    uint16_t nop_inst = 0xbf00;
    kuKernelCpuUnrestrictedMemcpy((void *)(fn_base + 0xee), &nop_inst, sizeof(nop_inst));

    // 0x10e from fn_base (0x103546): 0xda09 (bge.n 10355c) -> b.n 10355c (0xe009) to bypass level check and jump straight to success
    uint16_t b_success = 0xe009;
    kuKernelCpuUnrestrictedMemcpy((void *)(fn_base + 0x10e), &b_success, sizeof(b_success));

    kuKernelFlushCaches((void *)(fn_base + 0xee), 0x40);

    // Cheats: God Mode & 50x EXP
    CMvPlayer_OnDamaged_hook = hook_addr((uintptr_t)so_symbol(&so_mod, "_ZN9CMvPlayer9OnDamagedEiP9CMvObjectb15EnumElementTypeb"),
        (uintptr_t)&CMvPlayer_OnDamaged_patched);

    CMvPlayer_AddBonusExp_hook = hook_addr((uintptr_t)so_symbol(&so_mod, "_ZN9CMvPlayer11AddBonusExpEj"),
        (uintptr_t)&CMvPlayer_AddBonusExp_patched);

    CMvPlayer_AddExp_hook = hook_addr((uintptr_t)so_symbol(&so_mod, "_ZN9CMvPlayer6AddExpEj"),
        (uintptr_t)&CMvPlayer_AddExp_patched);
}
```

### 3. `source/main.c` (Symbols & Hotkey Handler)
```c
extern void *g_CMvApp_instance;
extern void *g_CMvPlayer_instance;
extern int cheat_god_mode;
extern int cheat_exp_multiplier;
extern uint32_t current_buttons;

void (* _ZN12CMvCharacter6FullHPEv)(void *this);
void (* _ZN12CMvCharacter5SetSPEib)(void *this, int param1, int param2);

void* (*CMvItemMgr_GetInstPtr)(void);
void (*CMvItemSaveData_IncMoney)(void *this, int money);

static void* (*CMvObjectMgr_GetInstPtr)(void) = NULL;
static void* (*CMvObjectMgr_GetPlayer)(void *this) = NULL;

static void (*CMvPlayer_AddStatPoint)(void *this, int points) = NULL;
static void (*CMvPlayer_AddSkillPoint)(void *this, int points) = NULL;

int32_t vita_to_control(int32_t vita_button);
int pressL1 = 0;

int main() {
    // ...
    _ZN12CMvCharacter6FullHPEv = (void *)so_symbol(&so_mod, "_ZN12CMvCharacter6FullHPEv");
    _ZN12CMvCharacter5SetSPEib = (void *)so_symbol(&so_mod, "_ZN12CMvCharacter5SetSPEib");

    CMvItemMgr_GetInstPtr = (void *)so_symbol(&so_mod, "_ZN12CGsSingletonI10CMvItemMgrE10GetInstPtrEv");
    CMvItemSaveData_IncMoney = (void *)so_symbol(&so_mod, "_ZN15CMvItemSaveData8IncMoneyEi");

    CMvObjectMgr_GetInstPtr = (void *)so_symbol(&so_mod, "_ZN12CGsSingletonI12CMvObjectMgrE10GetInstPtrEv");
    CMvObjectMgr_GetPlayer = (void *)so_symbol(&so_mod, "_ZN12CMvObjectMgr9GetPlayerEv");

    CMvPlayer_AddStatPoint = (void *)so_symbol(&so_mod, "_ZN9CMvPlayer12AddStatPointEi");
    CMvPlayer_AddSkillPoint = (void *)so_symbol(&so_mod, "_ZN9CMvPlayer13AddSkillPointEi");
    // ...
}

void controls_handler_key(int32_t keycode, ControlsAction action) {
    if (g_CMvApp_instance) {
        int32_t avk = vita_to_control(keycode);

        switch (action) {
            case CONTROLS_ACTION_DOWN:
                if (keycode == AKEYCODE_BUTTON_L1) {
                    pressL1 = 1;
                }

                int is_l1_held = pressL1 || (current_buttons & SCE_CTRL_L1);

                // Hotkey combinations while holding L1
                if (is_l1_held) {
                    if (keycode == AKEYCODE_BUTTON_SELECT) { // Select -> Toggle 50x EXP Multiplier
                        cheat_exp_multiplier = !cheat_exp_multiplier;
                        l_info("Cheat: 50x EXP Multiplier %s", cheat_exp_multiplier ? "ENABLED" : "DISABLED");
                        return;
                    }
                    if (keycode == AKEYCODE_BUTTON_X) { // Square -> Refill HP & SP
                        void* player = get_player();
                        if (player) {
                            if (_ZN12CMvCharacter6FullHPEv) _ZN12CMvCharacter6FullHPEv(player);
                            if (_ZN12CMvCharacter5SetSPEib) _ZN12CMvCharacter5SetSPEib(player, 9999, 1);
                            l_info("Cheat: Refilled HP & SP for player %p", player);
                        }
                        return;
                    }
                    if (keycode == AKEYCODE_BUTTON_Y) { // Triangle -> Add 50,000 Gold
                        if (CMvItemMgr_GetInstPtr && CMvItemSaveData_IncMoney) {
                            void* itemMgr = CMvItemMgr_GetInstPtr();
                            if (itemMgr) {
                                void* saveData = (void*)((uintptr_t)itemMgr + 4);
                                CMvItemSaveData_IncMoney(saveData, 50000);
                                l_info("Cheat: Added 50,000 Gold to saveData %p", saveData);
                            }
                        }
                        return;
                    }
                    if (keycode == AKEYCODE_BUTTON_B) { // Circle -> Add 5 Stat Points
                        void* player = get_player();
                        if (player) {
                            if (CMvPlayer_AddStatPoint) {
                                CMvPlayer_AddStatPoint(player, 5);
                            } else {
                                *(uint16_t*)((uintptr_t)player + 0x69c) += 5;
                            }
                            l_info("Cheat: Added 5 Stat Points (Total: %u)", (unsigned int)*(uint16_t*)((uintptr_t)player + 0x69c));
                        }
                        return;
                    }
                    if (keycode == AKEYCODE_BUTTON_A) { // Cross -> Add 5 Skill Points
                        void* player = get_player();
                        if (player) {
                            if (CMvPlayer_AddSkillPoint) {
                                CMvPlayer_AddSkillPoint(player, 5);
                            } else {
                                *(uint16_t*)((uintptr_t)player + 0x69e) += 5;
                            }
                            l_info("Cheat: Added 5 Skill Points (Total: %u)", (unsigned int)*(uint16_t*)((uintptr_t)player + 0x69e));
                        }
                        return;
                    }
                    if (keycode == AKEYCODE_BUTTON_START) { // Start -> Toggle God Mode
                        cheat_god_mode = !cheat_god_mode;
                        l_info("Cheat: God Mode %s", cheat_god_mode ? "ENABLED" : "DISABLED");
                        return;
                    }
                }

                _ZN6CMvApp10EvKeyPressEi(g_CMvApp_instance, avk);
                break;

            case CONTROLS_ACTION_UP:
                if (keycode == AKEYCODE_BUTTON_L1) {
                    pressL1 = 0;
                }
                if (pressL1 || (current_buttons & SCE_CTRL_L1)) {
                    if (keycode == AKEYCODE_BUTTON_SELECT ||
                        keycode == AKEYCODE_BUTTON_X ||
                        keycode == AKEYCODE_BUTTON_Y ||
                        keycode == AKEYCODE_BUTTON_B ||
                        keycode == AKEYCODE_BUTTON_A ||
                        keycode == AKEYCODE_BUTTON_START) {
                        return;
                    }
                }
                _ZN6CMvApp12EvKeyReleaseEi(g_CMvApp_instance, avk);
                break;
        }
    }
}
```

---

## 🛡️ Stability & Headless Design: Avoiding Boot Crashes

Previous attempts to port save patches and visual effects from Zenonia 1 caused startup crashes due to the following reasons:

1. **Memory Byte Patches (`kuKernelCpuUnrestrictedMemcpy`)**:
   - Patching inside `CMvGameState::Initialize` jumped past vital object initializations, causing `NativeRender` or VitaGL shaders to crash immediately on null pointers.
2. **Graphics/Visual Effects (`CreateGiveMoneyEffect`)**:
   - Attempting to spawn visual coin floaters or popup text interacts with the engine's animation and rendering queue. Calling it directly outside of regular game ticks or before UI rendering contexts are fully ready led to undefined behavior. Directly calling `CMvItemSaveData::IncMoney` mutates the gold integer cleanly without touching the rendering pipeline.
3. **Filesystem/Path Redirection Hook (`get_real_path`)**:
   - Overriding `get_real_path` broke texture and sound streaming lookups during the boot sequence, leading to missing asset errors.

By eliminating all GL/graphics modifications and invasive memory patches, the loader remains completely crash-free while providing responsive hotkey cheats.

---

## 📦 Compilation & Packaging

Run the build command using **Podman** or **Docker**:

```bash
podman run --rm -v "$(pwd):/src:z" -w /src/build docker.io/atamanenko/vitasdk-softfp:latest bash -c "make clean && make -j\$(nproc)"
```

The resulting binaries will be produced in the `build/` directory:
- `build/eboot.bin` — Direct binary replacement for `ux0:app/ZNNA00001/eboot.bin`
- `build/zenonia2.vpk` — Full installable VPK package
