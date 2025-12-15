## win-app-template

在Windows系统中运行的C++程序框架，使用cmake编译 C++版本为c++23



### Windows（VS 2026）构建/安装示例
1. MDd（Debug 动态）：
- cmake -S . -B build-md-dbg -G "Visual Studio 18 2026" -A x64
-DCMAKE_MSVC_RUNTIME_LIBRARY="MultiThreadedDebugDLL" -DCMAKE_INSTALL_PREFIX=./install
- cmake --build build-md-dbg --config Debug
- cmake --install build-md-dbg --config Debug → 安装到 install/MDd
2. MD（Release 动态）：
- cmake -S . -B build-md-rel -G "Visual Studio 18 2026" -A x64 -DCMAKE_MSVC_RUNTIME_LIBRARY="MultiThreadedDLL"
-DCMAKE_INSTALL_PREFIX=./install
- cmake --build build-md-rel --config Release
- cmake --install build-md-rel --config Release → 安装到 install/MD
3. MTd（Debug 静态）：
```bash
cmake -S . -B build-mtd-dbg -G "Visual Studio 18 2026" -A x64 -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDebug -DCMAKE_INSTALL_PREFIX="%cd%/install"
cmake --build build-mtd-dbg --config Debug
cmake --install build-mtd-dbg --config Debug
```
4. MT（Release 静态）：
- cmake -S . -B build-mt-rel -G "Visual Studio 18 2026" -A x64 -DCMAKE_MSVC_RUNTIME_LIBRARY="MultiThreaded"
-DCMAKE_INSTALL_PREFIX=./install
- cmake --build build-mt-rel --config Release
- cmake --install build-mt-rel --config Release → 安装到 install/MT



### why?

为何 Windows 有 CRT 层面的异常与弹窗（与 Linux 不同）

- 错误来源多层
    - C++ 异常：throw/catch 范畴，可常规捕获。
    - SEH（结构化异常）：如访问违例 0xC0000005、非法指令、栈溢出，属于系统级异常，不等同于 C++ 异常，默认不被 try/
    catch 捕获。
    - CRT 运行时错误：invalid parameter、purecall、abort/assert 等由 MSVC CRT 管理，默认弹出 GUI 报错窗口后终止。
- 默认行为不同
    - Linux：统一以信号（SIGSEGV、SIGABRT 等）形式终止，通常不会弹窗，内核可生成 core 文件。
    - Windows：默认会弹出错误对话框（系统层/CRT），不中断时序会阻塞自动化；需要通过 API 显式关闭这些 UI 并接管错误
    路径。
- 影响
    - 如果仅靠 try/catch 和 signal handler，经常抓不到 CRT 的 invalid parameter 或 purecall 等，或者 SEH 在默认处理后
    弹窗。故需要：SetErrorMode、CRT handler、UnhandledExceptionFilter 等组合拳。


### dmp文件分析

- 符号路径设置（WinDbg：File > Symbol File Path）：
    - srvC:\symbolshttps://msdl.microsoft.com/download/symbols;D:\Github\cpp-proto\win-app-template\build-mtd-
    dbg\Debug
    - 然后 .reload /f
- 源码路径设置（必要时）：
    - File > Source File Path 添加 D:\Github\cpp-proto\win-app-template
    - 经典 WinDbg 可能仍乱码，优先用 WinDbg Preview 或 VS 打开 .dmp。
- 解析命令小抄：
    - !analyze -v     概要分析（异常类型/模块）
    - .ecxr           切到异常上下文
    - kv              查看当前栈（带参数）
    - uf win_app_template!TriggerSegmentationFaultSimulation  反汇编函数
    - .srcfix 或手动设 Source File Path，配合打开源文件定位行
