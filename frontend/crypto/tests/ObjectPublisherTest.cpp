#include "ObjectPublisher.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace test {
using namespace testing;

class ObjectPublisherTest : public Test
{
public:
    ObjectPublisherTest() = default;
};

TEST(ObjectPublisherTest, subscribe_unsubscribe_raii)
{
    ObjectPublisher<int> publisher;

    std::vector<int> inner_values;
    std::vector<int> outer_values;

    auto sub_outer = publisher.subscribe([&](const int & value) {
        outer_values.push_back(value);
    });

    {
        auto sub = publisher.subscribe([&](const int & value) {
            inner_values.push_back(value);
        });

        publisher.push(1);
        publisher.push(2);
    }
    publisher.push(3);
    publisher.push(4);

    ASSERT_THAT(inner_values, ElementsAre(1, 2));
    ASSERT_THAT(outer_values, ElementsAre(1, 2, 3, 4));
}

TEST(ObjectPublisherTest, update)
{
    ObjectPublisher<int> publisher;

    int outer_value = 0;
    int inner_value = 0;

    auto sub_outer = publisher.subscribe([&](const int & value) {
        outer_value = value;
    });

    {
        auto sub = publisher.subscribe([&](const int & value) {
            inner_value = value;
        });

        publisher.update([](int & value) {
            value = 1;
        });
    }
    publisher.update([](int & value) {
        value = 2;
    });

    EXPECT_EQ(outer_value, 2);
    EXPECT_EQ(inner_value, 1);
}

TEST(ObjectPublisherTest, delete_dangling_sub)
{
    int value = 0;
    {
        std::shared_ptr<ObjectSubscribtion<int>> sub;
        {
            ObjectPublisher<int> publisher;
            sub = publisher.subscribe([&](const int & v) { value = v; });
            publisher.push(1);
        }
    }
    EXPECT_EQ(value, 1);
}

TEST(ObjectPublisherTest, get_without_subs)
{
    ObjectPublisher<int> publisher;
    publisher.push(1);
    EXPECT_EQ(publisher.get(), 1);
}

TEST(ObjectPublisherTest, orderding)
{
    int coeff = 0;
    int first_value = 0;
    int second_value = 0;

    ObjectPublisher<int> publisher;
    auto sub1 = publisher.subscribe([&](const int & v) { first_value = v * ++coeff; });
    auto sub2 = publisher.subscribe([&](const int & v) { second_value = v * ++coeff; });

    publisher.push(1);
    EXPECT_EQ(first_value, 1);
    EXPECT_EQ(second_value, 2);

    publisher.update([&](int & v) { v = 11; });
    EXPECT_EQ(first_value, 33);
    EXPECT_EQ(second_value, 44);
}

}; // namespace test
