// ImageCodecOpenCL 不含 oneVPL 后端：提供"不可用"桩，保持 C API/CLI 的
// OIC_BACKEND_ONEVPL 路径编译链接（运行时返回不可用）。
#include "onevpl_test.h"

int oic_onevpl_encode(const char *, const char *, int) { return -1; }
int oic_onevpl_decode(const char *, const char *) { return -1; }
int oic_onevpl_available(void) { return 1; }
