#include "application/RecordExport.h"

#include <QBuffer>
#include <QFile>
#include <QHash>
#include <QImageReader>
#include <QPainter>
#include <QSaveFile>
#include <QVector>

#ifdef Q_OS_WIN
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <windows.h>
#endif

namespace {

/// The recorded frames come from the compositor, so the paper is transparent
/// (QML paints the white behind the canvas). A video or a GIF has no alpha to
/// speak of: flatten onto the same white the artist worked on.
QImage flattenedOverPaper(const QImage& frame)
{
    QImage out(frame.size(), QImage::Format_RGB32);
    out.fill(Qt::white);
    QPainter painter(&out);
    painter.drawImage(0, 0, frame);
    return out;
}

// ---------------------------------------------------------------------- GIF

void appendLE16(QByteArray& out, int value)
{
    out.append(static_cast<char>(value & 0xFF));
    out.append(static_cast<char>((value >> 8) & 0xFF));
}

/// GIF's LZW: variable-width codes, clear code when the table fills up.
QByteArray gifLzw(const QImage& indexed, int minCodeSize)
{
    const int clearCode = 1 << minCodeSize;
    const int endCode = clearCode + 1;
    int codeSize = minCodeSize + 1;
    int nextCode = endCode + 1;
    QHash<quint32, int> table;
    table.reserve(4096);

    QByteArray bits;
    quint32 bitBuffer = 0;
    int bitCount = 0;
    const auto writeCode = [&](int code) {
        bitBuffer |= static_cast<quint32>(code) << bitCount;
        bitCount += codeSize;
        while (bitCount >= 8) {
            bits.append(static_cast<char>(bitBuffer & 0xFF));
            bitBuffer >>= 8;
            bitCount -= 8;
        }
    };

    writeCode(clearCode);
    int prefix = -1;
    for (int y = 0; y < indexed.height(); ++y) {
        const uchar* row = indexed.constScanLine(y);
        for (int x = 0; x < indexed.width(); ++x) {
            const int pixel = row[x];
            if (prefix < 0) {
                prefix = pixel;
                continue;
            }
            const quint32 key = (static_cast<quint32>(prefix) << 8) | static_cast<quint32>(pixel);
            const auto found = table.constFind(key);
            if (found != table.constEnd()) {
                prefix = found.value();
                continue;
            }
            writeCode(prefix);
            if (nextCode < 4096) {
                table.insert(key, nextCode++);
                if (nextCode > (1 << codeSize) && codeSize < 12)
                    ++codeSize;
            } else {
                writeCode(clearCode);
                table.clear();
                codeSize = minCodeSize + 1;
                nextCode = endCode + 1;
            }
            prefix = pixel;
        }
    }
    if (prefix >= 0)
        writeCode(prefix);
    writeCode(endCode);
    if (bitCount > 0)
        bits.append(static_cast<char>(bitBuffer & 0xFF));
    return bits;
}

/// 3-3-2 bit RGB: 256 colours with no quantiser of our own and no dependence on
/// what Qt does when it is asked for an 8-bit image without a palette.
QImage toPaletted(const QImage& rgb)
{
    QImage out(rgb.size(), QImage::Format_Indexed8);
    QVector<QRgb> table(256);
    for (int i = 0; i < 256; ++i) {
        const int r = ((i >> 5) & 0x07) * 255 / 7;
        const int g = ((i >> 2) & 0x07) * 255 / 7;
        const int b = (i & 0x03) * 255 / 3;
        table[i] = qRgb(r, g, b);
    }
    out.setColorTable(table);
    for (int y = 0; y < rgb.height(); ++y) {
        const QRgb* src = reinterpret_cast<const QRgb*>(rgb.constScanLine(y));
        uchar* dst = out.scanLine(y);
        for (int x = 0; x < rgb.width(); ++x) {
            const QRgb c = src[x];
            dst[x] = static_cast<uchar>(((qRed(c) >> 5) << 5) | ((qGreen(c) >> 5) << 2)
                                        | (qBlue(c) >> 6));
        }
    }
    return out;
}

QByteArray frameDescriptor(const QImage& indexed)
{
    QByteArray out;
    appendLE16(out, indexed.width());
    appendLE16(out, indexed.height());
    // Local colour table, 256 entries, 8-bit colour resolution.
    out.append(static_cast<char>(0x80 | 0x07));
    const QVector<QRgb> table = indexed.colorTable();
    for (int i = 0; i < 256; ++i) {
        const QRgb c = i < table.size() ? table.at(i) : qRgb(0, 0, 0);
        out.append(static_cast<char>(qRed(c)));
        out.append(static_cast<char>(qGreen(c)));
        out.append(static_cast<char>(qBlue(c)));
    }
    return out;
}

} // namespace

namespace RecordExport {

bool writeGif(const QString& path, const RecordedFrames& frames, QString* error)
{
    if (frames.count <= 0) {
        if (error)
            *error = QObject::tr("Nothing recorded yet - draw something first.");
        return false;
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = QObject::tr("Could not write %1.").arg(path);
        return false;
    }

    QImage first;
    qint64 firstMs = 0;
    if (!frames.fetch(0, first, firstMs)) {
        if (error)
            *error = QObject::tr("The recording could not be read back.");
        return false;
    }
    first = flattenedOverPaper(first);

    QByteArray out;
    out.append("GIF89a", 6);
    // Logical screen descriptor (no global colour table: each frame carries one).
    appendLE16(out, first.width());
    appendLE16(out, first.height());
    out.append(static_cast<char>(0x70)); // 8-bit colour, no global table
    out.append(static_cast<char>(0));
    out.append(static_cast<char>(0));
    // NETSCAPE2.0: loop forever, which is what a timelapse wants.
    out.append("\x21\xFF\x0B", 3);
    out.append("NETSCAPE2.0", 11);
    out.append("\x03\x01\x00\x00\x00", 5);
    if (file.write(out) != out.size()) {
        if (error) *error = QObject::tr("Could not write GIF header.");
        return false;
    }
    out.clear();

    const int delay = std::max(2, 100 / kTimelapseFps);
    for (int i = 0; i < frames.count; ++i) {
        QImage frame;
        qint64 ms = 0;
        if (i == 0)
            frame = first;
        else if (!frames.fetch(i, frame, ms)) {
            if (error) *error = QObject::tr("Could not read frame %1.").arg(i);
            return false;
        }
        else
            frame = flattenedOverPaper(frame);

        const QImage indexed = toPaletted(frame);
        // Graphic control extension: keep the previous frame in place.
        out.append("\x21\xF9\x04", 3);
        out.append(static_cast<char>(0x04)); // disposal = 1 (do not dispose)
        appendLE16(out, delay);
        out.append(static_cast<char>(0));
        out.append(static_cast<char>(0));
        // Image descriptor + local colour table.
        out.append(static_cast<char>(0x2C));
        appendLE16(out, 0);
        appendLE16(out, 0);
        out.append(frameDescriptor(indexed));
        // LZW data, in sub-blocks of at most 255 bytes.
        out.append(static_cast<char>(8)); // min code size
        const QByteArray lzw = gifLzw(indexed, 8);
        for (int offset = 0; offset < lzw.size();) {
            const int chunk = std::min(255, static_cast<int>(lzw.size()) - offset);
            out.append(static_cast<char>(chunk));
            out.append(lzw.mid(offset, chunk));
            offset += chunk;
        }
        out.append(static_cast<char>(0));
        if (file.write(out) != out.size()) {
            if (error) *error = QObject::tr("Could not write GIF frame %1.").arg(i);
            return false;
        }
        out.clear();
    }
    out.append(static_cast<char>(0x3B)); // trailer

    if (file.write(out) != out.size()) {
        if (error) *error = QObject::tr("Could not finish GIF.");
        return false;
    }
    if (!file.commit()) {
        if (error)
            *error = QObject::tr("Could not write %1.").arg(path);
        return false;
    }
    return true;
}

// --------------------------------------------------------------------- MP4

#ifdef Q_OS_WIN

namespace {

/// MF wants a plain wide string; QProcess-style helpers do not apply here.
std::wstring toWide(const QString& text)
{
    return text.toStdWString();
}

template <typename T>
void release(T*& pointer)
{
    if (pointer) {
        pointer->Release();
        pointer = nullptr;
    }
}

} // namespace

bool mp4Supported() { return true; }

bool writeMp4(const QString& path, const RecordedFrames& frames, QString* error)
{
    if (frames.count <= 0) {
        if (error)
            *error = QObject::tr("Nothing recorded yet - draw something first.");
        return false;
    }

    // Media Foundation needs COM on this thread. The GUI thread usually has it
    // already (Qt's Windows plugin), and a different apartment is not fatal for
    // the sink writer, so ignore RPC_E_CHANGED_MODE.
    const HRESULT comInit = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool ownsCom = SUCCEEDED(comInit);
    if (FAILED(MFStartup(MF_VERSION, MFSTARTUP_LITE))) {
        if (error)
            *error = QObject::tr("Media Foundation is unavailable on this system.");
        if (ownsCom)
            CoUninitialize();
        return false;
    }

    IMFSinkWriter* writer = nullptr;
    IMFMediaType* outputType = nullptr;
    IMFMediaType* inputType = nullptr;
    DWORD streamIndex = 0;
    QString failure;

    const auto cleanup = [&]() {
        release(inputType);
        release(outputType);
        release(writer);
        MFShutdown();
        if (ownsCom)
            CoUninitialize();
    };

    QImage first;
    qint64 firstMs = 0;
    if (!frames.fetch(0, first, firstMs)) {
        if (error)
            *error = QObject::tr("The recording could not be read back.");
        cleanup();
        return false;
    }
    first = flattenedOverPaper(first);
    // H.264 wants even dimensions.
    const int width = first.width() & ~1;
    const int height = first.height() & ~1;
    if (width < 16 || height < 16) {
        if (error)
            *error = QObject::tr("The recording is too small to encode.");
        cleanup();
        return false;
    }

    HRESULT hr = MFCreateSinkWriterFromURL(toWide(path).c_str(), nullptr, nullptr, &writer);
    if (SUCCEEDED(hr))
        hr = MFCreateMediaType(&outputType);
    if (SUCCEEDED(hr)) {
        outputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        outputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
        outputType->SetUINT32(MF_MT_AVG_BITRATE, 4'000'000);
        outputType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
        MFSetAttributeSize(outputType, MF_MT_FRAME_SIZE, width, height);
        MFSetAttributeRatio(outputType, MF_MT_FRAME_RATE, kTimelapseFps, 1);
        MFSetAttributeRatio(outputType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
        hr = writer->AddStream(outputType, &streamIndex);
    }
    if (SUCCEEDED(hr))
        hr = MFCreateMediaType(&inputType);
    if (SUCCEEDED(hr)) {
        inputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        inputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
        inputType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
        MFSetAttributeSize(inputType, MF_MT_FRAME_SIZE, width, height);
        MFSetAttributeRatio(inputType, MF_MT_FRAME_RATE, kTimelapseFps, 1);
        MFSetAttributeRatio(inputType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
        hr = writer->SetInputMediaType(streamIndex, inputType, nullptr);
    }
    if (SUCCEEDED(hr))
        hr = writer->BeginWriting();

    const LONGLONG frameDuration = 10'000'000LL / kTimelapseFps;
    for (int i = 0; SUCCEEDED(hr) && i < frames.count; ++i) {
        QImage frame = first;
        qint64 ms = 0;
        if (i > 0) {
            if (!frames.fetch(i, frame, ms))
                continue;
            frame = flattenedOverPaper(frame);
        }
        if (frame.width() != width || frame.height() != height)
            frame = frame.scaled(width, height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        // Media Foundation reads a positive-stride RGB32 buffer bottom-up (the
        // DIB convention), so hand it the rows reversed. Without this the video
        // plays upside down; the selftest now decodes the written MP4 and
        // compares it against the recorder's own frames, which is what caught it.
        frame = frame.convertToFormat(QImage::Format_RGB32).flipped(Qt::Vertical);

        IMFMediaBuffer* buffer = nullptr;
        IMFSample* sample = nullptr;
        hr = MFCreateMemoryBuffer(static_cast<DWORD>(width) * height * 4, &buffer);
        if (SUCCEEDED(hr)) {
            BYTE* data = nullptr;
            hr = buffer->Lock(&data, nullptr, nullptr);
            if (SUCCEEDED(hr)) {
                // QImage rows are 4-byte aligned, so a row-by-row copy keeps the
                // stride honest even when the width is not a multiple of 4.
                MFCopyImage(data, width * 4, frame.constBits(), frame.bytesPerLine(), width * 4,
                            height);
                buffer->Unlock();
            }
        }
        if (SUCCEEDED(hr))
            hr = buffer->SetCurrentLength(static_cast<DWORD>(width) * height * 4);
        if (SUCCEEDED(hr))
            hr = MFCreateSample(&sample);
        if (SUCCEEDED(hr))
            hr = sample->AddBuffer(buffer);
        if (SUCCEEDED(hr))
            hr = sample->SetSampleTime(static_cast<LONGLONG>(i) * frameDuration);
        if (SUCCEEDED(hr))
            hr = sample->SetSampleDuration(frameDuration);
        if (SUCCEEDED(hr))
            hr = writer->WriteSample(streamIndex, sample);
        release(sample);
        release(buffer);
    }
    if (SUCCEEDED(hr))
        hr = writer->Finalize();

    if (FAILED(hr)) {
        failure = QObject::tr("Window's H.264 encoder refused the file (0x%1). Video export "
                              "needs the Media Feature Pack on Windows N editions.")
                      .arg(static_cast<quint32>(hr), 8, 16, QLatin1Char('0'));
    }
    cleanup();

    if (!failure.isEmpty()) {
        if (error)
            *error = failure;
        return false;
    }
    return true;
}

#else // !Q_OS_WIN

bool mp4Supported() { return false; }

bool writeMp4(const QString&, const RecordedFrames&, QString* error)
{
    if (error)
        *error = QObject::tr("MP4 export needs the platform encoder, which this build does not "
                             "have. Use the GIF or the .prismrec instead.");
    return false;
}

#endif

// -------------------------------------------------------------- verification

bool verifyVideo(const QString& path, int* frames, QString* error)
{
    QImageReader reader(path);
    if (!reader.canRead()) {
        // MP4 is not an image format Qt reads: on Windows let Media Foundation
        // decode it back instead, so "the file exists" is not the only evidence.
#ifdef Q_OS_WIN
        const HRESULT comInit = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        const bool ownsCom = SUCCEEDED(comInit);
        if (FAILED(MFStartup(MF_VERSION, MFSTARTUP_LITE))) {
            if (error)
                *error = QObject::tr("Media Foundation is unavailable on this system.");
            if (ownsCom)
                CoUninitialize();
            return false;
        }
        IMFSourceReader* sourceReader = nullptr;
        HRESULT hr = MFCreateSourceReaderFromURL(toWide(path).c_str(), nullptr, &sourceReader);
        // Count the samples as they arrive: converting them to RGB32 would need
        // the video-processor MFT configured, and the only question here is
        // whether the file really holds frames.
        int counted = 0;
        while (SUCCEEDED(hr)) {
            DWORD flags = 0;
            IMFSample* sample = nullptr;
            hr = sourceReader->ReadSample(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM),
                                          0, nullptr, &flags, nullptr, &sample);
            release(sample);
            if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM))
                break;
            ++counted;
        }
        release(sourceReader);
        MFShutdown();
        if (ownsCom)
            CoUninitialize();
        if (counted <= 0) {
            if (error)
                *error = QObject::tr("The decoder could not read any frame from %1.").arg(path);
            return false;
        }
        if (frames)
            *frames = counted;
        return true;
#else
        if (error)
            *error = QObject::tr("%1 is not a format this build can read back.").arg(path);
        return false;
#endif
    }

    // Animated GIF: Qt reads it back, which is exactly the check wanted.
    int counted = 0;
    while (true) {
        const QImage frame = reader.read();
        if (frame.isNull())
            break;
        ++counted;
    }
    if (counted <= 0) {
        if (error)
            *error = QObject::tr("The written file has no frames.");
        return false;
    }
    if (frames)
        *frames = counted;
    return true;
}

} // namespace RecordExport
