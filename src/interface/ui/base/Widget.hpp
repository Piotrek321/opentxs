// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <cstddef>
#include <iosfwd>
#include <memory>
#include <mutex>
#include <utility>

#include "internal/interface/ui/UI.hpp"
#include "internal/util/LogMacros.hpp"
#include "opentxs/core/identifier/Generic.hpp"
#include "opentxs/interface/ui/Widget.hpp"
#include "opentxs/network/zeromq/ListenCallback.hpp"
#include "opentxs/network/zeromq/message/Message.hpp"
#include "opentxs/network/zeromq/socket/Request.hpp"
#include "opentxs/network/zeromq/socket/Subscribe.hpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/Log.hpp"
#include "opentxs/util/Types.hpp"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace opentxs  // NOLINT
{
// inline namespace v1
// {
namespace api
{
namespace session
{
class Client;
class UI;
}  // namespace session
}  // namespace api

namespace network
{
namespace zeromq
{
class Message;

namespace socket
{
class Publish;
}  // namespace socket
}  // namespace zeromq
}  // namespace network
// }  // namespace v1
}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)

namespace opentxs::ui::implementation
{
template <typename T>
auto extract_custom_ptr(
    CustomData& custom,
    const std::size_t index = 0) noexcept -> std::unique_ptr<T>
{
    OT_ASSERT((index + 1) <= custom.size());

    auto& ptr = custom.at(index);
    auto output = std::unique_ptr<T>{static_cast<T*>(ptr)};

    OT_ASSERT(output);

    ptr = nullptr;

    return output;
}

template <typename T>
auto extract_custom(CustomData& custom, const std::size_t index = 0) noexcept
    -> T
{
    auto ptr = extract_custom_ptr<T>(custom, index);
    auto output = T{std::move(*ptr)};

    return output;
}

template <typename T>
auto peek_custom(CustomData& custom, const std::size_t index = 0) noexcept
    -> const T&
{
    return *reinterpret_cast<T*>(custom.at(index));
}

auto verify_empty(const CustomData& custom) noexcept -> bool;

class Widget : virtual public opentxs::ui::Widget
{
public:
    using Message = network::zeromq::Message;

    const api::session::Client& api_;

    class MessageFunctor
    {
    public:
        virtual void operator()(Widget* object, const Message&)
            const noexcept = 0;

        MessageFunctor(const MessageFunctor&) = delete;
        MessageFunctor(MessageFunctor&&) = delete;
        auto operator=(const MessageFunctor&) -> MessageFunctor& = delete;
        auto operator=(MessageFunctor&&) -> MessageFunctor& = delete;

        virtual ~MessageFunctor() = default;

    protected:
        MessageFunctor() noexcept = default;
    };

    template <typename T>
    class MessageProcessor : virtual public MessageFunctor
    {
    public:
        using Function = void (T::*)(const Message&);

        void operator()(Widget* object, const Message& message)
            const noexcept final
        {
            auto real = dynamic_cast<T*>(object);

            OT_ASSERT(nullptr != real);

            (real->*callback_)(message);
        }

        MessageProcessor(Function callback) noexcept
            : callback_(callback)
        {
        }
        MessageProcessor() = delete;
        MessageProcessor(const MessageProcessor&) = delete;
        MessageProcessor(MessageProcessor&&) = default;
        auto operator=(const MessageProcessor&) -> MessageProcessor& = delete;
        auto operator=(MessageProcessor&&) -> MessageProcessor& = default;

    private:
        Function callback_;
    };

    auto ClearCallbacks() const noexcept -> void override;
    auto SetCallback(SimpleCallback cb) const noexcept -> void final;
    auto WidgetID() const noexcept -> OTIdentifier final { return widget_id_; }

    Widget() = delete;
    Widget(const Widget&) = delete;
    Widget(Widget&&) = delete;
    auto operator=(const Widget&) -> Widget& = delete;
    auto operator=(Widget&&) -> Widget& = delete;

    ~Widget() override;

protected:
    using ListenerDefinition = std::pair<UnallocatedCString, MessageFunctor*>;
    using ListenerDefinitions = UnallocatedVector<ListenerDefinition>;

    const OTIdentifier widget_id_;

    virtual void setup_listeners(
        const ListenerDefinitions& definitions) noexcept;
    auto UpdateNotify() const noexcept -> void;

    Widget(
        const api::session::Client& api,
        const Identifier& id,
        const SimpleCallback& cb = {}) noexcept;

private:
    const api::session::UI& ui_;
    UnallocatedVector<OTZMQListenCallback> callbacks_;
    UnallocatedVector<OTZMQSubscribeSocket> listeners_;
};
}  // namespace opentxs::ui::implementation
