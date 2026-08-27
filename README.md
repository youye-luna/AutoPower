# AutoPower - 定时关机软件

一款基于 Win32 API 开发的 Windows 定时关机软件，纯 C++ 实现，无第三方依赖。

## 软件截图

<img width="1920" height="1080" alt="2026-08-27_14-03" src="https://github.com/user-attachments/assets/12ad40e8-ec01-4922-ad7d-2222c12bb1ad" />
<img width="1920" height="1080" alt="2026-08-27_14-03_1" src="https://github.com/user-attachments/assets/6c725d40-5877-4688-b2ed-0a0e28f69be6" />

## 功能特性

### 倒计时关机
- 倒计时关机设置（支持秒、分钟、小时单位切换）
- 快速设置按钮：10秒 / 30秒 / 1分钟 / 5分钟 / 10分钟 / 15分钟 / 30分钟 / 1小时 / 2小时 / 4小时
- 实时倒计时显示，剩余时间实时更新
- 关机前5分钟弹窗提醒，可一键取消
- 使用 `shutdown -s -t` 系统命令关机，关闭软件不影响关机

### 定时关机
- 支持输入 HH:MM 设置定时关机时间（自动判断今天/明天）
- 使用 `schtasks` 创建系统计划任务，关闭软件不影响执行
- 可查看任务状态、剩余时间，支持一键删除已创建的任务
- 权限不足时自动尝试 UAC 提权
- 定时与倒计时互锁，避免冲突

### 通用
- 标题栏实时显示当前任务状态
- 双标签页界面，清晰分区
- 完善的错误处理与用户反馈
- 单文件 exe，`/MT` 静态链接，无 DLL 依赖
- 支持 Windows 7 SP1 及以上

## 系统要求

- Windows 7 SP1 及以上（32位/64位）
- Visual Studio 2015+ 或任意支持 C++14 的 MSVC 编译器
- Windows SDK

## 项目结构

```
AutoPower/
├── AutoPower_single.cpp   # 完整源码（单文件）
├── resources/
│   ├── app_icon.ico        # 应用图标
│   └── app_icon.rc         # 资源文件
└── .gitignore
```

## 编译方法

在 Visual Studio Developer Command Prompt 中执行：

```powershell
# 1. 编译资源文件
rc resources\app_icon.rc

# 2. 编译源码
cl /utf-8 /EHsc /MT /O2 /DUNICODE /D_UNICODE /DWINVER=0x0601 /D_WIN32_WINNT=0x0601 /DWIN32_LEAN_AND_MEAN /c AutoPower_single.cpp /Fo:AutoPower.obj

# 3. 链接生成 exe
link /SUBSYSTEM:WINDOWS /OUT:AutoPower.exe AutoPower.obj app_icon.res user32.lib gdi32.lib comctl32.lib advapi32.lib shell32.lib
```

编译完成后得到单个 `AutoPower.exe`，无需任何 DLL，直接双击运行。

## 许可证

[Apache License 2.0](LICENSE)
