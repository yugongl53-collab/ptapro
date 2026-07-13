# 二维码/条形码生成与识别工具

## 项目定位

本项目计划基于 Qt 构建桌面端图形界面，并接入 ZXing 提供二维码与条形码生成/识别能力，形成一个同时支持二维码与条形码生成、图片识别和摄像头实时识别的工具。

当前版本已经实现生成器与识别器的第一版能力：支持 QR Code、Code 128、EAN-13、UPC-A、Code 39，能够保存图片、从文件识别和通过摄像头实时识别。

## 初始目录结构

```text
.
├── CMakeLists.txt
├── include/ptapro/
│   ├── core/              # 编解码通用数据结构与枚举
│   ├── services/          # 生成、识别等业务服务接口
│   └── ui/                # Qt 主窗口声明
└── src/
    ├── core/
    ├── services/
    └── ui/
```

## 模块职责

- `core`：定义码制、编码请求、编码结果、识别结果等跨模块数据结构。
- `services`：封装生成与识别流程。当前使用 ZXing 生成和识别 QR Code、Code 128、EAN-13、UPC-A、Code 39。
- `ui`：负责 Qt Widgets 界面、用户输入、文件选择和结果展示，不直接依赖具体第三方编解码实现。

## 已实现功能

- 统一生成界面：通过下拉框选择 QR Code、Code 128、EAN-13、UPC-A 或 Code 39。
- 内容输入：输入文本、URL、联系人信息、产品序列号等内容后，点击“立即生成”刷新预览。
- QR Code 参数：支持 L/M/Q/H 纠错等级、边距大小和中心 Logo 图片。
- 条形码参数：支持条码下方显示可读文本或数字。
- 商品码校验：EAN-13 支持 12/13 位数字，UPC-A 支持 11/12 位数字，并会校验或自动补全校验位。
- 保存图片：支持 PNG、JPG、BMP 和 SVG。
- 文件识别：打开本地图片后识别全部二维码/条形码，并在预览中标出位置。
- 识别增强：在 ZXing 解码前尝试灰度化、Otsu 二值化、自适应阈值和形态学闭运算，提高低对比度或轻微断裂图像的识别率。
- 摄像头识别：使用 Qt6 Multimedia 读取摄像头视频流，定时分析画面并显示识别结果。
- 结果处理：支持复制识别内容；当内容是 HTTP/HTTPS URL 时，可用浏览器打开。

## 项目依赖

### 必需依赖

| 依赖 | 用途 | CMake 查找方式 |
| --- | --- | --- |
| CMake 3.16+ | 配置和生成构建系统 | `cmake_minimum_required(VERSION 3.16)` |
| 支持 C++17 的编译器 | 编译 Qt/C++ 业务代码 | `set(CMAKE_CXX_STANDARD 17)` |
| Qt Widgets | 桌面图形界面、文件选择、图片预览和交互控件 | 优先 `find_package(Qt6 COMPONENTS Widgets)`，未找到 Qt6 时回退到 `find_package(Qt5 COMPONENTS Widgets)` |
| ZXing-C++ | 二维码/条形码生成与识别核心能力 | `find_package(ZXing REQUIRED)`，需要提供 `ZXing::ZXing` CMake target |

### 可选依赖

| 依赖 | 启用条件 | 功能影响 |
| --- | --- | --- |
| Qt6 Multimedia | 使用 Qt6 且能找到 `Qt6::Multimedia` | 启用摄像头实时识别；未找到时应用仍可编译，但摄像头识别不可用 |
| OpenCV | 能找到 `core`、`imgproc`、`imgcodecs`、`objdetect`、`videoio` 组件 | 当前作为预留适配依赖；未安装不会影响现有生成、文件识别和 Qt 图像预处理能力 |

### 常见安装提示

Ubuntu/Debian 环境可参考：

```bash
sudo apt install build-essential cmake qt6-base-dev qt6-multimedia-dev libzxing-dev
```

如果使用 Qt5 构建，可安装：

```bash
sudo apt install qtbase5-dev
```

如需安装可选 OpenCV：

```bash
sudo apt install libopencv-dev
```

macOS Homebrew 环境可参考：

```bash
brew install cmake qt zxing-cpp
```

如果依赖安装在非系统默认路径，需要在配置时通过 `CMAKE_PREFIX_PATH` 指定 Qt、ZXing 等包的 CMake 配置目录，例如：

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH="/path/to/qt;/path/to/zxing"
```

## 构建方式

```bash
cmake -S . -B build
cmake --build build
```

配置阶段会检查 Qt Widgets 与 ZXing。若缺少必需依赖，`cmake -S . -B build` 会直接失败；若仅缺少可选依赖，CMake 会跳过对应能力并继续生成构建文件。

## 后续计划

1. 补充核心服务单元测试和典型图片样本测试。
2. 增加更多码制和批量生成能力。
3. 优化 QR Code Logo 的遮挡比例提示和纠错等级联动。
4. 增加摄像头设备选择和识别帧率配置。
5. 如果后续需要完全替代 ZXing，可单独实现 QR Code 数据编码、Reed-Solomon 纠错、矩阵填充和掩码选择。
