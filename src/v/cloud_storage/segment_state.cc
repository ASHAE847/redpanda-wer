/*
 * Copyright 2022 Redpanda Data, Inc.
 *
 * Licensed as a Redpanda Enterprise file under the Redpanda Community
 * License (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 *
 * https://github.com/redpanda-data/redpanda/blob/master/licenses/rcl.md
 */

#include "cloud_storage/segment_state.h"

#include "cloud_storage/remote_partition.h"
#include "cloud_storage/remote_segment.h"
#include "utils/retry_chain_node.h"

namespace cloud_storage {

offloaded_segment_state
materialized_segment_state::offload(remote_partition* partition) {
    _hook.unlink();
    for (auto&& rs : readers) {
        partition->evict_reader(std::move(rs));
    }
    partition->evict_segment(std::move(segment));
    partition->_probe.segment_offloaded();
    return offloaded_segment_state(base_rp_offset);
}

materialized_segment_state::materialized_segment_state(
  model::offset base_offset, kafka::offset off_key, remote_partition& p)
  : base_rp_offset(base_offset)
  , offset_key(off_key)
  , segment(ss::make_lw_shared<remote_segment>(
      p._api, p._cache, p._bucket, p._manifest, base_offset, p._rtc))
  , atime(ss::lowres_clock::now()) {
    p._materialized.push_back(*this);
}

void materialized_segment_state::return_reader(
  std::unique_ptr<remote_segment_batch_reader> state) {
    atime = ss::lowres_clock::now();
    readers.push_back(std::move(state));
}

/// Borrow reader or make a new one.
/// In either case return a reader.
std::unique_ptr<remote_segment_batch_reader>
materialized_segment_state::borrow_reader(
  const storage::log_reader_config& cfg,
  retry_chain_logger& ctxlog,
  partition_probe& probe) {
    atime = ss::lowres_clock::now();
    for (auto it = readers.begin(); it != readers.end(); it++) {
        if ((*it)->config().start_offset == cfg.start_offset) {
            // here we're reusing the existing reader
            auto tmp = std::move(*it);
            tmp->config() = cfg;
            readers.erase(it);
            vlog(
              ctxlog.debug,
              "reusing existing reader, config: {}",
              tmp->config());
            return tmp;
        }
    }
    vlog(ctxlog.debug, "creating new reader, config: {}", cfg);
    return std::make_unique<remote_segment_batch_reader>(segment, cfg, probe);
}

ss::future<> materialized_segment_state::stop() {
    for (auto& rs : readers) {
        co_await rs->stop();
    }
    co_await segment->stop();
}

offloaded_segment_state::offloaded_segment_state(model::offset base_offset)
  : base_rp_offset(base_offset) {}

std::unique_ptr<materialized_segment_state>
offloaded_segment_state::materialize(
  remote_partition& p, kafka::offset offset_key) {
    auto st = std::make_unique<materialized_segment_state>(
      base_rp_offset, offset_key, p);
    p._probe.segment_materialized();
    return st;
}

ss::future<> offloaded_segment_state::stop() { return ss::now(); }

offloaded_segment_state offloaded_segment_state::offload(remote_partition*) {
    return offloaded_segment_state(base_rp_offset);
}

} // namespace cloud_storage