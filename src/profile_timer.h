#pragma once

#include "basic_types.h"
#include "osdef.h"

#if defined(PLATFORM_UNIX)
#include <ctime>
#endif
    
#define NSEC_TO_SEC(nsec) (((double)nsec) / 1000000000.0)
#define NSEC_TO_MSEC(nsec) (((double)nsec) / 1000000.0)
#define NSEC_TO_USEC(nsec) (((double)nsec) / 1000.0)
#define SEC_TO_NSEC(sec) i64(sec * 1000000000)
#define MSEC_TO_NSEC(msec) i64(msec * 1000000)
#define USEC_TO_NSEC(usec) i64(usec * 1000)

namespace nslib
{

enum ptimer_type
{
    PTIMER_TYPE_REALTIME,
    PTIMER_TYPE_PROCESS_CPU,
    PTIMER_TYPE_THREAD_CPU
};

struct ptimespec
{
#if defined(PLATFORM_UNIX) 
    timespec t;
#elif defined(PLATFORM_WIN32)
    i64 t;
    i64 f;
#endif
};

struct profile_timepoints
{
    int ctype{};

    // Most recent timepoint - set at restart and update
    ptimespec split;

    // Time point at restart
    ptimespec restart;

    // Time, in us, between current split point and previous split (updated with ptimer_split)
    i64 dt_ns;

    // Time, in s, between current split point and previous split (updated with ptimer_split)
    double dt;
};

inline double nanos_to_sec(i64 ns) {
    return (double)ns / 1000000000.0;
}

ptimespec ptimer_cur(int ptype);

ptimespec ptimer_diff(const ptimespec *start, const ptimespec *end);

i64 ptimer_nsec(const ptimespec *spec);

// Restart the timer setting all timepoints to current time
void ptimer_restart(profile_timepoints *ptimer);

// Update the timer split timespec with current time and dt_ns with elapsed nanoseconds since previous split
void ptimer_split(profile_timepoints *ptimer);

// Return elapsed nanoseconds between now and last split
i64 ptimer_split_dt(const profile_timepoints *ptimer);

// Return elapsed nanoseconds between now and last restart
i64 ptimer_elapsed_dt(const profile_timepoints *ptimer);
} // namespace nslib
