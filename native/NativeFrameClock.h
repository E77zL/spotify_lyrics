// MIT. Per-thread high-resolution frame deadlines; never change the system timer resolution.
#pragma once
#include <windows.h>
#include <algorithm>
#include <cmath>
class NativeFrameClock
{
    HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, 2, TIMER_MODIFY_STATE | SYNCHRONIZE);
    double deadline = 0, interval = 0.1;

  public:
    static double now()
    {
        static const double frequency = [] {
            LARGE_INTEGER f{};
            QueryPerformanceFrequency(&f);
            return static_cast<double>(f.QuadPart);
        }();
        LARGE_INTEGER value{};
        QueryPerformanceCounter(&value);
        return value.QuadPart / frequency;
    }
    NativeFrameClock()
    {
        if (!timer)
            timer = CreateWaitableTimerW(nullptr, FALSE, nullptr);
    }
    ~NativeFrameClock()
    {
        if (timer)
            CloseHandle(timer);
    }
    void request()
    {
        if (interval > 0.02)
            deadline = std::min(deadline, now());
    }
    bool due() const
    {
        return now() >= deadline;
    }
    void advance(double seconds)
    {
        const double time = now();
        if (seconds != interval)
            deadline = time + seconds;
        else
        {
            deadline += seconds;
            if (deadline <= time)
                deadline += (std::floor((time - deadline) / seconds) + 1) * seconds;
        }
        interval = seconds;
    }
    void wait()
    {
        const double remaining = std::max(0.0001, deadline - now());
        LARGE_INTEGER due{};
        due.QuadPart = -static_cast<LONGLONG>(remaining * 10000000);
        if (timer && SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE))
            MsgWaitForMultipleObjectsEx(1, &timer, INFINITE, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        else
            MsgWaitForMultipleObjectsEx(0, nullptr, static_cast<DWORD>(std::ceil(remaining * 1000)),
                                        QS_ALLINPUT, MWMO_INPUTAVAILABLE);
    }
};
