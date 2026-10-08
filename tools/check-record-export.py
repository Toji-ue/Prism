"""Check an exported MP4/GIF against the .prismrec of the same session.

Why this exists next to `--selftest`: `RecordExport::verifyVideo` decodes through
our own Media Foundation path, so encoder and decoder agree on a wrong
orientation and a round-trip still passes. The first MP4 export shipped upside
down for exactly that reason. Orientation needs a decoder we did not write.

    python tools/check-record-export.py export.mp4 export.gif session.prismrec

Needs ffmpeg (on PATH, or `pip install imageio-ffmpeg`) and Pillow.
Each line prints the mean per-channel difference against the record's frame and
against the same frame flipped vertically: the smaller number names the
orientation, so a correct export shows the plain difference winning.
"""
import base64
import io
import json
import os
import shutil
import subprocess
import sys
import tempfile

from PIL import Image


def ffmpeg_exe():
    exe = os.environ.get("IMAGEIO_FFMPEG") or shutil.which("ffmpeg")
    if not exe:
        try:  # pip install imageio-ffmpeg ships a binary, which is often all there is
            import imageio_ffmpeg

            exe = imageio_ffmpeg.get_ffmpeg_exe()
        except ImportError:
            exe = None
    if not exe:
        sys.exit("ffmpeg not found: put it on PATH, or pip install imageio-ffmpeg")
    return exe


def rec_frames(path):
    """The recorder's own frames, flattened onto the white paper the exporter uses."""
    frames = []
    for entry in json.load(open(path))["frames"]:
        image = Image.open(io.BytesIO(base64.b64decode(entry["png"])))
        if image.mode in ("RGBA", "LA", "P"):
            image = image.convert("RGBA")
            image = Image.alpha_composite(Image.new("RGBA", image.size, (255, 255, 255, 255)), image)
        frames.append(image.convert("RGB"))
    return frames


def video_frames(path):
    out = tempfile.mkdtemp(prefix="prism-export-")
    subprocess.run([ffmpeg_exe(), "-y", "-loglevel", "error", "-i", path,
                    os.path.join(out, "f%03d.png")], check=True)
    frames = [Image.open(os.path.join(out, f)).convert("RGB")
              for f in sorted(os.listdir(out))]
    shutil.rmtree(out, ignore_errors=True)
    return frames


def diff(a, b):
    """Mean absolute per-channel difference, 0..255; None when sizes differ."""
    if a.size != b.size:
        return None
    pa, pb = a.load(), b.load()
    total = count = 0
    for y in range(0, a.size[1], 3):
        for x in range(0, a.size[0], 3):
            ca, cb = pa[x, y], pb[x, y]
            total += abs(ca[0] - cb[0]) + abs(ca[1] - cb[1]) + abs(ca[2] - cb[2])
            count += 3
    return total / max(1, count)


def main(argv):
    if len(argv) < 2:
        sys.exit(__doc__)
    *exports, record = argv
    source = rec_frames(record)
    print(f"record: {len(source)} frames, {source[0].size[0]}x{source[0].size[1]}")
    for path in exports:
        frames = video_frames(path)
        size = f"{frames[0].size[0]}x{frames[0].size[1]}" if frames else "-"
        print(f"{os.path.basename(path)}: {len(frames)} frames, {size}")
        if len(frames) != len(source):
            print(f"  !! frame count differs: export {len(frames)} vs record {len(source)}")
        for i, frame in enumerate(frames[:len(source)]):
            upright = diff(source[i], frame)
            flipped = diff(source[i], frame.transpose(Image.FLIP_TOP_BOTTOM))
            verdict = "ok" if upright is not None and (flipped is None or upright <= flipped) else "UPSIDE DOWN"
            print(f"  frame {i}: upright {upright} | flipped {flipped} -> {verdict}")


if __name__ == "__main__":
    main(sys.argv[1:])
