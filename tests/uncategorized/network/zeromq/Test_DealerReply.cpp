// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <gtest/gtest.h>
#include <opentxs/opentxs.hpp>
#include <chrono>
#include <ctime>
#include <string_view>
#include <thread>
#include <utility>

namespace ot = opentxs;
namespace zmq = ot::network::zeromq;

namespace ottest
{
using namespace std::literals::chrono_literals;

class Test_DealerReply : public ::testing::Test
{
public:
    const zmq::Context& context_;

    const ot::UnallocatedCString testMessage_{"zeromq test message"};
    const ot::UnallocatedCString testMessage2_{"zeromq test message 2"};
    const ot::UnallocatedCString testMessage3_{"zeromq test message 3"};

    const ot::UnallocatedCString endpoint_{
        "inproc://opentxs/test/dealer_reply_test"};

    void dealerSocketThread(const ot::UnallocatedCString& msg);

    Test_DealerReply()
        : context_(ot::Context().ZMQ())
    {
    }
};

void Test_DealerReply::dealerSocketThread(const ot::UnallocatedCString& msg)
{
    bool replyProcessed{false};
    auto listenCallback = zmq::ListenCallback::Factory(
        [this, &replyProcessed](zmq::Message&& input) -> void {
            const auto inputString =
                ot::UnallocatedCString{input.Body().begin()->Bytes()};
            bool match =
                inputString == testMessage2_ || inputString == testMessage3_;
            EXPECT_TRUE(match);

            replyProcessed = true;
        });

    ASSERT_NE(nullptr, &listenCallback.get());

    auto dealerSocket =
        context_.DealerSocket(listenCallback, zmq::socket::Direction::Connect);

    ASSERT_NE(nullptr, &dealerSocket.get());
    ASSERT_EQ(zmq::socket::Type::Dealer, dealerSocket->Type());

    dealerSocket->SetTimeouts(0ms, -1ms, 30000ms);
    dealerSocket->Start(endpoint_);

    auto message = opentxs::network::zeromq::Message{};
    message.StartBody();
    message.AddFrame(msg);
    auto sent = dealerSocket->Send(std::move(message));

    ASSERT_TRUE(sent);

    auto end = std::time(nullptr) + 5;
    while (!replyProcessed && std::time(nullptr) < end) { ot::Sleep(100ms); }

    EXPECT_TRUE(replyProcessed);
}

TEST_F(Test_DealerReply, Dealer_Reply)
{
    bool replyReturned{false};
    auto replyCallback = zmq::ReplyCallback::Factory(
        [this,
         &replyReturned](zmq::Message&& input) -> ot::network::zeromq::Message {
            EXPECT_EQ(1, input.size());
            EXPECT_EQ(input.Header().size(), 0);
            EXPECT_EQ(1, input.Body().size());

            const auto inputString =
                ot::UnallocatedCString{input.Body().begin()->Bytes()};

            EXPECT_EQ(testMessage_, inputString);

            auto reply = ot::network::zeromq::reply_to_message(input);

            EXPECT_EQ(reply.size(), 0);
            EXPECT_EQ(reply.Header().size(), 0);
            EXPECT_EQ(reply.Body().size(), 0);

            reply.AddFrame(inputString);

            EXPECT_EQ(1, reply.size());
            EXPECT_EQ(reply.Header().size(), 0);
            EXPECT_EQ(1, reply.Body().size());
            EXPECT_EQ(
                inputString, ot::UnallocatedCString{reply.Body_at(0).Bytes()});

            replyReturned = true;

            return reply;
        });

    ASSERT_NE(nullptr, &replyCallback.get());

    auto replySocket =
        context_.ReplySocket(replyCallback, zmq::socket::Direction::Bind);

    ASSERT_NE(nullptr, &replySocket.get());
    ASSERT_EQ(zmq::socket::Type::Reply, replySocket->Type());

    replySocket->SetTimeouts(0ms, 30000ms, -1ms);
    replySocket->Start(endpoint_);

    bool replyProcessed{false};

    auto dealerCallback = zmq::ListenCallback::Factory(
        [this, &replyProcessed](zmq::Message&& input) -> void {
            EXPECT_EQ(2, input.size());
            EXPECT_EQ(input.Header().size(), 0);
            EXPECT_EQ(1, input.Body().size());

            const auto inputString =
                ot::UnallocatedCString{input.Body().begin()->Bytes()};

            EXPECT_EQ(testMessage_, inputString);

            replyProcessed = true;
        });

    ASSERT_NE(nullptr, &dealerCallback.get());

    auto dealerSocket =
        context_.DealerSocket(dealerCallback, zmq::socket::Direction::Connect);

    ASSERT_NE(nullptr, &dealerSocket.get());
    ASSERT_EQ(zmq::socket::Type::Dealer, dealerSocket->Type());

    dealerSocket->SetTimeouts(0ms, -1ms, 30000ms);
    dealerSocket->Start(endpoint_);

    auto message = opentxs::network::zeromq::Message{};
    message.StartBody();
    message.AddFrame(testMessage_);

    ASSERT_TRUE(2 == message.size());
    ASSERT_TRUE(0 == message.Header().size());
    ASSERT_TRUE(1 == message.Body().size());
    ASSERT_EQ(testMessage_, ot::UnallocatedCString{message.Body_at(0).Bytes()});
    ASSERT_EQ(testMessage_, ot::UnallocatedCString{message.at(1).Bytes()});

    auto sent = dealerSocket->Send(std::move(message));

    ASSERT_TRUE(sent);

    auto end = std::time(nullptr) + 5;
    while (!replyReturned && std::time(nullptr) < end) { ot::Sleep(100ms); }

    EXPECT_TRUE(replyReturned);

    end = std::time(nullptr) + 5;
    while (!replyProcessed && std::time(nullptr) < end) { ot::Sleep(100ms); }

    EXPECT_TRUE(replyProcessed);
}

TEST_F(Test_DealerReply, Dealer_2_Reply_1)
{
    auto replyCallback = zmq::ReplyCallback::Factory(
        [this](zmq::Message&& input) -> ot::network::zeromq::Message {
            const auto inputString =
                ot::UnallocatedCString{input.Body().begin()->Bytes()};
            bool match =
                inputString == testMessage2_ || inputString == testMessage3_;
            EXPECT_TRUE(match);

            auto reply = ot::network::zeromq::reply_to_message(input);
            reply.AddFrame(inputString);
            return reply;
        });

    ASSERT_NE(nullptr, &replyCallback.get());

    auto replySocket =
        context_.ReplySocket(replyCallback, zmq::socket::Direction::Bind);

    ASSERT_NE(nullptr, &replySocket.get());
    ASSERT_EQ(zmq::socket::Type::Reply, replySocket->Type());

    replySocket->SetTimeouts(0ms, 30000ms, -1ms);
    replySocket->Start(endpoint_);

    std::thread dealerSocketThread1(
        &Test_DealerReply::dealerSocketThread, this, testMessage2_);
    std::thread dealerSocketThread2(
        &Test_DealerReply::dealerSocketThread, this, testMessage3_);

    dealerSocketThread1.join();
    dealerSocketThread2.join();
}

TEST_F(Test_DealerReply, Dealer_Reply_Multipart)
{
    bool replyReturned{false};
    auto replyCallback = zmq::ReplyCallback::Factory(
        [this,
         &replyReturned](zmq::Message&& input) -> ot::network::zeromq::Message {
            // ReplySocket removes the delimiter frame.
            EXPECT_EQ(4, input.size());
            EXPECT_EQ(1, input.Header().size());
            EXPECT_EQ(2, input.Body().size());

            for (const auto& frame : input.Header()) {
                EXPECT_EQ(testMessage_, frame.Bytes());
            }

            for (const auto& frame : input.Body()) {
                bool match = frame.Bytes() == testMessage2_ ||
                             frame.Bytes() == testMessage3_;

                EXPECT_TRUE(match);
            }

            auto reply = ot::network::zeromq::reply_to_message(input);
            for (const auto& frame : input.Body()) { reply.AddFrame(frame); }
            replyReturned = true;
            return reply;
        });

    ASSERT_NE(nullptr, &replyCallback.get());

    auto replySocket =
        context_.ReplySocket(replyCallback, zmq::socket::Direction::Bind);

    ASSERT_NE(nullptr, &replySocket.get());
    ASSERT_EQ(zmq::socket::Type::Reply, replySocket->Type());

    replySocket->SetTimeouts(0ms, 30000ms, -1ms);
    replySocket->Start(endpoint_);

    bool replyProcessed{false};

    auto dealerCallback = zmq::ListenCallback::Factory(
        [this, &replyProcessed](zmq::Message&& input) -> void {
            // ReplySocket puts the delimiter frame back when it sends the
            // reply.
            ASSERT_EQ(5, input.size());
            ASSERT_EQ(input.Header().size(), 0);
            ASSERT_EQ(4, input.Body().size());

            const auto header = ot::UnallocatedCString{input.at(1).Bytes()};

            ASSERT_EQ(testMessage_, header);

            for (auto i{3u}; i < input.size(); ++i) {
                const auto frame = input.at(i).Bytes();
                bool match = frame == testMessage2_ || frame == testMessage3_;

                EXPECT_TRUE(match);
            }

            replyProcessed = true;
        });

    ASSERT_NE(nullptr, &dealerCallback.get());

    auto dealerSocket =
        context_.DealerSocket(dealerCallback, zmq::socket::Direction::Connect);

    ASSERT_NE(nullptr, &dealerSocket.get());
    ASSERT_EQ(zmq::socket::Type::Dealer, dealerSocket->Type());

    dealerSocket->SetTimeouts(0ms, -1ms, 30000ms);
    dealerSocket->Start(endpoint_);

    auto multipartMessage = opentxs::network::zeromq::Message{};
    multipartMessage.StartBody();
    multipartMessage.AddFrame(testMessage_);
    multipartMessage.StartBody();
    multipartMessage.AddFrame(testMessage2_);
    multipartMessage.AddFrame(testMessage3_);

    auto sent = dealerSocket->Send(std::move(multipartMessage));

    ASSERT_TRUE(sent);

    auto end = std::time(nullptr) + 5;
    while (!replyReturned && std::time(nullptr) < end) { ot::Sleep(100ms); }

    EXPECT_TRUE(replyReturned);

    end = std::time(nullptr) + 5;
    while (!replyProcessed && std::time(nullptr) < end) { ot::Sleep(100ms); }

    EXPECT_TRUE(replyProcessed);
}
}  // namespace ottest
