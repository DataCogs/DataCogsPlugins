#!/usr/bin/env python3
"""Generate artwork for IRs that have no sibling photo.

The plugin's photo panel shows "<ir name>.jpg" next to each loaded IR.
Real captures (OpenAIR) ship with real photos; everything else - the
Voxengo and EchoThief collections, and any abstract space - has nothing.
This walks the IR library, builds one image prompt per photo-less .wav,
and either:

  --backend prompts   (default) write prompts.json next to this script,
                      ready to feed any image generator by hand
  --backend replicate call Replicate's Flux Schnell (needs
                      REPLICATE_API_TOKEN) and save the sibling .jpg
  --backend openai    call the OpenAI Images API (needs OPENAI_API_KEY)
                      and save the result as the sibling .jpg directly

House style: these are deliberately *illustrations*, not fake photos -
a consistent moody architectural style in the plugin's steel/graphite
palette. Generating photoreal fakes of real named buildings would be
misleading in a project that trades on transparency; pass --style photo
if you want it anyway.

Resume-safe: existing images are never overwritten, so re-running after
an interruption (or after adding new IRs) only fills the gaps.

Usage:
  python3 generate-ir-photos.py                       # write prompts.json
  python3 generate-ir-photos.py --backend openai      # generate everything
  python3 generate-ir-photos.py --backend openai --limit 5   # trial run
"""

import argparse
import base64
import json
import os
import sys
import time
import urllib.request
from pathlib import Path

IMAGE_EXTS = (".jpg", ".jpeg", ".png")

ILLUSTRATION_SUFFIX = (
    "Moody atmospheric architectural illustration, muted steel-blue and "
    "graphite palette, soft volumetric light, painterly, wide angle, "
    "no people, no text, no watermark."
)
PHOTO_SUFFIX = (
    "Atmospheric wide-angle interior photograph, natural light, "
    "no people, no text, no watermark."
)

# Keyword -> scene description. First match wins; order matters (e.g.
# "drum room" before "room"). The fallback handles the abstract names.
SCENES = [
    ("cabinet",      "a close-mic'd vintage guitar speaker cabinet in a dim recording booth"),
    ("drum room",    "a compact wood-panelled drum recording room with a kit set up"),
    ("cathedral",    "a vast gothic cathedral nave with towering stone columns"),
    ("minster",      "a vast gothic cathedral nave with towering stone columns"),
    ("church",       "an old stone church interior with wooden pews and tall windows"),
    ("chapel",       "a small vaulted stone chapel lit by candles"),
    ("sanctuary",    "a hushed sanctuary hall with high shadowed ceilings"),
    ("mausoleum",    "a cavernous domed stone mausoleum, cold and monumental"),
    ("lodge",        "an ornate wood-panelled ceremonial lodge hall"),
    ("opera",        "a grand horseshoe opera house with tiered gilded balconies"),
    ("salon",        "an 18th-century French salon with parquet, mirrors and chandeliers"),
    ("concert hall", "a grand symphonic concert hall with warm wooden acoustics"),
    ("musikverein",  "a golden neoclassical concert hall with tiered balconies"),
    ("hall",         "a grand empty hall with a high ceiling and hard reflective surfaces"),
    ("cave",         "a small prehistoric rock cave lit by a shaft of daylight"),
    ("garage",       "an empty underground concrete parking garage, sodium lighting"),
    ("silo",         "the inside of a towering cylindrical grain silo, light from above"),
    ("stairwell",    "a tall echoing concrete stairwell spiralling upward"),
    ("tunnel",       "a long dim tunnel with parallel receding walls"),
    ("bridge",       "the underside of a large bridge with concrete pillars"),
    ("outside",      "a castle courtyard at dusk, stone walls all around"),
    ("chateau",      "a castle courtyard at dusk, stone walls all around"),
    ("space",        "a dark cosmic void with faint drifting nebulae"),
    ("star",         "a glittering starfield over an endless dark horizon"),
    ("room",         "an empty room with bare reflective walls and a single window"),
    ("chamber",      "a small stone chamber with a low vaulted ceiling"),
]

FALLBACK = "an imagined resonant space evoking \"{name}\", abstract architecture"


def scene_for (name: str) -> str:
    lowered = name.lower()
    for key, scene in SCENES:
        if key in lowered:
            return scene
    return FALLBACK.format (name=name)


def build_prompt (wav: Path, style_suffix: str) -> str:
    collection = wav.parent.name
    return f"{scene_for (wav.stem).capitalize()}. Impulse response '{wav.stem}' from the {collection} collection. {style_suffix}"


def find_missing (library: Path):
    for wav in sorted (library.rglob ("*.wav")):
        if not any (wav.with_suffix (ext).exists() for ext in IMAGE_EXTS):
            yield wav


def save_jpg (image_bytes: bytes, out_path: Path) -> None:
    # Downscale to a photo-panel-friendly jpg (the panel is 200px wide;
    # 800px leaves headroom for retina without bloating the library).
    from PIL import Image
    import io
    img = Image.open (io.BytesIO (image_bytes)).convert ("RGB")
    img.thumbnail ((800, 800), Image.LANCZOS)
    img.save (out_path, "JPEG", quality=85)


def generate_replicate (prompt: str, out_path: Path, size: str) -> None:
    # Flux Schnell, synchronous mode: Prefer: wait blocks until the image
    # is ready (schnell takes a couple of seconds).
    req = urllib.request.Request (
        "https://api.replicate.com/v1/models/black-forest-labs/flux-schnell/predictions",
        data=json.dumps ({
            "input": {
                "prompt": prompt,
                "aspect_ratio": "1:1",
                "output_format": "jpg",
                "output_quality": 90,
            }
        }).encode(),
        headers={
            "Authorization": f"Bearer {os.environ['REPLICATE_API_TOKEN']}",
            "Content-Type": "application/json",
            "Prefer": "wait",
        })
    with urllib.request.urlopen (req, timeout=180) as resp:
        payload = json.load (resp)

    # Prefer: wait gives up after ~60s (cold starts); poll the rest.
    deadline = time.time() + 180
    while payload.get ("status") in ("starting", "processing"):
        if time.time() > deadline:
            raise RuntimeError ("timed out waiting for prediction")
        time.sleep (2)
        poll = urllib.request.Request (payload["urls"]["get"], headers={
            "Authorization": f"Bearer {os.environ['REPLICATE_API_TOKEN']}"})
        with urllib.request.urlopen (poll, timeout=60) as resp:
            payload = json.load (resp)

    output = payload.get ("output")
    url = output[0] if isinstance (output, list) else output
    if payload.get ("status") != "succeeded" or not url:
        raise RuntimeError (payload.get ("error") or f"status {payload.get ('status')}")
    with urllib.request.urlopen (url, timeout=180) as resp:
        save_jpg (resp.read(), out_path)


def generate_openai (prompt: str, out_path: Path, size: str) -> None:
    req = urllib.request.Request (
        "https://api.openai.com/v1/images/generations",
        data=json.dumps ({
            "model": "gpt-image-1",
            "prompt": prompt,
            "size": size,
            "quality": "medium",
        }).encode(),
        headers={
            "Authorization": f"Bearer {os.environ['OPENAI_API_KEY']}",
            "Content-Type": "application/json",
        })
    with urllib.request.urlopen (req, timeout=300) as resp:
        payload = json.load (resp)
    png = base64.b64decode (payload["data"][0]["b64_json"])

    # Downscale to a photo-panel-friendly jpg (the panel is 200px wide;
    # 800px leaves headroom for retina without bloating the library).
    from PIL import Image
    import io
    img = Image.open (io.BytesIO (png)).convert ("RGB")
    img.thumbnail ((800, 800), Image.LANCZOS)
    img.save (out_path, "JPEG", quality=85)


def default_library() -> Path:
    primary = Path.home() / "Library/Audio/Impulse Responses/DataCogs"
    fallback = Path.home() / "Documents/DataCogs/Impulse Responses"
    return primary if primary.exists() else fallback


def main() -> int:
    ap = argparse.ArgumentParser (description=__doc__,
                                  formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument ("--library", type=Path, default=default_library(),
                     help="IR library root (default: the plugin's search path)")
    ap.add_argument ("--backend", choices=["prompts", "replicate", "openai"], default="prompts")
    ap.add_argument ("--style", choices=["illustration", "photo"], default="illustration")
    ap.add_argument ("--size", default="1024x1024")
    ap.add_argument ("--limit", type=int, default=0, help="stop after N images (0 = all)")
    ap.add_argument ("--dry-run", action="store_true",
                     help="list what would be generated, touch nothing")
    args = ap.parse_args()

    if not args.library.exists():
        print (f"error: IR library not found at {args.library}", file=sys.stderr)
        return 1
    required_key = { "openai": "OPENAI_API_KEY", "replicate": "REPLICATE_API_TOKEN" }.get (args.backend)
    if required_key and required_key not in os.environ:
        print (f"error: --backend {args.backend} needs {required_key} set", file=sys.stderr)
        return 1

    suffix = ILLUSTRATION_SUFFIX if args.style == "illustration" else PHOTO_SUFFIX
    missing = list (find_missing (args.library))
    if args.limit:
        missing = missing[: args.limit]
    print (f"{len (missing)} IRs without artwork under {args.library}")

    if args.backend == "prompts" or args.dry_run:
        manifest = [{ "wav": str (w),
                      "image": str (w.with_suffix (".jpg")),
                      "prompt": build_prompt (w, suffix) } for w in missing]
        out = Path (__file__).parent / "prompts.json"
        if not args.dry_run:
            out.write_text (json.dumps (manifest, indent=2))
            print (f"wrote {out}")
        else:
            for entry in manifest[:5]:
                print (f"  {Path (entry['wav']).stem}: {entry['prompt']}")
        return 0

    done = failed = 0
    for wav in missing:
        prompt = build_prompt (wav, suffix)
        target = wav.with_suffix (".jpg")
        print (f"[{done + failed + 1}/{len (missing)}] {wav.stem} ...", flush=True)
        generate = generate_openai if args.backend == "openai" else generate_replicate
        for attempt in range (3):
            try:
                generate (prompt, target, args.size)
                done += 1
                time.sleep (3)   # stay polite to the rate limiter
                break
            except Exception as e:   # keep going; rerun picks up the stragglers
                if "429" in str (e) and attempt < 2:
                    print ("  rate limited, backing off 60s...", flush=True)
                    time.sleep (60)
                    continue
                failed += 1
                print (f"  FAILED {wav.stem}: {e}", file=sys.stderr)
                break
    print (f"generated {done}, failed {failed}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit (main())
