# AutoPower 编译说明

## 系统要求

### 必需软件
- **CMake 3.10 或更高版本**
- **Qt 5.9.0 或更高版本** (推荐 Qt 5.15.2，包含 Core 和 Widgets 模块)
- **C++11 兼容编译器** (推荐 Visual Studio 2015 或更高版本，或 MinGW 4.9.2+)

### Windows 7 支持
- **操作系统**：Windows 7 SP1 32位/64位
- **Windows SDK**：Windows 7 SDK (推荐)
- **.NET Framework**：.NET Framework 4.5 或更高版本

### 可选软件
- **Ninja** (可选，用于更快的构建)
- **Visual Studio** (推荐，用于IDE开发)
- **Qt Creator** (推荐，用于Qt开发)

## 编译步骤

### 方法一：使用 Visual Studio + CMake (推荐)

#### 1. **安装依赖**
```bash
# 安装 Qt 5.9.0 或更高版本
# 下载地址: https://www.qt.io/download-qt-installer
# 选择 Qt 5.x 版本，包含 MSVC 2015 32-bit 或 64-bit 编译器
# 推荐版本：Qt 5.15.2 for Windows

# 安装 CMake
# 下载地址: https://cmake.org/download/
# 选择 Windows x86_64 Installer

# 安装 Windows 7 SDK (如果需要)
# 下载地址: https://developer.microsoft.com/en-us/windows/downloads/sdk-archive/
# 选择 Windows 7 SDK
```

#### 2. **生成 Visual Studio 项目**
```bash
# 创建构建目录
mkdir build
cd build

# 生成 Visual Studio 2019 项目 (32位)
cmake -G "Visual Studio 16 2019" -A Win32 ..

# 或者生成 Visual Studio 2017 项目 (32位)
cmake -G "Visual Studio 15 2017" -A Win32 ..

# 或者生成 64位版本
cmake -G "Visual Studio 16 2019" -A x64 ..
```

#### 3. **编译项目**
```bash
# 使用 CMake 编译
cmake --build . --config Release

# 或者使用 Visual Studio
# 在 Visual Studio 中打开 AutoPower.sln，选择 Release 配置，生成解决方案
```

#### 4. **运行程序**
```bash
# 可执行文件位置 (32位)
build\bin\Release\AutoPower.exe

# 可执行文件位置 (64位)
build\bin\x64\Release\AutoPower.exe
```

### 方法二：使用 MinGW + CMake

#### 1. **安装依赖**
```bash
# 安装 Qt 5.x for MinGW
# 下载地址: https://www.qt.io/download-qt-installer
# 选择 Qt 5.x 版本，包含 MinGW 32-bit 或 64-bit 编译器
# 推荐版本：Qt 5.15.2 for Windows with MinGW

# 安装 CMake
# 下载地址: https://cmake.org/download/
```

#### 2. **生成 Makefile**
```bash
# 创建构建目录
mkdir build
cd build

# 生成 32位 Makefile
cmake -G "MinGW Makefiles" -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ ..

# 或者生成 64位 Makefile
cmake -G "MinGW Makefiles" -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ ..
```

#### 3. **编译项目**
```bash
# 编译项目
cmake --build .
```

#### 4. **运行程序**
```bash
# 可执行文件位置 (32位)
build\AutoPower.exe

# 可执行文件位置 (64位)
build\AutoPower.exe
```

### 方法三：使用 Qt Creator

#### 1. **打开项目**
- 启动 Qt Creator
- 选择 "File" -> "Open File or Project"
- 选择 `CMakeLists.txt` 文件

#### 2. **配置项目**
- 在 "Projects" 选项卡中，选择构建套件 (Kits)
- 确保 Qt 版本和编译器正确配置
- 选择构建类型 (Debug/Release)

#### 3. **构建项目**
- 点击 "Build" -> "Build Project"
- 或使用快捷键 Ctrl+B

#### 4. **运行项目**
- 点击 "Run" -> "Run"
- 或使用快捷键 Ctrl+R

## Windows 7 特殊配置

### 1. **编译器设置**
```bash
# 确保编译器支持 C++11
# Visual Studio 2015 及更高版本支持 C++11
# MinGW 4.9.2 及更高版本支持 C++11

# 设置 Windows 7 兼容性
cmake -DCMAKE_SYSTEM_VERSION=7.1 ..
```

### 2. **Qt 配置**
```bash
# 确保使用支持 Windows 7 的 Qt 版本
# Qt 5.9.0 是支持 Windows 7 的最低版本
# Qt 5.15.2 是推荐版本

# 设置 Qt 路径
cmake -DQt5_DIR=C:/Qt/5.15.2/msvc2019_64/lib/cmake/Qt5 ..
```

### 3. **Windows SDK 设置**
```bash
# 如果使用 Visual Studio，确保安装了 Windows 7 SDK
# 在 Visual Studio Installer 中安装 "Windows 7 SDK"
```

## 编译选项

### CMake 选项
```bash
# 指定 Qt 版本路径
cmake -DQt5_DIR=C:/Qt/5.15.2/msvc2019_64/lib/cmake/Qt5 ..

# 设置 Windows 版本
cmake -DCMAKE_SYSTEM_VERSION=7.1 ..

# 启用调试信息
cmake -DCMAKE_BUILD_TYPE=Debug ..

# 启用优化
cmake -DCMAKE_BUILD_TYPE=Release ..

# 设置位数 (32位)
cmake -A Win32 ..

# 设置位数 (64位)
cmake -A x64 ..

# 添加自定义定义
cmake -DAUTOPOWER_VERSION=1.0.0 -DWIN7_COMPAT=ON ..
```

### 编译配置
- **Debug**: 包含调试信息，禁用优化
- **Release**: 启用优化，禁用调试信息
- **RelWithDebInfo**: 启用优化，包含调试信息
- **MinSizeRel**: 最小化文件大小，启用优化

## 常见问题

### 1. CMake 找不到 Qt
```bash
# 手动指定 Qt 路径
cmake -DQt5_DIR=C:/Qt/5.15.2/msvc2019_64/lib/cmake/Qt5 ..

# 或者设置环境变量
set Qt5_DIR=C:/Qt/5.15.2/msvc2019_64/lib/cmake/Qt5
cmake ..
```

### 2. 编译器不支持 C++11
```bash
# 检查编译器版本
gcc --version
cl.exe

# 使用支持的 C++ 标准
cmake -DCMAKE_CXX_STANDARD=11 -DCMAKE_CXX_STANDARD_REQUIRED=ON ..
```

### 3. Windows 7 兼容性问题
```bash
# 确保 Windows SDK 版本正确
# Windows 7 需要 SDK 7.1 或更高版本

# 设置正确的 Windows 版本
cmake -DCMAKE_SYSTEM_VERSION=7.1 ..

# 添加 Windows 7 兼容性定义
cmake -DWIN7_COMPAT=ON ..
```

### 4. 链接错误
```bash
# 确保正确链接了所有需要的库
cmake --build . --verbose  # 显示详细编译信息

# 检查 Qt 版本兼容性
# Qt 5.9.0 是支持 Windows 7 的最低版本
```

### 5. 运行时错误
```bash
# 检查依赖库
# 确保 Qt DLL 文件在 PATH 中或可执行文件旁边

# 对于 Qt 应用，可以使用 windeployqt 工具
windeployqt build\bin\Release\AutoPower.exe

# 或者手动复制 DLL 文件
# 从 Qt 安装目录复制以下 DLL 到可执行文件目录:
# - Qt5Core.dll
# - Qt5Widgets.dll
# - vcruntime140.dll
# - vcruntime140_1.dll
```

## 部署说明

### 开发部署
```bash
# 复制必要的 Qt DLL 文件
windeployqt build\bin\Release\AutoPower.exe

# 或者手动复制 DLL 文件
# 从 Qt 安装目录复制以下 DLL 到可执行文件目录:
# - Qt5Core.dll
# - Qt5Widgets.dll
# - vcruntime140.dll
# - vcruntime140_1.dll
```

### 发布部署
1. **清理构建**
   ```bash
   # 清理构建目录
   cmake --build . --target clean
   ```

2. **重新编译**
   ```bash
   # 使用 Release 配置编译
   cmake --build . --config Release
   ```

3. **打包**
   ```bash
   # 创建发布包目录
   mkdir release
   
   # 复制可执行文件
   copy build\bin\Release\AutoPower.exe release\
   
   # 复制依赖文件
   windeployqt build\bin\Release\AutoPower.exe --dir release\
   
   # 复制文档
   copy ..\README.md release\
   copy ..\docs\user_manual.md release\
   ```

## 性能优化

### 编译时优化
```bash
# 启用链接时优化 (LTO)
cmake -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON ..

# 启用 Profile-Guided Optimization (PGO)
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
```

### 运行时优化
- 使用 Release 配置编译
- 启用 Qt 的快速渲染模式
- 优化资源文件大小

## 故障排除

### 查看详细编译信息
```bash
# 显示详细编译输出
cmake --build . --verbose

# 显示 CMake 配置信息
cmake -LH ..
```

### 清理项目
```bash
# 清理构建目录
rm -rf build/

# 或者使用 CMake
cmake --build . --target clean
```

### 重新配置
```bash
# 删除 CMake 缓存
rm -rf CMakeCache.txt CMakeFiles/

# 重新运行 CMake
cmake ..
```

## 版本控制

### 标记版本
```bash
# 创建版本标签
git tag -a v1.0.0 -m "Version 1.0.0"

# 推送标签
git push origin v1.0.0
```

### 生成发布包
```bash
# 创建发布包
cmake -P cmake/Package.cmake
```

---

如有问题，请参考项目文档或提交 Issue。