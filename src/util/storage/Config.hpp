// Copyright (c) 2010-2022 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <chrono>
#include <cstdint>
#include <functional>

#include "opentxs/util/Container.hpp"

#define OT_STORAGE_PRIMARY_PLUGIN_SQLITE "sqlite"
#define OT_STORAGE_PRIMARY_PLUGIN_LMDB "lmdb"
#define OT_STORAGE_PRIMARY_PLUGIN_MEMDB "mem"
#define OT_STORAGE_PRIMARY_PLUGIN_FS "fs"
#define STORAGE_CONFIG_PRIMARY_PLUGIN_KEY "primary_plugin"
#define STORAGE_CONFIG_FS_BACKUP_DIRECTORY_KEY "fs_backup_directory"
#define STORAGE_CONFIG_FS_ENCRYPTED_BACKUP_DIRECTORY_KEY "fs_encrypted_backup"

namespace opentxs
{
namespace api
{
class Legacy;
class Settings;
}  // namespace api

class Options;
class String;
}  // namespace opentxs

namespace opentxs::storage
{
using InsertCB =
    std::function<void(const UnallocatedCString&, const UnallocatedCString&)>;

class Config
{
public:
    static const UnallocatedCString default_plugin_;

    UnallocatedCString previous_primary_plugin_;
    UnallocatedCString primary_plugin_;
    bool migrate_plugin_;

    bool auto_publish_nyms_;
    bool auto_publish_servers_;
    bool auto_publish_units_;
    std::int64_t gc_interval_;
    UnallocatedCString path_;
    InsertCB dht_callback_;

    UnallocatedCString fs_primary_bucket_;
    UnallocatedCString fs_secondary_bucket_;
    UnallocatedCString fs_root_file_;
    UnallocatedCString fs_backup_directory_;
    UnallocatedCString fs_encrypted_backup_directory_;

    UnallocatedCString sqlite3_primary_bucket_;
    UnallocatedCString sqlite3_secondary_bucket_;
    UnallocatedCString sqlite3_control_table_;
    UnallocatedCString sqlite3_root_key_;
    UnallocatedCString sqlite3_db_file_;

    UnallocatedCString lmdb_primary_bucket_;
    UnallocatedCString lmdb_secondary_bucket_;
    UnallocatedCString lmdb_control_table_;
    UnallocatedCString lmdb_root_key_;

    Config(
        const api::Legacy& legacy,
        const api::Settings& options,
        const Options& cli,
        const String& dataFolder) noexcept;
};
}  // namespace opentxs::storage