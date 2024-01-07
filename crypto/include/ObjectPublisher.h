#pragma once

#include "ISubsription.h"
#include <crossguid2/crossguid/guid.hpp>
#include <functional>
#include <map>
#include <memory>

template <typename ObjectT>
class ObjectPublisher;

template <typename ObjectT>
class ObjectSubscribtion final : public ISubsription
{
public:
    ObjectSubscribtion(ObjectPublisher<ObjectT> & publisher, xg::Guid guid)
        : m_publisher(publisher)
        , m_guid(guid)
    {
    }

    ~ObjectSubscribtion() override
    {
        m_publisher.unsubscribe(m_guid);
    }

private:
    ObjectPublisher<ObjectT> & m_publisher;
    xg::Guid m_guid;
};

template <typename ObjectT>
class ObjectPublisher
{
public:
    ObjectPublisher() = default;

    void push(const ObjectT & object);
    void update(std::function<void(ObjectT &)> && update_callback);
    const ObjectT & get() const
    {
        return m_data;
    }

    std::unique_ptr<ObjectSubscribtion<ObjectT>> subscribe(std::function<void(const ObjectT &)> && update_callback);
    void unsubscribe(xg::Guid guid);

private:
    ObjectT m_data;
    std::map<xg::Guid, std::function<void(const ObjectT &)>> m_update_callbacks;
};

template <typename ObjectT>
void ObjectPublisher<ObjectT>::push(const ObjectT & object)
{
    m_data = object;
    for (const auto & [uuid, cb] : m_update_callbacks) {
        cb(object);
    }
}
template <typename ObjectT>
void ObjectPublisher<ObjectT>::update(std::function<void(ObjectT &)> && update_callback)
{
    update_callback(m_data);
    for (const auto & [uuid, cb] : m_update_callbacks) {
        cb(m_data);
    }
}

template <typename ObjectT>
std::unique_ptr<ObjectSubscribtion<ObjectT>> ObjectPublisher<ObjectT>::subscribe(std::function<void(const ObjectT &)> && update_callback)
{
    const auto [it, success] = m_update_callbacks.try_emplace(xg::newGuid(), std::move(update_callback));
    if (!success) {
        std::cout << "ERROR subscriber with uuid " << it->first << " already exists" << std::endl;
    }
    return std::make_unique<ObjectSubscribtion<ObjectT>>(*this, it->first);
}

template <typename ObjectT>
void ObjectPublisher<ObjectT>::unsubscribe(xg::Guid guid)
{
    m_update_callbacks.erase(guid);
}
