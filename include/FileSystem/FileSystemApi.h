#pragma once

#if defined(FS_API_EXPORT)
#define FS_API __declspec(dllexport)
#elif defined(FS_API_IMPORT)
#define FS_API __declspec(dllimport)
#endif