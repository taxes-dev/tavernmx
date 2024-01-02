#pragma once

#if defined(_WIN32)
#define TMX_WINDOWS
#elif defined(__APPLE__)
#define TMX_MACOS
#elif defined(__linux__)
#define TMX_LINUX
#else
#error "Unsupported platform"
#endif
