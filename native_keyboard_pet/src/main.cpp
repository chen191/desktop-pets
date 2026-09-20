#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#include <windows.h>
#include <gdiplus.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cwchar>
#include <memory>
#include <string>

#include "activity.h"

using namespace Gdiplus;

namespace {
#ifdef FLAME_MOUSE_EDITION
constexpr wchar_t kWindowClass[] = L"FlameKeyboardPetMouseEditionWindow";
constexpr wchar_t kMutexName[] = L"Local\\FlameKeyboardPetMouseEditionSingleInstance";
constexpr wchar_t kTestWindowClass[] = L"FlameKeyboardPetMouseEditionTestWindow";
constexpr wchar_t kTestMutexName[] = L"Local\\FlameKeyboardPetMouseEditionTestSingleInstance";
constexpr wchar_t kWindowTitle[] = L"火焰小蓝桌宠·鼠标版";
#else
constexpr wchar_t kWindowClass[] = L"FlameKeyboardPetWindow";
constexpr wchar_t kMutexName[] = L"Local\\FlameKeyboardPetSingleInstance";
constexpr wchar_t kTestWindowClass[] = L"FlameKeyboardPetTestWindow";
constexpr wchar_t kTestMutexName[] = L"Local\\FlameKeyboardPetTestSingleInstance";
constexpr wchar_t kWindowTitle[] = L"火焰小蓝桌宠";
#endif
constexpr int kSourceFrameSize = 96;
constexpr std::array<int, 3> kWindowSizes{96, 128, 160};
constexpr UINT_PTR kAnimationTimer = 1;
constexpr UINT kFrameIntervalMs = 30;
constexpr UINT kCuteCommand = 1001;
constexpr UINT kResetCommand = 1002;
constexpr UINT kExitCommand = 1003;
constexpr UINT kSmallSizeCommand = 1004;
constexpr UINT kStandardSizeCommand = 1005;
constexpr UINT kLargeSizeCommand = 1006;
constexpr UINT kQueryModeMessage = WM_APP + 17;
constexpr UINT kWakeExistingMessage = WM_APP + 18;
constexpr UINT kQuerySizeMessage = WM_APP + 19;
constexpr ULONGLONG kRecentKeysWindowMs = 1500;
constexpr ULONGLONG kManualCuteDurationMs = 1700;
#ifdef FLAME_MOUSE_EDITION
constexpr size_t kFrameCount = 16;
#else
constexpr size_t kFrameCount = 12;
#endif

constexpr std::array<const wchar_t*, kFrameCount> kFrameFiles{
    L"type_0.png", L"type_1.png", L"type_2.png", L"type_3.png",
    L"sleep_0.png", L"sleep_1.png", L"sleep_2.png", L"sleep_3.png",
    L"cute_0.png", L"cute_1.png", L"cute_2.png", L"cute_3.png",
#ifdef FLAME_MOUSE_EDITION
    L"mouse_0.png", L"mouse_1.png", L"mouse_2.png", L"mouse_3.png",
#endif
};

struct CachedFrame {
    HDC memory = nullptr;
    HBITMAP bitmap = nullptr;
    HGDIOBJ previousBitmap = nullptr;
};

HWND g_window = nullptr;
HANDLE g_mutex = nullptr;
HHOOK g_keyboardHook = nullptr;
#ifdef FLAME_MOUSE_EDITION
HHOOK g_mouseHook = nullptr;
#endif
ULONG_PTR g_gdiplusToken = 0;
std::array<CachedFrame, kFrameCount> g_frames;
std::array<ULONGLONG, 32> g_keyTimes{};
size_t g_keyWriteIndex = 0;
ULONGLONG g_lastKeyAt = 0;
#ifdef FLAME_MOUSE_EDITION
ULONGLONG g_lastMouseAt = 0;
ULONGLONG g_lastMouseClickAt = 0;
#endif
ULONGLONG g_startedAt = 0;
ULONGLONG g_manualCuteStartedAt = 0;
ULONGLONG g_manualCuteUntil = 0;
ULONGLONG g_sleepAfterMs = kDefaultSleepAfterMs;
const wchar_t* g_activeWindowClass = kWindowClass;
const wchar_t* g_activeMutexName = kMutexName;
size_t g_sizeIndex = 1;
bool g_isTestMode = false;
PetMode g_currentMode = PetMode::Cute;
size_t g_lastFrame = kFrameCount;

double g_x = 0.0;
double g_y = 0.0;
bool g_pressed = false;
bool g_dragging = false;
POINT g_pressCursor{};
double g_pressWindowX = 0.0;
double g_pressWindowY = 0.0;

int WindowSize() {
    return kWindowSizes[g_sizeIndex];
}

std::wstring ExecutableDirectory() {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring full(path);
    const auto slash = full.find_last_of(L"\\/");
    return slash == std::wstring::npos ? L"." : full.substr(0, slash);
}

std::wstring SettingsDirectory() {
    wchar_t localAppData[MAX_PATH]{};
    const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return L"";
    }
    return std::wstring(localAppData) + L"\\FlameKeyboardPet";
}

std::wstring SettingsFile() {
    const std::wstring directory = SettingsDirectory();
    return directory.empty() ? L"" : directory + L"\\settings.ini";
}

void LoadSizeSetting() {
    if (g_isTestMode) {
        g_sizeIndex = 1;
        return;
    }
    const std::wstring path = SettingsFile();
    if (path.empty()) {
        return;
    }
    const int savedSize = GetPrivateProfileIntW(L"pet", L"size", 128, path.c_str());
    for (size_t index = 0; index < kWindowSizes.size(); ++index) {
        if (kWindowSizes[index] == savedSize) {
            g_sizeIndex = index;
            return;
        }
    }
    g_sizeIndex = 1;
}

void SaveSizeSetting() {
    if (g_isTestMode) {
        return;
    }
    const std::wstring directory = SettingsDirectory();
    const std::wstring path = SettingsFile();
    if (directory.empty() || path.empty()) {
        return;
    }
    if (!CreateDirectoryW(directory.c_str(), nullptr) &&
        GetLastError() != ERROR_ALREADY_EXISTS) {
        return;
    }
    const std::wstring size = std::to_wstring(WindowSize());
    WritePrivateProfileStringW(L"pet", L"size", size.c_str(), path.c_str());
}

RECT CurrentWorkArea(const POINT& point) {
    RECT work{};
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    const HMONITOR monitor = MonitorFromPoint(point, MONITOR_DEFAULTTONEAREST);
    if (GetMonitorInfoW(monitor, &info)) {
        return info.rcWork;
    }
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    return work;
}

void ClampToWorkArea(const RECT& work) {
    const int size = WindowSize();
    g_x = std::clamp(g_x, static_cast<double>(work.left),
                     static_cast<double>(work.right - size));
    g_y = std::clamp(g_y, static_cast<double>(work.top),
                     static_cast<double>(work.bottom - size));
}

void DestroyFrame(CachedFrame& frame) {
    if (frame.memory && frame.previousBitmap) {
        SelectObject(frame.memory, frame.previousBitmap);
    }
    if (frame.bitmap) {
        DeleteObject(frame.bitmap);
    }
    if (frame.memory) {
        DeleteDC(frame.memory);
    }
    frame = {};
}

CachedFrame CreateFrame(Image* image, int size) {
    CachedFrame frame;
    HDC screen = GetDC(nullptr);
    frame.memory = CreateCompatibleDC(screen);

    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = size;
    info.bmiHeader.biHeight = -size;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    void* pixels = nullptr;
    frame.bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    ReleaseDC(nullptr, screen);
    if (!frame.memory || !frame.bitmap || !pixels) {
        DestroyFrame(frame);
        return frame;
    }

    frame.previousBitmap = SelectObject(frame.memory, frame.bitmap);
    ZeroMemory(pixels, size * size * 4);

    Bitmap canvas(size, size, PixelFormat32bppPARGB);
    Graphics graphics(&canvas);
    graphics.Clear(Color(0, 0, 0, 0));
    graphics.SetCompositingMode(CompositingModeSourceCopy);
    graphics.SetInterpolationMode(InterpolationModeNearestNeighbor);
    graphics.SetPixelOffsetMode(PixelOffsetModeHalf);
    graphics.DrawImage(image, Rect(0, 0, size, size));

    BitmapData bitmapData{};
    Rect lockRect(0, 0, size, size);
    if (canvas.LockBits(&lockRect, ImageLockModeRead, PixelFormat32bppPARGB,
                        &bitmapData) != Ok) {
        DestroyFrame(frame);
        return frame;
    }

    auto* destination = static_cast<BYTE*>(pixels);
    auto* source = static_cast<BYTE*>(bitmapData.Scan0);
    const int rowBytes = size * 4;
    for (int row = 0; row < size; ++row) {
        CopyMemory(destination + row * rowBytes, source + row * bitmapData.Stride,
                   rowBytes);
    }
    canvas.UnlockBits(&bitmapData);
    return frame;
}

void DestroyFrameSet(std::array<CachedFrame, kFrameCount>& frames) {
    for (auto& frame : frames) {
        DestroyFrame(frame);
    }
}

bool BuildFrames(int size, std::array<CachedFrame, kFrameCount>& frames) {
    const std::wstring directory = ExecutableDirectory() + L"\\assets\\actions\\";
    for (size_t index = 0; index < kFrameFiles.size(); ++index) {
        const std::wstring path = directory + kFrameFiles[index];
        std::unique_ptr<Image> image(Image::FromFile(path.c_str(), FALSE));
        if (!image || image->GetLastStatus() != Ok ||
            image->GetWidth() != kSourceFrameSize ||
            image->GetHeight() != kSourceFrameSize) {
            DestroyFrameSet(frames);
            return false;
        }
        frames[index] = CreateFrame(image.get(), size);
        if (!frames[index].memory || !frames[index].bitmap) {
            DestroyFrameSet(frames);
            return false;
        }
    }
    return true;
}

bool LoadFrames() {
    return BuildFrames(WindowSize(), g_frames);
}

void DestroyFrames() {
    DestroyFrameSet(g_frames);
}

void RenderFrame(size_t frameIndex, bool force = false) {
    if (frameIndex >= kFrameCount || (!force && frameIndex == g_lastFrame)) {
        return;
    }
    const CachedFrame& frame = g_frames[frameIndex];
    if (!frame.memory) {
        return;
    }
    g_lastFrame = frameIndex;

    POINT destination{static_cast<LONG>(std::lround(g_x)),
                      static_cast<LONG>(std::lround(g_y))};
    SIZE size{WindowSize(), WindowSize()};
    POINT source{0, 0};
    BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    HDC screen = GetDC(nullptr);
    UpdateLayeredWindow(g_window, screen, &destination, &size, frame.memory, &source,
                        0, &blend, ULW_ALPHA);
    ReleaseDC(nullptr, screen);
}

ULONGLONG SystemIdleMs() {
    LASTINPUTINFO info{};
    info.cbSize = sizeof(info);
    if (!GetLastInputInfo(&info)) {
        return 0;
    }
    return static_cast<DWORD>(GetTickCount() - info.dwTime);
}

ULONGLONG SinceLastKeyMs(ULONGLONG now) {
    return g_lastKeyAt == 0 || now < g_lastKeyAt
               ? UINT64_MAX
               : now - g_lastKeyAt;
}

#ifdef FLAME_MOUSE_EDITION
ULONGLONG SinceLastMouseMs(ULONGLONG now) {
    return g_lastMouseAt == 0 || now < g_lastMouseAt
               ? UINT64_MAX
               : now - g_lastMouseAt;
}
#endif

unsigned RecentKeyCount(ULONGLONG now) {
    unsigned count = 0;
    for (const ULONGLONG time : g_keyTimes) {
        if (time != 0 && now >= time && now - time <= kRecentKeysWindowMs) {
            ++count;
        }
    }
    return count;
}

void RecordKeyActivity() {
    const ULONGLONG now = GetTickCount64();
    g_lastKeyAt = now;
    g_keyTimes[g_keyWriteIndex % g_keyTimes.size()] = now;
    ++g_keyWriteIndex;
}

#ifdef FLAME_MOUSE_EDITION
void RecordMouseActivity(bool clicked) {
    const ULONGLONG now = GetTickCount64();
    g_lastMouseAt = now;
    if (clicked) {
        g_lastMouseClickAt = now;
    }
}
#endif

LRESULT CALLBACK KeyboardProcedure(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
        // Deliberately ignore vkCode and scanCode: only event timing is retained.
        const auto* event = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);
        if (event != nullptr) {
            RecordKeyActivity();
        }
    }
    return CallNextHookEx(g_keyboardHook, code, wParam, lParam);
}

#ifdef FLAME_MOUSE_EDITION
LRESULT CALLBACK MouseProcedure(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HC_ACTION) {
        // The mouse edition retains event timing only; coordinates are ignored.
        if (wParam == WM_MOUSEMOVE) {
            RecordMouseActivity(false);
        } else if (wParam == WM_LBUTTONDOWN || wParam == WM_RBUTTONDOWN ||
                   wParam == WM_MBUTTONDOWN || wParam == WM_XBUTTONDOWN ||
                   wParam == WM_MOUSEWHEEL || wParam == WM_MOUSEHWHEEL) {
            RecordMouseActivity(true);
        }
    }
    return CallNextHookEx(g_mouseHook, code, wParam, lParam);
}
#endif

size_t CuteFrame(ULONGLONG now) {
    ULONGLONG phase = 0;
    if (now < g_manualCuteUntil) {
        phase = (now - g_manualCuteStartedAt) % kManualCuteDurationMs;
        if (phase < 300) return 8;
        if (phase < 650) return 9;
        if (phase < 1050) return 10;
        if (phase < 1370) return 9;
        return 11;
    }

    phase = (now - g_startedAt) % 10500;
    if (phase < 7800) return 8;
    if (phase < 8150) return 9;
    if (phase < 8600) return 10;
    if (phase < 8950) return 9;
    return 11;
}

void UpdateAnimation() {
    const ULONGLONG now = GetTickCount64();
#ifdef FLAME_MOUSE_EDITION
    g_currentMode = SelectPetMode(SystemIdleMs(), SinceLastKeyMs(now),
                                  SinceLastMouseMs(now), g_sleepAfterMs);
#else
    g_currentMode = SelectPetMode(SystemIdleMs(), SinceLastKeyMs(now),
                                  g_sleepAfterMs);
#endif
    if (now < g_manualCuteUntil && g_currentMode != PetMode::Typing) {
        g_currentMode = PetMode::Cute;
    }

    size_t frame = 8;
    if (g_currentMode == PetMode::Typing) {
        const ULONGLONG interval = TypingFrameIntervalMs(RecentKeyCount(now));
        frame = static_cast<size_t>((now / interval) % 4);
#ifdef FLAME_MOUSE_EDITION
    } else if (g_currentMode == PetMode::Mousing) {
        if (g_lastMouseClickAt != 0 && now >= g_lastMouseClickAt &&
            now - g_lastMouseClickAt <= 180) {
            frame = 15;
        } else {
            frame = 12 + static_cast<size_t>((now / 135) % 3);
        }
#endif
    } else if (g_currentMode == PetMode::Sleeping) {
        frame = 4 + static_cast<size_t>((now / 520) % 4);
    } else {
        frame = CuteFrame(now);
    }
    RenderFrame(frame);
}

void ResetPosition() {
    POINT cursor{};
    GetCursorPos(&cursor);
    const RECT work = CurrentWorkArea(cursor);
    const int size = WindowSize();
    g_x = work.right - size - 24.0;
    g_y = work.bottom - size - 24.0;
    ClampToWorkArea(work);
    SetWindowPos(g_window, HWND_TOPMOST, static_cast<int>(g_x), static_cast<int>(g_y),
                 size, size, SWP_NOACTIVATE);
    RenderFrame(g_lastFrame == kFrameCount ? 8 : g_lastFrame, true);
}

void SetPetSize(size_t newSizeIndex) {
    if (newSizeIndex >= kWindowSizes.size() || newSizeIndex == g_sizeIndex) {
        return;
    }
    const int oldSize = WindowSize();
    const int newSize = kWindowSizes[newSizeIndex];
    std::array<CachedFrame, kFrameCount> newFrames{};
    if (!BuildFrames(newSize, newFrames)) {
        MessageBoxW(nullptr, L"切换尺寸时无法重新生成动画缓存。", kWindowTitle,
                    MB_ICONERROR);
        return;
    }
    DestroyFrames();
    g_frames = newFrames;
    g_sizeIndex = newSizeIndex;

    // Keep the visual center and foot line stable while growing upward.
    g_x += (oldSize - newSize) / 2.0;
    g_y += oldSize - newSize;
    POINT anchor{static_cast<LONG>(std::lround(g_x + newSize / 2.0)),
                 static_cast<LONG>(std::lround(g_y + newSize / 2.0))};
    ClampToWorkArea(CurrentWorkArea(anchor));
    SetWindowPos(g_window, HWND_TOPMOST, static_cast<int>(std::lround(g_x)),
                 static_cast<int>(std::lround(g_y)), newSize, newSize,
                 SWP_NOACTIVATE);
    g_lastFrame = kFrameCount;
    RenderFrame(8, true);
    SaveSizeSetting();
}

void TriggerCute() {
    const ULONGLONG now = GetTickCount64();
    g_manualCuteStartedAt = now;
    g_manualCuteUntil = now + kManualCuteDurationMs;
    g_lastFrame = kFrameCount;
}

void ApplyCommand(HWND window, UINT command) {
    if (command == kCuteCommand) {
        TriggerCute();
    } else if (command == kResetCommand) {
        ResetPosition();
    } else if (command == kSmallSizeCommand) {
        SetPetSize(0);
    } else if (command == kStandardSizeCommand) {
        SetPetSize(1);
    } else if (command == kLargeSizeCommand) {
        SetPetSize(2);
    } else if (command == kExitCommand) {
        DestroyWindow(window);
    }
}

void ShowContextMenu(HWND window, POINT point) {
    HMENU menu = CreatePopupMenu();
    HMENU sizeMenu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, kCuteCommand, L"立即卖萌");
    AppendMenuW(menu, MF_STRING, kResetCommand, L"回到桌面右下角");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(sizeMenu, MF_STRING | (g_sizeIndex == 0 ? MF_CHECKED : 0),
                kSmallSizeCommand, L"小号 96px");
    AppendMenuW(sizeMenu, MF_STRING | (g_sizeIndex == 1 ? MF_CHECKED : 0),
                kStandardSizeCommand, L"标准 128px");
    AppendMenuW(sizeMenu, MF_STRING | (g_sizeIndex == 2 ? MF_CHECKED : 0),
                kLargeSizeCommand, L"大号 160px");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(sizeMenu), L"桌宠大小");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kExitCommand, L"退出");
    SetForegroundWindow(window);
    const UINT command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                                        point.x, point.y, 0, window, nullptr);
    DestroyMenu(menu);
    ApplyCommand(window, command);
}

LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_TIMER:
            if (wParam == kAnimationTimer) {
                UpdateAnimation();
            }
            return 0;
        case WM_LBUTTONDOWN:
            GetCursorPos(&g_pressCursor);
            g_pressWindowX = g_x;
            g_pressWindowY = g_y;
            g_pressed = true;
            g_dragging = false;
            SetCapture(window);
            return 0;
        case WM_MOUSEMOVE:
            if (g_pressed) {
                POINT cursor{};
                GetCursorPos(&cursor);
                const int deltaX = cursor.x - g_pressCursor.x;
                const int deltaY = cursor.y - g_pressCursor.y;
                if (!g_dragging && (std::abs(deltaX) > 3 || std::abs(deltaY) > 3)) {
                    g_dragging = true;
                }
                if (g_dragging) {
                    g_x = g_pressWindowX + deltaX;
                    g_y = g_pressWindowY + deltaY;
                    ClampToWorkArea(CurrentWorkArea(cursor));
                    SetWindowPos(window, HWND_TOPMOST,
                                 static_cast<int>(std::lround(g_x)),
                                 static_cast<int>(std::lround(g_y)),
                                 WindowSize(), WindowSize(),
                                 SWP_NOACTIVATE | SWP_NOSIZE);
                    RenderFrame(g_lastFrame == kFrameCount ? 8 : g_lastFrame, true);
                }
            }
            return 0;
        case WM_LBUTTONUP: {
            const bool wasClick = g_pressed && !g_dragging;
            g_pressed = false;
            g_dragging = false;
            ReleaseCapture();
            if (wasClick) {
                TriggerCute();
            }
            return 0;
        }
        case WM_CAPTURECHANGED:
            g_pressed = false;
            g_dragging = false;
            return 0;
        case WM_RBUTTONUP: {
            POINT cursor{};
            GetCursorPos(&cursor);
            ShowContextMenu(window, cursor);
            return 0;
        }
        case WM_COMMAND:
            ApplyCommand(window, LOWORD(wParam));
            return 0;
        case kQueryModeMessage:
            return static_cast<LRESULT>(g_currentMode);
        case kQuerySizeMessage:
            return WindowSize();
        case kWakeExistingMessage:
            ShowWindow(window, SW_SHOWNOACTIVATE);
            SetWindowPos(window, HWND_TOPMOST, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            TriggerCute();
            return 0;
        case WM_DESTROY:
            KillTimer(window, kAnimationTimer);
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(window, message, wParam, lParam);
    }
}
}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    SetProcessDPIAware();
    if (std::wcsstr(GetCommandLineW(), L"--test-fast") != nullptr) {
        g_isTestMode = true;
        g_sleepAfterMs = 3000;
        g_activeWindowClass = kTestWindowClass;
        g_activeMutexName = kTestMutexName;
    }
    LoadSizeSetting();

    g_mutex = CreateMutexW(nullptr, TRUE, g_activeMutexName);
    if (!g_mutex || GetLastError() == ERROR_ALREADY_EXISTS) {
        if (g_mutex) {
            CloseHandle(g_mutex);
        }
        if (HWND existing = FindWindowW(g_activeWindowClass, nullptr)) {
            PostMessageW(existing, kWakeExistingMessage, 0, 0);
        }
        return 0;
    }

    GdiplusStartupInput gdiplusInput;
    if (GdiplusStartup(&g_gdiplusToken, &gdiplusInput, nullptr) != Ok) {
        CloseHandle(g_mutex);
        return 1;
    }

    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = WindowProcedure;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = g_activeWindowClass;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_HAND);
    windowClass.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(101));
    if (!RegisterClassW(&windowClass)) {
        GdiplusShutdown(g_gdiplusToken);
        CloseHandle(g_mutex);
        return 1;
    }

    g_window = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        g_activeWindowClass, kWindowTitle, WS_POPUP, 0, 0,
        WindowSize(), WindowSize(),
        nullptr, nullptr, instance, nullptr);
    if (!g_window) {
        GdiplusShutdown(g_gdiplusToken);
        CloseHandle(g_mutex);
        return 1;
    }

    if (!LoadFrames()) {
#ifdef FLAME_MOUSE_EDITION
        MessageBoxW(nullptr, L"缺少或无法读取 assets\\actions 中的 16 张动画图片。",
                    kWindowTitle, MB_ICONERROR);
#else
        MessageBoxW(nullptr, L"缺少或无法读取 assets\\actions 中的 12 张动画图片。",
                    kWindowTitle, MB_ICONERROR);
#endif
        DestroyFrames();
        DestroyWindow(g_window);
        GdiplusShutdown(g_gdiplusToken);
        CloseHandle(g_mutex);
        return 1;
    }

    g_keyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardProcedure, instance, 0);
#ifdef FLAME_MOUSE_EDITION
    g_mouseHook = SetWindowsHookExW(WH_MOUSE_LL, MouseProcedure, instance, 0);
    if (!g_keyboardHook || !g_mouseHook) {
        MessageBoxW(nullptr, L"无法监听键盘或鼠标活动，桌宠未启动。", kWindowTitle,
                    MB_ICONERROR);
        if (g_keyboardHook) {
            UnhookWindowsHookEx(g_keyboardHook);
        }
        if (g_mouseHook) {
            UnhookWindowsHookEx(g_mouseHook);
        }
#else
    if (!g_keyboardHook) {
        MessageBoxW(nullptr, L"无法监听键盘活动，桌宠未启动。", kWindowTitle,
                    MB_ICONERROR);
#endif
        DestroyFrames();
        DestroyWindow(g_window);
        GdiplusShutdown(g_gdiplusToken);
        CloseHandle(g_mutex);
        return 1;
    }

    g_startedAt = GetTickCount64();
    ResetPosition();
    RenderFrame(8, true);
    ShowWindow(g_window, SW_SHOWNOACTIVATE);
    SetTimer(g_window, kAnimationTimer, kFrameIntervalMs, nullptr);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    UnhookWindowsHookEx(g_keyboardHook);
#ifdef FLAME_MOUSE_EDITION
    UnhookWindowsHookEx(g_mouseHook);
#endif
    DestroyFrames();
    GdiplusShutdown(g_gdiplusToken);
    ReleaseMutex(g_mutex);
    CloseHandle(g_mutex);
    return static_cast<int>(message.wParam);
}
