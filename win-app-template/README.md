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