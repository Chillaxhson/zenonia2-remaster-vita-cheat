# Zenonia 2 Remaster Vita - Cheat Edition

<p align="center"><img src="./extras/screenshots/screenshot1.jpg"></p>

This is an enhanced fork of the **Zenonia 2 Remastered** port for the *PlayStation Vita*, featuring a clean, headless hotkey cheat engine.

The port works by loading the Android ARMv7 executables from the unofficial Android remaster by Ill-Hovercraft8548 in memory, resolving imports with native functions, and applying dynamic runtime patches.

---

## 🎮 Cheat Hotkeys

Cheats are activated in-game by holding **<kbd>L1</kbd>** and pressing a face or trigger button. Inputs during combinations are captured and suppressed so normal gameplay actions (such as menu opening or attacking) do not trigger unintentionally.

| Combination | Action | Description |
|---|---|---|
| **<kbd>L1</kbd> + <kbd>SELECT</kbd>** | **Toggle 50x EXP** | Toggles a 50x Experience Points multiplier ON / OFF |
| **<kbd>L1</kbd> + <kbd>SQUARE</kbd>** | **Refill HP & SP** | Instantly restores HP and SP to 100% |
| **<kbd>L1</kbd> + <kbd>TRIANGLE</kbd>** | **Add 50,000 Gold** | Adds +50,000 Gold directly to your inventory |
| **<kbd>L1</kbd> + <kbd>CIRCLE</kbd>** | **Add 5 Stat Points** | Adds +5 unassigned Character Stat Points |
| **<kbd>L1</kbd> + <kbd>CROSS</kbd>** | **Add 5 Skill Points** | Adds +5 unassigned Skill Points |
| **<kbd>L1</kbd> + <kbd>START</kbd>** | **Toggle God Mode** | Toggles invulnerability (damage immunity) ON / OFF |

---

## 🛡️ Stability & Compatibility Notes

- **Pure Headless Cheats**: No modifications are made to the OpenGL/VitaGL rendering pipeline, shaders, or visual popup UI. This eliminates startup crashes and maintains pristine graphic stability.
- **Clean Input Interception**: Combo inputs are trapped and consumed while holding **<kbd>L1</kbd>**, so in-game actions like menu toggling or attacking are never triggered accidentally.

---

## 🕹️ Standard Controls

- **Left Analog / D-Pad**: Move character / Navigate menus
- **Cross**: Attack / Confirm selection
- **Triangle**: Open In-Game Menu
- **R Trigger**: Skip dialogue / Rotate quick skill bar
- **Right Analog**: Use Skills

---

## 📥 Setup Instructions (For End Users)

### Prerequisites
- Install [kubridge](https://github.com/TheOfficialFloW/kubridge/releases/) and [FdFix](https://github.com/TheOfficialFloW/FdFix/releases/) by copying `kubridge.skprx` and `fd_fix.skprx` to your taiHEN plugins folder (`ux0:tai/`) and adding them under `*KERNEL` in `config.txt`:
  ```
  *KERNEL
  ux0:tai/kubridge.skprx
  ux0:tai/fd_fix.skprx
  ```
  *(Note: Do not install `fd_fix.skprx` if you already use the `rePatch` plugin).*
- Install `libshacccg.suprx` if not already installed ([Extraction Guide](https://samilops2.gitbook.io/vita-troubleshooting-guide/shader-compiler/extract-libshacccg.suprx)).
- Overclocking via [PSVshell](https://github.com/Electry/PSVshell/releases) to 500 MHz is recommended.

### Installation
1. Install `zenonia2.vpk` on your PS Vita using VitaShell.
2. Obtain your legal copy of *Zenonia 2 Remaster*.
3. Extract `assets/` and `res/` directories from the APK and transfer them to `ux0:data/zenonia2/`.
4. Extract `libgameDSO.so` from `lib/armeabi-v7a/` inside the APK, copy it to `ux0:data/zenonia2/`, and rename it to `libzenonia2.so`.

---

## 🏗️ Build Instructions (For Developers)

> [!WARNING]
> Do NOT use `vitasdk/vitasdk:latest` as modern toolchains default to `-mfloat-abi=hard`.
> `libzenonia2.so` strictly requires **`softfp`**. Always compile using the softfp Docker container.

Run the build via Podman or Docker:
```bash
podman run --rm -v "$(pwd):/src:z" -w /src/build docker.io/atamanenko/vitasdk-softfp:latest bash -c "make clean && make -j\$(nproc)"
```

For full reverse-engineering symbol listings, memory offsets, and architecture deep dives, refer to [GUIDE.md](GUIDE.md).

---

## Credits

- [TheFloW](https://github.com/TheOfficialFlow) for the original `.so` loader architecture.
- [Rinnegatamante](https://github.com/Rinnegatamante/) for VitaGL and PS Vita porting ecosystem.
- [gl33ntwine](https://github.com/v-atamanenko/) for FalsoNDK and FalsoJNI reimplementations.
- [Rocroverss](https://github.com/Rocroverss) for Livearea assets.
- Ill-Hovercraft8548 for the Zenonia 2 Remaster Android port.