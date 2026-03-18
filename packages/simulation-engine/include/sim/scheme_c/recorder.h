#pragma once

#include <memory>
#include <string>
#include <vector>

#include "sim/scheme_c/types.h"

namespace sim::scheme_c {

class Recorder {
public:
    virtual ~Recorder() = default;

    virtual void recordSnapshot(const TickInfo& tick,
                                const std::vector<AxisState>& axes,
                                const std::vector<IoState>& io) = 0;
    virtual void recordEvent(const TickInfo& tick,
                             const std::string& source,
                             const std::string& message) = 0;
    virtual RecordingResult finish(const TickInfo& final_tick,
                                   const std::vector<AxisState>& final_axes,
                                   const std::vector<IoState>& final_io) = 0;
};

std::unique_ptr<Recorder> createTimelineRecorder();
void finalizeRecordingResult(RecordingResult& result,
                             SessionStatus status,
                             const std::string& termination_reason);
RecordingResult makeEmptyRecordingResult(SessionStatus status,
                                         const std::string& termination_reason);

}  // namespace sim::scheme_c
