# AutoPower - 定时关机软件

一款基于 Win32 API 开发的 Windows 定时关机软件，纯 C++ 实现，无第三方依赖。

## 功能特性

- 倒计时关机设置（支持秒、分钟、小时单位切换）
- 快速设置按钮：10秒 / 30秒 / 1分钟 / 5分钟 / 10分钟 / 15分钟 / 30分钟 / 1小时 / 2小时 / 4小时
- 实时倒计时显示
- 关机前5分钟提醒窗口
- 支持取消已设置的关机任务
- 单位切换时数值自动换算

## 系统要求

- Windows 7 SP1 及以上（32位/64位）
- Visual Studio 2015+ 或任意支持 C++11 的 MSVC 编译器
- Windows SDK

## 项目结构

```
AutoPower/
├── AutoPower_single.cpp   # 完整源码（单文件）
├── resources/
│   ├── app_icon.ico        # 应用图标
│   └── app_icon.rc         # 资源文件
├── AutoPower.png           # 图标原图
└── docs/
    ├── build.md            # 编译说明
    └── user_manual.md      # 用户手册
```
## 软件截图
<img width="486" height="493" alt="图片" src="https://github.com/user-attachments/assets/b7a03326-2a30-4f31-a9d5-a85fa257da46" />
<img width="1720" height="952" alt="图片" src="https://github.com/user-attachments/assets/ce9f5208-b528-474a-a253-7dbeabc91dbb" />
<img width="1720" height="952" alt="图片" src="https://github.com/user-attachments/assets/93f7ec4a-6704-4bba-b421-817b292c6d49" />


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

