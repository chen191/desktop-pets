#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#include <windows.h>

#include <iostream>
#include <string>

namespace {
#ifdef FLAME_MOUSE_EDITION
constexpr wchar_t kWindowClass[] = L"FlameKeyboardPetMouseEditionTestWindow";
#else
constexpr wchar_t kWindowClass[] = L"FlameKeyboardPetTestWindow";
#endif
constexpr UINT kQueryModeMessage = WM_APP + 17;
constexpr UINT kQuerySizeMessage = WM_APP + 19;
constexpr UINT kSmallSizeCommand = 1004;
constexpr UINT kStandardSizeCommand = 1005;
constexpr UINT kLargeSizeCommand = 1006;
constexpr LRESULT kTypingMode = 1;
#ifdef FLAME_MOUSE_EDITION
constexpr LRESULT kMousingMode = 2;
#else
constexpr LRESULT kCuteMode = 0;
#endif
}

int wmain(int argc, wchar_t** argv) {
    SetProcessDPIAware();
    if (argc != 2) {
        std::cerr << "usage: keyboard_integration_test.exe <pet.exe>\n";
        return 2;
    }

    std::wstring command = L"\"" + std::wstring(argv[1]) + L"\" --test-fast";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, 0,
                        nullptr, nullptr, &startup, &process)) {
        std::cerr << "CreateProcess failed: " << GetLastError() << '\n';
        return 3;
    }

    HWND window = nullptr;
    for (int attempt = 0; attempt < 80 && !window; ++attempt) {
        Sleep(50);
        window = FindWindowW(kWindowClass, nullptr);
    }
    if (!window) {
        TerminateProcess(process.hProcess, 4);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        std::cerr << "pet window not found\n";
        return 4;
    }
    const LRESULT initialSize = SendMessageW(window, kQuerySizeMessage, 0, 0);

    INPUT input[2]{};
    input[0].type = INPUT_KEYBOARD;
    input[0].ki.wVk = 'A';
    input[1] = input[0];
    input[1].ki.dwFlags = KEYEVENTF_KEYUP;
    if (SendInput(2, input, sizeof(INPUT)) != 2) {
        PostMessageW(window, WM_CLOSE, 0, 0);
        std::cerr << "SendInput failed: " << GetLastError() << '\n';
        return 5;
    }
    Sleep(45);
    const LRESULT mode = SendMessageW(window, kQueryModeMessage, 0, 0);
#ifndef FLAME_MOUSE_EDITION
    Sleep(1250);
#endif
    POINT originalCursor{};
    GetCursorPos(&originalCursor);
    INPUT mouse{};
    mouse.type = INPUT_MOUSE;
    mouse.mi.dx = 1;
    mouse.mi.dwFlags = MOUSEEVENTF_MOVE;
    if (SendInput(1, &mouse, sizeof(INPUT)) != 1) {
        PostMessageW(window, WM_CLOSE, 0, 0);
        std::cerr << "Mouse SendInput failed: " << GetLastError() << '\n';
        return 6;
    }
    Sleep(45);
    const LRESULT modeAfterMouse = SendMessageW(window, kQueryModeMessage, 0, 0);
    SetCursorPos(originalCursor.x, originalCursor.y);
    SendMessageW(window, WM_COMMAND, kSmallSizeCommand, 0);
    Sleep(80);
    const LRESULT smallSize = SendMessageW(window, kQuerySizeMessage, 0, 0);
    SendMessageW(window, WM_COMMAND, kStandardSizeCommand, 0);
    Sleep(80);
    const LRESULT standardSize = SendMessageW(window, kQuerySizeMessage, 0, 0);
    SendMessageW(window, WM_COMMAND, kLargeSizeCommand, 0);
    Sleep(80);
    const LRESULT selectedSize = SendMessageW(window, kQuerySizeMessage, 0, 0);
    RECT rect{};
    GetWindowRect(window, &rect);
    PostMessageW(window, WM_CLOSE, 0, 0);
    WaitForSingleObject(process.hProcess, 3000);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);

    if (mode != kTypingMode) {
        std::cerr << "expected typing mode, got " << mode << '\n';
        return 7;
    }
#ifdef FLAME_MOUSE_EDITION
    if (modeAfterMouse != kMousingMode) {
        std::cerr << "expected mousing mode, got " << modeAfterMouse << '\n';
        return 8;
    }
#else
    if (modeAfterMouse != kCuteMode) {
        std::cerr << "expected mouse input to leave cute mode unchanged, got "
                  << modeAfterMouse << '\n';
        return 8;
    }
#endif
    if (initialSize != 128) {
        std::cerr << "expected default 128px window, got " << initialSize << '\n';
        return 9;
    }
    if (smallSize != 96 || standardSize != 128) {
        std::cerr << "expected size sequence 96 -> 128, got " << smallSize
                  << " -> " << standardSize << '\n';
        return 10;
    }
    if (selectedSize != 160 || rect.right - rect.left != 160 ||
        rect.bottom - rect.top != 160) {
        std::cerr << "expected 160px window, got selected=" << selectedSize
                  << " rect=" << (rect.right - rect.left) << 'x'
                  << (rect.bottom - rect.top) << '\n';
        return 11;
    }
#ifdef FLAME_MOUSE_EDITION
    std::cout << "keyboard_integration_test=OK typing=1 mousing=2 sizes=96/128/160 default=128\n";
#else
    std::cout << "keyboard_integration_test=OK typing=1 mouse_ignored=1 sizes=96/128/160 default=128\n";
#endif
    return 0;
}
