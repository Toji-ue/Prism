"""Independently check MP4/GIF orientation against Prism record v1/v2/v3.

Requires Pillow and external ffmpeg (PATH or IMAGEIO_FFMPEG). Only sampled
frames are decoded to disk, including the first and last. Example:
  python tools/check-record-export.py --seconds 120 --fps 30 --end 5 video.mp4 record.prismrec
Omit --seconds for legacy one-source-frame-per-video-frame exports.
"""
import argparse
import base64
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
from PIL import Image, ImageChops, ImageStat

LIMIT = 128 * 1024 * 1024

class RecordFrames:
    def __init__(self, path, timeline=None):
        self.path = path
        self.frames = []
        with open(path, "rb") as f:
            header = json.loads(f.readline(8 * 1024 * 1024 + 1))
            if header["version"] == 3:
                entries = {}
                for entry in header["entries"]:
                    entries[entry["name"]] = (f.tell(), entry["bytes"])
                    f.seek(entry["bytes"], 1)
                rows = header["manifest"]["timelines"]
                row = next((r for r in rows if r["id"] == timeline), rows[-1])
                for name in row["sessions"]:
                    offset, size = entries[name]
                    self._index(f, offset, offset + size)
            else:
                self._index(f, 0, os.path.getsize(path))
        if not self.frames:
            raise ValueError("No source frames")

    def _index(self, f, start, end):
        f.seek(start)
        header = json.loads(f.readline(8 * 1024 * 1024 + 1))
        if header["version"] == 1:
            if end - start > LIMIT:
                raise ValueError("Legacy record exceeds safe size")
            f.seek(start)
            self.frames.extend(json.loads(f.read(end - start))["frames"])
        elif header["version"] == 2:
            while f.tell() < end:
                offset = f.tell()
                line = f.readline(min(LIMIT + 1, end - offset))
                if not line.endswith(b"\n") or len(line) > LIMIT:
                    raise ValueError("Truncated or oversized source frame")
                self.frames.append((offset, len(line)))
        else:
            raise ValueError("Unsupported nested record")

    def image(self, index):
        entry = self.frames[index]
        if isinstance(entry, tuple):
            with open(self.path, "rb") as f:
                f.seek(entry[0])
                entry = json.loads(f.read(entry[1]))
        image = Image.open(io.BytesIO(base64.b64decode(entry["png"]))).convert("RGBA")
        return Image.alpha_composite(Image.new("RGBA", image.size, "white"), image).convert("RGB")


def fit(image, size):
    image = image.copy()
    image.thumbnail(size, Image.Resampling.LANCZOS)
    # Qt's export scales small sources up too.
    factor = min(size[0] / image.width, size[1] / image.height)
    image = image.resize((round(image.width * factor), round(image.height * factor)), Image.Resampling.LANCZOS)
    out = Image.new("RGB", size, "white")
    out.paste(image, ((size[0] - image.width) // 2, (size[1] - image.height) // 2))
    return out


def diff(a, b):
    return sum(ImageStat.Stat(ImageChops.difference(a, b)).mean) / 3


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("paths", nargs="+")
    parser.add_argument("--seconds", type=int)
    parser.add_argument("--fps", type=int, default=12)
    parser.add_argument("--end", type=int, default=0)
    parser.add_argument("--intro", type=int, default=0)
    parser.add_argument("--input-fps", type=float, default=0)
    parser.add_argument("--timeline")
    args = parser.parse_args()
    if len(args.paths) < 2:
        parser.error("Supply video(s), then source record")
    ffmpeg = os.environ.get("IMAGEIO_FFMPEG") or shutil.which("ffmpeg")
    if not ffmpeg:
        parser.error("Set IMAGEIO_FFMPEG to an external ffmpeg executable")
    source = RecordFrames(args.paths[-1], args.timeline)
    body = round((args.seconds - args.intro - args.end) * args.fps) if args.seconds else len(source.frames)
    if args.input_fps:
        import math
        body = max(2 if len(source.frames) > 1 else 1, math.ceil(len(source.frames) / args.input_fps * args.fps))
    intro, end = args.intro * args.fps, args.end * args.fps
    count = intro + body + end
    samples = sorted({round(i * (count - 1) / min(15, count - 1)) for i in range(min(16, count))}) if count > 1 else [0]
    select = "+".join(f"eq(n\\,{n})" for n in samples)
    failures = 0
    for path in args.paths[:-1]:
        with tempfile.TemporaryDirectory(prefix="prism-orientation-") as temp:
            subprocess.run([ffmpeg, "-v", "error", "-i", path, "-vf", f"select={select}", "-vsync", "0", str(Path(temp) / "f%05d.png")], check=True)
            images = sorted(Path(temp).glob("f*.png"))
            if len(images) != len(samples):
                raise ValueError("Video is missing expected sample frames, including the end")
            for n, file in zip(samples, images):
                index = len(source.frames)-1 if n < intro or n >= intro + body else (n-intro)*(len(source.frames)-1)//max(1, body-1)
                frame = Image.open(file).convert("RGB")
                expected = fit(source.image(index), frame.size)
                upright, flipped = diff(expected, frame), diff(expected, frame.transpose(Image.Transpose.FLIP_TOP_BOTTOM))
                good = upright <= flipped and upright < 35
                failures += not good
                print(f"{Path(path).name} frame {n}: upright={upright:.3f}, flipped={flipped:.3f} {'PASS' if good else 'FAIL'}")
    raise SystemExit(bool(failures))

if __name__ == "__main__":
    main()
