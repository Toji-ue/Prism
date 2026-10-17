#pragma once

struct CaptureSettings {
    int intervalMs = 1500;
    int maxSide = 1920; // Zero preserves the original canvas dimensions.
    bool valid() const {
        return intervalMs >= 500 && intervalMs <= 60000 &&
               (maxSide == 0 || maxSide == 720 || maxSide == 1920);
    }
};
