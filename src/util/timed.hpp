// IWYU pragma: no_include "opentxs/network/zeromq/socket/SocketType.hpp"

#pragma once

#include <chrono>

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace opentxs  // NOLINT
{

template <typename M, typename CLK>
struct Timed {
    M m_;
    std::chrono::time_point<CLK> t_scheduled_;
};

template <typename M, typename CLK>
struct TimedBefore {
    bool operator()(const Timed<M, CLK>& left, const Timed<M, CLK>& right)
    {
        return left.t_scheduled_ > right.t_scheduled_;
    }
};

}  // namespace opentxs
// NOLINTEND(modernize-concat-nested-namespaces)
