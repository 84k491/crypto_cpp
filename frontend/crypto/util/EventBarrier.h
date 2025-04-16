#include "EventLoopSubscriber.h"
#include "Events.h"

#include <future>

template <class... Args>
class EventBarrier
{
public:
    EventBarrier(EventLoopSubscriber<Args...> & el)
        : m_promise()
        , m_future(m_promise.get_future())
    {
        BarrierEvent ev;
        m_sub = el.invoker().template register_invoker<BarrierEvent>([this, guid = ev.guid](const auto & b_ev) {
            if (guid == b_ev.guid) {
                m_promise.set_value();
            }
        });
        el.push_event(ev);
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
