#include "EventLoop.h"

#include "EventBarrier.h"
#include "EventChannel.h"
#include "EventLoopSubscriber.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>

namespace test {
using namespace testing;

class EventLoopTest : public Test
{
public:
    EventLoopTest() {}

protected:
    EventLoop el;
    EventChannel<BarrierEvent> barrier_channel;
};

TEST_F(EventLoopTest, BasicEventHandle)
{
    EventChannel<int> ch;

    size_t events_handled = 0;
    auto sub = std::make_unique<EventSubcriber>(el);

    sub->subscribe(ch, [&](int v) {
        ++events_handled;
        EXPECT_EQ(v, 1);
    });

    ch.push(1);
    EventBarrier eb{el, barrier_channel};
    eb.wait();
    EXPECT_EQ(events_handled, 1);

    sub.reset();

    ch.push(2);
    EXPECT_EQ(events_handled, 1);
}

struct MockObject
{
    MockObject(EventLoop & el, std::atomic_bool & destructed)
        : m_el{el}
        , m_destructed(destructed)
        , m_sub(el)
    {
    }

    ~MockObject()
    {
        m_destructed = true;
    }

    void sub(auto & ch)
    {
        m_sub.subscribe(ch, [this](int) {
            if (m_destructed) {
                FAIL();
            }

            std::this_thread::sleep_for(std::chrono::milliseconds{100});
        });
    }

    EventLoop & m_el;

    std::atomic_bool & m_destructed;
    EventSubcriber m_sub;
};

// TODO implement event drop after subscriber destruction
TEST_F(EventLoopTest, DISABLED_UnsubWithEventsInLoop)
{
    std::atomic_bool destructed = false;

    EventChannel<int> ch;
    auto obj = std::make_unique<MockObject>(el, destructed);
    obj->sub(ch);

    ch.push(2);
    {
        EventBarrier eb{el, barrier_channel};
        eb.wait();
    }

    ch.push(3);
    ch.push(4);

    obj.reset();
    EXPECT_TRUE(destructed);

    {
        EventBarrier eb{el, barrier_channel};
        eb.wait();
    }
}

// TODO add test for delayed events

} // namespace test
