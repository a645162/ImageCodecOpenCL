# OpenCL 后端的自检（返回码约定：0 = PASS、2 = SKIP、其它 = FAIL）。

add_executable(metrics_test metrics_test.cpp)
target_link_libraries(metrics_test PRIVATE
  OpenCLImageCodec::Metrics)
# 头文件在模块 src/ 下（内部接口），测试与库共用同一份定义。
target_include_directories(metrics_test PRIVATE
  "${CMAKE_SOURCE_DIR}/src/libs/OpenCL.Image.Codec.Metrics/src")

add_test(NAME metrics_test COMMAND metrics_test)
# SKIP（无设备 / 不支持 fp64）不算失败，但也不是「通过」——ctest 用 SKIP_RETURN_CODE 区分。
set_tests_properties(metrics_test PROPERTIES SKIP_RETURN_CODE 2)
