// Concord
//
// Copyright (c) 2018-2020 VMware, Inc. All Rights Reserved.
//
// This product is licensed to you under the Apache 2.0 license (the "License").
// You may not use this product except in compliance with the Apache 2.0
// License.
//
// This product may include a number of subcomponents with separate copyright
// notices and license terms. Your use of these subcomponents is subject to the
// terms and conditions of the subcomponent's license, as noted in the LICENSE
// file.

#pragma once

#include "assertUtils.hpp"
#include "OpenTracing.hpp"
#include "communication/CommFactory.hpp"
#include "Replica.hpp"
#include "ReplicaConfig.hpp"
#include "SimpleStateTransfer.hpp"
#include "FileStorage.hpp"
#include <thread>
#include "commonDefs.h"
#include "simple_test_replica_behavior.hpp"
#include "threshsign/IThresholdSigner.h"
#include "threshsign/IThresholdVerifier.h"

using namespace bftEngine;
using namespace bft::communication;
using namespace std;

logging::Logger replicaLogger = logging::getLogger("simpletest.replica");

#define test_assert_replica(statement, message)                          \
  {                                                                      \
    if (!(statement)) {                                                  \
      LOG_FATAL(replicaLogger, "assert fail with message: " << message); \
      ConcordAssert(false);                                              \
    }                                                                    \
  }


// The replica state machine.
class SimpleAppState : public IRequestsHandler {
 private:
  uint64_t client_to_index(NodeNum clientId) { return clientId - numOfReplicas; }

  uint64_t get_last_state_value(NodeNum clientId) {
    auto index = client_to_index(clientId);
    return statePtr[index].lastValue;
  }

  uint64_t get_last_state_num(NodeNum clientId) {
    auto index = client_to_index(clientId);
    return statePtr[index].stateNum;
  }

  void set_last_state_value(NodeNum clientId, uint64_t value) {
    auto index = client_to_index(clientId);
    statePtr[index].lastValue = value;
  }

  void set_last_state_num(NodeNum clientId, uint64_t value) {
    auto index = client_to_index(clientId);
    statePtr[index].stateNum = value;
  }

 public:
  std::shared_ptr<ControlHandlers> getControlHandlers() override { return nullptr; }

  SimpleAppState(uint16_t numCl, uint16_t numRep)
      : statePtr{new SimpleAppState::State[numCl]}, numOfClients{numCl}, numOfReplicas{numRep} {}
  ~SimpleAppState() { delete[] statePtr; }

  // Handler for the upcall from Concord-BFT.
  int execute(uint16_t clientId,
              uint64_t sequenceNum,
              uint8_t flags,
              uint32_t requestSize,
              const char *request,
              uint32_t maxReplySize,
              char *outReply,
              uint32_t &outActualReplySize,
              uint32_t &outActualReplicaSpecificInfoSize,
              concordUtils::SpanWrapper &span) override {
    // Not currently used
    outActualReplicaSpecificInfoSize = 0;

    bool readOnly = flags & READ_ONLY_FLAG;
    if (readOnly) {
      // Our read-only request includes only a type, no argument.
#ifndef SBFT_PAYLOAD_SIZE
      test_assert_replica(requestSize == sizeof(uint64_t), "requestSize =! " << sizeof(uint64_t));

      // We only support the READ operation in read-only mode.
      test_assert_replica(*reinterpret_cast<const uint64_t *>(request) == READ_VAL_REQ, "request is NOT " << READ_VAL_REQ);
#endif
      // Copy the latest register value to the reply buffer.
      test_assert_replica(maxReplySize >= sizeof(uint64_t), "maxReplySize < " << sizeof(uint64_t));
      uint64_t *pRet = reinterpret_cast<uint64_t *>(outReply);
      auto lastValue = get_last_state_value(clientId);
      *pRet = lastValue;
      outActualReplySize = sizeof(uint64_t);
    } else {
      // Our read-write request includes one eight-byte argument, in addition to
      // the request type.
#ifndef SBFT_PAYLOAD_SIZE
      test_assert_replica(requestSize == 2 * sizeof(uint64_t), "requestSize != " << 2 * sizeof(uint64_t));

      // We only support the WRITE operation in read-write mode.
      const uint64_t *pReqId = reinterpret_cast<const uint64_t *>(request);
      test_assert_replica(*pReqId == SET_VAL_REQ, "*preqId != " << SET_VAL_REQ);

      // The value to write is the second eight bytes of the request.
      const uint64_t *pReqVal = (pReqId + 1);

      // Modify the register state.
      set_last_state_value(clientId, *pReqVal);
      // Count the number of times we've modified it.
#endif
      auto stateNum = get_last_state_num(clientId);
      set_last_state_num(clientId, stateNum + 1);
      // Reply with the number of times we've modified the register.
      test_assert_replica(maxReplySize >= sizeof(uint64_t), "maxReplySize < " << sizeof(uint64_t));
      uint64_t *pRet = reinterpret_cast<uint64_t *>(outReply);
      *pRet = stateNum;
      outActualReplySize = sizeof(uint64_t);

      st->markUpdate(statePtr, sizeof(State) * numOfClients);
    }
    return 0;
  }

  void execute(ExecutionRequestsQueue &requests,
               const std::string &batchCid,
               concordUtils::SpanWrapper &parent_span) override {
    for (auto &req : requests) {
      // Not currently used
      req.outReplicaSpecificInfoSize = 0;

      bool readOnly = req.flags & READ_ONLY_FLAG;
      if (readOnly) {
        // Our read-only request includes only a type, no argument.
#ifndef SBFT_PAYLOAD_SIZE

        test_assert_replica(req.request.size() == sizeof(uint64_t), "requestSize =! " << sizeof(uint64_t));

        // We only support the READ operation in read-only mode.
        test_assert_replica(*reinterpret_cast<const uint64_t *>(req.request.c_str()) == READ_VAL_REQ,
                            "request is NOT " << READ_VAL_REQ);

#endif
        // Copy the latest register value to the reply buffer.
        test_assert_replica(req.outReply.size() >= sizeof(uint64_t), "maxReplySize < " << sizeof(uint64_t));
        uint64_t *pRet = const_cast<uint64_t *>(reinterpret_cast<const uint64_t *>(req.outReply.c_str()));
        auto lastValue = get_last_state_value(req.clientId);
        *pRet = lastValue;
        req.outActualReplySize = sizeof(uint64_t);
      } else {
        // Our read-write request includes one eight-byte argument, in addition to
        // the request type.
#ifndef SBFT_PAYLOAD_SIZE
        test_assert_replica(req.request.size() == 2 * sizeof(uint64_t), "requestSize != " << 2 * sizeof(uint64_t));

        // We only support the WRITE operation in read-write mode.
        const uint64_t *pReqId = reinterpret_cast<const uint64_t *>(req.request.c_str());
        test_assert_replica(*pReqId == SET_VAL_REQ, "*preqId != " << SET_VAL_REQ);

        // The value to write is the second eight bytes of the request.
        const uint64_t *pReqVal = (pReqId + 1);

        // Modify the register state.
        set_last_state_value(req.clientId, *pReqVal);
        // Count the number of times we've modified it.
#endif
        auto stateNum = get_last_state_num(req.clientId);
        set_last_state_num(req.clientId, stateNum + 1);

        // Reply with the number of times we've modified the register.
        test_assert_replica(req.outReply.size() >= sizeof(uint64_t), "maxReplySize < " << sizeof(uint64_t));
        uint64_t *pRet = const_cast<uint64_t *>(reinterpret_cast<const uint64_t *>(req.outReply.c_str()));
        *pRet = stateNum;
        req.outActualReplySize = sizeof(uint64_t);

        st->markUpdate(statePtr, sizeof(State) * numOfClients);
      }
      req.outExecutionStatus = 0;
    }
  }

  struct State {
    // Number of modifications made.
    uint64_t stateNum = 0;
    // Register value.
    uint64_t lastValue = 0;
  };
  State *statePtr;

  uint16_t numOfClients;
  uint16_t numOfReplicas;

  bftEngine::SimpleInMemoryStateTransfer::ISimpleInMemoryStateTransfer *st = nullptr;
};

class SimpleTestReplica {
 private:
  ICommunication *comm;
  bftEngine::IReplica::IReplicaPtr replica = nullptr;
  const ReplicaConfig &replicaConfig;
  std::thread *runnerThread = nullptr;
  ISimpleTestReplicaBehavior *behaviorPtr;
  IRequestsHandler *statePtr;
  shared_ptr<ControlStateManager> control_state_manager_;
  bftEngine::SimpleInMemoryStateTransfer::ISimpleInMemoryStateTransfer *inMemoryST_;

 public:
  SimpleTestReplica(ICommunication *commObject,
                    IRequestsHandler *state,
                    const ReplicaConfig &rc,
                    ISimpleTestReplicaBehavior *behvPtr,
                    bftEngine::SimpleInMemoryStateTransfer::ISimpleInMemoryStateTransfer *inMemoryST,
                    MetadataStorage *metaDataStorage)
      : comm{commObject}, replicaConfig{rc}, behaviorPtr{behvPtr}, statePtr(state), inMemoryST_(inMemoryST) {
    replica = IReplica::createNewReplica(rc, state, inMemoryST, comm, metaDataStorage);
  }

  ~SimpleTestReplica() {
    // TODO(DD): Reset manually because apparently the order matters - fixit
    replica.reset();
    if (comm) {
      comm->Stop();
      delete comm;
    }
    if (behaviorPtr) {
      delete behaviorPtr;
    }
    if (statePtr) {
      delete statePtr;
    }
  }

  uint16_t get_replica_id() { return replicaConfig.replicaId; }

  void start() {
    replica->start();
    control_state_manager_ = std::make_shared<ControlStateManager>(inMemoryST_, inMemoryST_->numberOfReservedPages());
    control_state_manager_->disable();
    replica->setControlStateManager(control_state_manager_);
  }

  void stop() {
    replica->stop();
    if (runnerThread) {
      runnerThread->join();
    }
    LOG_INFO(replicaLogger, "replica " << replicaConfig.replicaId << " stopped");
  }

  bool isRunning() { return replica->isRunning(); }

  void run() {
    if (replica->isRunning() && behaviorPtr->to_be_restarted()) {
      uint32_t initialSleepBetweenRestartsMillis = behaviorPtr->get_initial_sleep_between_restarts_ms();
      LOG_INFO(replicaLogger, "Restarting replica in " << initialSleepBetweenRestartsMillis << " ms");
      std::this_thread::sleep_for(std::chrono::milliseconds(initialSleepBetweenRestartsMillis));
    }
    while (replica->isRunning()) {
      bool toBeRestarted = behaviorPtr->to_be_restarted();
      if (toBeRestarted) {
        if (replica && replica->isRunning()) {
          uint32_t downTime = behaviorPtr->get_down_time_millis();
          LOG_INFO(replicaLogger, "Restarting replica");
          replica->restartForDebug(downTime);
          replica->setControlStateManager(control_state_manager_);
          behaviorPtr->on_restarted();
          LOG_INFO(replicaLogger, "Replica restarted");
        }
      } else {
        std::this_thread::sleep_for(std::chrono::seconds(1));
      }
    }
  }

  void run_non_blocking() { runnerThread = new std::thread(std::bind(&SimpleTestReplica::run, this)); }

  static SimpleTestReplica *create_replica(ISimpleTestReplicaBehavior *behv,
                                           ReplicaParams rp,
                                           MetadataStorage *metaDataStorage) {
    TestCommConfig testCommConfig(replicaLogger);
    ReplicaConfig &replicaConfig = ReplicaConfig::instance();
    testCommConfig.GetReplicaConfig(rp.replicaId, rp.keysFilePrefix, &replicaConfig);
    replicaConfig.numOfClientProxies = rp.numOfClients;
    replicaConfig.viewChangeProtocolEnabled = rp.viewChangeEnabled;
    replicaConfig.viewChangeTimerMillisec = rp.viewChangeTimeout;
    replicaConfig.replicaId = rp.replicaId;
    replicaConfig.statusReportTimerMillisec = 10000;
    replicaConfig.concurrencyLevel = 1;
    replicaConfig.debugPersistentStorageEnabled =
        rp.persistencyMode == PersistencyMode::InMemory || rp.persistencyMode == PersistencyMode::File;

    // This is the state machine that the replica will drive.
    SimpleAppState *simpleAppState = new SimpleAppState(rp.numOfClients, rp.numOfReplicas);

#ifdef USE_COMM_PLAIN_TCP
    PlainTcpConfig conf =
        testCommConfig.GetTCPConfig(true, rp.replicaId, rp.numOfClients, rp.numOfReplicas, rp.configFileName);
#elif USE_COMM_TLS_TCP
    TlsTcpConfig conf =
        testCommConfig.GetTlsTCPConfig(true, rp.replicaId, rp.numOfClients, rp.numOfReplicas, rp.configFileName);
#else
    PlainUdpConfig conf =
        testCommConfig.GetUDPConfig(true, rp.replicaId, rp.numOfClients, rp.numOfReplicas, rp.configFileName);
#endif
    auto comm = bft::communication::CommFactory::create(conf);

    bftEngine::SimpleInMemoryStateTransfer::ISimpleInMemoryStateTransfer *st =
        bftEngine::SimpleInMemoryStateTransfer::create(simpleAppState->statePtr,
                                                       sizeof(SimpleAppState::State) * rp.numOfClients,
                                                       replicaConfig.replicaId,
                                                       replicaConfig.fVal,
                                                       replicaConfig.cVal,
                                                       true);

    simpleAppState->st = st;
    SimpleTestReplica *replica = new SimpleTestReplica(comm, simpleAppState, replicaConfig, behv, st, metaDataStorage);
    return replica;
  }
};
