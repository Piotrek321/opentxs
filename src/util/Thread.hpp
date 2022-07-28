// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <string_view>

namespace opentxs
{
const int MAX_THREAD_NAME_SIZE{15};
enum class ThreadPriority {
    Idle,
    Lowest,
    BelowNormal,
    Normal,
    AboveNormal,
    Highest,
    TimeCritical,
};

auto print(ThreadPriority priority) noexcept -> const char*;
auto SetThisThreadsName(std::string_view threadName) noexcept -> void;
auto SetThisThreadsPriority(ThreadPriority priority) noexcept -> void;

auto adjustThreadName(
    std::string_view threadName,
    std::string&& appender) noexcept -> std::string;

constexpr std::string_view loggerThreadName{"Logger\0"};
static_assert(
    loggerThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view periodicThreadName{"Periodic\0"};
static_assert(
    periodicThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view balanceOracleThreadName{"BalOrcl\0"};
static_assert(
    balanceOracleThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view dhtNymThreadName{"DHTnym\0"};
static_assert(
    dhtNymThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view dhtServerThreadName{"DHTsrv\0"};
static_assert(
    dhtServerThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view dhtUnitThreadName{"DHTunt\0"};
static_assert(
    dhtUnitThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view zapThreadName{"ZAP\0"};
static_assert(zapThreadName.size() <= MAX_THREAD_NAME_SIZE, "name is too long");

constexpr std::string_view asioThreadStartThreadName{"AsioThrStart\0"};
static_assert(
    asioThreadStartThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view blockchainThreadName{"Blockchain\0"};
static_assert(
    blockchainThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view startupPublisherThreadName{"StartupPublish\0"};
static_assert(
    startupPublisherThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view updateManagerThreadName{"UpdateManager\0"};
static_assert(
    updateManagerThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view contactsThreadName{"Contacts\0"};
static_assert(
    contactsThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view OTXAccountThreadName{"OTX_account\0"};
static_assert(
    OTXAccountThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view OTXNotificationListenerThreadName{"OTXNtfnLstnr\0"};
static_assert(
    OTXNotificationListenerThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view OTXNymListenerThreadName{"OTXNymLstnr\0"};
static_assert(
    OTXNymListenerThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view OTXServerListenerThreadName{"OTXServerLstnr\0"};
static_assert(
    OTXServerListenerThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view OTXUnitListenerThreadName{"OTXUnitLstnr\0"};
static_assert(
    OTXUnitListenerThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view storageNymsThreadName{"Storage_nyms\0"};
static_assert(
    storageNymsThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view storageServersThreadName{"Storage_servers\0"};
static_assert(
    storageServersThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view storageUnitsThreadName{"Storage_units\0"};
static_assert(
    storageUnitsThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view walletThreadName{"Wallet\0"};
static_assert(
    walletThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view mailCacheThreadName{"MailCache\0"};
static_assert(
    mailCacheThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view schedulerPublishNymThreadName{"SchdrPublishNym\0"};
static_assert(
    schedulerPublishNymThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view schedulerRefreshNymThreadName{"SchdrRefreshNym\0"};
static_assert(
    schedulerRefreshNymThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view schedulerPublishServerThreadName{
    "SchdrPublishSvr\0"};
static_assert(
    schedulerPublishServerThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view schedulerRefreshServerThreadName{
    "SchdrRefreshSvr\0"};
static_assert(
    schedulerRefreshServerThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view schedulerPublishUnitThreadName{"SchdrPblishUnt\0"};
static_assert(
    schedulerPublishUnitThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view schedulerRefreshUnitThreadName{"SchdrRefshUnt\0"};
static_assert(
    schedulerRefreshUnitThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view schedulerThreadName{"Scheduler\0"};
static_assert(
    schedulerThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view notaryMintThreadName{"NotaryMint\0"};
static_assert(
    notaryMintThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view subchainThreadName{"Subchain\0"};
static_assert(
    subchainThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view subchainStateDataFilterThreadName{"SSDFilter\0"};
static_assert(
    subchainStateDataFilterThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view subchainStateDataPrehash1ThreadName{"SSDPrehash1\0"};
static_assert(
    subchainStateDataPrehash1ThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view subchainStateDataPrehash2ThreadName{"SSDPrehash2\0"};
static_assert(
    subchainStateDataPrehash2ThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view blockchainSyncThreadName{"BlockchainSync\0"};
static_assert(
    blockchainSyncThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view ZMQIncomingExternalThreadName{"ZMQIncomExt\0"};
static_assert(
    ZMQIncomingExternalThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view ZMQIncomingInternalThreadName{"ZMQIncomInt\0"};
static_assert(
    ZMQIncomingInternalThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view processBlockThreadName{"ProcessBlock\0"};
static_assert(
    processBlockThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view shutdownReceiverThreadName{"ShutdownRcvr\0"};
static_assert(
    shutdownReceiverThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view messageProcessorThreadName{"MsgProcessor\0"};
static_assert(
    messageProcessorThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view workerThreadName{"Worker\0"};
static_assert(
    workerThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view sendMonitorThreadName{"SendMonitor\0"};
static_assert(
    sendMonitorThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view RPCPushReceiverThreadName{"RPCPushReceiver\0"};
static_assert(
    RPCPushReceiverThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view RPCTaskThreadName{"RPCTask\0"};
static_assert(
    RPCTaskThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view blockchainAccountActivityThreadName{
    "BCAccountActv\0"};
static_assert(
    blockchainAccountActivityThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view widgetThreadName{"Widget\0"};
static_assert(
    widgetThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view unitListThreadName{"UnitList\0"};
static_assert(
    unitListThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view serverConnectionAsyncThreadName{"ServerConnAsync\0"};
static_assert(
    serverConnectionAsyncThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view serverConnectionRegistrationThreadName{
    "SvrConnRegistr\0"};
static_assert(
    serverConnectionRegistrationThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view asioTransmitThreadName{"AsioTransmit\0"};
static_assert(
        asioTransmitThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view P2PClientThreadName{"P2PClient\0"};
static_assert(
    P2PClientThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view P2PServerThreadName{"P2PServer\0"};
static_assert(
    P2PServerThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view pairEventListenerThreadName{"PairEventLsnr\0"};
static_assert(
    pairEventListenerThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view proxyListenerThreadName{"ProxyListener\0"};
static_assert(
    proxyListenerThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view proxySenderThreadName{"ProxySender\0"};
static_assert(
    proxySenderThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view proxyThreadName{"Proxy\0"};
static_assert(
    proxyThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view bidirectionalThreadName{"Bidirectional\0"};
static_assert(
    bidirectionalThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view dealerThreadName{"Dealer\0"};
static_assert(
    dealerThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view replyThreadName{"Reply\0"};
static_assert(
    replyThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view pullThreadName{"Pull\0"};
static_assert(
    pullThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view pairThreadName{"Pair\0"};
static_assert(
    pairThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view receiverThreadName{"Receiver\0"};
static_assert(
    receiverThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view routerThreadName{"Router\0"};
static_assert(
    routerThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view subscribeThreadName{"Subscribe\0"};
static_assert(
    subscribeThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view handlerThreadName{"Handler\0"};
static_assert(
    handlerThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view pairNymThreadName{"PairNym\0"};
static_assert(
    pairNymThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view pairReplyThreadName{"PairReply\0"};
static_assert(
    pairNymThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view pairRequestThreadName{"PairRequest\0"};
static_assert(
    pairNymThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view messageProcessorFrontendThreadName{
    "MsgPrcsrFront\0"};
static_assert(
    messageProcessorFrontendThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view storeThreadName{"Store\0"};
static_assert(
    storeThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view garbageCollectedThreadName{"GarbCollecd\0"};
static_assert(
    garbageCollectedThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view storageGcThreadName{"StorageGc\0"};
static_assert(
    storageGcThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view generalThreadName{"General\0"};
static_assert(
    generalThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view networkThreadName{"Network\0"};
static_assert(
    networkThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view storageThreadName{"Storage\0"};
static_assert(
    storageThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view unknownThreadPoolTypeThreadName{"UnknownThdPool\0"};
static_assert(
    unknownThreadPoolTypeThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

constexpr std::string_view asioDataThreadName{"AsioData\0"};
static_assert(
    asioDataThreadName.size() <= MAX_THREAD_NAME_SIZE,
    "name is too long");

}  // namespace opentxs
