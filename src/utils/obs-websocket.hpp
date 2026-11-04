#pragma once

#include <obs.h>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>

// Rolling {bytes, timestamp ns} sample per output name, used to derive bitrate.
using VendorStatsSamples = std::unordered_map<std::string, std::pair<uint64_t, uint64_t>>;

// blog() the line, then send it to websocket clients as a "log" event if logging
// is enabled and the level clears the threshold. Safe to call from any thread.
void vendor_log(int level, const char *where, obs_data_t *extra, const char *fmt, ...);

// Emits "output_state_changed". UI thread only.
void vendor_emit_output_state(const char *name, bool active, const char *last_error = nullptr, long long stop_code = 0);

// Fills one output's live health into `d`. With `samples`, also derives the
// bitrate since the previous sample; without, bitrate_kbps is 0.
void vendor_fill_output_stats(obs_data_t *d, const char *name, obs_output_t *output, VendorStatsSamples *samples);

// True when an "output_stats" round is due, and starts the next interval. UI thread only.
bool vendor_stats_due();

void vendor_emit_output_stats(obs_data_t *stats);
