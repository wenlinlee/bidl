// Copyright 2018 VMware, all rights reserved

/**
 * @file RocksDBClient.h
 *
 *  @brief Header file containing the RocksDBClientIterator and RocksDBClient
 *  class definitions.
 *
 *  Objects of RocksDBClientIterator contain an iterator for database along with
 *  a pointer to the client object.
 *
 *  Objects of RocksDBClient signify connections with RocksDB database. They
 *  contain variables for storing the database directory path, connection object
 *  and an optional comparator.
 *
 */

#pragma once

#ifdef USE_ROCKSDB
#include "Logger.hpp"
#include <rocksdb/db.h>
#include <rocksdb/utilities/transaction_db.h>
#include <rocksdb/sst_file_manager.h>
#include "storage/db_interface.h"
#include "storage/storage_metrics.h"

#include <map>
#include <optional>
#include <vector>

namespace concord {
namespace storage {
namespace rocksdb {

class Client;

class ClientIterator : public concord::storage::IDBClient::IDBClientIterator {
  friend class Client;

 public:
  ClientIterator(const Client* _parentClient, logging::Logger);
  ~ClientIterator() { delete m_iter; }

  // Inherited via IDBClientIterator
  KeyValuePair first() override;
  KeyValuePair last() override;
  KeyValuePair seekAtLeast(const concordUtils::Sliver& _searchKey) override;
  KeyValuePair seekAtMost(const concordUtils::Sliver& _searchKey) override;
  KeyValuePair previous() override;
  KeyValuePair next() override;
  KeyValuePair getCurrent() override;
  bool isEnd() override;
  concordUtils::Status getStatus() override;

 private:
  logging::Logger logger;

  ::rocksdb::Iterator* m_iter;

  // Reference to the RocksDBClient
  const Client* m_parentClient;

  concordUtils::Status m_status;
};

class Client : public concord::storage::IDBClient {
 public:
  Client(std::string _dbPath) : m_dbPath(_dbPath) {}
  Client(std::string _dbPath, std::unique_ptr<const ::rocksdb::Comparator>&& comparator)
      : m_dbPath(_dbPath), comparator_(std::move(comparator)) {}

  ~Client() {
    // Clear column family handles before the DB as handle destruction calls a DB instance member and we want that to
    // happen before we delete the DB pointer.
    cf_handles_.clear();
    if (txn_db_) {
      // If we're using a TransactionDB, it wraps the base DB, so release it
      // instead of releasing the base DB.
      dbInstance_.release();
      delete txn_db_;
    }
  }

  void init(bool readOnly = false) override;
  concordUtils::Status get(const concordUtils::Sliver& _key, concordUtils::Sliver& _outValue) const override;
  concordUtils::Status get(const concordUtils::Sliver& _key,
                           char*& buf,
                           uint32_t bufSize,
                           uint32_t& _realSize) const override;
  concordUtils::Status has(const Sliver& _key) const override;
  IDBClientIterator* getIterator() const override;
  concordUtils::Status freeIterator(IDBClientIterator* _iter) const override;
  concordUtils::Status put(const concordUtils::Sliver& _key, const concordUtils::Sliver& _value) override;
  concordUtils::Status del(const concordUtils::Sliver& _key) override;
  concordUtils::Status multiGet(const KeysVector& _keysVec, ValuesVector& _valuesVec) override;
  concordUtils::Status multiPut(const SetOfKeyValuePairs& _keyValueMap) override;
  concordUtils::Status multiDel(const KeysVector& _keysVec) override;
  concordUtils::Status rangeDel(const Sliver& _beginKey, const Sliver& _endKey) override;
  ::rocksdb::Iterator* getNewRocksDbIterator() const;
  bool isNew() override;
  ITransaction* beginTransaction() override;
  void setAggregator(std::shared_ptr<concordMetrics::Aggregator> aggregator) override {
    storage_metrics_.setAggregator(aggregator);
  }

  static logging::Logger& logger() {
    static logging::Logger logger_ = logging::getLogger("concord.storage.rocksdb");
    return logger_;
  }

 private:
  struct Options {
    ::rocksdb::Options db_options;
    ::rocksdb::TransactionDBOptions txn_options;

    void applyOptimizations();
  };

  // Initialize a DB.
  // If Options are provided, use them as is.
  // If Options are not provided, try to load them from an options file.
  // If `applyOptimizations` is set, apply optimizations on top of the provided or the loaded ones.
  void initDB(bool readOnly, const std::optional<Options>&, bool applyOptimizations);
  concordUtils::Status launchBatchJob(::rocksdb::WriteBatch& _batchJob);
  concordUtils::Status get(const concordUtils::Sliver& _key, std::string& _value) const;
  bool keyIsBefore(const concordUtils::Sliver& _lhs, const concordUtils::Sliver& _rhs) const;

  // Column family unique pointers that are managed solely by Client. This allows us to use a raw
  // pointer in CfDeleter as we know the column family unique pointer will not be moved out of Client.
  struct CfDeleter {
    void operator()(::rocksdb::ColumnFamilyHandle* h) const noexcept {
      if (!client_->dbInstance_->DestroyColumnFamilyHandle(h).ok()) {
        std::terminate();
      }
    }
    Client* client_{nullptr};
  };
  using CfUniquePtr = std::unique_ptr<::rocksdb::ColumnFamilyHandle, CfDeleter>;

  // Guard against double init.
  bool initialized_{false};

  // Database path on directory (used for connection).
  std::string m_dbPath;

  std::string default_opt_config_name = "OPTIONS_DEFAULT.ini";

  // Database object (created on connection).
  std::unique_ptr<::rocksdb::DB> dbInstance_;
  ::rocksdb::TransactionDB* txn_db_ = nullptr;
  std::unique_ptr<const ::rocksdb::Comparator> comparator_;
  std::map<std::string, CfUniquePtr> cf_handles_;

  // Metrics
  mutable RocksDbStorageMetrics storage_metrics_;

  friend class NativeClient;
};

::rocksdb::Slice toRocksdbSlice(const concordUtils::Sliver& _s);
concordUtils::Sliver fromRocksdbSlice(::rocksdb::Slice _s);
concordUtils::Sliver copyRocksdbSlice(::rocksdb::Slice _s);

}  // namespace rocksdb
}  // namespace storage
}  // namespace concord
#endif  // USE_ROCKSDB
