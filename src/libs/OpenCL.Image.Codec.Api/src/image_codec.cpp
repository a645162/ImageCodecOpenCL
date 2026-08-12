// src/api/image_codec.cpp - 对外 C API 实现。
//
// 模块文件入口（image_codec_*_decode/encode，见各 *_test.h）接受文件路径，
// 本层做内存 buffer <-> 临时文件的桥接：
//   decode: buffer -> 临时输入文件 -> 模块解码 -> 临时 BMP -> image_codec_image
//   encode: image_codec_image -> 临时 BMP -> 模块编码 -> 输出文件 -> buffer
// 临时文件放在系统临时目录（Windows GetTempPathA/GetTempFileNameA）。
#include "OpenCLImageCodec/image_codec.h"

#include "../../OpenCL.Image.Codec.Core/src/ocl_device.h"
#include "../../OpenCL.Image.Codec.Jpeg/src/bmp_io.h"
#include "../../OpenCL.Image.Codec.Jpeg/src/jpeg_test.h"
#include "../../OpenCL.Image.Codec.Tiff/src/tiff_test.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <unistd.h>
#endif

using image_codec::OclDevice;
using image_codec::OclDeviceInfo;

// 不透明句柄实际结构。
struct image_codec_codec {
  int backend;  // image_codec_backend 的整数值
};

namespace {

// 在系统临时目录创建一个唯一临时文件路径（文件已被创建，可 fopen 覆盖写）。
bool make_temp_path(std::string *out) {
#if defined(_WIN32)
  char dir[MAX_PATH];
  const DWORD n = GetTempPathA(MAX_PATH, dir);
  if (n == 0 || n >= MAX_PATH) {
    return false;
  }
  char path[MAX_PATH];
  if (GetTempFileNameA(dir, "image_codec", 0, path) == 0) {
    return false;
  }
  *out = path;
  return true;
#else
  char tmpl[] = "/tmp/image_codec_XXXXXX";
  int fd = ::mkstemp(tmpl);
  if (fd < 0) {
    return false;
  }
  ::close(fd);
  *out = tmpl;
  return true;
#endif
}

bool write_file(const std::string &path, const uint8_t *data, size_t size) {
  FILE *f = std::fopen(path.c_str(), "wb");
  if (f == nullptr) {
    return false;
  }
  const size_t n = std::fwrite(data, 1, size, f);
  std::fclose(f);
  return n == size;
}

bool read_file(const std::string &path, std::vector<uint8_t> *out) {
  FILE *f = std::fopen(path.c_str(), "rb");
  if (f == nullptr) {
    return false;
  }
  std::fseek(f, 0, SEEK_END);
  const long sz = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  if (sz < 0) {
    std::fclose(f);
    return false;
  }
  out->resize(static_cast<size_t>(sz));
  const size_t n = std::fread(out->data(), 1, out->size(), f);
  std::fclose(f);
  return n == out->size();
}

}  // namespace

extern "C" {

// ---- 状态码 ----
const char *image_codec_status_string(image_codec_status status) {
  switch (status) {
    case IMAGE_CODEC_OK:
      return "success";
    case IMAGE_CODEC_ERR_INVALID_ARGUMENT:
      return "invalid argument";
    case IMAGE_CODEC_ERR_OUT_OF_MEMORY:
      return "out of memory";
    case IMAGE_CODEC_ERR_BACKEND_UNAVAILABLE:
      return "backend unavailable";
    case IMAGE_CODEC_ERR_UNSUPPORTED_FORMAT:
      return "unsupported format";
    case IMAGE_CODEC_ERR_DECODE_FAILED:
      return "decode failed";
    case IMAGE_CODEC_ERR_ENCODE_FAILED:
      return "encode failed";
    case IMAGE_CODEC_ERR_IO:
      return "I/O error";
    case IMAGE_CODEC_ERR_INTERNAL:
      return "internal error";
    default:
      return "unknown status";
  }
}

// ---- 生命周期 ----
image_codec_status image_codec_codec_create(image_codec_codec **out_codec, image_codec_backend backend) {
  if (out_codec == nullptr) {
    return IMAGE_CODEC_ERR_INVALID_ARGUMENT;
  }
  *out_codec = nullptr;
  if (backend != IMAGE_CODEC_BACKEND_OPENCL && backend != IMAGE_CODEC_BACKEND_CPU) {
    return IMAGE_CODEC_ERR_INVALID_ARGUMENT;
  }
  image_codec_codec *c = static_cast<image_codec_codec *>(std::malloc(sizeof(image_codec_codec)));
  if (c == nullptr) {
    return IMAGE_CODEC_ERR_OUT_OF_MEMORY;
  }
  c->backend = static_cast<int>(backend);
  *out_codec = c;
  return IMAGE_CODEC_OK;
}

void image_codec_codec_destroy(image_codec_codec *codec) { std::free(codec); }

// ---- 解码 ----
image_codec_status image_codec_codec_decode(image_codec_codec *codec, image_codec_format format,
                            const uint8_t *in, size_t in_len,
                            image_codec_image *out) {
  if (codec == nullptr || out == nullptr) {
    return IMAGE_CODEC_ERR_INVALID_ARGUMENT;
  }
  if (in == nullptr || in_len == 0) {
    return IMAGE_CODEC_ERR_INVALID_ARGUMENT;
  }
  std::memset(out, 0, sizeof(*out));

  std::string in_path, out_bmp;
  if (!make_temp_path(&in_path) || !make_temp_path(&out_bmp)) {
    return IMAGE_CODEC_ERR_IO;
  }
  if (!write_file(in_path, in, in_len)) {
    std::remove(in_path.c_str());
    std::remove(out_bmp.c_str());
    return IMAGE_CODEC_ERR_IO;
  }

  const int backend = codec->backend;
  int rc = -1;
  switch (format) {
    case IMAGE_CODEC_FORMAT_JPEG:
      // image_codec_jpeg_decode backend: 0=opencl, 1=cpu。
      rc = image_codec_jpeg_decode(in_path.c_str(), out_bmp.c_str(),
                           backend == IMAGE_CODEC_BACKEND_CPU ? 1 : 0);
      break;
    case IMAGE_CODEC_FORMAT_TIFF:
      // image_codec_tiff_decode backend: 0=opencl, 2=cpu —— 与 image_codec_backend 枚举值一致。
      rc = image_codec_tiff_decode(in_path.c_str(), out_bmp.c_str(), backend);
      break;
    default:
      std::remove(in_path.c_str());
      std::remove(out_bmp.c_str());
      return IMAGE_CODEC_ERR_UNSUPPORTED_FORMAT;
  }

  std::remove(in_path.c_str());
  if (rc != 0) {
    std::remove(out_bmp.c_str());
    return IMAGE_CODEC_ERR_DECODE_FAILED;
  }

  // 读模块写出的 BMP -> image_codec_image（RGB top-down）。
  std::vector<uint8_t> rgb;
  int w = 0, h = 0;
  std::string err;
  if (image_codec::jpeg::bmp_read(out_bmp, &rgb, &w, &h, &err) != 0 || rgb.empty()) {
    std::remove(out_bmp.c_str());
    return IMAGE_CODEC_ERR_DECODE_FAILED;
  }
  std::remove(out_bmp.c_str());

  out->width = static_cast<uint32_t>(w);
  out->height = static_cast<uint32_t>(h);
  out->channels = 3;
  out->stride = static_cast<size_t>(w) * 3;
  out->data_size = rgb.size();
  out->data = static_cast<uint8_t *>(std::malloc(rgb.size()));
  if (out->data == nullptr) {
    std::memset(out, 0, sizeof(*out));
    return IMAGE_CODEC_ERR_OUT_OF_MEMORY;
  }
  std::memcpy(out->data, rgb.data(), rgb.size());
  return IMAGE_CODEC_OK;
}

// ---- 编码 ----
image_codec_status image_codec_codec_encode(image_codec_codec *codec, image_codec_format format,
                            const image_codec_image *img,
                            const image_codec_encode_params *params, uint8_t *out,
                            size_t *out_len, size_t out_capacity) {
  if (codec == nullptr || img == nullptr || out == nullptr ||
      out_len == nullptr) {
    return IMAGE_CODEC_ERR_INVALID_ARGUMENT;
  }
  if (img->data == nullptr || img->width == 0 || img->height == 0 ||
      img->channels != 3) {
    return IMAGE_CODEC_ERR_INVALID_ARGUMENT;
  }

  const int backend = codec->backend;
  const int jpeg_quality = params != nullptr ? params->jpeg_quality : 85;
  const int tiff_compression =
      params != nullptr ? params->tiff_compression : 5;
  const int tiff_rows_per_strip =
      params != nullptr ? params->tiff_rows_per_strip : 0;

  // 参数校验。
  switch (format) {
    case IMAGE_CODEC_FORMAT_JPEG:
      if (jpeg_quality < 1 || jpeg_quality > 100) {
        return IMAGE_CODEC_ERR_INVALID_ARGUMENT;
      }
      break;
    case IMAGE_CODEC_FORMAT_TIFF:
      if (tiff_compression != 1 && tiff_compression != 5) {
        return IMAGE_CODEC_ERR_INVALID_ARGUMENT;
      }
      if (tiff_rows_per_strip < 0) {
        return IMAGE_CODEC_ERR_INVALID_ARGUMENT;
      }
      break;
    default:
      return IMAGE_CODEC_ERR_UNSUPPORTED_FORMAT;
  }

  // 打包像素行（处理 stride != width*channels 的情况）。
  const size_t row_bytes = static_cast<size_t>(img->width) * img->channels;
  std::vector<uint8_t> packed;
  const uint8_t *rgb = img->data;
  if (img->stride != 0 && img->stride != row_bytes) {
    packed.resize(row_bytes * img->height);
    for (uint32_t y = 0; y < img->height; y++) {
      std::memcpy(packed.data() + static_cast<size_t>(y) * row_bytes,
                  img->data + static_cast<size_t>(y) * img->stride, row_bytes);
    }
    rgb = packed.data();
  }

  std::string in_bmp, out_path;
  if (!make_temp_path(&in_bmp) || !make_temp_path(&out_path)) {
    return IMAGE_CODEC_ERR_IO;
  }
  std::string err;
  if (image_codec::jpeg::bmp_write(in_bmp, rgb, static_cast<int>(img->width),
                           static_cast<int>(img->height), &err) != 0) {
    std::remove(in_bmp.c_str());
    std::remove(out_path.c_str());
    return IMAGE_CODEC_ERR_IO;
  }

  int rc = -1;
  switch (format) {
    case IMAGE_CODEC_FORMAT_JPEG:
      rc = image_codec_jpeg_encode(in_bmp.c_str(), out_path.c_str(),
                           backend == IMAGE_CODEC_BACKEND_CPU ? 1 : 0, jpeg_quality);
      break;
    case IMAGE_CODEC_FORMAT_TIFF:
      rc = image_codec_tiff_encode(in_bmp.c_str(), out_path.c_str(), backend,
                           tiff_compression, tiff_rows_per_strip);
      break;
    default:
      break;
  }
  std::remove(in_bmp.c_str());
  if (rc != 0) {
    std::remove(out_path.c_str());
    return IMAGE_CODEC_ERR_ENCODE_FAILED;
  }

  std::vector<uint8_t> encoded;
  if (!read_file(out_path, &encoded)) {
    std::remove(out_path.c_str());
    return IMAGE_CODEC_ERR_IO;
  }
  std::remove(out_path.c_str());

  if (encoded.size() > out_capacity) {
    *out_len = encoded.size();  // 告知所需大小，支持先查后写。
    return IMAGE_CODEC_ERR_OUT_OF_MEMORY;
  }
  std::memcpy(out, encoded.data(), encoded.size());
  *out_len = encoded.size();
  return IMAGE_CODEC_OK;
}

// ---- 释放 ----
void image_codec_image_free(image_codec_image *img) {
  if (img == nullptr) {
    return;
  }
  if (img->data != nullptr) {
    std::free(img->data);
  }
  std::memset(img, 0, sizeof(*img));
}

// ---- 设备枚举 ----
image_codec_status image_codec_device_list(char ***names, int *count) {
  if (names == nullptr || count == nullptr) {
    return IMAGE_CODEC_ERR_INVALID_ARGUMENT;
  }
  *names = nullptr;
  *count = 0;

  std::vector<OclDeviceInfo> devices;
  try {
    devices = OclDevice::enumerate();
  } catch (...) {
    return IMAGE_CODEC_ERR_INTERNAL;
  }
  if (devices.empty()) {
    return IMAGE_CODEC_OK;
  }

  char **arr = static_cast<char **>(
      std::calloc(devices.size(), sizeof(char *)));
  if (arr == nullptr) {
    return IMAGE_CODEC_ERR_OUT_OF_MEMORY;
  }
  for (size_t i = 0; i < devices.size(); i++) {
    const OclDeviceInfo &d = devices[i];
    char buf[1024];
    std::snprintf(buf, sizeof(buf), "%s | %s | %s | CL %u.%u",
                  d.platformName.c_str(), d.deviceName.c_str(),
                  d.vendor.c_str(), d.major, d.minor);
    const size_t len = std::strlen(buf);
    char *name = static_cast<char *>(std::malloc(len + 1));
    if (name == nullptr) {
      image_codec_device_list_free(arr, static_cast<int>(i));
      return IMAGE_CODEC_ERR_OUT_OF_MEMORY;
    }
    std::memcpy(name, buf, len + 1);
    arr[i] = name;
  }
  *names = arr;
  *count = static_cast<int>(devices.size());
  return IMAGE_CODEC_OK;
}

void image_codec_device_list_free(char **names, int count) {
  if (names == nullptr) {
    return;
  }
  for (int i = 0; i < count; i++) {
    if (names[i] != nullptr) {
      std::free(names[i]);
    }
  }
  std::free(names);
}

}  // extern "C"
