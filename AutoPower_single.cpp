// AutoPower - 定时关机软件 (单文件版)
// 编译: cl /utf-8 /EHsc /MT /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN AutoPower_single.cpp /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib comctl32.lib advapi32.lib shell32.lib

#define WINVER 0x0601
#define _WIN32_WINNT 0x0601
#define NTDDI_VERSION 0x06010000

#include <windows.h>
#include <commctrl.h>
#include <string>
#include <sstream>
#include <ctime>
#include <chrono>
#include <stdexcept>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")

// ==================== 资源ID ====================
#define IDI_APP_ICON           2001

// ==================== 控件ID ====================
#define IDC_COUNTDOWN_EDIT     1001
#define IDC_UNIT_COMBO         1002
#define IDC_SET_BUTTON         1003
#define IDC_CANCEL_BUTTON      1004
#define IDC_COUNTDOWN_LABEL    1005
#define IDC_STATUS_LABEL       1006
#define IDC_QUICK_10S          1007
#define IDC_QUICK_30S          1008
#define IDC_QUICK_1M           1009
#define IDC_QUICK_5M           1010
#define IDC_QUICK_10M          1011
#define IDC_QUICK_15M          1012
#define IDC_QUICK_30M          1013
#define IDC_QUICK_1H           1014
#define IDC_QUICK_2H           1015
#define IDC_QUICK_4H           1016
#define IDC_TIME_HINT          1017
#define IDC_INPUT_GROUP        1018
#define IDC_COUNTDOWN_GROUP    1019
#define IDC_QUICK_GROUP        1020
#define IDC_STATUS_GROUP       1021

// ==================== 自定义消息 ====================
#define WM_UPDATE_COUNTDOWN    WM_USER + 1
#define WM_UPDATE_STATUS      WM_USER + 2
#define WM_SHUTDOWN_REMINDER   WM_USER + 3
#define WM_SHOW_ERROR         WM_USER + 4

// ==================== 错误类型枚举 ====================
enum ErrorType {
    Error_None = 0,
    Error_InvalidTimeFormat,
    Error_InvalidTimeValue,
    Error_PermissionDenied,
    Error_SystemCallFailed,
    Error_AlreadyInProgress,
    Error_TimerNotActive,
    Error_UnknownError
};

// ==================== ShutdownManager 类 ====================
class ShutdownManager {
public:
    enum ShutdownResult {
        Success,
        Failed,
        AccessDenied,
        AlreadyInProgress,
        InvalidParameter
    };

    enum ShutdownAction {
        Shutdown,
        Restart,
        Logoff
    };

    ShutdownManager();
    ~ShutdownManager();

    ShutdownResult setShutdownTimer(int seconds);
    ShutdownResult shutdownNow(ShutdownAction action = Shutdown);
    ShutdownResult cancelShutdown();
    bool hasPendingShutdown() const;
    int getRemainingSeconds() const;

private:
    bool m_hasPendingTask;
    int m_remainingSeconds;

    static VOID CALLBACK ShutdownTimerProc(HWND, UINT, UINT_PTR, DWORD);
    bool enableShutdownPrivileges();
};

ShutdownManager::ShutdownManager()
    : m_hasPendingTask(false), m_remainingSeconds(0) {}

ShutdownManager::~ShutdownManager() {}

bool ShutdownManager::enableShutdownPrivileges() {
    HANDLE hToken;
    TOKEN_PRIVILEGES tkp;

    if (!OpenProcessToken(GetCurrentProcess(),
                         TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
                         &hToken)) {
        return false;
    }

    LookupPrivilegeValueW(NULL, SE_SHUTDOWN_NAME, &tkp.Privileges[0].Luid);
    tkp.PrivilegeCount = 1;
    tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, (PTOKEN_PRIVILEGES)NULL, 0);
    return GetLastError() == 0;
}

ShutdownManager::ShutdownResult ShutdownManager::setShutdownTimer(int seconds) {
    if (seconds <= 0) return InvalidParameter;

    if (!enableShutdownPrivileges()) return AccessDenied;

    if (hasPendingShutdown()) cancelShutdown();

    static UINT_PTR nShutdownTimer = 0;
    if (nShutdownTimer != 0) {
        KillTimer(NULL, nShutdownTimer);
        nShutdownTimer = 0;
    }

    nShutdownTimer = SetTimer(NULL, 0, seconds * 1000, &ShutdownManager::ShutdownTimerProc);
    if (nShutdownTimer == 0) return Failed;

    m_hasPendingTask = true;
    m_remainingSeconds = seconds;
    return Success;
}

ShutdownManager::ShutdownResult ShutdownManager::shutdownNow(ShutdownAction action) {
    if (!enableShutdownPrivileges()) return AccessDenied;

    UINT flags = 0;
    switch (action) {
        case Shutdown:  flags = EWX_SHUTDOWN; break;
        case Restart:   flags = EWX_REBOOT;   break;
        case Logoff:    flags = EWX_LOGOFF;   break;
    }

    if (!ExitWindowsEx(flags, 0)) return Failed;
    return Success;
}

ShutdownManager::ShutdownResult ShutdownManager::cancelShutdown() {
    if (!hasPendingShutdown()) return Success;
    if (!enableShutdownPrivileges()) return AccessDenied;

    // 取消SetTimer设置的定时器
    static UINT_PTR nShutdownTimer = 0;
    if (nShutdownTimer != 0) {
        KillTimer(NULL, nShutdownTimer);
        nShutdownTimer = 0;
    }

    m_hasPendingTask = false;
    m_remainingSeconds = 0;
    return Success;
}

bool ShutdownManager::hasPendingShutdown() const { return m_hasPendingTask; }
int ShutdownManager::getRemainingSeconds() const { return m_remainingSeconds; }

VOID CALLBACK ShutdownManager::ShutdownTimerProc(HWND, UINT, UINT_PTR, DWORD) {
    ExitWindowsEx(EWX_SHUTDOWN, 0);
}

// ==================== TimerManager 类 ====================
class TimerManager {
public:
    explicit TimerManager(HWND hwnd);
    ~TimerManager();

    bool setShutdownTimer(int seconds);
    bool cancelShutdown();
    std::wstring getCountdownDisplayW() const;
    std::wstring getStatusDescriptionW() const;
    bool hasPendingTask() const;
    int getRemainingSeconds() const;

private:
    ShutdownManager* m_shutdownManager;
    HWND m_hwnd;
    std::wstring m_shutdownTime;
    FILETIME m_targetTime;
    bool m_taskActive;

    int calculateRemainingSeconds() const;
    std::wstring formatCountdown(int totalSeconds) const;
    void notifyError(ErrorType errorType, const std::wstring& message);
};

TimerManager::TimerManager(HWND hwnd)
    : m_hwnd(hwnd)
    , m_shutdownManager(new ShutdownManager())
    , m_taskActive(false) {
    ZeroMemory(&m_targetTime, sizeof(m_targetTime));
}

TimerManager::~TimerManager() {
    if (m_shutdownManager) delete m_shutdownManager;
}

bool TimerManager::setShutdownTimer(int seconds) {
    if (seconds <= 0) {
        notifyError(Error_InvalidTimeValue, L"无效的倒计时时间");
        return false;
    }

    FILETIME currentFt;
    GetSystemTimeAsFileTime(&currentFt);

    ULARGE_INTEGER uli;
    uli.LowPart = currentFt.dwLowDateTime;
    uli.HighPart = currentFt.dwHighDateTime;
    uli.QuadPart += (ULONGLONG)seconds * 10000000ULL;

    m_targetTime.dwLowDateTime = uli.LowPart;
    m_targetTime.dwHighDateTime = uli.HighPart;
    m_taskActive = true;

    SYSTEMTIME targetSt;
    FILETIME ft;
    ft.dwLowDateTime = uli.LowPart;
    ft.dwHighDateTime = uli.HighPart;
    FileTimeToSystemTime(&ft, &targetSt);
    wchar_t buf[16];
    swprintf_s(buf, L"%02d:%02d", targetSt.wHour, targetSt.wMinute);
    m_shutdownTime = buf;

    auto result = m_shutdownManager->setShutdownTimer(seconds);

    if (result == ShutdownManager::Success) return true;

    m_taskActive = false;
    switch (result) {
        case ShutdownManager::AccessDenied:
            notifyError(Error_PermissionDenied, L"无法获取关机权限");
            break;
        case ShutdownManager::Failed:
            notifyError(Error_SystemCallFailed, L"设置关机时间失败");
            break;
        default:
            notifyError(Error_UnknownError, L"未知错误");
            break;
    }
    return false;
}

bool TimerManager::cancelShutdown() {
    if (!m_taskActive) {
        notifyError(Error_TimerNotActive, L"没有活动的关机任务");
        return false;
    }

    auto result = m_shutdownManager->cancelShutdown();
    if (result == ShutdownManager::Success) {
        m_taskActive = false;
        m_shutdownTime.clear();
        ZeroMemory(&m_targetTime, sizeof(m_targetTime));
        return true;
    }
    return false;
}

std::wstring TimerManager::getCountdownDisplayW() const {
    if (!m_taskActive) return L"未启动关机倒计时";
    int totalSeconds = calculateRemainingSeconds();
    if (totalSeconds <= 0) return L"关机时间已到！";
    return formatCountdown(totalSeconds);
}

std::wstring TimerManager::getStatusDescriptionW() const {
    if (!m_taskActive) return L"没有待处理的关机任务";
    return L"关机时间: " + m_shutdownTime + L"  |  倒计时: " + getCountdownDisplayW();
}

bool TimerManager::hasPendingTask() const { return m_taskActive; }
int TimerManager::getRemainingSeconds() const {
    if (!m_taskActive) return 0;
    return calculateRemainingSeconds();
}

int TimerManager::calculateRemainingSeconds() const {
    if (!m_taskActive) return 0;

    FILETIME currentFt;
    GetSystemTimeAsFileTime(&currentFt);

    ULARGE_INTEGER current, target;
    current.LowPart = currentFt.dwLowDateTime;
    current.HighPart = currentFt.dwHighDateTime;
    target.LowPart = m_targetTime.dwLowDateTime;
    target.HighPart = m_targetTime.dwHighDateTime;

    if (target.QuadPart <= current.QuadPart) return 0;
    ULONGLONG diff = target.QuadPart - current.QuadPart;
    return (int)(diff / 10000000ULL);
}

std::wstring TimerManager::formatCountdown(int totalSeconds) const {
    if (totalSeconds <= 0) return L"关机时间已到！";

    int hours = totalSeconds / 3600;
    int minutes = (totalSeconds % 3600) / 60;
    int seconds = totalSeconds % 60;

    std::wstringstream ss;
    if (hours > 0) {
        ss << hours << L"小时 " << minutes << L"分 " << seconds << L"秒";
    } else if (minutes > 0) {
        ss << minutes << L"分 " << seconds << L"秒";
    } else {
        ss << seconds << L"秒";
    }
    return ss.str();
}

void TimerManager::notifyError(ErrorType errorType, const std::wstring& message) {
    std::wstring* pMsg = new std::wstring(message);
    PostMessageW(m_hwnd, WM_SHOW_ERROR, (WPARAM)pMsg, (LPARAM)errorType);
}

// ==================== MainWindow 类 ====================
class MainWindow {
public:
    MainWindow(HINSTANCE hInstance);
    ~MainWindow();

    bool create();
    void show(int nCmdShow);
    HWND getHwnd() const { return m_hwnd; }
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    HWND m_hwnd;
    HINSTANCE m_hInstance;
    HWND m_hCountdownEdit, m_hUnitCombo, m_hSetButton, m_hCancelButton;
    HWND m_hCountdownLabel, m_hStatusLabel;
    HWND m_hQuick10s, m_hQuick30s, m_hQuick1m, m_hQuick5m, m_hQuick10m;
    HWND m_hQuick15m, m_hQuick30m, m_hQuick1h, m_hQuick2h, m_hQuick4h;
    TimerManager* m_timerManager;
    UINT m_uiTimer;
    bool m_reminderShown;

    void createControls();
    void setControlFont(HWND hwnd, int size, bool bold = false);
    void updateCountdown();
    void updateStatus();
    void onSetShutdown();
    void onCancelShutdown();
    void onQuickSet(int seconds);
    void showReminder();
    void showError(const std::wstring& message);
    int getInputSeconds();
    LRESULT handleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    void handleCommand(WPARAM wParam);
};

static MainWindow* g_pMainWindow = NULL;
static const wchar_t* WINDOW_CLASS_NAME = L"AutoPowerMainWindow";

MainWindow::MainWindow(HINSTANCE hInstance)
    : m_hwnd(NULL), m_hInstance(hInstance)
    , m_hCountdownEdit(NULL), m_hUnitCombo(NULL)
    , m_hSetButton(NULL), m_hCancelButton(NULL)
    , m_hCountdownLabel(NULL), m_hStatusLabel(NULL)
    , m_hQuick10s(NULL), m_hQuick30s(NULL), m_hQuick1m(NULL)
    , m_hQuick5m(NULL), m_hQuick10m(NULL), m_hQuick15m(NULL)
    , m_hQuick30m(NULL), m_hQuick1h(NULL), m_hQuick2h(NULL), m_hQuick4h(NULL)
    , m_timerManager(NULL), m_uiTimer(0), m_reminderShown(false) {
    g_pMainWindow = this;
}

MainWindow::~MainWindow() {
    if (m_uiTimer) KillTimer(m_hwnd, m_uiTimer);
    if (m_timerManager) delete m_timerManager;
    g_pMainWindow = NULL;
}

bool MainWindow::create() {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = MainWindow::WndProc;
    wc.hInstance = m_hInstance;
    wc.hIcon = LoadIcon(m_hInstance, MAKEINTRESOURCE(IDI_APP_ICON));
    wc.hIconSm = LoadIcon(m_hInstance, MAKEINTRESOURCE(IDI_APP_ICON));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = WINDOW_CLASS_NAME;

    if (!RegisterClassExW(&wc)) return false;

    m_hwnd = CreateWindowExW(
        0, WINDOW_CLASS_NAME,
        L"AutoPower - 定时关机",
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 500, 500,
        NULL, NULL, m_hInstance, NULL);

    if (!m_hwnd) return false;

    createControls();
    m_timerManager = new TimerManager(m_hwnd);
    if (!m_timerManager) return false;
    m_uiTimer = SetTimer(m_hwnd, 1, 1000, NULL);
    updateCountdown();
    updateStatus();
    return true;
}

void MainWindow::show(int nCmdShow) {
    ShowWindow(m_hwnd, nCmdShow);
    UpdateWindow(m_hwnd);
}

void MainWindow::createControls() {
    int y = 12;

    CreateWindowExW(0, L"BUTTON", L"倒计时设置",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        10, y, 465, 65, m_hwnd, (HMENU)IDC_INPUT_GROUP, m_hInstance, NULL);

    CreateWindowExW(0, L"STATIC", L"时间：",
        WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE,
        20, y + 25, 40, 25, m_hwnd, NULL, m_hInstance, NULL);

    m_hCountdownEdit = CreateWindowExW(0, L"EDIT", L"30",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER | WS_BORDER,
        62, y + 23, 80, 26, m_hwnd, (HMENU)IDC_COUNTDOWN_EDIT, m_hInstance, NULL);

    m_hUnitCombo = CreateWindowExW(0, L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
        148, y + 23, 75, 120, m_hwnd, (HMENU)IDC_UNIT_COMBO, m_hInstance, NULL);
    SendMessageW(m_hUnitCombo, CB_ADDSTRING, 0, (LPARAM)L"秒");
    SendMessageW(m_hUnitCombo, CB_ADDSTRING, 0, (LPARAM)L"分钟");
    SendMessageW(m_hUnitCombo, CB_ADDSTRING, 0, (LPARAM)L"小时");
    SendMessageW(m_hUnitCombo, CB_SETCURSEL, 1, 0);

    m_hSetButton = CreateWindowExW(0, L"BUTTON", L"设置关机",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        235, y + 22, 100, 28, m_hwnd, (HMENU)IDC_SET_BUTTON, m_hInstance, NULL);

    m_hCancelButton = CreateWindowExW(0, L"BUTTON", L"取消关机",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP | WS_DISABLED,
        345, y + 22, 100, 28, m_hwnd, (HMENU)IDC_CANCEL_BUTTON, m_hInstance, NULL);

    y += 75;

    CreateWindowExW(0, L"BUTTON", L"倒计时显示",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        10, y, 465, 65, m_hwnd, (HMENU)IDC_COUNTDOWN_GROUP, m_hInstance, NULL);

    m_hCountdownLabel = CreateWindowExW(0, L"STATIC",
        L"未启动关机倒计时",
        WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE,
        20, y + 20, 440, 35, m_hwnd, (HMENU)IDC_COUNTDOWN_LABEL, m_hInstance, NULL);

    y += 75;

    CreateWindowExW(0, L"BUTTON", L"快捷设置",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        10, y, 465, 90, m_hwnd, (HMENU)IDC_QUICK_GROUP, m_hInstance, NULL);

    m_hQuick10s = CreateWindowExW(0, L"BUTTON", L"10秒",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        20, y + 22, 75, 25, m_hwnd, (HMENU)IDC_QUICK_10S, m_hInstance, NULL);

    m_hQuick30s = CreateWindowExW(0, L"BUTTON", L"30秒",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        102, y + 22, 75, 25, m_hwnd, (HMENU)IDC_QUICK_30S, m_hInstance, NULL);

    m_hQuick1m = CreateWindowExW(0, L"BUTTON", L"1分钟",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        184, y + 22, 75, 25, m_hwnd, (HMENU)IDC_QUICK_1M, m_hInstance, NULL);

    m_hQuick5m = CreateWindowExW(0, L"BUTTON", L"5分钟",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        266, y + 22, 75, 25, m_hwnd, (HMENU)IDC_QUICK_5M, m_hInstance, NULL);

    m_hQuick10m = CreateWindowExW(0, L"BUTTON", L"10分钟",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        348, y + 22, 75, 25, m_hwnd, (HMENU)IDC_QUICK_10M, m_hInstance, NULL);

    m_hQuick15m = CreateWindowExW(0, L"BUTTON", L"15分钟",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        20, y + 54, 75, 25, m_hwnd, (HMENU)IDC_QUICK_15M, m_hInstance, NULL);

    m_hQuick30m = CreateWindowExW(0, L"BUTTON", L"30分钟",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        102, y + 54, 75, 25, m_hwnd, (HMENU)IDC_QUICK_30M, m_hInstance, NULL);

    m_hQuick1h = CreateWindowExW(0, L"BUTTON", L"1小时",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        184, y + 54, 75, 25, m_hwnd, (HMENU)IDC_QUICK_1H, m_hInstance, NULL);

    m_hQuick2h = CreateWindowExW(0, L"BUTTON", L"2小时",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        266, y + 54, 75, 25, m_hwnd, (HMENU)IDC_QUICK_2H, m_hInstance, NULL);

    m_hQuick4h = CreateWindowExW(0, L"BUTTON", L"4小时",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        348, y + 54, 75, 25, m_hwnd, (HMENU)IDC_QUICK_4H, m_hInstance, NULL);

    y += 100;

    CreateWindowExW(0, L"BUTTON", L"状态信息",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        10, y, 465, 50, m_hwnd, (HMENU)IDC_STATUS_GROUP, m_hInstance, NULL);

    m_hStatusLabel = CreateWindowExW(0, L"STATIC",
        L"没有待处理的关机任务",
        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
        20, y + 18, 440, 25, m_hwnd, (HMENU)IDC_STATUS_LABEL, m_hInstance, NULL);

    setControlFont(m_hCountdownLabel, 20, true);
    setControlFont(m_hStatusLabel, 10);
    setControlFont(m_hCountdownEdit, 12, true);
    setControlFont(m_hSetButton, 10, true);
    setControlFont(m_hCancelButton, 10, true);
    setControlFont(m_hQuick10s, 10);
    setControlFont(m_hQuick30s, 10);
    setControlFont(m_hQuick1m, 10);
    setControlFont(m_hQuick5m, 10);
    setControlFont(m_hQuick10m, 10);
    setControlFont(m_hQuick15m, 10);
    setControlFont(m_hQuick30m, 10);
    setControlFont(m_hQuick1h, 10);
    setControlFont(m_hQuick2h, 10);
    setControlFont(m_hQuick4h, 10);
}

void MainWindow::setControlFont(HWND hwnd, int size, bool bold) {
    HDC hdc = GetDC(hwnd);
    int nHeight = -MulDiv(size, GetDeviceCaps(hdc, LOGPIXELSY), 72);
    ReleaseDC(hwnd, hdc);

    HFONT hFont = CreateFontW(
        nHeight, 0, 0, 0,
        bold ? FW_BOLD : FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");

    SendMessage(hwnd, WM_SETFONT, (WPARAM)hFont, TRUE);
}

int MainWindow::getInputSeconds() {
    wchar_t buf[32] = {};
    GetWindowTextW(m_hCountdownEdit, buf, 32);
    int value = 0;
    try { value = std::stoi(buf); } catch (...) { return -1; }
    if (value <= 0) return -1;

    int unit = (int)SendMessageW(m_hUnitCombo, CB_GETCURSEL, 0, 0);
    switch (unit) {
        case 0:  return value;
        case 1:  return value * 60;
        case 2:  return value * 3600;
        default: return value * 60;
    }
}

void MainWindow::updateCountdown() {
    if (!m_timerManager) return;
    std::wstring countdown = m_timerManager->getCountdownDisplayW();
    SetWindowTextW(m_hCountdownLabel, countdown.c_str());
}

void MainWindow::updateStatus() {
    if (!m_timerManager) return;
    std::wstring status = m_timerManager->getStatusDescriptionW();
    SetWindowTextW(m_hStatusLabel, status.c_str());

    BOOL hasTask = m_timerManager->hasPendingTask() ? TRUE : FALSE;
    EnableWindow(m_hCancelButton, hasTask);

    std::wstring title = L"AutoPower - 定时关机";
    if (m_timerManager->hasPendingTask()) title += L" - 关机已设置";
    SetWindowTextW(m_hwnd, title.c_str());
}

void MainWindow::onSetShutdown() {
    int totalSeconds = getInputSeconds();
    if (totalSeconds <= 0) {
        showError(L"请输入有效的倒计时时间");
        return;
    }
    if (totalSeconds > 86400) {
        showError(L"倒计时不能超过24小时");
        return;
    }
    if (m_timerManager->setShutdownTimer(totalSeconds)) {
        std::wstring msg = L"关机已设置，" + m_timerManager->getCountdownDisplayW();
        MessageBoxW(m_hwnd, msg.c_str(), L"设置成功", MB_OK | MB_ICONINFORMATION);
        updateStatus();
    }
}

void MainWindow::onCancelShutdown() {
    if (m_timerManager->cancelShutdown()) {
        MessageBoxW(m_hwnd, L"关机任务已取消", L"取消成功", MB_OK | MB_ICONINFORMATION);
        m_reminderShown = false;
        updateStatus();
        updateCountdown();
    }
}

void MainWindow::onQuickSet(int seconds) {
    if (m_timerManager->setShutdownTimer(seconds)) {
        std::wstring msg;
        if (seconds < 60) {
            msg = std::to_wstring(seconds) + L"秒后关机";
        } else if (seconds % 3600 == 0) {
            msg = std::to_wstring(seconds / 3600) + L"小时后关机";
        } else {
            msg = std::to_wstring(seconds / 60) + L"分钟后关机";
        }
        MessageBoxW(m_hwnd, msg.c_str(), L"设置成功", MB_OK | MB_ICONINFORMATION);
        updateStatus();
    }
}

void MainWindow::showReminder() {
    if (m_reminderShown) return;
    m_reminderShown = true;

    int result = MessageBoxW(m_hwnd,
        L"系统将在 5 分钟后关机！\n\n"
        L"请立即保存您的工作并关闭所有应用程序。\n\n"
        L"点击「是」取消关机，点击「否」继续关机。",
        L"关机提醒",
        MB_YESNO | MB_ICONWARNING | MB_TOPMOST);

    if (result == IDYES) onCancelShutdown();
}

void MainWindow::showError(const std::wstring& message) {
    MessageBoxW(m_hwnd, message.c_str(), L"AutoPower 错误", MB_OK | MB_ICONERROR);
}

LRESULT MainWindow::handleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_TIMER:
            if (wParam == 1) {
                updateCountdown();
                if (m_timerManager && m_timerManager->hasPendingTask()) {
                    if (m_timerManager->getRemainingSeconds() == 300 && !m_reminderShown) {
                        showReminder();
                    }
                }
            }
            return 0;

        case WM_COMMAND:
            handleCommand(wParam);
            return 0;

        case WM_SHOW_ERROR: {
            std::wstring* pMsg = reinterpret_cast<std::wstring*>(wParam);
            if (pMsg) { showError(*pMsg); delete pMsg; }
            return 0;
        }

        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wParam;
            HWND hCtrl = (HWND)lParam;
            if (hCtrl == m_hCountdownLabel) {
                wchar_t text[256] = {};
                GetWindowTextW(hCtrl, text, 256);
                std::wstring ws(text);
                if (ws.find(L"已到") != std::wstring::npos) {
                    SetTextColor(hdc, RGB(220, 0, 0));
                } else if (ws.find(L"秒") != std::wstring::npos ||
                           ws.find(L"分") != std::wstring::npos ||
                           ws.find(L"小时") != std::wstring::npos) {
                    SetTextColor(hdc, RGB(200, 100, 0));
                } else {
                    SetTextColor(hdc, RGB(128, 128, 128));
                }
                SetBkColor(hdc, RGB(255, 255, 255));
                return (LRESULT)GetStockObject(WHITE_BRUSH);
            }
            return DefWindowProcW(m_hwnd, msg, wParam, lParam);
        }

        case WM_CLOSE:
            DestroyWindow(m_hwnd);
            return 0;

        case WM_DESTROY:
            if (g_pMainWindow && g_pMainWindow->m_uiTimer) {
                KillTimer(g_pMainWindow->m_hwnd, g_pMainWindow->m_uiTimer);
            }
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProcW(m_hwnd, msg, wParam, lParam);
    }
    return 0;
}

void MainWindow::handleCommand(WPARAM wParam) {
    int id = LOWORD(wParam);
    int notif = HIWORD(wParam);

    if (notif == CBN_SELCHANGE && id == IDC_UNIT_COMBO) {
        wchar_t buf[32] = {};
        GetWindowTextW(m_hCountdownEdit, buf, 32);
        int val = 0;
        try { val = std::stoi(buf); } catch (...) { return; }
        if (val <= 0) return;

        static int lastUnit = 1;
        int curUnit = (int)SendMessageW(m_hUnitCombo, CB_GETCURSEL, 0, 0);

        int totalSec = val;
        switch (lastUnit) {
            case 0: totalSec = val; break;
            case 1: totalSec = val * 60; break;
            case 2: totalSec = val * 3600; break;
        }

        int newVal = totalSec;
        switch (curUnit) {
            case 0: newVal = totalSec; break;
            case 1: newVal = totalSec / 60; break;
            case 2: newVal = totalSec / 3600; break;
        }
        if (newVal <= 0) newVal = 1;

        wchar_t newBuf[32];
        _itow_s(newVal, newBuf, 10);
        SetWindowTextW(m_hCountdownEdit, newBuf);
        lastUnit = curUnit;
        return;
    }

    if (notif != BN_CLICKED) return;

    switch (id) {
        case IDC_SET_BUTTON:    onSetShutdown(); break;
        case IDC_CANCEL_BUTTON: onCancelShutdown(); break;
        case IDC_QUICK_10S:     onQuickSet(10); break;
        case IDC_QUICK_30S:     onQuickSet(30); break;
        case IDC_QUICK_1M:      onQuickSet(60); break;
        case IDC_QUICK_5M:      onQuickSet(300); break;
        case IDC_QUICK_10M:     onQuickSet(600); break;
        case IDC_QUICK_15M:     onQuickSet(900); break;
        case IDC_QUICK_30M:     onQuickSet(1800); break;
        case IDC_QUICK_1H:      onQuickSet(3600); break;
        case IDC_QUICK_2H:      onQuickSet(7200); break;
        case IDC_QUICK_4H:      onQuickSet(14400); break;
    }
}

LRESULT CALLBACK MainWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (g_pMainWindow && g_pMainWindow->getHwnd() == hwnd) {
        return g_pMainWindow->handleMessage(msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ==================== 程序入口 ====================
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    INITCOMMONCONTROLSEX icc = {};
    icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icc.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);

    MainWindow mainWindow(hInstance);
    if (!mainWindow.create()) {
        MessageBoxW(NULL, L"窗口创建失败！", L"AutoPower 错误", MB_ICONERROR);
        return 1;
    }

    mainWindow.show(nCmdShow);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (!IsDialogMessage(mainWindow.getHwnd(), &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int)msg.wParam;
}
