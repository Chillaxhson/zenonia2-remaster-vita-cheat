/*
 * Copyright (C) 2023 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

/**
 * @file  patch.c
 * @brief Patching some of the .so internal functions or bridging them to native
 *        for better compatibility.
 */

#include <kubridge.h>
#include <so_util/so_util.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "utils/logger.h"

extern so_module so_mod;
static so_hook CMvAppC1Ev_hook;
static so_hook CMvPlayerC2Ei_hook;
static so_hook CGsGraphicsC2Ebbbi_hook;

static so_hook get_real_path_hook;
static so_hook CreateInvalidDataPopup_hook;
static so_hook _ZN20GVUIPlayerController19InitialPlayerPadSetEv_hook;
static so_hook _ZN11CMvGraphics10SetQualityE16EnumQualityLevel_hook;

void *g_CMvApp_instance = NULL;
void *g_CMvPlayer_instance = NULL;
void *g_CGsGraphics_instance = NULL;

extern int settings_graphicsqualty;

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

void CMvAppC1Ev_patched(void *this) {
    g_CMvApp_instance = this;
    SO_CONTINUE(void *, CMvAppC1Ev_hook, this);
}

static so_hook CMvPlayerC2Ev_hook;
void CMvPlayerC2Ev_patched(void *this) {
    g_CMvPlayer_instance = this;
    l_debug("CMvPlayer::CMvPlayer(): Hooked into function");
    SO_CONTINUE(void *, CMvPlayerC2Ev_hook, this);
}

void CMvPlayerC2Ei_patched(void *this, int param) {
    g_CMvPlayer_instance = this;
    l_debug("CMvPlayer::CMvPlayer(int): Hooked into function");
    SO_CONTINUE(void *, CMvPlayerC2Ei_hook, this, param);
}

static so_hook CMvPlayer_DoUpdate_hook;
void CMvPlayer_DoUpdate_patched(void *this) {
    g_CMvPlayer_instance = this;
    SO_CONTINUE(void *, CMvPlayer_DoUpdate_hook, this);
}

void CGsGraphicsC2Ebbbi_patched(void *this, int param1, int param2, int param3, int param4) {
    g_CGsGraphics_instance = this;
    l_debug("CGsGraphics::CGsGraphics: Hooked into function");
    SO_CONTINUE(void *, CGsGraphicsC2Ebbbi_hook, this, param1, param2, param3, param4);
}

int get_real_path_patched(const char *in_path, char *out_path) {
    if (!in_path || !out_path) return 0;

    l_debug("get_real_path: in_path='%s'", in_path);

    if (strncmp(in_path, "ux0:", 4) == 0 || strncmp(in_path, "app0:", 5) == 0) {
        snprintf(out_path, 256, "%s", in_path);
        return 1;
    }

    const char *rel = in_path;
    while (*rel == '/') {
        rel++;
    }

    snprintf(out_path, 256, "%s%s", DATA_PATH, rel);
    l_debug("get_real_path: out_path='%s'", out_path);
    return 1;
}

// Suppress "Savefile corrupted, please recreate your character" false popup
void CreateInvalidDataPopup_patched(void *this) {
    l_warn("CMvSystemMenu::CreateInvalidDataPopup called - suppressed!");
}

void _ZN20GVUIPlayerController19InitialPlayerPadSetEv_patched(void *this) {        // how annoying. THIS is what gets rid of the touch display.
    l_debug("GVUIPlayerController::InitialPlayerPadSet: Hooked into function");
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

void _ZN11CMvGraphics10SetQualityE16EnumQualityLevel_patched(void *this, int param) {
    l_debug("CMvGraphics::SetQuality Hooked into function %d vs %d", param, settings_graphicsqualty);

    if (settings_graphicsqualty < 0) {
        settings_graphicsqualty = 0;
    }

    if (settings_graphicsqualty > 2) {
        settings_graphicsqualty = 2;
    }

    if (this) {
        *(int *)((uintptr_t)this + 8) = settings_graphicsqualty;
    }
}

void so_patch(void) {
    CMvAppC1Ev_hook = hook_addr((uintptr_t)so_symbol(&so_mod, "_ZN6CMvAppC1Ev"),
        (uintptr_t)&CMvAppC1Ev_patched);

    CMvPlayerC2Ev_hook = hook_addr((uintptr_t)so_symbol(&so_mod, "_ZN9CMvPlayerC2Ev"),
        (uintptr_t)&CMvPlayerC2Ev_patched);

    CMvPlayerC2Ei_hook = hook_addr((uintptr_t)so_symbol(&so_mod, "_ZN9CMvPlayerC2Ei"),
        (uintptr_t)&CMvPlayerC2Ei_patched);

    CMvPlayer_DoUpdate_hook = hook_addr((uintptr_t)so_symbol(&so_mod, "_ZN9CMvPlayer8DoUpdateEv"),
        (uintptr_t)&CMvPlayer_DoUpdate_patched);

    CGsGraphicsC2Ebbbi_hook = hook_addr((uintptr_t)so_symbol(&so_mod, "_ZN11CGsGraphicsC2Ebbbi"),
        (uintptr_t)&CGsGraphicsC2Ebbbi_patched);        

    get_real_path_hook = hook_addr((uintptr_t)so_symbol(&so_mod, "_Z13get_real_pathPcS_"),
        (uintptr_t)&get_real_path_patched);

    CreateInvalidDataPopup_hook = hook_addr((uintptr_t)so_symbol(&so_mod, "_ZN13CMvSystemMenu22CreateInvalidDataPopupEv"),
        (uintptr_t)&CreateInvalidDataPopup_patched);

    _ZN20GVUIPlayerController19InitialPlayerPadSetEv_hook = hook_addr((uintptr_t)so_symbol(&so_mod, "_ZN20GVUIPlayerController19InitialPlayerPadSetEv"),
        (uintptr_t)&_ZN20GVUIPlayerController19InitialPlayerPadSetEv_patched); 

    _ZN11CMvGraphics10SetQualityE16EnumQualityLevel_hook = hook_addr((uintptr_t)so_symbol(&so_mod, "_ZN11CMvGraphics10SetQualityE16EnumQualityLevel"),
        (uintptr_t)&_ZN11CMvGraphics10SetQualityE16EnumQualityLevel_patched); 

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
    l_info("Patched CGsEncryptFile::LoadBegin checks at %p to guarantee decryption and prevent save deletion.", (void *)lb_base);

    // Bypass anti-tamper DRM checks in CMvGameState::LoadGameData(int slot):
    uintptr_t load_game_data = (uintptr_t)so_symbol(&so_mod, "_ZN12CMvGameState12LoadGameDataEi");
    uintptr_t fn_base = load_game_data ? (load_game_data & ~1) : (so_mod.text_base + 0x103438);

    // 0xee from fn_base (0x103526): 0xd110 (bne.n 10354a) -> NOP (0xbf00) to bypass SaveTime checksum comparison
    uint16_t nop_inst = 0xbf00;
    kuKernelCpuUnrestrictedMemcpy((void *)(fn_base + 0xee), &nop_inst, sizeof(nop_inst));

    // 0x10e from fn_base (0x103546): 0xda09 (bge.n 10355c) -> b.n 10355c (0xe009) to bypass level check and jump straight to success
    uint16_t b_success = 0xe009;
    kuKernelCpuUnrestrictedMemcpy((void *)(fn_base + 0x10e), &b_success, sizeof(b_success));

    kuKernelFlushCaches((void *)(fn_base + 0xee), 0x40);
    l_info("Patched LoadGameData DRM checks at %p to eliminate false 'Savefile corrupted' popup.", (void *)fn_base);

    // Cheats: God Mode & 50x EXP
    CMvPlayer_OnDamaged_hook = hook_addr((uintptr_t)so_symbol(&so_mod, "_ZN9CMvPlayer9OnDamagedEiP9CMvObjectb15EnumElementTypeb"),
        (uintptr_t)&CMvPlayer_OnDamaged_patched);

    CMvPlayer_AddBonusExp_hook = hook_addr((uintptr_t)so_symbol(&so_mod, "_ZN9CMvPlayer11AddBonusExpEj"),
        (uintptr_t)&CMvPlayer_AddBonusExp_patched);

    CMvPlayer_AddExp_hook = hook_addr((uintptr_t)so_symbol(&so_mod, "_ZN9CMvPlayer6AddExpEj"),
        (uintptr_t)&CMvPlayer_AddExp_patched);
}
