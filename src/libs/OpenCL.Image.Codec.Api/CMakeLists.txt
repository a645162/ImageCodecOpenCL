# 对外 C API（include/OpenCLImageCodec/imagecodec.h / imagecodec.hpp 的实现库）。
# 桥接各模块文件入口（imagecodec_*_test.h）与内存 buffer。
add_library(OpenCLImageCodec.Api STATIC
  src/imagecodec.cpp
  "${CMAKE_SOURCE_DIR}/include/OpenCLImageCodec/imagecodec.h"
  "${CMAKE_SOURCE_DIR}/include/OpenCLImageCodec/imagecodec.hpp")

target_include_directories(OpenCLImageCodec.Api PUBLIC
  "${CMAKE_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/src")

target_link_libraries(OpenCLImageCodec.Api PUBLIC
  OpenCLImageCodec.Core
  OpenCLImageCodec.Tiff
  OpenCLImageCodec.Jpeg)
# 构建本库时 IMAGECODEC_API 取 dllexport 分支（共享库场景）；静态库下为空。
target_compile_definitions(OpenCLImageCodec.Api PRIVATE IMAGECODEC_BUILDING_LIB)

# 命名空间别名：target_link_libraries(app PRIVATE OpenCLImageCodec::Api)
add_library(OpenCLImageCodec::Api ALIAS OpenCLImageCodec.Api)
