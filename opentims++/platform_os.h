#pragma once

#if defined (__unix__) || (defined (__APPLE__) && defined (__MACH__))
    #define OPENTIMS_UNIX
    #include <unistd.h>
    #ifndef _POSIX_VERSION
        #warning "Seems we're on Unix but not POSIX. Things might break."
    #endif
#elif defined(_WIN32) || defined(_WIN64)
    #define OPENTIMS_WINDOWS
#endif
