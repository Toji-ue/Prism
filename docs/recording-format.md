# Recording formats and timeline links

## Live sessions: v2, legacy v1

Recording continues to write v2 JSONL: one `prism-record` header, then independently
flushed PNG frame entries. The SHA-256 input remains `previousHash + decimal ms +
PNG bytes`. v1 is the legacy single JSON object with a `frames` array and the
same hash rule. Compression/resolution changes produce new PNG bytes; existing
sessions are never rewritten. A hash chain does not establish authorship.

Recovery accepts the complete valid prefix of v2. Strict verification rejects a
truncated or damaged tail. Completion/gap information is separate sidecar
metadata; a valid prefix without completion evidence requires Partial export.

## Portable bundles: v3

The first line is compact UTF-8 JSON ending in LF:

- `format`: `prism-record`; `version`: `3`.
- `manifest`: version 1, containing `timelines`. Each has a UUID `id`, display
  `name`, optional parent UUID and ordered unique `sessions` filenames.
- `entries`: one per referenced session: UUID `.prismrec` `name`, integer `bytes`,
  hex SHA-256 of exactly those bytes, and source sidecar `metadata`.

Immediately after LF, append each entry's raw source bytes in entry order, with
no padding or separator. The manifest's session order controls playback; payload
order does not. Nested v3 entries are rejected. Original v1/v2 streams are kept
byte-for-byte. A live snapshot records only the acknowledged prefix; its bundle
metadata marks that snapshot completed while retaining known gaps. A partial
export never clears missing-coverage flags. Portable manifests omit local artwork
paths and fingerprints.

The reader validates the whole manifest, lengths, references, parent cycles,
source hashes and embedded record hashes before Storage publication. Filenames
must be UUIDs, so input cannot choose an extraction path. Import stages on the
recording volume, publishes with fresh filenames/IDs and rolls back on failure
or cancellation before the timeline index commits. Import does not change the
open artwork or automatically bind a local path.

Bounds: 8 MiB manifest/local-index JSON, 4,096 timeline rows, 4,096 sessions per
row/bundle, 1 TiB per source payload, 128 MiB per encoded frame line and legacy
v1 file. Frame indexing is limited to 1,000,000 frames per file/timeline snapshot.
Frame decoding rejects invalid dimensions or sides above 32768 px; above 64 Mi
pixels the preview/video reader validates at most 1 GiB decoded pixels,
preallocates without changing Qt's global allocation policy, then reduces to
1920 px. Original source bytes remain unchanged; allocation failures are reported. Oversized input is rejected visibly, never
truncated as a successful full import. Native bundles preserve original source bytes independently of
video sampling or the four-minute video limit.

## Local index

`recordings/timelines.json` is a version-1 JSON index committed with `QSaveFile`.
Local rows additionally hold canonical artwork paths and the fingerprint of the
last successfully saved file. Opening a matching path/fingerprint attaches the
new session; mismatches offer Continue previous/New timeline. Save As freezes
the parent session and creates a child referencing prior sources. Link artwork
explicitly binds moved/imported timelines; reopen the artwork to continue.

Deleting a timeline retains every session used by another timeline. Direct raw
session deletion is refused while any timeline references it or the local index
cannot be validated. No age-based
cleanup occurs. Artwork/project files are unaffected by recording deletion.

## Video

Settings are independent of capture: target 60/120/240 seconds, 12/24/30 fps,
1280/1920 px long edge, optional final-image intro and final hold. Custom Input
FPS calculates body duration from source frame count. Total duration including
intro/end cannot exceed 240 seconds. Source frames are selected in chronological
order, with the first/last retained; no stroke interpolation is performed.
Frames of different sizes/aspects fit the same white output without cropping.
Low-resolution frames may be enlarged but cannot gain source detail.

MP4 uses the Windows platform encoder and GIF uses Prism's writer. Both support
cooperative progress/cancel. Existing destination files survive failures/cancel;
MP4 is read back and its sample count checked before atomic publication. GIF
centisecond delays accumulate rational FPS timing, avoiding the former fixed
12.5-fps rounding. Hardware/soak evidence remains separate from unit and native
interaction coverage.
