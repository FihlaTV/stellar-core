#include "util/Timer.h"
#include "util/Logging.h"

namespace stellar
{

using namespace std;


VirtualClock::time_point
VirtualClock::now() noexcept
{
    return mNow;
}

VirtualClock::time_point
VirtualClock::next()
{
    if (mEvents.empty())
    {
        return time_point::max();
    }
    else
    {
        return mEvents.top()->mWhen;
    }
}

void
VirtualClock::enqueue(VirtualClockEvent const& ve)
{
    // LOG(DEBUG) << "VirtualClock::enqueue";
    mEvents.emplace(make_shared<VirtualClockEvent>(ve));
}

void
VirtualClock::cancelAllEventsFrom(VirtualTimer *v)
{
    // LOG(DEBUG) << "VirtualClock::cancelAllEventsFrom";
    // Brute force approach; could be done better with a linked list
    // per timer, as is done in asio. For the time being this should
    // be tolerable.
    vector<shared_ptr<VirtualClockEvent>> toCancel;
    vector<shared_ptr<VirtualClockEvent>> toKeep;
    toCancel.reserve(mEvents.size());
    toKeep.reserve(mEvents.size());
    while (!mEvents.empty())
    {
        if (mEvents.top()->mTimer == v)
        {
            toCancel.emplace_back(mEvents.top());
        }
        else
        {
            toKeep.emplace_back(mEvents.top());
        }
        mEvents.pop();
    }

    for (auto& e : toCancel)
    {
        e->mCallback(asio::error::operation_aborted);
    }
    mEvents =
        priority_queue<shared_ptr<VirtualClockEvent>>(toKeep.begin(),
                                                      toKeep.end());
}

size_t
VirtualClock::advanceTo(time_point n) {
    vector<shared_ptr<VirtualClockEvent>> toDispatch;
    toDispatch.reserve(mEvents.size());
    // LOG(DEBUG) << "VirtualClock::advanceTo("
    //            << n.time_since_epoch().count() << ")";
    mNow = n;
    while (!mEvents.empty())
    {
        if (mEvents.top()->mWhen >= mNow)
            break;
        toDispatch.emplace_back(mEvents.top());
        mEvents.pop();
    }
    // LOG(DEBUG) << "VirtualClock::advanceTo running "
    //            << toDispatch.size() << " events";
    for (auto& e : toDispatch) {
        // LOG(DEBUG) << "VirtualClock::advanceTo callback set for @ "
        //            << e->mWhen.time_since_epoch().count();
        e->mCallback(asio::error_code());
    }
    // LOG(DEBUG) << "VirtualClock::advanceTo done";
    return toDispatch.size();
}

VirtualTimer::VirtualTimer(VirtualClock &c)
    : mClock(c)
    , mExpiryTime(mClock.now())
    , mCancelled(false)
{
}

VirtualTimer::~VirtualTimer()
{
    cancel();
}

void
VirtualTimer::cancel()
{
    if (!mCancelled)
    {
        mCancelled = true;
        mClock.cancelAllEventsFrom(this);
    }
}

void
VirtualTimer::expires_at(VirtualClock::time_point t)
{
    mExpiryTime = t;
}

void
VirtualTimer::expires_from_now(VirtualClock::duration d)
{
    mExpiryTime = mClock.now() + d;
}

void
VirtualTimer::async_wait(function<void(asio::error_code)> const& fn)
{
    if (!mCancelled)
    {
        mClock.enqueue(VirtualClockEvent{mExpiryTime, fn, this});
    }
}



}