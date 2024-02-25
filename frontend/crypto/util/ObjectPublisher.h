#pragma once

#include "ISubsription.h"

#include <crossguid2/crossguid/guid.hpp>
#include <functional>
#include <map>
#include <memory>
#include <vector>

template <typename ObjectT>
class ObjectPublisher;

template <typename ObjectT>
class ObjectSubscribtion final : public ISubsription
{
    friend class ObjectPublisher<ObjectT>;

public:
    ObjectSubscribtion(ObjectPublisher<ObjectT> & publisher, xg::Guid guid)
        : m_publisher(&publisher)
        , m_guid(guid)
    {
    }

    ~ObjectSubscribtion() override
    {
        if (m_publisher) {
            m_publisher->unsubscribe(m_guid);
        }
    }

private:
    ObjectPublisher<ObjectT> * m_publisher;
    xg::Guid m_guid;
};

template <typename ObjectT>
class ObjectPublisher
{
public:
    ObjectPublisher() = default;
    ~ObjectPublisher();

    void push(const ObjectT & object);
    void update(std::function<void(ObjectT &)> && update_callback);
    const ObjectT & get() const
    {
        return m_data;
    }

    [[nodiscard]] std::shared_ptr<ObjectSubscribtion<ObjectT>> subscribe(std::function<void(const ObjectT &)> && update_callback);
    void unsubscribe(xg::Guid guid);

private:
    ObjectT m_data;
    std::vector<std::tuple<
            xg::Guid,
            std::function<void(const ObjectT &)>,
            std::weak_ptr<ObjectSubscribtion<ObjectT>>>>
            m_update_callbacks;
};

template <typename ObjectT>
void ObjectPublisher<ObjectT>::push(const ObjectT & object)
{
    m_data = object;
    for (const auto & [uuid, cb, wptr] : m_update_callbacks) {
        cb(object);
    }
}
template <typename ObjectT>
void ObjectPublisher<ObjectT>::update(std::function<void(ObjectT &)> && update_callback)
{
    update_callback(m_data);
    for (const auto & [uuid, cb, wptr] : m_update_callbacks) {
        cb(m_data);
    }
}

template <typename ObjectT>
std::shared_ptr<ObjectSubscribtion<ObjectT>> ObjectPublisher<ObjectT>::subscribe(std::function<void(const ObjectT &)> && update_callback)
{
    const auto guid = xg::newGuid();
    auto sptr = std::make_shared<ObjectSubscribtion<ObjectT>>(*this, guid);

    m_update_callbacks.push_back({guid, std::move(update_callback), std::weak_ptr{sptr}});
    return sptr;
}

template <typename ObjectT>
void ObjectPublisher<ObjectT>::unsubscribe(xg::Guid guid)
{
    for (auto it = m_update_callbacks.begin(); it != m_update_callbacks.end(); ++it) {
        if (std::get<xg::Guid>(*it) == guid) {
            m_update_callbacks.erase(it);
            break;
        }
    }
}

template <typename ObjectT>
ObjectPublisher<ObjectT>::~ObjectPublisher()
{
    for (const auto [guid, cb, wptr] : m_update_callbacks) {
        if (auto sptr = wptr.lock()) {
            sptr->m_publisher = nullptr;
        }
    }
    m_update_callbacks.clear();
}
