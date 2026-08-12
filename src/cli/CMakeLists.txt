add_executable(image_codec main.cpp)
target_link_libraries(image_codec PRIVATE
  OpenCLImageCodec::OpenCLImageCodec)
