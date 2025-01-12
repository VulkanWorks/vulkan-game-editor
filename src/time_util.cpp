#include "time_util.h"

using namespace std::chrono;

TimePoint applicationStartTime;

TimePoint::TimePoint(steady_clock::time_point timePoint)
    : timePoint(timePoint) {}

TimePoint TimePoint::now()
{
    return TimePoint(steady_clock::now());
}

time_t TimePoint::elapsedMillis(TimePoint start) const
{
    return duration_cast<milliseconds>(this->timePoint - start.timePoint).count();
}

time_t TimePoint::elapsedMillis() const
{
    auto now = steady_clock::now();
    return duration_cast<milliseconds>(now - this->timePoint).count();
}

time_t TimePoint::elapsedMicros() const
{
    auto now = steady_clock::now();
    return duration_cast<microseconds>(now - this->timePoint).count();
}

time_t TimePoint::elapsedNanos() const
{
    auto now = steady_clock::now();
    return duration_cast<nanoseconds>(now - this->timePoint).count();
}

TimePoint TimePoint::sinceStart()
{
    return applicationStartTime;
}

void TimePoint::setApplicationStartTimePoint()
{
    applicationStartTime = TimePoint::now();
}