// AutoPower - 定时关机软件 (单文件版)
// 编译: cl /utf-8 /EHsc /MT /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN AutoPower_single.cpp /link /SUBSYSTEM:WINDOWS,5.01 user32.lib gdi32.lib comctl32.lib advapi32.lib shell32.lib

#define WINVER 0x0601
#define _WIN32_WINNT 0x0601
#define NTDDI_VERSION 0x06010000

#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <objidl.h>
#include <gdiplus.h>
#include <string>
#include <sstream>
#include <vector>
#include <ctime>
#include <chrono>
#include <stdexcept>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "gdiplus.lib")

// ==================== 资源ID ====================
#define IDI_APP_ICON           2001
#define IDB_LOGO_PNG           2002

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
#define IDC_SECONDS_HINT      1025
#define IDC_PAGE_TAB          1026
#define IDC_SCHED_CREATE      1027
#define IDC_SCHED_DELETE      1028
#define IDC_SCHED_CLOCK       1029
#define IDC_SCHED_STATE       1030
#define IDC_SCHED_HOUR        1032
#define IDC_SCHED_MIN         1033
#define IDC_SCHED_HINT        1034
#define IDC_SCHED_GROUP       1045
#define IDC_SCHED_CLOCK_GROUP 1046
#define IDC_SCHED_STATE_GROUP 1048
#define IDC_ABOUT_LOGO        1049
#define IDC_ABOUT_TITLE       1050
#define IDC_ABOUT_VERSION     1051
#define IDC_ABOUT_DESC        1052
#define IDC_ABOUT_LINK        1053
#define IDC_ABOUT_LICENSE     1054
#define IDC_ABOUT_COPYRIGHT   1055
#define IDC_ABOUT_DESC2       1056

// ==================== 关于页面常量 ====================
#define APP_VERSION           L"1.0.2-1"
#define GITHUB_URL            L"https://github.com/youye-luna/AutoPower"

// ==================== 定时关机任务常量 ====================
#define SCHED_TASK_NAME       L"\"关机\""

// ==================== 关于页面 Logo ====================
static Gdiplus::Bitmap* g_pLogoBmp = NULL;

// 从资源中的 PNG (RCDATA) 加载 GDI+ 位图
static Gdiplus::Bitmap* LoadPngFromResource(HINSTANCE hInst, UINT resId) {
    HRSRC hRes = FindResourceW(hInst, MAKEINTRESOURCEW(resId), RT_RCDATA);
    if (!hRes) return NULL;
    HGLOBAL hMem = LoadResource(hInst, hRes);
    if (!hMem) return NULL;
    const void* pData = LockResource(hMem);
    DWORD size = SizeofResource(hInst, hRes);
    if (!pData || !size) return NULL;

    HGLOBAL hBuf = GlobalAlloc(GMEM_MOVEABLE, size);
    if (!hBuf) return NULL;
    void* pBuf = GlobalLock(hBuf);
    if (!pBuf) { GlobalFree(hBuf); return NULL; }
    memcpy(pBuf, pData, size);
    GlobalUnlock(hBuf);

    IStream* pStream = NULL;
    if (FAILED(CreateStreamOnHGlobal(hBuf, TRUE, &pStream))) {
        GlobalFree(hBuf);
        return NULL;
    }
    Gdiplus::Bitmap* pTmp = Gdiplus::Bitmap::FromStream(pStream);
    Gdiplus::Bitmap* pClone = pTmp ?
        pTmp->Clone(0, 0, (INT)pTmp->GetWidth(), (INT)pTmp->GetHeight(),
                    PixelFormat32bppARGB) : NULL;
    delete pTmp;
    pStream->Release();   // fDeleteOnRelease=TRUE，同时释放 hBuf
    return pClone;
}

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

    ShutdownManager();
    ~ShutdownManager();

    ShutdownResult setShutdownTimer(int seconds);
    ShutdownResult cancelShutdown();
    bool hasPendingShutdown() const;
    int getRemainingSeconds() const;

private:
    bool m_hasPendingTask;
    int m_remainingSeconds;

    ShutdownResult setSystemShutdown(int seconds);
    ShutdownResult cancelSystemShutdown();
};

ShutdownManager::ShutdownManager()
    : m_hasPendingTask(false), m_remainingSeconds(0) {}

ShutdownManager::~ShutdownManager() {}

ShutdownManager::ShutdownResult ShutdownManager::setShutdownTimer(int seconds) {
    if (seconds <= 0) return InvalidParameter;

    // 使用系统命令模式：shutdown -s -t
    return setSystemShutdown(seconds);
}

ShutdownManager::ShutdownResult ShutdownManager::cancelShutdown() {
    if (!hasPendingShutdown()) return Success;

    // 使用 shutdown -a 取消系统关机任务
    return cancelSystemShutdown();
}

ShutdownManager::ShutdownResult ShutdownManager::setSystemShutdown(int seconds) {
    // 先取消已有的系统关机任务
    if (hasPendingShutdown()) cancelSystemShutdown();

    std::wstring params = L"-s -t " + std::to_wstring(seconds);
    HINSTANCE hInst = ShellExecuteW(NULL, L"open", L"shutdown.exe",
        params.c_str(), NULL, SW_HIDE);
    // ShellExecuteW 返回值大于32表示成功
    if ((INT_PTR)hInst <= 32) return Failed;

    m_hasPendingTask = true;
    m_remainingSeconds = seconds;
    return Success;
}

ShutdownManager::ShutdownResult ShutdownManager::cancelSystemShutdown() {
    HINSTANCE hInst = ShellExecuteW(NULL, L"open", L"shutdown.exe",
        L"-a", NULL, SW_HIDE);
    if ((INT_PTR)hInst <= 32) return Failed;

    m_hasPendingTask = false;
    m_remainingSeconds = 0;
    return Success;
}

bool ShutdownManager::hasPendingShutdown() const { return m_hasPendingTask; }
int ShutdownManager::getRemainingSeconds() const { return m_remainingSeconds; }

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

// ==================== schtasks 命令辅助 ====================
static int g_lastSchExit = 0;   // 最近一次 schtasks 的退出码

// 以隐藏窗口方式运行 schtasks，同步等待并返回退出码
static bool RunSchtasks(const std::wstring& args) {
    std::wstring cmd = L"schtasks.exe " + args;
    std::vector<wchar_t> buf(cmd.begin(), cmd.end());
    buf.push_back(L'\0');

    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {};

    if (!CreateProcessW(NULL, buf.data(), NULL, NULL, FALSE,
                        CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        g_lastSchExit = -1;
        return false;
    }
    WaitForSingleObject(pi.hProcess, 30000);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    g_lastSchExit = (int)code;
    return true;
}

// 查询系统计划任务「关机」是否存在
static bool SchTaskExists() {
    RunSchtasks(std::wstring(L"/query /tn ") + SCHED_TASK_NAME);
    return g_lastSchExit == 0;
}

// 提权方式执行 schtasks（触发 UAC）
static bool ElevatedRunSchtasks(const std::wstring& args) {
    SHELLEXECUTEINFOW sei = {};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
    sei.lpVerb = L"runas";
    sei.lpFile = L"schtasks.exe";
    sei.lpParameters = args.c_str();
    sei.nShow = SW_HIDE;
    if (!ShellExecuteExW(&sei) || !sei.hProcess) return false;
    WaitForSingleObject(sei.hProcess, 60000);
    DWORD code = 1;
    GetExitCodeProcess(sei.hProcess, &code);
    CloseHandle(sei.hProcess);
    return code == 0;
}

// 时间格式化：HH:MM:SS
static void GetClockString(wchar_t* buf, size_t cch) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    swprintf_s(buf, cch, L"%02u:%02u:%02u", st.wHour, st.wMinute, st.wSecond);
}

// 计算「现在」到目标 FILETIME 的剩余秒数
static long long DiffSecondsFromNow(const FILETIME& target) {
    FILETIME now;
    GetSystemTimeAsFileTime(&now);
    ULARGE_INTEGER t, n;
    t.LowPart = target.dwLowDateTime;   t.HighPart = target.dwHighDateTime;
    n.LowPart = now.dwLowDateTime;      n.HighPart = now.dwHighDateTime;
    if (t.QuadPart <= n.QuadPart) return 0;
    return (long long)((t.QuadPart - n.QuadPart) / 10000000ULL);
}

// 把剩余秒数格式化为“X小时X分X秒”
static std::wstring FormatRemaining(long long totalSeconds) {
    if (totalSeconds <= 0) return L"0秒";
    long long h = totalSeconds / 3600;
    int m = (int)(totalSeconds % 3600) / 60;
    int s = (int)(totalSeconds % 60);
    std::wstringstream ss;
    if (h > 0) ss << h << L"小时";
    if (m > 0) ss << m << L"分";
    ss << s << L"秒";
    return ss.str();
}

// 把 "HH:MM" 解析为 FILETIME：当天该时刻；若已过去则取明天同一时刻
static bool ParseHHMMToTarget(const std::wstring& hhmm, FILETIME& out) {
    if (hhmm.size() != 5 || hhmm[2] != L':') return false;
    int h = _wtoi(hhmm.substr(0, 2).c_str());
    int m = _wtoi(hhmm.substr(3, 2).c_str());
    if (h < 0 || h > 23 || m < 0 || m > 59) return false;

    SYSTEMTIME st;
    GetLocalTime(&st);
    st.wHour = (WORD)h;
    st.wMinute = (WORD)m;
    st.wSecond = 0;
    st.wMilliseconds = 0;

    FILETIME ft;
    if (!SystemTimeToFileTime(&st, &ft)) return false;

    ULARGE_INTEGER tgt, now;
    tgt.LowPart = ft.dwLowDateTime;   tgt.HighPart = ft.dwHighDateTime;
    FILETIME nft;
    GetSystemTimeAsFileTime(&nft);
    now.LowPart = nft.dwLowDateTime;  now.HighPart = nft.dwHighDateTime;

    if (tgt.QuadPart <= now.QuadPart)
        tgt.QuadPart += 24ULL * 3600ULL * 10000000ULL; // 已过时刻 -> 明天

    out.dwLowDateTime = tgt.LowPart;
    out.dwHighDateTime = tgt.HighPart;
    return true;
}

// 删除系统中的「关机」计划任务；失败时询问是否提权重试（触发 UAC）
static bool DeleteShutdownTask(HWND owner) {
    RunSchtasks(std::wstring(L"/delete /tn ") + SCHED_TASK_NAME + L" /f");
    if (g_lastSchExit == 0) return true;

    int r = MessageBoxW(owner,
        L"直接删除失败（可能需要管理员权限）。\n是否以管理员身份重试？",
        L"权限不足", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON1);
    if (r != IDYES) return false;
    return ElevatedRunSchtasks(
        std::wstring(L"/delete /tn ") + SCHED_TASK_NAME + L" /f");
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
    HWND m_hSecondsHint;
    HWND m_hQuick10s, m_hQuick30s, m_hQuick1m, m_hQuick5m, m_hQuick10m;
    HWND m_hQuick15m, m_hQuick30m, m_hQuick1h, m_hQuick2h, m_hQuick4h;
    HWND m_hTab;
    // 页面切换需要的容器句柄
    HWND m_hTimeStatic, m_hInputGroupBox;
    HWND m_hCountdownGroupBox, m_hQuickGroupBox, m_hStatusGroupBox;
    // 页面2：定时关机
    HWND m_hSchedClock, m_hSchedCreateBtn, m_hSchedDeleteBtn, m_hSchedState;
    HWND m_hSchedTimeLabel, m_hSchedHourEdit, m_hSchedColon, m_hSchedMinEdit;
    HWND m_hSchedHint;
    HWND m_hSchedGroupBox, m_hSchedClockGroup, m_hSchedStateGroup;
    // 页面3：关于
    HWND m_hAboutLogo, m_hAboutTitle, m_hAboutVersion, m_hAboutDesc;
    HWND m_hAboutDesc2, m_hAboutLink, m_hAboutLicense, m_hAboutCopyright;
    bool m_schedActive;
    std::wstring m_schedHHMM;
    FILETIME m_schedTarget;

    TimerManager* m_timerManager;
    UINT m_uiTimer;
    bool m_reminderShown;

    void createControls();
    void setControlFont(HWND hwnd, int size, bool bold = false);
    void updateCountdown();
    void updateStatus();
    std::wstring getGlobalStatusText();
    void updateTitle();
    void updateInterlock();
    void onSetShutdown();
    void onCancelShutdown();
    void onQuickSet(int seconds);
    void updateSecondsHint();
    void showReminder();
    void showError(const std::wstring& message);
    int getInputSeconds();
    LRESULT handleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    void handleCommand(WPARAM wParam);

    // 标签页 / 定时关机
    void applyPage(int index);
    void onCreateSchedule();
    void onDeleteSchedule();
    void updateSchedStateText();
    void updateSchedClock();
    void syncSchedFromSystem();
};

static MainWindow* g_pMainWindow = NULL;
static const wchar_t* WINDOW_CLASS_NAME = L"AutoPowerMainWindow";

MainWindow::MainWindow(HINSTANCE hInstance)
    : m_hwnd(NULL), m_hInstance(hInstance)
    , m_hCountdownEdit(NULL), m_hUnitCombo(NULL)
    , m_hSetButton(NULL), m_hCancelButton(NULL)
    , m_hCountdownLabel(NULL), m_hStatusLabel(NULL)
    , m_hSecondsHint(NULL)
    , m_hQuick10s(NULL), m_hQuick30s(NULL), m_hQuick1m(NULL)
    , m_hQuick5m(NULL), m_hQuick10m(NULL), m_hQuick15m(NULL)
    , m_hQuick30m(NULL), m_hQuick1h(NULL), m_hQuick2h(NULL), m_hQuick4h(NULL)
    , m_hTab(NULL)
    , m_hTimeStatic(NULL), m_hInputGroupBox(NULL)
    , m_hCountdownGroupBox(NULL), m_hQuickGroupBox(NULL), m_hStatusGroupBox(NULL)
    , m_hSchedClock(NULL), m_hSchedCreateBtn(NULL), m_hSchedDeleteBtn(NULL), m_hSchedState(NULL)
    , m_hSchedTimeLabel(NULL), m_hSchedHourEdit(NULL), m_hSchedColon(NULL), m_hSchedMinEdit(NULL)
    , m_hSchedHint(NULL)
    , m_hSchedGroupBox(NULL), m_hSchedClockGroup(NULL), m_hSchedStateGroup(NULL)
    , m_hAboutLogo(NULL), m_hAboutTitle(NULL), m_hAboutVersion(NULL), m_hAboutDesc(NULL)
    , m_hAboutDesc2(NULL), m_hAboutLink(NULL), m_hAboutLicense(NULL), m_hAboutCopyright(NULL)
    , m_schedActive(false)
    , m_timerManager(NULL), m_uiTimer(0), m_reminderShown(false) {
    ZeroMemory(&m_schedTarget, sizeof(m_schedTarget));
    g_pMainWindow = this;
}

MainWindow::~MainWindow() {
    if (m_uiTimer) KillTimer(m_hwnd, m_uiTimer);
    if (m_timerManager) delete m_timerManager;
    delete g_pLogoBmp; g_pLogoBmp = NULL;
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
    syncSchedFromSystem();
    applyPage(0);
    return true;
}

void MainWindow::show(int nCmdShow) {
    ShowWindow(m_hwnd, nCmdShow);
    UpdateWindow(m_hwnd);
}

void MainWindow::createControls() {
    // ====== 标签页 ======
    m_hTab = CreateWindowExW(0, WC_TABCONTROLW, L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | TCS_TABS,
        6, 6, 472, 430, m_hwnd, (HMENU)IDC_PAGE_TAB, m_hInstance, NULL);
    SendMessageW(m_hTab, WM_SETFONT,
        (WPARAM)(HFONT)GetStockObject(DEFAULT_GUI_FONT), TRUE);

    TCITEMW tie = {};
    tie.mask = TCIF_TEXT;
    tie.pszText = (LPWSTR)L"倒计时关机";
    SendMessageW(m_hTab, TCM_INSERTITEMW, 0, (LPARAM)&tie);
    tie.pszText = (LPWSTR)L"定时关机";
    SendMessageW(m_hTab, TCM_INSERTITEMW, 1, (LPARAM)&tie);
    tie.pszText = (LPWSTR)L"关于";
    SendMessageW(m_hTab, TCM_INSERTITEMW, 2, (LPARAM)&tie);

    int y = 44;

    // ====== 页面1：倒计时设置组 ======
    m_hInputGroupBox = CreateWindowExW(0, L"BUTTON", L"倒计时设置",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        10, y, 465, 85, m_hwnd, (HMENU)IDC_INPUT_GROUP, m_hInstance, NULL);

    m_hTimeStatic = CreateWindowExW(0, L"STATIC", L"时间：",
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

    m_hSecondsHint = CreateWindowExW(0, L"STATIC", L"= 1800 秒",
        WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE,
        20, y + 55, 425, 20, m_hwnd, (HMENU)IDC_SECONDS_HINT, m_hInstance, NULL);

    y += 95;

    m_hCountdownGroupBox = CreateWindowExW(0, L"BUTTON", L"倒计时显示",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        10, y, 465, 65, m_hwnd, (HMENU)IDC_COUNTDOWN_GROUP, m_hInstance, NULL);

    m_hCountdownLabel = CreateWindowExW(0, L"STATIC",
        L"未启动关机倒计时",
        WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE,
        20, y + 20, 440, 35, m_hwnd, (HMENU)IDC_COUNTDOWN_LABEL, m_hInstance, NULL);

    y += 75;

    m_hQuickGroupBox = CreateWindowExW(0, L"BUTTON", L"快捷设置",
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

    m_hStatusGroupBox = CreateWindowExW(0, L"BUTTON", L"状态信息",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        10, y, 465, 60, m_hwnd, (HMENU)IDC_STATUS_GROUP, m_hInstance, NULL);

    m_hStatusLabel = CreateWindowExW(0, L"STATIC",
        L"没有待处理的关机任务。\n可设置倒计时关机或定时关机。",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        20, y + 15, 440, 38, m_hwnd, (HMENU)IDC_STATUS_LABEL, m_hInstance, NULL);

    // ====== 页面2：定时关机（布局与页面1一致） ======
    int sy = 44;

    // 定时设置组（对应"倒计时设置"）
    m_hSchedGroupBox = CreateWindowExW(0, L"BUTTON", L"定时设置",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        10, sy, 465, 85, m_hwnd, (HMENU)IDC_SCHED_GROUP, m_hInstance, NULL);

    m_hSchedTimeLabel = CreateWindowExW(0, L"STATIC", L"关机时间：",
        WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE,
        20, sy + 25, 78, 25, m_hwnd, NULL, m_hInstance, NULL);

    SYSTEMTIME stNow; GetLocalTime(&stNow);
    wchar_t initBuf[4];

    m_hSchedHourEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_CENTER | ES_NUMBER,
        98, sy + 23, 46, 30, m_hwnd, (HMENU)IDC_SCHED_HOUR, m_hInstance, NULL);
    SendMessageW(m_hSchedHourEdit, EM_LIMITTEXT, 2, 0);
    swprintf_s(initBuf, L"%02u", stNow.wHour);
    SetWindowTextW(m_hSchedHourEdit, initBuf);

    m_hSchedColon = CreateWindowExW(0, L"STATIC", L"：",
        WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE,
        144, sy + 25, 18, 25, m_hwnd, NULL, m_hInstance, NULL);

    m_hSchedMinEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_CENTER | ES_NUMBER,
        162, sy + 23, 46, 30, m_hwnd, (HMENU)IDC_SCHED_MIN, m_hInstance, NULL);
    SendMessageW(m_hSchedMinEdit, EM_LIMITTEXT, 2, 0);
    swprintf_s(initBuf, L"%02u", stNow.wMinute);
    SetWindowTextW(m_hSchedMinEdit, initBuf);

    m_hSchedCreateBtn = CreateWindowExW(0, L"BUTTON", L"定时关机",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP,
        236, sy + 22, 100, 28, m_hwnd, (HMENU)IDC_SCHED_CREATE, m_hInstance, NULL);

    m_hSchedDeleteBtn = CreateWindowExW(0, L"BUTTON", L"删除定时任务",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP | WS_DISABLED,
        345, sy + 22, 100, 28, m_hwnd, (HMENU)IDC_SCHED_DELETE, m_hInstance, NULL);

    m_hSchedHint = CreateWindowExW(0, L"STATIC", L"今天/明天 --:-- 关机",
        WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE,
        20, sy + 55, 425, 20, m_hwnd, (HMENU)IDC_SCHED_HINT, m_hInstance, NULL);

    sy += 95;

    // 当前时间组（对应"倒计时显示"）
    m_hSchedClockGroup = CreateWindowExW(0, L"BUTTON", L"当前时间",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        10, sy, 465, 65, m_hwnd, (HMENU)IDC_SCHED_CLOCK_GROUP, m_hInstance, NULL);

    m_hSchedClock = CreateWindowExW(0, L"STATIC", L"当前系统时间：--:--:--",
        WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE,
        20, sy + 20, 440, 35, m_hwnd, (HMENU)IDC_SCHED_CLOCK, m_hInstance, NULL);

    sy += 75;

    // 状态信息组（对应页面1状态信息）
    m_hSchedStateGroup = CreateWindowExW(0, L"BUTTON", L"状态信息",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        10, sy, 465, 60, m_hwnd, (HMENU)IDC_SCHED_STATE_GROUP, m_hInstance, NULL);

    m_hSchedState = CreateWindowExW(0, L"STATIC",
        L"当前没有定时关机任务。",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        20, sy + 15, 440, 38, m_hwnd, (HMENU)IDC_SCHED_STATE, m_hInstance, NULL);

    // ====== 页面3：关于 ======
    g_pLogoBmp = LoadPngFromResource(m_hInstance, IDB_LOGO_PNG);

    // 内容块在标签页内纵向居中
    int ay = 112;

    // Logo（自绘 STATIC，85x85，与标题组合整体居中）
    m_hAboutLogo = CreateWindowExW(0, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
        110, ay, 85, 85, m_hwnd, (HMENU)IDC_ABOUT_LOGO, m_hInstance, NULL);

    // 标题 + 版本（Logo 右侧横向排布，组合块垂直居中）
    m_hAboutTitle = CreateWindowExW(0, L"STATIC", L"AutoPower",
        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
        205, ay + 10, 170, 36, m_hwnd, (HMENU)IDC_ABOUT_TITLE, m_hInstance, NULL);

    m_hAboutVersion = CreateWindowExW(0, L"STATIC", L"版本 " APP_VERSION,
        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
        205, ay + 52, 170, 22, m_hwnd, (HMENU)IDC_ABOUT_VERSION, m_hInstance, NULL);

    // 简介（两排居中显示）
    m_hAboutDesc = CreateWindowExW(0, L"STATIC",
        L"基于 Win32 API 的 Windows 定时关机软件",
        WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE,
        10, ay + 96, 465, 22, m_hwnd, (HMENU)IDC_ABOUT_DESC, m_hInstance, NULL);

    m_hAboutDesc2 = CreateWindowExW(0, L"STATIC",
        L"纯 C++ 实现，无第三方依赖。",
        WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE,
        10, ay + 122, 465, 22, m_hwnd, (HMENU)IDC_ABOUT_DESC2, m_hInstance, NULL);

    m_hAboutLicense = CreateWindowExW(0, L"STATIC",
        L"开源协议：Mozilla Public License 2.0",
        WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE,
        10, ay + 156, 465, 22, m_hwnd, (HMENU)IDC_ABOUT_LICENSE, m_hInstance, NULL);

    m_hAboutCopyright = CreateWindowExW(0, L"STATIC",
        L"Copyright (c) 2026 youye-luna",
        WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE,
        10, ay + 182, 465, 22, m_hwnd, (HMENU)IDC_ABOUT_COPYRIGHT, m_hInstance, NULL);

    // GitHub 按钮（放在所有文字最下面，点击打开仓库主页）
    m_hAboutLink = CreateWindowExW(0, L"BUTTON", L"GitHub 仓库",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        162, ay + 212, 160, 32, m_hwnd, (HMENU)IDC_ABOUT_LINK, m_hInstance, NULL);

    // ====== 字体 ======
    setControlFont(m_hTab, 11, true);
    setControlFont(m_hCountdownLabel, 20, true);
    setControlFont(m_hStatusLabel, 10);
    setControlFont(m_hCountdownEdit, 12, true);
    setControlFont(m_hSetButton, 10, true);
    setControlFont(m_hCancelButton, 10, true);
    setControlFont(m_hSecondsHint, 9);
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
    setControlFont(m_hSchedClock, 18, true);
    setControlFont(m_hSchedTimeLabel, 11);
    setControlFont(m_hSchedHourEdit, 13, true);
    setControlFont(m_hSchedColon, 12, true);
    setControlFont(m_hSchedMinEdit, 13, true);
    setControlFont(m_hSchedCreateBtn, 10, true);
    setControlFont(m_hSchedDeleteBtn, 10, true);
    setControlFont(m_hSchedHint, 9);
    setControlFont(m_hSchedState, 10);
    setControlFont(m_hAboutTitle, 20, true);
    setControlFont(m_hAboutVersion, 10);
    setControlFont(m_hAboutDesc, 10);
    setControlFont(m_hAboutDesc2, 10);
    setControlFont(m_hAboutLink, 10, true);
    setControlFont(m_hAboutLicense, 10);
    setControlFont(m_hAboutCopyright, 9);

    updateSecondsHint();
    updateSchedClock();
    updateSchedStateText();
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

// 全局任务状态主文本：倒计时 / 定时 同时只有一个，两页状态信息共用
std::wstring MainWindow::getGlobalStatusText() {
    bool hasCountdown = m_timerManager && m_timerManager->hasPendingTask();
    if (hasCountdown) {
        int sec = m_timerManager->getRemainingSeconds();
        if (sec < 0) sec = 0;
        wchar_t buf[32];
        if (sec < 60)
            swprintf_s(buf, L"倒计时关机：%d秒后关机", sec);
        else
            swprintf_s(buf, L"倒计时关机：%d分钟后关机", (sec + 59) / 60);
        return std::wstring(buf);
    }
    if (m_schedActive) {
        if (m_schedHHMM.empty()) return L"定时关机：任务已创建";
        return L"定时关机：" + m_schedHHMM + L" 关机";
    }
    return L"没有待处理的关机任务";
}

void MainWindow::updateStatus() {
    if (!m_timerManager) return;
    BOOL hasTask = m_timerManager->hasPendingTask() ? TRUE : FALSE;
    EnableWindow(m_hCancelButton, hasTask);
    // 两页状态框共用同一全局文本，统一样式统一内容
    updateSchedStateText();
}

// 互斥：倒计时与定时关机同时只能存在一个，激活一方时禁用另一方的设置控件
void MainWindow::updateInterlock() {
    bool countdownActive = m_timerManager && m_timerManager->hasPendingTask();

    // 定时关机激活时 → 禁用倒计时设置控件（保留取消按钮）
    BOOL countdownEnabled = m_schedActive ? FALSE : TRUE;
    if (m_hCountdownEdit) EnableWindow(m_hCountdownEdit, countdownEnabled);
    if (m_hUnitCombo) EnableWindow(m_hUnitCombo, countdownEnabled);
    if (m_hSetButton) EnableWindow(m_hSetButton, countdownEnabled);
    HWND quickBtns[] = { m_hQuick10s, m_hQuick30s, m_hQuick1m, m_hQuick5m,
                         m_hQuick10m, m_hQuick15m, m_hQuick30m, m_hQuick1h,
                         m_hQuick2h, m_hQuick4h };
    for (HWND b : quickBtns) if (b) EnableWindow(b, countdownEnabled);

    // 倒计时激活时 → 禁用定时关机设置控件（保留删除按钮）
    if (m_hSchedCreateBtn) {
        BOOL schedEnabled = countdownActive ? FALSE : TRUE;
        EnableWindow(m_hSchedHourEdit, schedEnabled);
        EnableWindow(m_hSchedMinEdit, schedEnabled);
        EnableWindow(m_hSchedCreateBtn, schedEnabled);
    }
}

// 标题栏实时显示：无任务 / 倒计时关机-XX后关机 / 定时关机-XX:XX关机
void MainWindow::updateTitle() {
    std::wstring title = L"AutoPower - ";
    bool hasCountdown = m_timerManager && m_timerManager->hasPendingTask();

    if (hasCountdown) {
        int sec = m_timerManager->getRemainingSeconds();
        if (sec < 0) sec = 0;
        wchar_t buf[32];
        if (sec < 60)
            swprintf_s(buf, L"倒计时关机-%d秒后关机", sec);
        else
            swprintf_s(buf, L"倒计时关机-%d分钟后关机", (sec + 59) / 60);
        title += buf;
    } else if (m_schedActive) {
        title += L"定时关机-";
        title += m_schedHHMM.empty() ? L"任务已创建" : (m_schedHHMM + L"关机");
    } else {
        title += L"无任务";
    }
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
        msg += L"\n\n关机命令: shutdown -s -t " + std::to_wstring(totalSeconds) + L"\n关闭软件不影响关机，可用 shutdown -a 取消。";
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

void MainWindow::updateSecondsHint() {
    int totalSeconds = getInputSeconds();
    if (totalSeconds <= 0) {
        SetWindowTextW(m_hSecondsHint, L"请输入有效时间");
        return;
    }
    std::wstring hint = L"= " + std::to_wstring(totalSeconds) + L" 秒";
    if (totalSeconds >= 3600) {
        int h = totalSeconds / 3600;
        int m = (totalSeconds % 3600) / 60;
        int s = totalSeconds % 60;
        hint += L"  (" + std::to_wstring(h) + L"小时" 
            + (m > 0 ? std::to_wstring(m) + L"分" : L"")
            + (s > 0 ? std::to_wstring(s) + L"秒" : L"") + L")";
    } else if (totalSeconds >= 60) {
        int m = totalSeconds / 60;
        int s = totalSeconds % 60;
        hint += L"  (" + std::to_wstring(m) + L"分"
            + (s > 0 ? std::to_wstring(s) + L"秒" : L"") + L")";
    }
    SetWindowTextW(m_hSecondsHint, hint.c_str());
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
        msg += L"\n\n关机命令: shutdown -s -t " + std::to_wstring(seconds) + L"\n关闭软件不影响关机，可用 shutdown -a 取消。";
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
                updateTitle();
                if (m_timerManager && m_timerManager->hasPendingTask()) {
                    if (m_timerManager->getRemainingSeconds() == 300 && !m_reminderShown) {
                        showReminder();
                    }
                }

                // 页面2：实时时钟
                updateSchedClock();

                // 定时关机兜底触发：软件运行时到达目标时刻则确保执行关机
                if (m_schedActive && !m_schedHHMM.empty() &&
                    m_schedTarget.dwHighDateTime != 0) {
                    long long rem = DiffSecondsFromNow(m_schedTarget);
                    // 每分钟刷新一次状态文本中的剩余时间
                    static int s_lastMin = -1;
                    SYSTEMTIME stN; GetLocalTime(&stN);
                    if (s_lastMin != stN.wMinute) {
                        s_lastMin = stN.wMinute;
                        updateSchedStateText();
                    }
                    if (rem <= 0) {
                        ShellExecuteW(NULL, L"open", L"shutdown.exe",
                            L"-s -t 0", NULL, SW_HIDE);
                        m_schedActive = false;
                        ZeroMemory(&m_schedTarget, sizeof(m_schedTarget));
                        updateSchedStateText();
                    }
                }
            }
            return 0;

        case WM_COMMAND:
            handleCommand(wParam);
            return 0;

        case WM_DRAWITEM: {
            DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lParam;
            if (dis && dis->CtlID == IDC_ABOUT_LOGO) {
                FillRect(dis->hDC, &dis->rcItem,
                    (HBRUSH)GetSysColorBrush(COLOR_BTNFACE));
                if (g_pLogoBmp) {
                    Gdiplus::Graphics g(dis->hDC);
                    g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
                    UINT iw = g_pLogoBmp->GetWidth(), ih = g_pLogoBmp->GetHeight();
                    if (iw > 0 && ih > 0) {
                        int dw = dis->rcItem.right - dis->rcItem.left;
                        int dh = dis->rcItem.bottom - dis->rcItem.top;
                        double scale = min((double)dw / iw, (double)dh / ih);
                        int w = (int)(iw * scale), h = (int)(ih * scale);
                        int x = dis->rcItem.left + (dw - w) / 2;
                        int y2 = dis->rcItem.top + (dh - h) / 2;
                        g.DrawImage(g_pLogoBmp, x, y2, w, h);
                    }
                }
                return TRUE;
            }
            return 0;
        }

        case WM_NOTIFY: {
            NMHDR* nmh = (NMHDR*)lParam;
            if (nmh && nmh->hwndFrom == m_hTab &&
                nmh->idFrom == IDC_PAGE_TAB && nmh->code == TCN_SELCHANGE) {
                int idx = (int)SendMessageW(m_hTab, TCM_GETCURSEL, 0, 0);
                applyPage(idx);
            }
            return 0;
        }

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
            if (hCtrl == m_hSchedState) {
                wchar_t text[512] = {};
                GetWindowTextW(hCtrl, text, 512);
                std::wstring ws(text);
                if (ws.find(L"失败") != std::wstring::npos ||
                    ws.find(L"错误") != std::wstring::npos) {
                    SetTextColor(hdc, RGB(192, 0, 0));      // 失败/错误 -> 红色
                } else if (ws.find(L"已创建") != std::wstring::npos ||
                           ws.find(L"已成功") != std::wstring::npos) {
                    SetTextColor(hdc, RGB(0, 128, 0));      // 任务存在 -> 绿色
                } else {
                    SetTextColor(hdc, RGB(100, 100, 100));  // 无任务 -> 灰色
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
        _itow(newVal, newBuf, 10);
        SetWindowTextW(m_hCountdownEdit, newBuf);
        lastUnit = curUnit;
        updateSecondsHint();
        return;
    }

    if (notif == EN_CHANGE && id == IDC_COUNTDOWN_EDIT) {
        updateSecondsHint();
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
        case IDC_SCHED_CREATE:  onCreateSchedule(); break;
        case IDC_SCHED_DELETE:  onDeleteSchedule(); break;
        case IDC_ABOUT_LINK:
            ShellExecuteW(NULL, L"open", GITHUB_URL, NULL, NULL, SW_SHOWNORMAL);
            break;
    }
}

// ==================== 标签页与定时关机 ====================
void MainWindow::applyPage(int index) {
    const HWND page1[] = {
        m_hInputGroupBox, m_hTimeStatic, m_hCountdownEdit,
        m_hUnitCombo, m_hSetButton, m_hCancelButton, m_hSecondsHint,
        m_hCountdownGroupBox, m_hCountdownLabel,
        m_hQuickGroupBox,
        m_hQuick10s, m_hQuick30s, m_hQuick1m, m_hQuick5m, m_hQuick10m,
        m_hQuick15m, m_hQuick30m, m_hQuick1h, m_hQuick2h, m_hQuick4h,
        m_hStatusGroupBox, m_hStatusLabel
    };
    const HWND page2[] = {
        m_hSchedGroupBox,
        m_hSchedClock,
        m_hSchedTimeLabel, m_hSchedHourEdit, m_hSchedColon, m_hSchedMinEdit,
        m_hSchedCreateBtn, m_hSchedDeleteBtn, m_hSchedState, m_hSchedHint,
        m_hSchedClockGroup, m_hSchedStateGroup
    };
    const HWND page3[] = {
        m_hAboutLogo, m_hAboutTitle, m_hAboutVersion, m_hAboutDesc,
        m_hAboutDesc2, m_hAboutLink, m_hAboutLicense, m_hAboutCopyright
    };

    int show1 = (index == 0) ? SW_SHOW : SW_HIDE;
    int show2 = (index == 1) ? SW_SHOW : SW_HIDE;
    int show3 = (index == 2) ? SW_SHOW : SW_HIDE;
    for (HWND h : page1) if (h) ShowWindow(h, show1);
    for (HWND h : page2) if (h) ShowWindow(h, show2);
    for (HWND h : page3) if (h) ShowWindow(h, show3);

    // 标题随页面切换
    updateTitle();
}

void MainWindow::onCreateSchedule() {
    // 从页面输入框读取时间（格式校验）
    wchar_t hb[8] = {}, mb[8] = {};
    GetWindowTextW(m_hSchedHourEdit, hb, 8);
    GetWindowTextW(m_hSchedMinEdit, mb, 8);
    int hour, minute;
    try { hour = std::stoi(hb); minute = std::stoi(mb); }
    catch (...) {
        showError(L"时间无效！请输入数字，小时范围 0-23，分钟范围 0-59。");
        return;
    }
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59) {
        showError(L"时间超出范围！小时应为 0-23，分钟应为 0-59。");
        return;
    }

    wchar_t tbuf[8];
    swprintf_s(tbuf, L"%02d:%02d", hour, minute);
    std::wstring hhmm = tbuf;

    // 再次解析为目标时刻（当天已过则取明天，双保险）
    FILETIME target;
    if (!ParseHHMMToTarget(hhmm, target)) {
        showError(L"时间无效！小时范围 0-23，分钟范围 0-59。");
        return;
    }

    // 同名任务已存在时询问是否覆盖
    if (SchTaskExists()) {
        int r = MessageBoxW(m_hwnd,
            L"系统任务计划程序中已存在名为“关机”的任务。\n是否删除旧任务并创建新的？",
            L"任务已存在", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
        if (r != IDYES) return;
        if (!DeleteShutdownTask(m_hwnd)) {
            showError(L"无法删除旧的“关机”任务，未创建新任务。\n"
                      L"请以管理员身份运行本软件后重试。");
            return;
        }
    }

    std::wstring args = std::wstring(L"/create /tn ") + SCHED_TASK_NAME +
                        L" /tr \"shutdown /s\" /sc once /st " + hhmm;

    bool ok = RunSchtasks(args) && g_lastSchExit == 0;

    // 权限不足兜底：询问是否提权重试
    if (!ok) {
        wchar_t codeBuf[32];
        swprintf_s(codeBuf, L"%d", g_lastSchExit);
        int r = MessageBoxW(m_hwnd,
            ((std::wstring)L"创建定时关机任务失败（错误代码 " + codeBuf +
             L"）。\n可能需要管理员权限，是否以管理员身份重试？").c_str(),
            L"权限不足", MB_YESNO | MB_ICONWARNING);
        if (r == IDYES) ok = ElevatedRunSchtasks(args);
    }

    if (!ok) {
        showError(L"定时关机任务创建失败！\n"
                  L"请检查：1) 是否拒绝了管理员授权；\n"
                  L"      2) schtasks.exe 是否可用；\n"
                  L"      3) 以管理员身份运行本软件后重试。");
        return;
    }

    m_schedTarget = target;
    m_schedHHMM = hhmm;
    m_schedActive = true;
    updateSchedStateText();

    // 成功反馈：说明当天/明天执行
    SYSTEMTIME stT;
    FileTimeToSystemTime(&target, &stT);
    SYSTEMTIME stN; GetLocalTime(&stN);
    bool tomorrow = (stT.wDay != stN.wDay || stT.wMonth != stN.wMonth);
    std::wstring info = L"定时关机任务已成功创建！\n\n关机时刻：" + hhmm +
        (tomorrow ? L"（明天）" : L"（今天）") +
        L"\n到达该时刻时系统将自动关机。\n"
        L"关闭本软件不影响该任务的执行。";
    MessageBoxW(m_hwnd, info.c_str(), L"操作成功", MB_OK | MB_ICONINFORMATION);
}

void MainWindow::onDeleteSchedule() {
    if (!SchTaskExists()) {
        m_schedActive = false;
        m_schedHHMM.clear();
        ZeroMemory(&m_schedTarget, sizeof(m_schedTarget));
        updateSchedStateText();
        MessageBoxW(m_hwnd,
            L"系统中当前不存在“关机”计划任务。",
            L"提示", MB_OK | MB_ICONINFORMATION);
        return;
    }

    int r = MessageBoxW(m_hwnd,
        L"确定要删除系统“任务计划程序”中的“关机”任务吗？\n删除后将不再自动关机。",
        L"删除定时关机任务", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
    if (r != IDYES) return;

    if (!DeleteShutdownTask(m_hwnd)) {
        showError(L"删除失败！无法从系统任务计划程序中移除“关机”任务。\n"
                  L"可能原因：权限不足或用户拒绝管理员授权。");
        return;
    }

    m_schedActive = false;
    m_schedHHMM.clear();
    ZeroMemory(&m_schedTarget, sizeof(m_schedTarget));
    updateSchedStateText();
    MessageBoxW(m_hwnd,
        L"“关机”计划任务已成功删除，定时关机已取消。",
        L"操作成功", MB_OK | MB_ICONINFORMATION);
}

void MainWindow::updateSchedStateText() {
    if (!m_hSchedState) return;

    std::wstring text;
    bool countdownActive = m_timerManager && m_timerManager->hasPendingTask();
    if (m_schedActive) {
        if (!m_schedHHMM.empty()) {
            text = getGlobalStatusText();
            long long rem = DiffSecondsFromNow(m_schedTarget);
            if (rem > 0) text += L"，剩余约 " + FormatRemaining(rem);
            text += L"。\n请到「定时关机」页点击「删除定时任务」移除该计划。";
        } else {
            // 启动时检测到系统中已有任务但时间未知
            text = L"检测到系统中已存在“关机”计划任务（软件启动时发现）。\n"
                   L"具体时间请在 Windows 任务计划程序中查看。";
        }
    } else if (countdownActive) {
        text = getGlobalStatusText() + L"。\n请到「倒计时关机」页点击「取消关机」取消本次关机。";
    } else {
        text = L"没有待处理的关机任务。\n可设置倒计时关机或定时关机。";
    }

    // 两个状态框统一样式、统一内容
    SetWindowTextW(m_hSchedState, text.c_str());
    if (m_hStatusLabel)
        SetWindowTextW(m_hStatusLabel, text.c_str());

    if (m_hSchedDeleteBtn)
        EnableWindow(m_hSchedDeleteBtn, m_schedActive ? TRUE : FALSE);
    if (m_hCancelButton)
        EnableWindow(m_hCancelButton, countdownActive ? TRUE : FALSE);
    updateInterlock();
    updateTitle();
}

void MainWindow::updateSchedClock() {
    if (!m_hSchedClock) return;
    wchar_t clock[16], buf[40];
    GetClockString(clock, 16);
    swprintf_s(buf, L"当前系统时间：%s", clock);
    SetWindowTextW(m_hSchedClock, buf);

    // 换算提示（对应页面1的"= 1800 秒"）：显示目标时间与今天/明天
    if (m_hSchedHint) {
        wchar_t hb[8] = {}, mb[8] = {}, hint[64];
        GetWindowTextW(m_hSchedHourEdit, hb, 8);
        GetWindowTextW(m_hSchedMinEdit, mb, 8);
        int hh = -1, mm = -1;
        try { hh = std::stoi(hb); mm = std::stoi(mb); } catch (...) {}
        if (hh >= 0 && hh <= 23 && mm >= 0 && mm <= 59) {
            SYSTEMTIME st; GetLocalTime(&st);
            bool tomorrow = (hh * 60 + mm) <= (st.wHour * 60 + st.wMinute);
            swprintf_s(hint, L"%s %02d:%02d 关机",
                       tomorrow ? L"明天" : L"今天", hh, mm);
        } else {
            swprintf_s(hint, L"请输入有效时间（小时 0-23，分钟 0-59）");
        }
        SetWindowTextW(m_hSchedHint, hint);
    }
}

// 启动时检测系统中是否已存在「关机」计划任务并同步界面状态
void MainWindow::syncSchedFromSystem() {
    bool exists = SchTaskExists();
    if (exists) {
        if (!m_schedActive || m_schedHHMM.empty()) {
            m_schedActive = true;   // 时间未知 -> 状态文本按“已有任务(未知时间)”显示
        }
    } else {
        m_schedActive = false;
        m_schedHHMM.clear();
        ZeroMemory(&m_schedTarget, sizeof(m_schedTarget));
    }
    updateSchedStateText();
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

    // GDI+ 初始化（关于页面 Logo 绘制用）
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken = 0;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    MainWindow mainWindow(hInstance);
    if (!mainWindow.create()) {
        MessageBoxW(NULL, L"窗口创建失败！", L"AutoPower 错误", MB_ICONERROR);
        Gdiplus::GdiplusShutdown(gdiplusToken);
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

    Gdiplus::GdiplusShutdown(gdiplusToken);
    return (int)msg.wParam;
}
