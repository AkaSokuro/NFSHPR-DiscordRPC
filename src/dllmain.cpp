// @AkaSokuro
#include "config.h"

#include <discord_game_sdk/discord.h>
#include <inireader/INIReader.h>

#include <ctime>
#include <stdio.h>
#include <windows.h>
#include <iostream>

namespace {
    discord::Core* core{};
    discord::Activity activity{};
}

bool AccessMemory(const std::wstring& moduleName, uintptr_t baseOffset, const std::vector<intptr_t>& offsets,
    void* buffer, size_t size, bool write = false) {
    HMODULE hModule = GetModuleHandle(moduleName.c_str());
    if (!hModule) {
        return false;
    }

    uintptr_t address = (uintptr_t)hModule + baseOffset;

    for (size_t i = 0; i < offsets.size(); ++i) {
        if (!ReadProcessMemory(GetCurrentProcess(), (LPCVOID)address, &address, sizeof(address), nullptr)) {
            std::cerr << "Failed to read memory at step " << i << std::endl;
            return false;
        }
        address += offsets[i];
    }

    if (write) {
        if (!WriteProcessMemory(GetCurrentProcess(), (LPVOID)address, buffer, size, nullptr)) {
            std::cerr << "Failed to write memory at final address." << std::endl;
            return false;
        }
    }
    else {
        if (!ReadProcessMemory(GetCurrentProcess(), (LPCVOID)address, buffer, size, nullptr)) {
            std::cerr << "Failed to read memory at final address." << std::endl;
            return false;
        }
    }

    return true;
}


void UpdateActivity(const std::string& state, const std::string& details) {
    if (!core) return;

    activity.SetState(state.c_str());
    activity.SetDetails(details.c_str());

    core->ActivityManager().UpdateActivity(activity, [](discord::Result result) {
        std::cout << ((result == discord::Result::Ok) ? "Succeeded" : "Failed")
            << " updating activity!\n";
        });
}

void Update() {
    INIReader ini(CONFIG_NAME);

    int gameMode;
    AccessMemory(GAMEPROC_NAME, 0x129C4A8, { 0x67A00 }, &gameMode, sizeof(gameMode));
    std::string gameModeStr;
    switch (gameMode) {
    case 529909: gameModeStr = "Hot Pursuit"; break;
    case 529914: gameModeStr = "Interceptor"; break;
    case 529920: gameModeStr = "Race"; break;
    case 1100320: gameModeStr = "Most Wanted"; break;
    case 1104000: gameModeStr = "Arm Race"; break;
    default: gameModeStr = "No Mode"; break;
    }

    int series;
    AccessMemory(GAMEPROC_NAME, 0x01276650, { 0x5B9B4 }, &series, sizeof(series));
    std::string seriesStr;
    switch (series) {
    case 1: seriesStr = "Sports"; break;
    case 2: seriesStr = "Performance"; break;
    case 3: seriesStr = "Super"; break;
    case 4: seriesStr = "Exotic"; break;
    case 5: seriesStr = "Hyper"; break;
    default: seriesStr = "Sport"; break;
    }

    int lobbyType;
    AccessMemory(GAMEPROC_NAME, 0x01276650, { 0x5B9B8 }, &lobbyType, sizeof(lobbyType));
    std::string lobbyTypeStr;
    switch (lobbyType) {
    case 2: lobbyTypeStr = "Invite Only"; break;
    case 3: lobbyTypeStr = "Friends Only"; break;
    default: lobbyTypeStr = "Public"; break;
    }

    signed int teamId;
    AccessMemory(GAMEPROC_NAME, 0x129C4A8, { 0x67A38 }, &teamId, sizeof(teamId));
    std::string teamStr;
    switch (teamId) {
    case 0: teamStr = "Racer"; break;
    case 1: teamStr = "Cop"; break;
    default: teamStr = "Racer"; break;
    }

    int isMw;
    AccessMemory(GAMEPROC_NAME, 0x129C4A8, { 0x67A78 }, &isMw, sizeof(isMw));
    if (isMw == 0 && gameMode == 1100320) {
        teamStr = "MW";
    }

    int plrSize;
    AccessMemory(GAMEPROC_NAME, 0x1288A58, { 0x8, 0x19640, 0x768, 0x270, 0x8, 0x78, -0x22C8 }, &plrSize, sizeof(plrSize));

    int inWorld;
    AccessMemory(GAMEPROC_NAME, 0x1296B78, { 0x2784 }, &inWorld, sizeof(inWorld));

    char inWorldOffline;
    AccessMemory(GAMEPROC_NAME, 0x129C498, { 0x118038 }, &inWorldOffline, sizeof(inWorldOffline));

    char plrNameBuffer[16] = {};
    AccessMemory(GAMEPROC_NAME, 0x01279210, { 0xFD9F }, &plrNameBuffer, sizeof(plrNameBuffer));
    std::string plrName(plrNameBuffer);

    std::string showLobbyType = ini.Get("Config", "ShowLobbyType", "1");
    std::string showGameMode = ini.Get("Config", "ShowGameMode", "1");
    std::string showSeries = ini.Get("Config", "ShowCarSeries", "1");
    std::string showTeam = ini.Get("Config", "ShowRole", "1");
    std::string showPlayerSize = ini.Get("Config", "ShowPlayerSize", "1");

    if (showLobbyType == "0") { lobbyTypeStr = ""; }
    if (showGameMode == "0") { gameModeStr = ""; }
    if (showSeries == "0") { seriesStr = ""; }
    if (showTeam == "0") { teamStr = ""; }

    if (gameMode == 0) {
        activity.GetParty().GetSize().SetCurrentSize(0);
        activity.GetParty().GetSize().SetMaxSize(0);

        if (inWorldOffline == 1) {
            UpdateActivity("Singleplayer Event", "In Game");
        }
        else {
            if (!plrName.empty()) { UpdateActivity("Idle", "In Autolog"); }
            else { UpdateActivity("Offline - Idle", "In Autolog"); }
        }
        return;
    }

    if (showPlayerSize == "1") {
        activity.GetParty().GetSize().SetCurrentSize(plrSize);
        if (gameMode == 529914) { activity.GetParty().GetSize().SetMaxSize(2); }
        else { activity.GetParty().GetSize().SetMaxSize(8); }
    }

    std::string state = lobbyTypeStr;
    if (!state.empty()) state += " - ";
    if (inWorld == 0) { state += "In Lobby"; } else { state += "In Game"; }

    std::string details;
    if (!gameModeStr.empty()) {
        details += gameModeStr;
    }
    if (!seriesStr.empty()) {
        if (!details.empty()) details += " - ";
        details += seriesStr;
    }
    if (!teamStr.empty()) {
        if (!details.empty()) details += " : ";
        details += teamStr;
    }
    if (details.empty()) { details = "CLASSIFIED"; }

    UpdateActivity(state, details);
}

DWORD WINAPI MainThread(LPVOID lpParam) {
    INIReader ini(CONFIG_NAME);
    if (ini.ParseError() < 0) { return 0; }

    std::string disableRPC = ini.Get("Config", "DisableRPC", "0");
    if (disableRPC == "1") { return 0; }

    auto result = discord::Core::Create(1316417194032238632, DiscordCreateFlags_Default, &core);
    if (result != discord::Result::Ok || !core) {
        printf("Failed to initialize Discord Core: %d\n", static_cast<int>(result));
        return 0;
    }

    std::string icon = ini.Get("Config", "Icon", IMG);
    // activity.SetName("NFS: Hot Pursuit Remastered");
    activity.GetAssets().SetLargeImage(icon.c_str());
    activity.GetAssets().SetLargeText(IMG_TXT);
    activity.GetTimestamps().SetStart(time(0));
    activity.SetType(discord::ActivityType::Playing);
    UpdateActivity("Idle", "In Autolog");

    core->ActivityManager().RegisterSteam(GAME_ID);

    while (true) {
        Update();
        core->RunCallbacks();
        Sleep(UPDATE_INTERVAL);
    }
    return 0;
}

// DLL entry point
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        CreateThread(NULL, 0, MainThread, hModule, 0, NULL);
    }
    return TRUE;
}
