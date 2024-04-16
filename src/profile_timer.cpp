#if defined (_WIN32)
#define NOMINMAX
#include <windows.h>
#endif

#include "profile_timer.h"

namespace nslib
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
#elif defined(_WIN32)
    LARGE_INTEGER t, f;
    QueryPerformanceCounter(&t);
    QueryPerformanceFrequency(&f);
    temp.t = t.QuadPart;
    temp.f = f.QuadPart;
#endif
    return temp;
}

    
ptimespec ptimer_diff(const ptimespec *start, const ptimespec *end)
{
    ptimespec temp;
#if defined(IS_POSIX_SYSTEM)
    temp.t.tv_sec = end->t.tv_sec - start->t.tv_sec;
    temp.t.tv_nsec = end->t.tv_nsec - start->t.tv_nsec;
#elif defined(_WIN32)
    temp.t = end->t - start->t;
    temp.f = end->f;
#endif
    return temp;
}

i64 ptimer_nsec(const ptimespec *spec)
{
    i64 ret{};
#if defined(IS_POSIX_SYSTEM)
    ret = spec->t.tv_sec * 1000000000 + spec->t.tv_nsec;
#elif defined(_WIN32)
    ret = SEC_TO_NSEC(spec->t) / spec->f;
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
    ptimer->dt = nanos_to_sec(ptimer->dt_ns);
    ptimer->split = cur;
}

i64 ptimer_split_dt(const profile_timepoints *ptimer)
{
    ptimespec cur = ptimer_cur(ptimer->ctype);
    ptimespec split_dt = ptimer_diff(&ptimer->split, &cur);
    return ptimer_nsec(&split_dt);
}

i64 ptimer_elapsed_dt(const profile_timepoints *ptimer)
{
    ptimespec cur = ptimer_cur(ptimer->ctype);
    ptimespec split_dt = ptimer_diff(&ptimer->restart, &cur);
    return ptimer_nsec(&split_dt);
}


} // namespace nslib
