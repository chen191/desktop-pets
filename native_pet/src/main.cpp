#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#include <windows.h>
#include <gdiplus.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

using namespace Gdiplus;

#ifndef PET_WINDOW_CLASS
#define PET_WINDOW_CLASS L"ChiikawaLitePetWindow"
#endif

#ifndef PET_MUTEX_NAME
#define PET_MUTEX_NAME L"Local\\ChiikawaLitePetSingleInstance"
#endif

#ifndef PET_ASSET_FILE
#define PET_ASSET_FILE L"chiikawa.png"
#endif

#ifndef PET_ERROR_TITLE
#define PET_ERROR_TITLE L"Chiikawa Lite Pet"
#endif

#ifndef PET_MOVE_SCALE_Y
#define PET_MOVE_SCALE_Y 0.97f
#endif

namespace {
constexpr wchar_t kWindowClass[] = PET_WINDOW_CLASS;
constexpr wchar_t kMutexName[] = PET_MUTEX_NAME;
constexpr int kWindowSize = 88;
constexpr int kSpriteSize = 70;
constexpr UINT_PTR kAnimationTimer = 1;
constexpr UINT kFrameIntervalMs = 33;
constexpr UINT kFollowCommand = 1001;
constexpr UINT kResetCommand = 1002;
constexpr UINT kExitCommand = 1003;

HWND g_window = nullptr;
HANDLE g_mutex = nullptr;
ULONG_PTR g_gdiplusToken = 0;
std::unique_ptr<Image> g_sprite;
double g_x = 0.0;
double g_y = 0.0;
bool g_following = true;
bool g_dragging = false;
POINT g_dragOffset{};
POINT g_previousCursor{};
int g_followSide = -1;
ULONGLONG g_startedAt = 0;

struct CachedFrame {
    HDC memory = nullptr;
    HBITMAP bitmap = nullptr;
    HGDIOBJ previousBitmap = nullptr;
};

std::vector<CachedFrame> g_idleFrames;
std::vector<CachedFrame> g_moveFrames;
int g_lastFrameIndex = -1;
bool g_lastFrameMoving = false;

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

CachedFrame CreateFrame(float bounce, float angle, float scaleY) {
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
    frame.previousBitmap = SelectObject(frame.memory, frame.bitmap);
    ZeroMemory(pixels, kWindowSize * kWindowSize * 4);

    Bitmap canvas(kWindowSize, kWindowSize, PixelFormat32bppPARGB);
    Graphics graphics(&canvas);
    graphics.Clear(Color(0, 0, 0, 0));
    graphics.SetSmoothingMode(SmoothingModeHighQuality);
    graphics.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    graphics.SetPixelOffsetMode(PixelOffsetModeHighQuality);
    graphics.SetCompositingMode(CompositingModeSourceCopy);

    const float left = (kWindowSize - kSpriteSize) / 2.0f;
    const float top = (kWindowSize - kSpriteSize) / 2.0f - bounce;
    const float centerX = left + kSpriteSize / 2.0f;
    const float centerY = top + kSpriteSize / 2.0f;

    Matrix transform;
    transform.Translate(centerX, centerY);
    transform.Rotate(angle);
    transform.Scale(1.0f, scaleY);
    transform.Translate(-centerX, -centerY);
    graphics.SetTransform(&transform);
    graphics.DrawImage(g_sprite.get(), RectF(left, top, kSpriteSize, kSpriteSize));
    graphics.ResetTransform();

    BitmapData bitmapData{};
    Rect lockRect(0, 0, kWindowSize, kWindowSize);
    if (canvas.LockBits(&lockRect, ImageLockModeRead, PixelFormat32bppPARGB, &bitmapData) == Ok) {
        auto* destinationPixels = static_cast<BYTE*>(pixels);
        auto* sourcePixels = static_cast<BYTE*>(bitmapData.Scan0);
        const int rowBytes = kWindowSize * 4;
        for (int row = 0; row < kWindowSize; ++row) {
            CopyMemory(destinationPixels + row * rowBytes,
                       sourcePixels + row * bitmapData.Stride, rowBytes);
        }
        canvas.UnlockBits(&bitmapData);
    }

    ReleaseDC(nullptr, screen);
    return frame;
}

void BuildFrameCache() {
    constexpr int idleCount = 12;
    constexpr int moveCount = 8;
    constexpr double pi = 3.14159265358979323846;
    g_idleFrames.reserve(idleCount);
    g_moveFrames.reserve(moveCount);
    for (int index = 0; index < idleCount; ++index) {
        const double phase = 2.0 * pi * index / idleCount;
        g_idleFrames.push_back(CreateFrame(
            static_cast<float>(std::sin(phase) * 1.2), 0.0f,
            1.0f + static_cast<float>(std::sin(phase) * 0.012)));
    }
    for (int index = 0; index < moveCount; ++index) {
        const double phase = 2.0 * pi * index / moveCount;
        g_moveFrames.push_back(CreateFrame(
            static_cast<float>(std::abs(std::sin(phase)) * 5.0),
            static_cast<float>(std::sin(phase) * 4.2), PET_MOVE_SCALE_Y));
    }
}

void DestroyFrameCache() {
    auto destroy = [](std::vector<CachedFrame>& frames) {
        for (auto& frame : frames) {
            if (frame.memory && frame.previousBitmap) {
                SelectObject(frame.memory, frame.previousBitmap);
            }
            if (frame.bitmap) {
                DeleteObject(frame.bitmap);
            }
            if (frame.memory) {
                DeleteDC(frame.memory);
            }
        }
        frames.clear();
    };
    destroy(g_idleFrames);
    destroy(g_moveFrames);
}

void RenderPet(bool moving, bool force = false) {
    auto& frames = moving ? g_moveFrames : g_idleFrames;
    if (frames.empty()) {
        return;
    }
    const ULONGLONG elapsed = GetTickCount64() - g_startedAt;
    const int frameInterval = moving ? 80 : 160;
    const int frameIndex = static_cast<int>((elapsed / frameInterval) % frames.size());
    if (!force && moving == g_lastFrameMoving && frameIndex == g_lastFrameIndex) {
        return;
    }
    g_lastFrameMoving = moving;
    g_lastFrameIndex = frameIndex;
    const CachedFrame& frame = frames[frameIndex];

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
    ClampToWorkArea(work);
    SetWindowPos(g_window, HWND_TOPMOST, static_cast<int>(g_x), static_cast<int>(g_y),
                 kWindowSize, kWindowSize, SWP_NOACTIVATE);
}

void UpdateMotion() {
    POINT cursor{};
    GetCursorPos(&cursor);
    bool moving = false;

    if (g_following && !g_dragging) {
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
        const RECT work = CurrentWorkArea(cursor);
        ClampToWorkArea(work);
        SetWindowPos(g_window, HWND_TOPMOST, static_cast<int>(std::lround(g_x)),
                     static_cast<int>(std::lround(g_y)), kWindowSize, kWindowSize,
                     SWP_NOACTIVATE | SWP_NOSIZE);
    }

    g_previousCursor = cursor;
    RenderPet(moving);
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
            g_dragging = true;
            g_following = false;
            POINT cursor{};
            GetCursorPos(&cursor);
            g_dragOffset.x = cursor.x - static_cast<LONG>(std::lround(g_x));
            g_dragOffset.y = cursor.y - static_cast<LONG>(std::lround(g_y));
            SetCapture(window);
            return 0;
        }
        case WM_MOUSEMOVE:
            if (g_dragging) {
                POINT cursor{};
                GetCursorPos(&cursor);
                g_x = cursor.x - g_dragOffset.x;
                g_y = cursor.y - g_dragOffset.y;
                ClampToWorkArea(CurrentWorkArea(cursor));
                SetWindowPos(window, HWND_TOPMOST, static_cast<int>(g_x), static_cast<int>(g_y),
                             kWindowSize, kWindowSize, SWP_NOACTIVATE | SWP_NOSIZE);
            }
            return 0;
        case WM_LBUTTONUP:
            g_dragging = false;
            ReleaseCapture();
            return 0;
        case WM_RBUTTONUP: {
            POINT cursor{};
            GetCursorPos(&cursor);
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
        if (HWND existing = FindWindowW(kWindowClass, nullptr)) {
            ShowWindow(existing, SW_SHOWNOACTIVATE);
        }
        return 0;
    }

    GdiplusStartupInput gdiplusInput;
    if (GdiplusStartup(&g_gdiplusToken, &gdiplusInput, nullptr) != Ok) {
        return 1;
    }

    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = WindowProcedure;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = kWindowClass;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_HAND);
    if (!RegisterClassW(&windowClass)) {
        GdiplusShutdown(g_gdiplusToken);
        return 1;
    }

    g_window = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        kWindowClass, L"", WS_POPUP, 0, 0, kWindowSize, kWindowSize,
        nullptr, nullptr, instance, nullptr);
    if (!g_window) {
        GdiplusShutdown(g_gdiplusToken);
        return 1;
    }

    const std::wstring imagePath = ExecutableDirectory() + L"\\assets\\" PET_ASSET_FILE;
    g_sprite.reset(Image::FromFile(imagePath.c_str(), FALSE));
    if (!g_sprite || g_sprite->GetLastStatus() != Ok) {
        MessageBoxW(nullptr, L"Desktop pet image is missing from the assets folder.",
                    PET_ERROR_TITLE, MB_ICONERROR);
        DestroyWindow(g_window);
        GdiplusShutdown(g_gdiplusToken);
        return 1;
    }

    g_startedAt = GetTickCount64();
    BuildFrameCache();
    ResetPosition();
    RenderPet(false, true);
    ShowWindow(g_window, SW_SHOWNOACTIVATE);
    SetTimer(g_window, kAnimationTimer, kFrameIntervalMs, nullptr);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    g_sprite.reset();
    DestroyFrameCache();
    GdiplusShutdown(g_gdiplusToken);
    if (g_mutex) {
        ReleaseMutex(g_mutex);
        CloseHandle(g_mutex);
    }
    return static_cast<int>(message.wParam);
}
