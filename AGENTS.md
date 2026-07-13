# Repository Guidelines

## 项目结构与模块组织

本仓库是基于 Qt Widgets、CMake 和 ZXing 的二维码/条形码桌面工具。主要结构如下：

- `CMakeLists.txt`：项目配置、Qt 与 ZXing 依赖声明。
- `include/ptapro/core/`：跨模块数据类型与枚举，例如码制、纠错等级、编解码结果。
- `include/ptapro/services/` 与 `src/services/`：生成、识别等业务服务，当前使用 ZXing。
- `include/ptapro/ui/` 与 `src/ui/`：Qt 主窗口和界面交互逻辑。
- `src/main.cpp`：应用入口。
- `README.md`：中文项目说明与构建方式。

目前仓库还没有独立的 `tests/` 或资源目录；新增测试、样例图片或图标时应分别放入 `tests/`、`assets/` 等清晰目录。

## 构建、测试与开发命令

```bash
cmake -S . -B build
```

配置 CMake 构建目录，并检查 Qt、ZXing 等依赖。

```bash
cmake --build build
```

编译桌面程序，生成 `build/ptapro`。

```bash
./build/ptapro
```

本地启动图形界面。当前未配置自动化测试框架；提交前至少执行一次完整构建。

## 代码风格与命名约定

使用 C++17 和 Qt 类型。缩进使用 4 个空格，不使用制表符。类名采用 `PascalCase`，函数和局部变量采用 `lowerCamelCase`，私有成员变量使用尾随下划线，例如 `encoderService_`。业务逻辑、边界条件、第三方库适配和非显而易见实现必须写简洁注释。文档内容使用中文。

## 测试指南

当前没有测试框架。新增核心逻辑时优先为 `services` 层补充单元测试，建议测试文件命名为 `*Test.cpp`，并通过 CMake 注册到 `ctest`。生成器测试应覆盖 QR Code、Code 128、EAN-13、空输入、EAN 校验位错误和保存格式。

## 提交与 Pull Request 规范

提交信息遵循当前历史中的 Conventional Commits 风格，例如：

- `feat: 接入 ZXing 实现编码生成器`
- `chore: 初始化 Qt 编解码工具架构`

每个独立变更单独提交。PR 应说明变更目的、主要实现、验证命令；涉及界面调整时附截图；关联 issue 时在描述中链接。
