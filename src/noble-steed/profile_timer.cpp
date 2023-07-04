#include "profile_timer.h"

namespace noble_steed
{
u64 ptimer_elapsed(const profile_timepoint *tpoint)
{
    return std::chrono::duration_cast<profile_duration>(profile_clock::now() - *tpoint).count();
}

void ptimer_restart(profile_timepoints * ptimer)
{
    ptimer->dt_us = 0;
    ptimer->total_us = 0;
    ptimer->cur = profile_clock::now();
}

void ptimer_update(profile_timepoints * ptimer)
{
    profile_timepoint nw = profile_clock::now();
    ptimer->dt_us = std::chrono::duration_cast<profile_duration>(nw - ptimer->cur).count();
    ptimer->total_us += ptimer->dt_us;
    ptimer->cur = nw;
}
} // namespace noble_steed
