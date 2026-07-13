cmake_minimum_required(VERSION 3.16)

project(ptapro
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Qt 的 MOC/UIC/RCC 自动处理能让后续新增 UI 文件和资源文件时少写样板配置。
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 QUIET COMPONENTS Widgets)
if(Qt6_FOUND)
    set(PTAPRO_QT_PACKAGE Qt6)
else()
    find_package(Qt5 REQUIRED COMPONENTS Widgets)
    set(PTAPRO_QT_PACKAGE Qt5)
endif()

# ZXing 负责二维码和一维条码的真实生成/识别能力，属于当前功能的关键依赖。
find_package(ZXing REQUIRED)

# OpenCV 先作为可选依赖接入，避免在初始架构阶段阻断没有 OpenCV 的环境。
# 后续实现真实识别能力时，可以在 services 层内逐步打开对应适配代码。
find_package(OpenCV QUIET COMPONENTS core imgproc imgcodecs objdetect videoio)

add_executable(ptapro
    include/ptapro/core/CodecTypes.h
    include/ptapro/services/DecoderService.h
    include/ptapro/services/EncoderService.h
    include/ptapro/ui/MainWindow.h
    src/core/CodecTypes.cpp
    src/main.cpp
    src/services/DecoderService.cpp
    src/services/EncoderService.cpp
    src/ui/MainWindow.cpp
)

target_include_directories(ptapro
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ptapro
    PRIVATE
        ${PTAPRO_QT_PACKAGE}::Widgets
        ZXing::ZXing
)

if(OpenCV_FOUND)
    target_compile_definitions(ptapro PRIVATE PTAPRO_HAS_OPENCV=1)
    target_link_libraries(ptapro PRIVATE ${OpenCV_LIBS})
endif()

if(Qt6_FOUND)
    qt_finalize_executable(ptapro)
endif()
