// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "0_stdafx.hpp"    // IWYU pragma: associated
#include "1_Internal.hpp"  // IWYU pragma: associated
#include "blockchain/node/wallet/feeoracle/FeeSource.hpp"  // IWYU pragma: associated
#include "internal/blockchain/node/wallet/Factory.hpp"  // IWYU pragma: associated

#include <boost/system/error_code.hpp>
#include <exception>
#include <utility>

#include "internal/api/network/Asio.hpp"
#include "internal/network/zeromq/socket/Factory.hpp"
#include "internal/util/LogMacros.hpp"
#include "opentxs/api/network/Asio.hpp"
#include "opentxs/api/session/Session.hpp"
#include "opentxs/blockchain/BlockchainType.hpp"
#include "opentxs/core/Amount.hpp"
#include "opentxs/core/display/Scale.hpp"
#include "opentxs/network/zeromq/Pipeline.hpp"
#include "opentxs/network/zeromq/ZeroMQ.hpp"
#include "opentxs/network/zeromq/message/FrameSection.hpp"
#include "opentxs/network/zeromq/message/Message.hpp"
#include "opentxs/network/zeromq/socket/SocketType.hpp"
#include "opentxs/util/Log.hpp"
#include "opentxs/util/Options.hpp"

namespace opentxs::factory
{
auto FeeSources(
    const api::Session& api,
    const blockchain::Type chain,
    const std::string_view endpoint,
    alloc::Resource* mr) noexcept
    -> ForwardList<blockchain::node::wallet::FeeSource>
{
    if (nullptr == mr) { mr = alloc::System(); }

    using ReturnType = blockchain::node::wallet::FeeSource::Imp;
    auto alloc = ReturnType::allocator_type{mr};
    auto out = ForwardList<blockchain::node::wallet::FeeSource>{alloc};

    if (api.GetOptions().TestMode()) { return out; }

    switch (chain) {
        case blockchain::Type::Bitcoin: {

            return BTCFeeSources(api, endpoint, mr);
        }
        default: {

            return out;
        }
    }
}
}  // namespace opentxs::factory

namespace opentxs::blockchain::node::wallet
{
const display::Scale FeeSource::Imp::scale_{"", "", {{10, 0}}, 0, 3};

FeeSource::Imp::Imp(
    const api::Session& api,
    CString endpoint,
    CString hostname,
    CString path,
    bool https,
    allocator_type&& alloc) noexcept
    : Allocated(std::move(alloc))
    , Worker(api, "FeeSource")
    , hostname_(std::move(hostname))
    , path_(std::move(path))
    , https_(https)
    , asio_(network::zeromq::MakeArbitraryInproc(allocator_.resource()))
    , rd_()
    , eng_(rd_())
    , dist_(-60, 60)
    , to_oracle_([&] {
        auto out = factory::ZMQSocket(
            api_.Network().ZeroMQ(),
            opentxs::network::zeromq::socket::Type::Publish);
        const auto rc = out.Connect(endpoint.c_str());

        OT_ASSERT(rc);

        return out;
    }())
    , future_(std::nullopt)
    , timer_(api.Network().Asio().Internal().GetTimer())
    , last_job_{}
{
    pipeline_.BindSubscriber(asio_, [](auto) { return MakeWork(Work::init); });
    start();
}

auto FeeSource::Imp::to_string(Work value) const noexcept -> const std::string&
{
    static const auto Map = std::map<Work, std::string>{
        {Work::shutdown, "shutdown"},
        {Work::query, "query"},
        {Work::init, "init"},
        {Work::statemachine, "statemachine"},
    };
    try {
        return Map.at(value);
    } catch (...) {
        LogError()(__FUNCTION__)("invalid FeeSource job: ")(
            static_cast<OTZMQWorkType>(value))
            .Flush();

        OT_FAIL;
    }
}

auto FeeSource::Imp::jitter() noexcept -> std::chrono::seconds
{
    return std::chrono::seconds{dist_(eng_)};
}

auto FeeSource::Imp::pipeline(network::zeromq::Message&& in) -> void
{
    if (!running_.load()) {
        protect_shutdown([this] { shut_down(); });
        return;
    }

    const auto body = in.Body();

    OT_ASSERT(0 < body.size());

    const auto work = body.at(0).as<Work>();
    last_job_ = work;

    switch (work) {
        case Work::shutdown: {
            protect_shutdown([this] { shut_down(); });
        } break;
        case Work::query: {
            query();
        } break;
        case Work::init: {
            startup();
        } break;
        case Work::statemachine: {
            do_work();
        } break;
        default: {
            LogError()(OT_PRETTY_CLASS())("unhandled type: ")(
                static_cast<OTZMQWorkType>(work))
                .Flush();

            OT_FAIL;
        }
    }
}

template <typename Rate, typename Scale>
std::optional<Amount> FeeSource::Imp::process_value(Rate rate, Scale scale)
    const noexcept
{
    auto value{static_cast<std::int64_t>(rate * scale)};

    LogTrace()(OT_PRETTY_CLASS())("obtained scaled amount ")(scale_.Format(
        value))(" from raw input ")(rate)(" and scale value ")(scale)
        .Flush();
    return (0 > value) ? std::nullopt : std::optional<Amount>(value);
}

auto FeeSource::Imp::process_double(double rate, unsigned long long int scale)
    const noexcept -> std::optional<Amount>
{
    return process_value(rate, static_cast<double>(scale));
}

auto FeeSource::Imp::process_int(
    std::int64_t rate,
    unsigned long long int scale) const noexcept -> std::optional<Amount>
{
    return process_value(rate, scale);
}

auto FeeSource::Imp::query() noexcept -> void
{
    future_ = api_.Network().Asio().Internal().FetchJson(
        hostname_, path_, https_, asio_);
}

auto FeeSource::Imp::reset_timer() noexcept -> void
{
    static constexpr auto interval = std::chrono::minutes{15};
    timer_.SetRelative(interval + jitter());
    timer_.Wait([this](const auto& error) {
        if (error) {
            if (boost::system::errc::operation_canceled != error.value()) {
                LogError()(OT_PRETTY_CLASS())(error).Flush();
            }
        } else {
            pipeline_.Push(MakeWork(Work::query));
        }
    });
}

auto FeeSource::Imp::startup() noexcept -> void
{
    query();
    reset_timer();
}

auto FeeSource::Imp::state_machine() noexcept -> int
{
    if (!future_.has_value()) { return -1; }

    auto& future = future_.value();
    static constexpr auto limit = 5ms;
    static constexpr auto ready = std::future_status::ready;

    try {
        if (const auto status = future.wait_for(limit); status == ready) {
            if (const auto data = process(future.get()); data.has_value()) {
                auto work = MakeWork(OT_ZMQ_INTERNAL_SIGNAL);
                data->Serialize(work.AppendBytes());
                to_oracle_.Send(std::move(work));
            }

            reset_timer();

            return -1;
        } else {
            LogError()(OT_PRETTY_CLASS())("Future is not ready").Flush();

            return 20;
        }
    } catch (const std::exception& e) {
        LogError()(e.what()).Flush();

        return -1;
    }
}

auto FeeSource::Imp::last_job_str() const noexcept -> std::string
{
    return to_string(last_job_);
}

auto FeeSource::Imp::Shutdown() noexcept -> void
{
    protect_shutdown([this] { shut_down(); });
}

auto FeeSource::Imp::shut_down() noexcept -> void
{
    timer_.Cancel();
    close_pipeline();
    // TODO MT-34 investigate what other actions might be needed
}

FeeSource::Imp::~Imp()
{
    protect_shutdown([this] { shut_down(); });
}
}  // namespace opentxs::blockchain::node::wallet

namespace opentxs::blockchain::node::wallet
{
FeeSource::FeeSource(Imp* imp) noexcept
    : imp_(imp)
{
    OT_ASSERT(nullptr != imp);
}

FeeSource::FeeSource(FeeSource&& rhs, allocator_type alloc) noexcept
    : imp_(rhs.imp_)
{
    rhs.imp_ = nullptr;

    OT_ASSERT(imp_->get_allocator() == alloc);
}

auto FeeSource::get_allocator() const noexcept -> allocator_type
{
    return imp_->get_allocator();
}

auto FeeSource::Shutdown() noexcept -> void { return imp_->Shutdown(); }

FeeSource::~FeeSource()
{
    if (nullptr != imp_) {
        auto alloc = imp_->get_allocator();
        // TODO c++20 use delete_object
        imp_->~Imp();
        alloc.resource()->deallocate(imp_, sizeof(Imp), alignof(Imp));
        imp_ = nullptr;
    }
}
}  // namespace opentxs::blockchain::node::wallet
