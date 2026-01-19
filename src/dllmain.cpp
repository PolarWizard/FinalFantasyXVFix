/*
 * MIT License
 *
 * Copyright (c) 2025 Dominik Protasewicz
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

// System includes
#include <windows.h>
#include <psapi.h>
#include <shlwapi.h>
#include <fstream>
#include <iostream>
#include <string>
#include <filesystem>
#include <format>
#include <numeric>
#include <numbers>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <bit>

// Local includes
#include "utils.hpp"

// Version string
constexpr std::string VERSION = "1.0.0";

// .yml to struct
typedef struct hud_t {
    bool enable;
} hud_t;

typedef struct fps_t {
    bool enable;
    u32 value;
} fps_t;

typedef struct features_t {
    hud_t hud;
    fps_t fps;
} features_t;

typedef struct yml_t {
    std::string name;
    bool masterEnable;
    features_t feature;
} yml_t;

// Globals
namespace {
    Utils::ModuleInfo module(GetModuleHandle(nullptr));

    u32 currWidth = 0;
    u32 currHeight = 0;
    f32 currAspectRatio = 0;

    f32 nativeAspectRatio = (16.0f / 9.0f);

    YAML::Node config = YAML::LoadFile("FinalFantasyXVFix.yml");
    yml_t yml{};
}

/**
 * @brief Opens and initializes logging system.
 *
 * @return void
 */
void logOpen() {
    // spdlog initialisation
    auto logger = spdlog::basic_logger_mt("FinalFantasyXVFix", "FinalFantasyXVFix.log", true);
    spdlog::set_default_logger(logger);
    spdlog::flush_on(spdlog::level::debug);

    // Get game name and exe path
    WCHAR exePath[_MAX_PATH] = { 0 };
    GetModuleFileNameW(module.address, exePath, MAX_PATH);
    std::filesystem::path exeFilePath = exePath;
    module.name = exeFilePath.filename().string();

    // Log module details
    LOG("-------------------------------------");
    LOG("Compiler: {:s}", Utils::getCompilerInfo());
    LOG("Compiled: {:s} at {:s}", __DATE__, __TIME__);
    LOG("Version: {:s}", VERSION);
    LOG("Module Name: {:s}", module.name);
    LOG("Module Path: {:s}", exeFilePath.string());
    LOG("Module Addr: 0x{:x}", reinterpret_cast<u64>(module.address));
    LOG("-------------------------------------");
}

/**
 * @brief Closes and cleans up the logging system.
 *
 * @details
 * Flushes all pending log messages, drops all loggers, and shuts down the spdlog system.
 * This function should be called when the DLL is being unloaded to ensure all log data
 * is properly written to disk.
 *
 * @return void
 */
void logClose() {
    spdlog::drop_all();
    spdlog::shutdown();
}

/**
 * @brief Reads and parses configuration settings from a YAML file.
 *
 * @return void
 */
void readYml() {
    yml.name = config["name"].as<std::string>();

    yml.masterEnable = config["masterEnable"].as<bool>();

    yml.feature.hud.enable = config["features"]["hud"]["enable"].as<bool>();

    yml.feature.fps.enable = config["features"]["fps"]["enable"].as<bool>();
    yml.feature.fps.value = config["features"]["fps"]["value"].as<u32>();

    // Get that info!
    LOG("Name: {}", yml.name);
    LOG("MasterEnable: {}", yml.masterEnable);
    LOG("Feature.Hud.Enable: {}", yml.feature.hud.enable);
    LOG("Feature.Fps.Enable: {}", yml.feature.fps.enable);
    LOG("Feature.Fps.Value: {}", yml.feature.fps.value);
}

/**
 * @brief Gets the current resolution settings.
 *
 * @details
 * This function gets the current resolution settings set ingame. This game has excellent ultrawide support,
 * so there is nothing to do ourselves we just need those resolution settings so that the HUD feature works
 * properly for any resolution value selected.
 *
 * How was this found?
 * This is a modern game using a modern engine, you wont find a resolution table in the exe, as that stuff
 * is fetched dynamically using DirectX APIs, so regardless of your monitor resolution the game will give
 * a plethora of resolution choices for you to choose from that will work on your monitor.
 *
 * Thankfully this wasnt too hard to break down. This is strictly an ultrawide mod, so using Cheat Engine
 * I scanned for the current resolution width and I kept repeating the process until I got a small number
 * of hits. I landed on two hits that are in game exe memory space. One was used as read/write, but the other
 * one was write only:
 * ffxv_s.exe+749AAC2 - 0F11 05 276416FD      - movups [ffxv_s.exe+4600EF0],xmm0
 *
 * What is being written above is actually the rect representing the viewport where first 2 bytes represent
 * the origin coordinates, typically (0,0) starting top left of the window, and the next 2 bytes would be the
 * end coordinates, typically (width, height) ending bottom right of the window. We are only interested in the
 * width height pair so we grab that from xmm0, do the calculations we need, and cache it so its usable by the
 * HUD feature so that the HUD can be constrained and centered properly to 16:9.
 *
 * @return void
 */
void getResolution() {
    Utils::SignatureHook hook1 {
        .tag = __func__,
        .signature = "0F 11 05 ?? ?? ?? ??    0F 10 8A 20 01 00 00"
    };

    bool enable = yml.masterEnable;
    LOG("Fix {}", enable ? "Enabled" : "Disabled");
    Utils::injectHook(enable, module, hook1,
        [](SafetyHookContext& ctx) {
            currWidth = std::bit_cast<u32>(ctx.xmm0.f32[2]);
            currHeight = std::bit_cast<u32>(ctx.xmm0.f32[3]);
            currAspectRatio = static_cast<f32>(currWidth) / static_cast<f32>(currHeight);
        }
    );
}

/**
 * @brief Constrains and centers the HUD to 16:9 boundaries.
 *
 * @details
 * This function applies two patches to the HUD logic code and then injects a hook which will constrain and
 * center the HUD to 16:9 boundaries.
 *
 * How was this found?
 * This was a good 2 to 3 hours of head scratching and just brute forcing various strategies hoping that
 * something stuck and would make a visible HUD change. Despite using DirectX the game had no projection
 * matrix's in its memory, meaning it was all most likely calculated on the fly in functions in a just-in-time
 * fashion when it was needed.
 *
 * Thankfully using DirectX we know for a fact that this game must store NDCs (Normalized Device Coordinates)
 * somwhere for each UI object. Thankfully after quite a bit of digging and experimenting, I had a few good hits
 * for 2/width, an NDC, which led me to some code which had a lot of hardcoded 1920 and 1080 numbers. So
 * regardless of resolution the game was applying normalization to it somehow and this led me to this monstrosity:
 * ffxv_s.exe+7F2FD3 - 74 37                 - je ffxv_s.exe+7F300C
 * ffxv_s.exe+7F2FD5 - 83 F9 02              - cmp ecx,02
 * ffxv_s.exe+7F2FD8 - 74 0D                 - je ffxv_s.exe+7F2FE7
 * ffxv_s.exe+7F2FDA - 83 F9 05              - cmp ecx,05
 * ffxv_s.exe+7F2FDD - 75 5E                 - jne ffxv_s.exe+7F303D
 * ffxv_s.exe+7F2FDF - 41 B9 80070000        - mov r9d,00000780
 * ffxv_s.exe+7F2FE5 - EB 56                 - jmp ffxv_s.exe+7F303D
 * ffxv_s.exe+7F2FE7 - 41 69 C9 38040000     - imul ecx,r9d,00000438
 * ffxv_s.exe+7F2FEE - B8 49D6B9F2           - mov eax,F2B9D649
 * ffxv_s.exe+7F2FF3 - 0F57 C0               - xorps xmm0,xmm0
 * ffxv_s.exe+7F2FF6 - F7 E1                 - mul ecx
 * ffxv_s.exe+7F2FF8 - C1 EA 0A              - shr edx,0A
 * ffxv_s.exe+7F2FFB - 8D 82 80F8FFFF        - lea eax,[rdx-00000780]
 * ffxv_s.exe+7F3001 - F3 48 0F2A C0         - cvtsi2ss xmm0,rax
 * ffxv_s.exe+7F3006 - F3 0F58 F0            - addss xmm6,xmm0
 * ffxv_s.exe+7F300A - EB 31                 - jmp ffxv_s.exe+7F303D
 * ffxv_s.exe+7F300C - 41 69 C9 38040000     - imul ecx,r9d,00000438
 * ffxv_s.exe+7F3013 - B8 49D6B9F2           - mov eax,F2B9D649
 * ffxv_s.exe+7F3018 - 0F57 C0               - xorps xmm0,xmm0
 * ffxv_s.exe+7F301B - F7 E1                 - mul ecx
 * ffxv_s.exe+7F301D - C1 EA 0A              - shr edx,0A
 * ffxv_s.exe+7F3020 - 8D 82 80F8FFFF        - lea eax,[rdx-00000780]
 * ffxv_s.exe+7F3026 - D1 E8                 - shr eax,1
 * ffxv_s.exe+7F3028 - F3 48 0F2A C0         - cvtsi2ss xmm0,rax
 * ffxv_s.exe+7F302D - F3 0F58 F0            - addss xmm6,xmm0
 * ffxv_s.exe+7F3031 - EB 0A                 - jmp ffxv_s.exe+7F303D
 * ffxv_s.exe+7F3033 - 44 8B 44 24 4C        - mov r8d,[rsp+4C]
 * ffxv_s.exe+7F3038 - 44 8B 4C 24 48        - mov r9d,[rsp+48]
 * ffxv_s.exe+7F303D - F3 0F10 05 EF148902   - movss xmm0,[ffxv_s.exe+3084534]
 *
 * From here I just started noping some of the jumps just to see what would happen and the UI started getting
 * squished, stretched, or disappeared, so I was definitely in the right area.
 *
 * After some experimentation I applied the first patches and hook as seen below, and then just slowly tweaked
 * things so they worked as expected for all ultrawide resolutions. In the hook itself I do some wierd math
 * which also took some time to understand why it works. I'll spare the majority of details, but as mentioned
 * earlier the 1920 and 1080 hardcoded values are very important in the calculations and if you look at the code
 * you will see that the calculations always revolve around those numbers.
 *
 * @return void
 */
void featureHud() {
    Utils::SignaturePatch patch1 {
        .tag = __func__,
        .signature = "74 11    41 B9 80 07 00 00    41 B8 38 04 00 00    E9 93 00 00 00",
        .patch = "90 90"
    };
    Utils::SignaturePatch patch2 {
        .tag = __func__,
        .signature = "E9 93 00 00 00    48 8B CF    E8 ?? ?? ?? ??    44 8B F0",
        .patch = "E9 7E 00 00 00"
    };
    Utils::SignatureHook hook1 {
        .tag = __func__,
        .signature = "F3 48 0F 2A C0    F3 0F 58 F0    EB 0A"
    };

    bool enable = yml.masterEnable & yml.feature.hud.enable;
    LOG("Fix {}", enable ? "Enabled" : "Disabled");
    Utils::injectPatch(enable, module, patch1);
    Utils::injectPatch(enable, module, patch2);
    Utils::injectHook(enable, module, hook1,
        [](SafetyHookContext& ctx) {
            f32 value = *reinterpret_cast<f32*>(ctx.rdi + 0x250);
            u32 adjustedWidth = static_cast<u32>((currAspectRatio / nativeAspectRatio) * 1920.0f);
            u32 adjustedOffset = static_cast<u32>((static_cast<f32>(adjustedWidth) - 1920.0f) / 2.0f);
            ctx.rax = ((value != 0) && (value != static_cast<f32>(adjustedOffset))) ? 0 : adjustedOffset;
            ctx.r9 = adjustedWidth;
        }
    );
}

/**
 * @brief Sets the FPS to the user specified value.
 *
 * @details
 * This function injects a new user specified FPS cap from the YAML file.
 *
 * How was this found?
 * This was comically easy to not only find, but also fix. Using Cheat Engine I initially did an integer scan
 * for the currently set FPS value in the graphics settings. Then I changed the value in the settings and did
 * a subsequent scan which cut down on memory locations, and kept doing that until I got a small list of about
 * 5 memory locations that where changing to the FPS number that I set in the settings. One by one I edited
 * them until I found one that not only changed the FPS cap in game, but also changed all the other ones.
 *
 * The memory location that controlled everything was then monitored to see what accesses it, and eventually
 * I led to the exact code that reads it, this tiny getter function:
 * ffxv_s.exe+4D2BA0 - 8B 41 08              - mov eax,[rcx+08]
 * ffxv_s.exe+4D2BA3 - C3                    - ret
 *
 * And just for completeness, the setter as well:
 * ffxv_s.exe+2615A70 - 89 51 08              - mov [rcx+08],edx
 * ffxv_s.exe+2615A73 - C3                    - ret
 *
 * The solution here really is not that good, but given the number of replica getters in the game code, having
 * a hook directly in the function just isnt feasible, so we hook around a function that the game constantly calls
 * and we inject a new value where the game constantly gets the FPS cap.
 *
 * It should also be noted that there are many locations that call the FPS cap getter function. If they all are not
 * in sync with one another, the game either runs faster or slower than normal.
 *
 * @note This feature makes the in game FPS cap setting useless.
 *
 * @return void
 */
void featureFps() {
    Utils::SignatureHook hook1 {
        .tag = __func__,
        .signature = "FF 50 08    85 C0    7E 1F    48 8B 4B 08"
    };

    bool enable = yml.masterEnable & yml.feature.fps.enable;
    LOG("Fix {}", enable ? "Enabled" : "Disabled");
    Utils::injectHook(enable, module, hook1,
        [](SafetyHookContext& ctx) {
            u32 fps = yml.feature.fps.value;
            *reinterpret_cast<u32*>(ctx.rcx + 0x8) = fps == 0 ? 9999 : fps; // 9999... you get the reference?
        }
    );
}

/**
 * @brief This function serves as the entry point for the DLL. It performs the following tasks:
 * 1. Initializes the logging system.
 * 2. Reads the configuration from a YAML file.
 * 3. Applies a center UI fix.
 *
 * @param lpParameter Unused parameter.
 * @return Always returns TRUE to indicate successful execution.
 */
DWORD WINAPI Main(void* lpParameter) {
    logOpen();
    readYml();
    getResolution();
    featureHud();
    featureFps();
    logClose();
    return true;
}

/**
 * @brief Entry point for a DLL, called by the system when the DLL is loaded or unloaded.
 *
 * This function handles various events related to the DLL's lifetime and performs actions
 * based on the reason for the call. Specifically, it creates a new thread when the DLL is
 * attached to a process.
 *
 * @details
 * The `DllMain` function is called by the system when the DLL is loaded or unloaded. It handles
 * different reasons for the call specified by `ul_reason_for_call`. In this implementation:
 *
 * - **DLL_PROCESS_ATTACH**: When the DLL is loaded into the address space of a process, it
 *   creates a new thread to run the `Main` function. The thread priority is set to the highest,
 *   and the thread handle is closed after creation.
 *
 * - **DLL_THREAD_ATTACH**: Called when a new thread is created in the process. No action is taken
 *   in this implementation.
 *
 * - **DLL_THREAD_DETACH**: Called when a thread exits cleanly. No action is taken in this implementation.
 *
 * - **DLL_PROCESS_DETACH**: Called when the DLL is unloaded from the address space of a process.
 *   No action is taken in this implementation.
 *
 * @param hModule Handle to the DLL module. This parameter is used to identify the DLL.
 * @param ul_reason_for_call Indicates the reason for the call (e.g., process attach, thread attach).
 * @param lpReserved Reserved for future use. This parameter is typically NULL.
 * @return BOOL Always returns TRUE to indicate successful execution.
 */
BOOL APIENTRY DllMain(
    HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
) {
    HANDLE mainHandle;
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        mainHandle = CreateThread(NULL, 0, Main, 0, NULL, 0);
        if (mainHandle)
        {
            SetThreadPriority(mainHandle, THREAD_PRIORITY_HIGHEST);
            CloseHandle(mainHandle);
        }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
