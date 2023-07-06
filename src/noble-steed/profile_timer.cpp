#include "profile_timer.h"

namespace noble_steed
{
ptimespec ptimer_cur(int ptype)
{
    ptimespec temp;
#if defined(IS_POSIX_SYSTEM)
    clockid_t t;
    switch (ptype) {
    case (PTIMER_TYPE_REALTIME):
        t = CLOCK_REALTIME;
        break;
    case (PTIMER_TYPE_PROCESS_CPU):
        t = CLOCK_PROCESS_CPUTIME_ID;
        break;
    case(PTIMER_TYPE_THREAD_CPU):
        t = CLOCK_THREAD_CPUTIME_ID;
        break;
    }
    clock_gettime(t, &temp.t);
#endif
    return temp;
}

    
ptimespec ptimer_diff(const ptimespec *start, const ptimespec *end)
{
    ptimespec temp;
#if defined(IS_POSIX_SYSTEM)
    temp.t.tv_sec = end->t.tv_sec - start->t.tv_sec;
    temp.t.tv_nsec = end->t.tv_nsec - start->t.tv_nsec;
#endif
    return temp;
}

u64 ptimer_nsec(const ptimespec *spec)
{
    u64 ret{};
#if defined(IS_POSIX_SYSTEM)
    ret = spec->t.tv_sec * 1000000000 + spec->t.tv_nsec;
#endif
    return ret;
}

void ptimer_restart(profile_timepoints * ptimer)
{
    ptimer->restart = ptimer_cur(ptimer->ctype);
    ptimer->split = ptimer->restart;
    ptimer->dt_ns = 0;
}

void ptimer_split(profile_timepoints * ptimer)
{
    ptimespec cur = ptimer_cur(ptimer->ctype);
    ptimespec split_dt = ptimer_diff(&ptimer->split, &cur);
    ptimer->dt_ns = ptimer_nsec(&split_dt);
    ptimer->split = cur;
}

u64 ptimer_split_dt(const profile_timepoints *ptimer)
{
    ptimespec cur = ptimer_cur(ptimer->ctype);
    ptimespec split_dt = ptimer_diff(&ptimer->split, &cur);
    return ptimer_nsec(&split_dt);
}

u64 ptimer_elapsed_dt(const profile_timepoints *ptimer)
{
    ptimespec cur = ptimer_cur(ptimer->ctype);
    ptimespec split_dt = ptimer_diff(&ptimer->restart, &cur);
    return ptimer_nsec(&split_dt);
}

} // namespace noble_steed
