#pragma once

#include <chrono>
#include "basic_types.h"

namespace noble_steed
{

using profile_clock = std::chrono::high_resolution_clock;
using profile_duration = std::chrono::microseconds;
using profile_timepoint = std::chrono::time_point<profile_clock>;

struct profile_timepoints
{
    /// Most recent timepoint - set at restart and update
    profile_timepoint cur;

    /// Time, in us, since last update
    u64 dt_us;

    /// Time, in us, since last restart
    u64 total_us;
};

/// Return time elapsed, in us, since tpoint
u64 ptimer_elapsed(const profile_timepoint *tpoint);

/// Update cur with time right now, and set ptimer->dt_us and ptimer->total_us to 0
void ptimer_restart(profile_timepoints * ptimer);

/// Update both ptimer->dt_us and ptimer->total_us with elapsed time since ptimer->cur, and update ptimer->cur with time now
void ptimer_update(profile_timepoints * ptimer);
}
