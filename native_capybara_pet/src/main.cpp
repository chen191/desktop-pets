#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#include <windows.h>
#include <gdiplus.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <string>

using namespace Gdiplus;

namespace {
constexpr wchar_t kWindowClass[] = L"CapybaraLuluAnimatedPetWindow";
constexpr wchar_t kMutexName[] = L"Local\\CapybaraLuluAnimatedPetSingleInstance";
constexpr int kWindowSize = 88;
constexpr int kSpriteSize = 70;
constexpr UINT_PTR kAnimationTimer = 1;
constexpr UINT kFrameIntervalMs = 33;
constexpr UINT kFollowCommand = 1001;
constexpr UINT kResetCommand = 1002;
constexpr UINT kExitCommand = 1003;
constexpr ULONGLONG kSleepAfterMs = 20000;
constexpr ULONGLONG kWaveDurationMs = 1200;

enum class Pose : size_t {
    Neutral,
    Blink,
    Wave,
    WalkA,
    WalkB,
    Sleep,
    Count,
};

constexpr std::array<const wchar_t*, static_cast<size_t>(Pose::Count)> kPoseFiles{
    L"neutral.png",
    L"blink.png",
    L"wave.png",
    L"walk_a.png",
    L"walk_b.png",
    L"sleep.png",
};

struct CachedFrame {
    HDC memory = nullptr;
    HBITMAP bitmap = nullptr;
    HGDIOBJ previousBitmap = nullptr;
};

HWND g_window = nullptr;
HANDLE g_mutex = nullptr;
ULONG_PTR g_gdiplusToken = 0;
std::array<std::unique_ptr<Image>, static_cast<size_t>(Pose::Count)> g_images;
std::array<CachedFrame, static_cast<size_t>(Pose::Count)> g_frames;
Pose g_lastPose = Pose::Count;

double g_x = 0.0;
double g_y = 0.0;
bool g_following = true;
bool g_pressed = false;
bool g_dragging = false;
POINT g_pressCursor{};
double g_pressWindowX = 0.0;
double g_pressWindowY = 0.0;
POINT g_previousCursor{};
int g_followSide = -1;
ULONGLONG g_startedAt = 0;
ULONGLONG g_lastCursorMotionAt = 0;
ULONGLONG g_waveUntil = 0;

std::wstring ExecutableDirectory() {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring full(path);
    const auto slash = full.find_last_of(L"\\/");
    return slash == std::wstring::npos ? L"." : full.substr(0, slash);
}

RECT CurrentWorkArea(const POINT& point) {
    RECT work{};
    MONITORINFO info{sizeof(info)};
    const HMONITOR monitor = MonitorFromPoint(point, MONITOR_DEFAULTTONEAREST);
    if (GetMonitorInfoW(monitor, &info)) {
        return info.rcWork;
    }
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    return work;
}

void ClampToWorkArea(const RECT& work) {
    g_x = std::clamp(g_x, static_cast<double>(work.left),
                     static_cast<double>(work.right - kWindowSize));
    g_y = std::clamp(g_y, static_cast<double>(work.top),
                     static_cast<double>(work.bottom - kWindowSize));
}

CachedFrame CreateFrame(Image* image) {
    CachedFrame frame;
    HDC screen = GetDC(nullptr);
    frame.memory = CreateCompatibleDC(screen);

    BITMAPINFO bitmapInfo{};
    bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth = kWindowSize;
    bitmapInfo.bmiHeader.biHeight = -kWindowSize;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;

    void* pixels = nullptr;
    frame.bitmap = CreateDIBSection(screen, &bitmapInfo, DIB_RGB_COLORS, &pixels, nullptr, 0);
    if (!frame.memory || !frame.bitmap || !pixels) {
        ReleaseDC(nullptr, screen);
        return frame;
    }
    frame.previousBitmap = SelectObject(frame.memory, frame.bitmap);
    ZeroMemory(pixels, kWindowSize * kWindowSize * 4);

    Bitmap canvas(kWindowSize, kWindowSize, PixelFormat32bppPARGB);
    Graphics graphics(&canvas);
    graphics.Clear(Color(0, 0, 0, 0));
    graphics.SetSmoothingMode(SmoothingModeHighQuality);
    graphics.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    graphics.SetPixelOffsetMode(PixelOffsetModeHighQuality);
    graphics.SetCompositingMode(CompositingModeSourceCopy);

    const float offset = (kWindowSize - kSpriteSize) / 2.0f;
    graphics.DrawImage(image, RectF(offset, offset, kSpriteSize, kSpriteSize));

    BitmapData bitmapData{};
    Rect lockRect(0, 0, kWindowSize, kWindowSize);
    if (canvas.LockBits(&lockRect, ImageLockModeRead, PixelFormat32bppPARGB, &bitmapData) == Ok) {
        auto* destination = static_cast<BYTE*>(pixels);
        auto* source = static_cast<BYTE*>(bitmapData.Scan0);
        const int rowBytes = kWindowSize * 4;
        for (int row = 0; row < kWindowSize; ++row) {
            CopyMemory(destination + row * rowBytes, source + row * bitmapData.Stride, rowBytes);
        }
        canvas.UnlockBits(&bitmapData);
    }

    ReleaseDC(nullptr, screen);
    return frame;
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

void DestroyFrames() {
    for (auto& frame : g_frames) {
        DestroyFrame(frame);
    }
    for (auto& image : g_images) {
        image.reset();
    }
}

bool LoadFrames() {
    const std::wstring directory = ExecutableDirectory() + L"\\assets\\actions\\";
    for (size_t index = 0; index < kPoseFiles.size(); ++index) {
        const std::wstring path = directory + kPoseFiles[index];
        g_images[index].reset(Image::FromFile(path.c_str(), FALSE));
        if (!g_images[index] || g_images[index]->GetLastStatus() != Ok) {
            return false;
        }
        g_frames[index] = CreateFrame(g_images[index].get());
        if (!g_frames[index].memory || !g_frames[index].bitmap) {
            return false;
        }
    }
    return true;
}

void RenderPose(Pose pose, bool force = false) {
    if (!force && pose == g_lastPose) {
        return;
    }
    const CachedFrame& frame = g_frames[static_cast<size_t>(pose)];
    if (!frame.memory) {
        return;
    }
    g_lastPose = pose;

    POINT destination{static_cast<LONG>(std::lround(g_x)), static_cast<LONG>(std::lround(g_y))};
    SIZE size{kWindowSize, kWindowSize};
    POINT source{0, 0};
    BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    HDC screen = GetDC(nullptr);
    UpdateLayeredWindow(g_window, screen, &destination, &size, frame.memory, &source,
                        0, &blend, ULW_ALPHA);
    ReleaseDC(nullptr, screen);
}

void ResetPosition() {
    POINT cursor{};
    GetCursorPos(&cursor);
    const RECT work = CurrentWorkArea(cursor);
    g_x = work.right - kWindowSize - 28.0;
    g_y = work.bottom - kWindowSize - 28.0;
    g_followSide = -1;
    g_previousCursor = cursor;
    g_lastCursorMotionAt = GetTickCount64();
    ClampToWorkArea(work);
    SetWindowPos(g_window, HWND_TOPMOST, static_cast<int>(g_x), static_cast<int>(g_y),
                 kWindowSize, kWindowSize, SWP_NOACTIVATE);
}

void TriggerWave() {
    const ULONGLONG now = GetTickCount64();
    g_waveUntil = now + kWaveDurationMs;
    g_lastCursorMotionAt = now;
    RenderPose(Pose::Wave, true);
}

void UpdateMotion() {
    POINT cursor{};
    GetCursorPos(&cursor);
    const ULONGLONG now = GetTickCount64();
    if (cursor.x != g_previousCursor.x || cursor.y != g_previousCursor.y) {
        g_lastCursorMotionAt = now;
    }

    bool moving = false;
    if (g_following && !g_pressed) {
        const int cursorDeltaX = cursor.x - g_previousCursor.x;
        if (std::abs(cursorDeltaX) >= 3) {
            g_followSide = cursorDeltaX > 0 ? -1 : 1;
        }

        const double targetX = cursor.x + g_followSide * 58.0 - kWindowSize / 2.0;
        const double targetY = cursor.y + 30.0;
        const double deltaX = targetX - g_x;
        const double deltaY = targetY - g_y;
        const double distance = std::sqrt(deltaX * deltaX + deltaY * deltaY);
        moving = distance > 4.0;
        const double easing = distance > 220.0 ? 0.12 : 0.075;
        g_x += deltaX * easing;
        g_y += deltaY * easing;
        ClampToWorkArea(CurrentWorkArea(cursor));
        SetWindowPos(g_window, HWND_TOPMOST, static_cast<int>(std::lround(g_x)),
                     static_cast<int>(std::lround(g_y)), kWindowSize, kWindowSize,
                     SWP_NOACTIVATE | SWP_NOSIZE);
    }

    Pose pose = Pose::Neutral;
    if (now < g_waveUntil) {
        pose = Pose::Wave;
    } else if (moving) {
        pose = ((now / 130) % 2 == 0) ? Pose::WalkA : Pose::WalkB;
    } else if (now - g_lastCursorMotionAt >= kSleepAfterMs) {
        pose = Pose::Sleep;
    } else if ((now - g_startedAt) % 4200 >= 3960) {
        pose = Pose::Blink;
    }
    RenderPose(pose);
    g_previousCursor = cursor;
}

void ShowContextMenu(HWND window, POINT screenPoint) {
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING | (g_following ? MF_CHECKED : 0), kFollowCommand,
                L"跟随鼠标");
    AppendMenuW(menu, MF_STRING, kResetCommand, L"回到桌面右下角");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kExitCommand, L"退出");
    SetForegroundWindow(window);
    const UINT command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                                        screenPoint.x, screenPoint.y, 0, window, nullptr);
    DestroyMenu(menu);
    if (command == kFollowCommand) {
        g_following = !g_following;
        g_lastCursorMotionAt = GetTickCount64();
    } else if (command == kResetCommand) {
        ResetPosition();
    } else if (command == kExitCommand) {
        DestroyWindow(window);
    }
}

LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_TIMER:
            if (wParam == kAnimationTimer) {
                UpdateMotion();
            }
            return 0;
        case WM_LBUTTONDOWN: {
            GetCursorPos(&g_pressCursor);
            g_pressWindowX = g_x;
            g_pressWindowY = g_y;
            g_pressed = true;
            g_dragging = false;
            g_lastCursorMotionAt = GetTickCount64();
            SetCapture(window);
            return 0;
        }
        case WM_MOUSEMOVE:
            if (g_pressed) {
                POINT cursor{};
                GetCursorPos(&cursor);
                const int deltaX = cursor.x - g_pressCursor.x;
                const int deltaY = cursor.y - g_pressCursor.y;
                if (!g_dragging && (std::abs(deltaX) > 3 || std::abs(deltaY) > 3)) {
                    g_dragging = true;
                    g_following = false;
                }
                if (g_dragging) {
                    g_x = g_pressWindowX + deltaX;
                    g_y = g_pressWindowY + deltaY;
                    ClampToWorkArea(CurrentWorkArea(cursor));
                    SetWindowPos(window, HWND_TOPMOST, static_cast<int>(std::lround(g_x)),
                                 static_cast<int>(std::lround(g_y)), kWindowSize, kWindowSize,
                                 SWP_NOACTIVATE | SWP_NOSIZE);
                }
            }
            return 0;
        case WM_LBUTTONUP: {
            const bool wasClick = g_pressed && !g_dragging;
            g_pressed = false;
            g_dragging = false;
            ReleaseCapture();
            if (wasClick) {
                TriggerWave();
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
            g_lastCursorMotionAt = GetTickCount64();
            ShowContextMenu(window, cursor);
            return 0;
        }
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
    g_mutex = CreateMutexW(nullptr, TRUE, kMutexName);
    if (!g_mutex || GetLastError() == ERROR_ALREADY_EXISTS) {
        if (g_mutex) {
            CloseHandle(g_mutex);
        }
        if (HWND existing = FindWindowW(kWindowClass, nullptr)) {
            ShowWindow(existing, SW_SHOWNOACTIVATE);
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
    windowClass.lpszClassName = kWindowClass;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_HAND);
    if (!RegisterClassW(&windowClass)) {
        GdiplusShutdown(g_gdiplusToken);
        CloseHandle(g_mutex);
        return 1;
    }

    g_window = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        kWindowClass, L"", WS_POPUP, 0, 0, kWindowSize, kWindowSize,
        nullptr, nullptr, instance, nullptr);
    if (!g_window) {
        GdiplusShutdown(g_gdiplusToken);
        CloseHandle(g_mutex);
        return 1;
    }

    if (!LoadFrames()) {
        MessageBoxW(nullptr, L"Desktop pet action images are missing from assets\\actions.",
                    L"Capybara LuLu Lite Pet", MB_ICONERROR);
        DestroyFrames();
        DestroyWindow(g_window);
        GdiplusShutdown(g_gdiplusToken);
        CloseHandle(g_mutex);
        return 1;
    }

    g_startedAt = GetTickCount64();
    g_lastCursorMotionAt = g_startedAt;
    ResetPosition();
    RenderPose(Pose::Neutral, true);
    ShowWindow(g_window, SW_SHOWNOACTIVATE);
    SetTimer(g_window, kAnimationTimer, kFrameIntervalMs, nullptr);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    DestroyFrames();
    GdiplusShutdown(g_gdiplusToken);
    ReleaseMutex(g_mutex);
    CloseHandle(g_mutex);
    return static_cast<int>(message.wParam);
}
