// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"    // IWYU pragma: associated
#include "1_Internal.hpp"  // IWYU pragma: associated
#include "interface/ui/blockchainselection/BlockchainSelection.hpp"  // IWYU pragma: associated

#include <future>
#include <memory>
#include <string_view>
#include <utility>

#include "interface/ui/base/List.hpp"
#include "internal/api/network/Blockchain.hpp"
#include "internal/util/LogMacros.hpp"
#include "opentxs/api/network/Blockchain.hpp"
#include "opentxs/api/network/Network.hpp"
#include "opentxs/api/session/Client.hpp"
#include "opentxs/api/session/Endpoints.hpp"
#include "opentxs/blockchain/Blockchain.hpp"
#include "opentxs/blockchain/Types.hpp"
#include "opentxs/core/identifier/Generic.hpp"
#include "opentxs/interface/ui/Blockchains.hpp"
#include "opentxs/network/zeromq/Pipeline.hpp"
#include "opentxs/network/zeromq/message/Frame.hpp"
#include "opentxs/network/zeromq/message/FrameSection.hpp"
#include "opentxs/network/zeromq/message/Message.hpp"
#include "opentxs/network/zeromq/message/Message.tpp"
#include "opentxs/util/Container.hpp"
#include "opentxs/util/Log.hpp"
#include "opentxs/util/Pimpl.hpp"
#include "util/tuning.hpp"

namespace zmq = opentxs::network::zeromq;

namespace opentxs::factory
{
auto BlockchainSelectionModel(
    const api::session::Client& api,
    const ui::Blockchains type,
    const SimpleCallback& cb) noexcept
    -> std::unique_ptr<ui::internal::BlockchainSelection>
{
    using ReturnType = ui::implementation::BlockchainSelection;

    return std::make_unique<ReturnType>(api, type, cb);
}
auto BlockchainSelectionModel(
    const api::session::Client& api,
    const std::vector<opentxs::blockchain::Type>& chains,
    const SimpleCallback& cb) noexcept
    -> std::unique_ptr<ui::internal::BlockchainSelection>
{
    using ReturnType = ui::implementation::BlockchainSelection;

    return std::make_unique<ReturnType>(api, chains, cb);
}

}  // namespace opentxs::factory

namespace opentxs::ui::implementation
{
auto BlockchainSelection::to_str(Work value) -> std::string
{
    static auto Map = std::map<Work, std::string>{
        {Work::shutdown, "shutdown"},
        {Work::statechange, "statechange"},
        {Work::enable, "enable"},
        {Work::disable, "disable"},
        {Work::init, "init"},
        {Work::statemachine, "statemachine"}};
    auto i = Map.find(value);
    return i == Map.end() ? std::string{"???"} : i->second;
}

BlockchainSelection::BlockchainSelection(
    const api::session::Client& api,
    const ui::Blockchains type,
    const SimpleCallback& cb) noexcept
    : BlockchainSelectionList(api, Identifier::Factory(), cb, false)
    , Worker(api, "BlockchainSelection")
    , filter_(filter(type))
    , chain_state_([&] {
        auto out = UnallocatedMap<blockchain::Type, bool>{};

        for (const auto chain : filter_) { out[chain] = false; }

        return out;
    }())
    , enabled_count_(0)
    , enabled_callback_()
    , last_job_{}
{
    init_executor(
        {UnallocatedCString{api.Endpoints().BlockchainStateChange()}});
    start();
    pipeline_.Push(MakeWork(Work::init));
}

BlockchainSelection::BlockchainSelection(
    const api::session::Client& api,
    const std::vector<opentxs::blockchain::Type>& chains,
    const SimpleCallback& cb) noexcept
    : BlockchainSelectionList(api, Identifier::Factory(), cb, false)
    , Worker(api, "BlockchainSelection chains")
    , filter_(filter(chains))
    , chain_state_([&] {
        auto out = UnallocatedMap<blockchain::Type, bool>{};

        for (const auto chain : filter_) { out[chain] = false; }

        return out;
    }())
    , enabled_count_(0)
    , enabled_callback_()
    , last_job_{}
{
    init_executor(
        {UnallocatedCString{api.Endpoints().BlockchainStateChange()}});
    start();
    pipeline_.Push(MakeWork(Work::init));
}

auto BlockchainSelection::construct_row(
    const BlockchainSelectionRowID& id,
    const BlockchainSelectionSortKey& index,
    CustomData& custom) const noexcept -> RowPointer
{
    return factory::BlockchainSelectionItem(
        *this, Widget::api_, id, index, custom);
}

auto BlockchainSelection::Disable(const blockchain::Type type) const noexcept
    -> bool
{
    pipeline_.Push([&] {
        auto out = network::zeromq::tagged_message(Work::disable);
        out.AddFrame(type);

        return out;
    }());

    return true;
}

auto BlockchainSelection::disable(const Message& in) noexcept -> void
{
    const auto body = in.Body();

    OT_ASSERT(1 < body.size());

    const auto chain = body.at(1).as<blockchain::Type>();
    process_state(chain, false);
    Widget::api_.Network().Blockchain().Disable(chain);
}

auto BlockchainSelection::Enable(const blockchain::Type type) const noexcept
    -> bool
{
    pipeline_.Push([&] {
        auto out = network::zeromq::tagged_message(Work::enable);
        out.AddFrame(type);

        return out;
    }());

    return true;
}

auto BlockchainSelection::enable(const Message& in) noexcept -> void
{
    const auto body = in.Body();

    OT_ASSERT(1 < body.size());

    const auto chain = body.at(1).as<blockchain::Type>();
    process_state(chain, true);
    Widget::api_.Network().Blockchain().Enable(chain);
}

auto BlockchainSelection::EnabledCount() const noexcept -> std::size_t
{
    return enabled_count_.load();
}

auto BlockchainSelection::EnabledChains() const noexcept
    -> UnallocatedSet<blockchain::Type>
{
    return Widget::api_.Network().Blockchain().EnabledChains();
}

auto BlockchainSelection::filter(const std::vector<opentxs::blockchain::Type>& chains) noexcept
    -> UnallocatedSet<blockchain::Type>
{
    auto complete = blockchain::SupportedChains();
    auto output = decltype(complete){};

    for (const auto& chain : complete) 
    {
        if(std::find(std::begin(chains), std::end(chains), chain)!=std::end(chains))
            output.emplace(chain);
    }            
    return output;
}

auto BlockchainSelection::filter(const ui::Blockchains type) noexcept
    -> UnallocatedSet<blockchain::Type>
{
    auto complete = blockchain::SupportedChains();

    switch (type) {
        case Blockchains::Main: {
            auto output = decltype(complete){};

            for (const auto& chain : complete) {
                if (false == blockchain::IsTestnet(chain)) {
                    output.emplace(chain);
                }
            }

            return output;
        }
        case Blockchains::Test: {
            auto output = decltype(complete){};

            for (const auto& chain : complete) {
                if (blockchain::IsTestnet(chain)) { output.emplace(chain); }
            }

            return output;
        }
        case Blockchains::All:
        default: {

            return complete;
        }
    }
}

auto BlockchainSelection::pipeline(Message&& in) noexcept -> void
{
    if (false == running_.load()) { return; }

    const auto body = in.Body();

    if (1 > body.size()) {
        LogError()(OT_PRETTY_CLASS())("Invalid message").Flush();

        OT_FAIL;
    }

    const auto work = [&] {
        try {

            return body.at(0).as<Work>();
        } catch (...) {

            OT_FAIL;
        }
    }();
    last_job_ = work;

    switch (work) {
        case Work::shutdown: {
            protect_shutdown([this] { shut_down(); });
        } break;
        case Work::statechange: {
            process_state(in);
        } break;
        case Work::enable: {
            enable(in);
        } break;
        case Work::disable: {
            disable(in);
        } break;
        case Work::init: {
            startup();
        } break;
        case Work::statemachine: {
            do_work();
        } break;
        default: {
            LogError()(OT_PRETTY_CLASS())("Unhandled type: ")(
                static_cast<OTZMQWorkType>(work))
                .Flush();

            OT_FAIL;
        }
    }
}

auto BlockchainSelection::state_machine() noexcept -> int { return SM_off; }

auto BlockchainSelection::shut_down() noexcept -> void
{
    close_pipeline();
    // TODO MT-34 investigate what other actions might be needed
}

auto BlockchainSelection::process_state(const Message& in) noexcept -> void
{
    const auto body = in.Body();

    OT_ASSERT(2 < body.size());

    process_state(body.at(1).as<blockchain::Type>(), body.at(2).as<bool>());
}

auto BlockchainSelection::process_state(
    const blockchain::Type chain,
    const bool enabled) const noexcept -> void
{
    if (0 == filter_.count(chain)) { return; }

    auto& isEnabled = chain_state_.at(chain);

    if (isEnabled) {
        if (enabled) {
            // Do nothing
        } else {
            isEnabled = false;
            --enabled_count_;
            enabled_callback_.run(chain, enabled, enabled_count_.load());
        }
    } else {
        if (enabled) {
            isEnabled = true;
            ++enabled_count_;
            enabled_callback_.run(chain, enabled, enabled_count_.load());
        } else {
            // Do nothing
        }
    }

    auto custom = CustomData{};
    custom.emplace_back(new bool{enabled});
    const_cast<BlockchainSelection&>(*this).add_item(
        chain,
        {UnallocatedCString{print(chain)}, blockchain::IsTestnet(chain)},
        custom);
}

auto BlockchainSelection::Set(EnabledCallback&& cb) const noexcept -> void
{
    enabled_callback_.set(std::move(cb));
}

auto BlockchainSelection::startup() noexcept -> void
{
    allow_command_processing();
    const auto& api = Widget::api_.Network().Blockchain().Internal();

    for (const auto& chain : filter_) {
        process_state(chain, api.IsEnabled(chain));
    }

    finish_startup();
}

auto BlockchainSelection::last_job_str() const noexcept -> std::string
{
    return std::string{to_str(last_job_)};
}

BlockchainSelection::~BlockchainSelection()
{
    wait_for_startup();
    protect_shutdown([this] { shut_down(); });
}
}  // namespace opentxs::ui::implementation
