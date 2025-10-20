#include "EventChannel.h"
#include "EventLoopSubscriber.h"
#include "Events.h"

#include <future>

class EventBarrier
{
public:
    EventBarrier(EventLoop & el, EventChannel<BarrierEvent> & ch)
        : m_future(m_promise.get_future())
        , m_sub{el}
    {
        BarrierEvent ev;
        m_sub.subscribe(
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
    EventSubcriber m_sub;
};
