#pragma once

#include <QImage>
#include <QString>
#include <functional>

/// Everything the video/GIF writers need from a recording: frames on demand.
///
/// Counting + fetching instead of handing over a list keeps a long session out of
/// memory — 400 frames at 720px decoded is roughly 800 MB.
struct RecordedFrames {
    int count = 0;
    /// Decodes frame \p index into \p out with its timestamp in \p ms.
    std::function<bool(int index, QImage& out, qint64& ms)> fetch;
};

namespace RecordExport {

/// Frames per second of an exported timelapse. The recorder captures sparsely
/// (one frame per 1.5s of drawing), so playing at the real timestamps would give
/// a slideshow; a fixed 12 fps is a timelapse anyone will watch.
constexpr int kTimelapseFps = 12;

/// Writes an animated GIF89a (no dependency: the encoder is ours).
///
/// ponytail: Qt's 6x7x6 palette, no dithering and 1-bit transparency — fine for
/// line art on paper, visibly banded on a photo. A quantiser is the upgrade path.
[[nodiscard]] bool writeGif(const QString& path, const RecordedFrames& frames, QString* error);

/// True when this build can write MP4 (Windows Media Foundation here).
[[nodiscard]] bool mp4Supported();

/// Writes an H.264 MP4 through the platform encoder (Media Foundation on
/// Windows) — no bundled codec, no FFmpeg.
[[nodiscard]] bool writeMp4(const QString& path, const RecordedFrames& frames, QString* error);

/// Opens the written file again and counts decodable frames. A writer nobody can
/// verify is a writer that silently produces empty files.
[[nodiscard]] bool verifyVideo(const QString& path, int* frames, QString* error);

} // namespace RecordExport
