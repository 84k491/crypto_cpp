#include "EventLoopSubscriber.h"
#include "Events.h"

#include <future>

template <class... Args>
class EventBarrier
{
public:
    EventBarrier(EventLoopSubscriber<Args...> & el, EventChannel<BarrierEvent> & ch)
        : m_future(m_promise.get_future())
    {
        BarrierEvent ev;
        m_sub = ch.subscribe(el.m_event_loop, [this, guid = ev.guid](const auto & b_ev) {
            if (guid == b_ev.guid) {
                m_promise.set_value();
            } }, Priority::Barrier);
        ch.push(ev);
    }

    void wait()
    {
        m_future.wait();
    };

private:
    std::promise<void> m_promise;
    std::future<void> m_future;
    std::shared_ptr<ISubscription> m_sub;
};
