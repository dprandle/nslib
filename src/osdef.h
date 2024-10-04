#pragma once

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
#    define PLATFORM_WIN32
#    ifdef _WIN64
#        define PLATFORM_WIN32_64BIT
#    else
#        define PLATFORM_WIN32_32BIT
#    endif
#elif __APPLE__
#    define PLATFORM_UNIX
#    define PLATFORM_APPLE
#    define PLATFORM_POSIX
#    include <TargetConditionals.h>
#    if TARGET_IPHONE_SIMULATOR
#        define PLATFORM_APPLE_IPHONE_SIM
#    elif TARGET_OS_MACCATALYST
#        define PLATFORM_APPLE_OS_MACCATALYST
#    elif TARGET_OS_IPHONE
#        define PLATFORM_APPLE_IOS
#    elif TARGET_OS_MAC
#        define PLATFORM_APPLE_MACOS
#    else
#        error "Unknown Apple platform"
#    endif
#elif __ANDROID__
#    define PLATFORM_UNIX
#    define PLATFORM_LINUX
#    define PLATFORM_POSIX
#    define PLATFORM_ANDROID
#elif __linux__
#    define PLATFORM_UNIX
#    define PLATFORM_LINUX
#    define PLATFORM_POSIX
#elif __unix__ // all unices not caught above
#    define PLATFORM_UNIX
#    define PLATFORM_POSIX
#elif defined(_POSIX_VERSION)
#    define PLATFORM_POSIX
#else
#    error "Unknown compiler"
#endif

#ifdef PLATFORM_WIN32
#    ifdef DLL_EXPORTS
#        define dllapi __declspec(dllexport)
#    else
#        define dllapi __declspec(dllimport)
#    endif
#else
#    define dllapi
#endif
