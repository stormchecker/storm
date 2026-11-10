#pragma once

// Exports storm-parsers' public symbols; the library is compiled with hidden visibility.
#if defined(__GNUC__) || defined(__clang__)
#define STORM_PARSERS_API __attribute__((visibility("default")))
#else
#define STORM_PARSERS_API
#endif