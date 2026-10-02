#include "utils/init.h"
#include "utils/glutil.h"

#include <psp2/kernel/threadmgr.h>

#include <falso_jni/FalsoJNI.h>
#include <so_util/so_util.h>

#include "reimpl/controls.h"

#include "audio.h"
#include "utils/logger.h"
#include <kubridge.h>
#include <psp2/io/fcntl.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/clib.h>
#include <psp2/ctrl.h>
#include <string.h>

int _newlib_heap_size_user = 256 * 1024 * 1024;

#ifdef USE_SCELIBC_IO
int sceLibcHeapSize = 4 * 1024 * 1024;
#endif

so_module so_mod;
extern void *g_CMvApp_instance;
extern void *g_CMvPlayer_instance;
extern void *g_CGsGraphics_instance;
extern int cheat_god_mode;
extern int cheat_exp_multiplier;
extern uint32_t current_buttons;

extern int settings_capframerate;

int (* _ZN6CMvApp10EvKeyPressEi)(void *this, int keycode);
int (* _ZN6CMvApp12EvKeyReleaseEi)(void *this, int keycode);

int (* _ZN6CMvApp14EvPointerPressEP15MC_PointerEvent)(void *this, void *event);
int (* _ZN6CMvApp16EvPointerReleaseEP15MC_PointerEvent)(void *this, void *event);
int (* _ZN6CMvApp13EvPointerMoveEP15MC_PointerEvent)(void *this, void *event);

void (* _ZN12CMvCharacter6FullHPEv)(void *this);
void (* _ZN12CMvCharacter5SetSPEib)(void *this, int param1, int param2);

void* (*CMvItemMgr_GetInstPtr)(void);
void (*CMvItemSaveData_IncMoney)(void *this, int money);

static void* (*CMvObjectMgr_GetInstPtr)(void) = NULL;
static void* (*CMvObjectMgr_GetPlayer)(void *this) = NULL;

static void (*CMvPlayer_AddStatPoint)(void *this, int points) = NULL;
static void (*CMvPlayer_AddSkillPoint)(void *this, int points) = NULL;

static void* get_player(void) {
    if (g_CMvPlayer_instance) {
        return g_CMvPlayer_instance;
    }
    if (CMvObjectMgr_GetInstPtr && CMvObjectMgr_GetPlayer) {
        void* objMgr = CMvObjectMgr_GetInstPtr();
        if (objMgr) {
            void* p = CMvObjectMgr_GetPlayer(objMgr);
            if (p) {
                g_CMvPlayer_instance = p;
                return p;
            }
        }
    }
    return NULL;
}

int32_t vita_to_control(int32_t vita_button);

int pressL1 = 0;

static void crash_handler(KuKernelExceptionContext *ctx) {
    char msg[1024];
    uint32_t pc_rel = (ctx->pc >= 0x98000000 && ctx->pc < 0x99000000) ? (ctx->pc - 0x98000000) : 0;
    uint32_t lr_rel = (ctx->lr >= 0x98000000 && ctx->lr < 0x99000000) ? (ctx->lr - 0x98000000) : 0;
    sceClibSnprintf(msg, sizeof(msg),
        "\n========================================\n"
        "FATAL CRASH DETECTED!\n"
        "Exception Type: %u\n"
        "PC: 0x%08X (so+0x%X)\n"
        "LR: 0x%08X (so+0x%X)\n"
        "SP: 0x%08X\n"
        "R0: 0x%08X  R1: 0x%08X\n"
        "R2: 0x%08X  R3: 0x%08X\n"
        "R4: 0x%08X  R5: 0x%08X\n"
        "R6: 0x%08X  R7: 0x%08X\n"
        "R8: 0x%08X  R9: 0x%08X\n"
        "R10: 0x%08X  R11: 0x%08X\n"
        "R12: 0x%08X\n"
        "========================================\n",
        (unsigned int)ctx->exceptionType,
        (unsigned int)ctx->pc, (unsigned int)pc_rel,
        (unsigned int)ctx->lr, (unsigned int)lr_rel,
        (unsigned int)ctx->sp,
        (unsigned int)ctx->r0, (unsigned int)ctx->r1,
        (unsigned int)ctx->r2, (unsigned int)ctx->r3,
        (unsigned int)ctx->r4, (unsigned int)ctx->r5,
        (unsigned int)ctx->r6, (unsigned int)ctx->r7,
        (unsigned int)ctx->r8, (unsigned int)ctx->r9,
        (unsigned int)ctx->r10, (unsigned int)ctx->r11,
        (unsigned int)ctx->r12
    );
    sceClibPrintf("%s", msg);

    SceUID fd = sceIoOpen(DATA_PATH "boot.log", SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 0777);
    if (fd >= 0) {
        sceIoWrite(fd, msg, strlen(msg));
        sceIoClose(fd);
    }
    SceUID cfd = sceIoOpen(DATA_PATH "crash.log", SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
    if (cfd >= 0) {
        sceIoWrite(cfd, msg, strlen(msg));
        sceIoClose(cfd);
    }

    sceKernelExitProcess(0);
}

int main() {
    // Truncate boot.log for fresh launch
    SceUID bfd = sceIoOpen(DATA_PATH "boot.log", SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
    if (bfd >= 0) sceIoClose(bfd);

    l_info("=== Zenonia 2 Starting ===");

    // Register hardware exception handlers via kubridge
    kuKernelRegisterExceptionHandler(KU_KERNEL_EXCEPTION_TYPE_DATA_ABORT, crash_handler, NULL, NULL);
    kuKernelRegisterExceptionHandler(KU_KERNEL_EXCEPTION_TYPE_PREFETCH_ABORT, crash_handler, NULL, NULL);
    kuKernelRegisterExceptionHandler(KU_KERNEL_EXCEPTION_TYPE_UNDEFINED_INSTRUCTION, crash_handler, NULL, NULL);
    l_info("Hardware exception handlers registered.");

    l_info("Calling soloader_init_all...");
    soloader_init_all();
    l_info("soloader_init_all completed.");

    int (*JNI_OnLoad)(void *jvm) = (void *)so_symbol(&so_mod, "JNI_OnLoad");
    void (*NativeInit)(void*, void*, int, int) = (void *)so_symbol(&so_mod, "Java_com_gamevil_nexus2_Natives_NativeInit");
    void (*NativeResize)(void*, void*, int, int) = (void *)so_symbol(&so_mod, "Java_com_gamevil_nexus2_Natives_NativeResize");
    void (*NativeRender)(void*, void*) = (void *)so_symbol(&so_mod, "Java_com_gamevil_nexus2_Natives_NativeRender");

    _ZN12CMvCharacter6FullHPEv = (void *)so_symbol(&so_mod, "_ZN12CMvCharacter6FullHPEv");
    _ZN12CMvCharacter5SetSPEib = (void *)so_symbol(&so_mod, "_ZN12CMvCharacter5SetSPEib");

    CMvItemMgr_GetInstPtr = (void *)so_symbol(&so_mod, "_ZN12CGsSingletonI10CMvItemMgrE10GetInstPtrEv");
    CMvItemSaveData_IncMoney = (void *)so_symbol(&so_mod, "_ZN15CMvItemSaveData8IncMoneyEi");

    CMvObjectMgr_GetInstPtr = (void *)so_symbol(&so_mod, "_ZN12CGsSingletonI12CMvObjectMgrE10GetInstPtrEv");
    CMvObjectMgr_GetPlayer = (void *)so_symbol(&so_mod, "_ZN12CMvObjectMgr9GetPlayerEv");

    CMvPlayer_AddStatPoint = (void *)so_symbol(&so_mod, "_ZN9CMvPlayer12AddStatPointEi");
    CMvPlayer_AddSkillPoint = (void *)so_symbol(&so_mod, "_ZN9CMvPlayer13AddSkillPointEi");

    _ZN6CMvApp10EvKeyPressEi = (void *)so_symbol(&so_mod, "_ZN6CMvApp10EvKeyPressEi");
    _ZN6CMvApp12EvKeyReleaseEi = (void *)so_symbol(&so_mod, "_ZN6CMvApp12EvKeyReleaseEi");

    _ZN6CMvApp14EvPointerPressEP15MC_PointerEvent = (void *)so_symbol(&so_mod, "_ZN6CMvApp14EvPointerPressEP15MC_PointerEvent");
    _ZN6CMvApp16EvPointerReleaseEP15MC_PointerEvent = (void *)so_symbol(&so_mod, "_ZN6CMvApp16EvPointerReleaseEP15MC_PointerEvent");
    _ZN6CMvApp13EvPointerMoveEP15MC_PointerEvent = (void *)so_symbol(&so_mod, "_ZN6CMvApp13EvPointerMoveEP15MC_PointerEvent");

    l_info("Calling JNI_OnLoad...");
    JNI_OnLoad(&jvm);
    l_info("JNI_OnLoad completed.");

    l_info("Calling audio_init...");
    audio_init();
    l_info("audio_init completed.");

    l_info("Calling gl_init...");
    gl_init();
    l_info("gl_init completed.");

    if(settings_capframerate) {
        l_info("settings_capframerate is true, calling eglSwapInterval(0, 2)...");
        eglSwapInterval(0, 2);
    }

    l_info("Calling NativeInit(960, 544)...");
    NativeInit(&jvm, NULL, 960, 544);
    l_info("NativeInit completed.");

    l_info("Calling NativeResize(960, 544)...");
    NativeResize(&jvm, NULL, 960, 544);
    l_info("NativeResize completed.");

    l_info("Entering main loop...");
    int frame_count = 0;
    while (1) {
        controls_poll();
        NativeRender(&jvm, NULL);
        gl_swap();
        if (frame_count < 10) {
            frame_count++;
            l_info("Render loop frame %d completed.", frame_count);
        }
    }
    sceKernelExitDeleteThread(0);
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
                        } else {
                            l_warn("Cheat: Refill HP & SP failed - player is NULL");
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
                            } else {
                                l_warn("Cheat: Add Gold failed - itemMgr is NULL");
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
                        } else {
                            l_warn("Cheat: Add Stat Points failed - player is NULL");
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
                        } else {
                            l_warn("Cheat: Add Skill Points failed - player is NULL");
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

void controls_handler_touch(int32_t id, float x, float y, ControlsAction action) {
    if (g_CMvApp_instance) {
        // the game defaults to an internal resolution of 400x240, so we need to scale the touch coordinates accordingly
        float xx = x * ((float)400 / (float)960); 
        float yy = y * ((float)240 / (float)544);

        int touches[2] = { (int)xx, (int)yy };

        switch (action) {
            case CONTROLS_ACTION_DOWN:
                l_debug("controls_handler_touch: PointerPress at (%f, %f)", xx, yy);
                _ZN6CMvApp14EvPointerPressEP15MC_PointerEvent(g_CMvApp_instance, touches);
                break;
            case CONTROLS_ACTION_UP:
                l_debug("controls_handler_touch: PointerRelease at (%f, %f)", x, y);
                _ZN6CMvApp16EvPointerReleaseEP15MC_PointerEvent(g_CMvApp_instance, touches);
                break;
            case CONTROLS_ACTION_MOVE:
                l_debug("controls_handler_touch: PointerMove at (%f, %f)", x, y);
                _ZN6CMvApp13EvPointerMoveEP15MC_PointerEvent(g_CMvApp_instance, touches);
                break;
        }
    } else {
        l_debug("controls_handler_touch: g_GVUISystem_instance is NULL, cannot handle touch event");
    }
}

void controls_handler_analog(ControlsStickId which, float x, float y, ControlsAction action) {
    if(which == CONTROLS_STICK_LEFT) {

        if(action == CONTROLS_ACTION_MOVE){
            if(x  > 0.5) {
                _ZN6CMvApp10EvKeyPressEi(g_CMvApp_instance, -4);
            } else if(x < -0.5) {
                _ZN6CMvApp10EvKeyPressEi(g_CMvApp_instance, -3);
            }
            
            if(y > 0.5) {
                _ZN6CMvApp10EvKeyPressEi(g_CMvApp_instance, -2);
            } else if(y < -0.5) {
                _ZN6CMvApp10EvKeyPressEi(g_CMvApp_instance, -1);
            } 
        }

        if(action == CONTROLS_ACTION_UP){
            _ZN6CMvApp12EvKeyReleaseEi(g_CMvApp_instance, -1);
            _ZN6CMvApp12EvKeyReleaseEi(g_CMvApp_instance, -2);
            _ZN6CMvApp12EvKeyReleaseEi(g_CMvApp_instance, -3);
            _ZN6CMvApp12EvKeyReleaseEi(g_CMvApp_instance, -4);
        }
    }

    if(which == CONTROLS_STICK_RIGHT) {

        if(action == CONTROLS_ACTION_MOVE){
            if(x  > 0.5) {
                _ZN6CMvApp10EvKeyPressEi(g_CMvApp_instance, 51);
            } else if(x < -0.5) {
                _ZN6CMvApp10EvKeyPressEi(g_CMvApp_instance, 49);
            }
            
            if(y > 0.5) {
                _ZN6CMvApp10EvKeyPressEi(g_CMvApp_instance, 55);
            } else if(y < -0.5) {
                _ZN6CMvApp10EvKeyPressEi(g_CMvApp_instance, 57);
            } 
        }

        if(action == CONTROLS_ACTION_UP){
            _ZN6CMvApp12EvKeyReleaseEi(g_CMvApp_instance, 49); // use skill 1
            _ZN6CMvApp12EvKeyReleaseEi(g_CMvApp_instance, 51); // use skill 2
            _ZN6CMvApp12EvKeyReleaseEi(g_CMvApp_instance, 55); // use skill 3
            _ZN6CMvApp12EvKeyReleaseEi(g_CMvApp_instance, 57); // use skill 4
        }
    }

}

int32_t vita_to_control(int32_t vita_button) {
    switch (vita_button) {
        /*
        Nothing      : 0
        Move Up      : -1
        Move Down    : -2
        Move Left    : -3
        Move Right   : -4
        ???          : -5
        Show Map     : -6
        Attack       : -7
        Unknown Menu : -8
        Change Volume Up   : -13
        Change Volume Down      : -14
        Show Menu    : -16
        Quick Save   : -10
        Unknown Menu 2 : -11
        Change Skill Row: 35
        Use Skill 1 : 49
        run north?   : 50
        Use Skill 2 : 51
        run south?   : 52
        Attack      : 53 
        run east?   : 54
        Use Skill 3 : 55
        run south?   : 56
        Use Skill 4 : 57
        */

        case AKEYCODE_DPAD_UP: return -1;
        case AKEYCODE_DPAD_DOWN: return -2;
        case AKEYCODE_DPAD_LEFT: return -3;
        case AKEYCODE_DPAD_RIGHT: return -4;
        case AKEYCODE_BUTTON_A: return -7;
        //case AKEYCODE_BUTTON_X: return -13;
        case AKEYCODE_BUTTON_Y: return -16;
        case AKEYCODE_BUTTON_START: return -6;
        case AKEYCODE_BUTTON_L1: return 0; 
        
        case AKEYCODE_BUTTON_B: return -12;
        case AKEYCODE_BUTTON_R1: return 11; 

        //case AKEYCODE_BUTTON_SELECT: return -10;
        
        default: return 0;
    }
}