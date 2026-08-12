#pragma once

#if defined __linux__ || defined __linux
#define LINUX
#elif defined TARGET_OS_MAC || defined __apple__ || defined __APPLE__
#define MACOS
#elif defined _WIN32 || defined _WIN64
#error Windows detected, but not supported.
#else
#error Could not detect Operating System
#endif

#ifdef __aarch64__
#define ARM
#endif
