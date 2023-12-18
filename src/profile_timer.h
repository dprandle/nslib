#pragma once

#include "basic_types.h"

#if defined(IS_POSIX_SYSTEM)
#include <time.h>
#endif

#define NSEC_TO_SEC(nsec) (((double)nsec) / 1000000000.0)
#define NSEC_TO_MSEC(nsec) (((double)nsec) / 1000000.0)
#define NSEC_TO_USEC(nsec) (((double)nsec) / 1000.0)
#define SEC_TO_NSEC(sec) u64(usec * 1000000000)
#define MSEC_TO_NSEC(msec) u64(usec * 1000000)
#define USEC_TO_NSEC(usec) u64(usec * 1000)

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
#if defined(IS_POSIX_SYSTEM) 
    timespec t;
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
    u64 dt_ns;

    // Time, in s, between current split point and previous split (updated with ptimer_split)
    double dt;
};

inline double nanos_to_sec(u64 ns) {
    return (double)ns / 1000000000.0;
}

ptimespec ptimer_cur(int ptype);

ptimespec ptimer_diff(const ptimespec *start, const ptimespec *end);

u64 ptimer_nsec(const ptimespec *spec);

// Restart the timer setting all timepoints to current time
void ptimer_restart(profile_timepoints *ptimer);

// Update the timer split timespec with current time and dt_ns with elapsed nanoseconds since previous split
void ptimer_split(profile_timepoints *ptimer);

// Return elapsed nanoseconds between now and last split
u64 ptimer_split_dt(const profile_timepoints *ptimer);

// Return elapsed nanoseconds between now and last restart
u64 ptimer_elapsed_dt(const profile_timepoints *ptimer);
} // namespace nslib
