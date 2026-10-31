// bmp_io.h - BMP 读写（8-bit 灰度 / 24-bit RGB），top-down RGB 语义。
//
// 位置说明：BMP 在本库里是**模块之间的中转格式**（C API 以文件路径为参数，
// 各模块之间用 BMP 传像素），不是产品路径。放在 Common 里是为了让 TIFF / PNG /
// JPEG 三个模块共用同一份实现，而不是各自复制一份只在细节上不同的版本。
//
// 通道保真：写 8-bit 灰度就是 1 通道，绝不为了省事升成 24-bit RGB；读的时候
// 也按 bpp 决定 channels。上层据此判断「1 通道进、1 通道出」是否成立。
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "image_buffer.h"

namespace image_codec {

// 读取 BMP（24-bit BI_RGB，或 8-bit 灰度 + 调色板）。
//   8-bit 时按调色板索引取样本（本库自己写出的灰度 BMP 使用恒等灰度调色板，
//   因此索引即灰度值；外部任意调色板也按索引解释，不做颜色空间猜测）。
// 成功返回 0；不支持的格式返回 -2；I/O 失败返回 -1。
int bmp_read(const std::string &path, ImageBuffer *img, std::string *err);

// 按 img.channels 写出 BMP：1 -> 8-bit 灰度（恒等调色板），3 -> 24-bit RGB。
// 成功返回 0。
int bmp_write(const std::string &path, const ImageBuffer &img,
              std::string *err);

// 便捷：从内存中的 top-down 像素写出 BMP（不复制到 ImageBuffer）。
int bmp_write_pixels(const std::string &path, const uint8_t *pixels, int width,
                     int height, int channels, std::string *err);

} // namespace image_codec
