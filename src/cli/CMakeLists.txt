add_executable(imagecodec main.cpp)
target_link_libraries(imagecodec PRIVATE
  OpenCLImageCodec::OpenCLImageCodec)
