// image_buffer.h - 各编解码模块共用的宿主内存图像缓冲（top-down 行序）。
//
// 为什么要有这一层：OpenCL 后端的三个格式（TIFF / PNG / JPEG）以前各自揣着一份
// 自己的像素容器（BGR24 / RGB24 / 裸 vector），通道数一变就要改三处。统一成
// 一个「width x height x channels，第 0 行为图像顶部」的结构后：
//   * 通道保真变成可检查的硬约束（channels 是结构体字段，不是隐含约定）；
//   * GPU 侧的上传/回读只有一套尺寸推导（stride = width * channels）。
//
// 支持 channels == 1（灰度）与 3（RGB）：这两个是本库对外承诺的通道数。
// Alpha 不放进来——本项目图像数据不含 Alpha，需要时由 UI 层自行补。
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace image_codec {

struct ImageBuffer {
  int width = 0;
  int height = 0;
  int channels = 0;          // 1 = 灰度, 3 = RGB
  std::vector<uint8_t> data; // top-down，width*channels 字节/行

  // 每行字节数（不含 padding）。
  size_t stride() const {
    return static_cast<size_t>(width) * static_cast<size_t>(channels);
  }

  // 像素总数（字节）。
  size_t size() const { return stride() * static_cast<size_t>(height); }

  bool valid() const {
    return width > 0 && height > 0 && (channels == 1 || channels == 3) &&
           data.size() >= size();
  }

  // 按尺寸/通道分配（保留内容不做保证，调用方自己填）。
  bool allocate(int w, int h, int c) {
    if (w <= 0 || h <= 0 || (c != 1 && c != 3)) {
      return false;
    }
    width = w;
    height = h;
    channels = c;
    data.assign(static_cast<size_t>(w) * static_cast<size_t>(c) *
                    static_cast<size_t>(h),
                0);
    return true;
  }

  void reset() {
    width = 0;
    height = 0;
    channels = 0;
    data.clear();
  }
};

} // namespace image_codec
