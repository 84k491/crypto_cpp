#include "EventLoopSubscriber.h"
#include "Events.h"

#include <future>

class EventBarrier
{
public:
    EventBarrier(EventLoopSubscriber & el, EventChannel<BarrierEvent> & ch)
        : m_future(m_promise.get_future())
    {
        BarrierEvent ev;
        m_sub = el.subscribe_for_sub(
                ch,
                [this, guid = ev.guid](const auto & b_ev) {
                    if (guid != b_ev.guid) {
                        return;
                    };
                    m_promise.set_value();
                },
                Priority::Barrier);
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
