#!/usr/bin/env python3
"""Recreate the original Little Moments demo tracks and app icon.

Only Python's standard library is required. Run from any directory:
    python3 tools/generate_samples.py
"""

from array import array
from pathlib import Path
import math
import struct
import sys
import wave
import zlib


ROOT = Path(__file__).resolve().parents[1]
SAMPLE_RATE = 22050
DURATION = 32
TAU = 2 * math.pi

# Four eight-second voicings per piece; melody is an original two-second phrase.
# MIDI pitches keep the gently filtered voices comfortably below 1 kHz.
TRACKS = (
    {
        "slug": "soft-landing",
        "title": "Soft Landing",
        "chords": ((48, 55, 59, 62), (45, 52, 55, 59),
                   (41, 48, 52, 55), (43, 50, 55, 57)),
        "melody": (67, 64, 62, 59, 64, 67, 71, 67,
                   69, 67, 64, 60, 62, 67, 62, 59),
        "phase": 0.0,
    },
    {
        "slug": "blue-hour",
        "title": "Blue Hour",
        "chords": ((45, 52, 55, 59), (41, 48, 52, 55),
                   (48, 55, 59, 62), (43, 50, 54, 57)),
        "melody": (64, 67, 64, 59, 60, 64, 67, 64,
                   62, 64, 67, 71, 69, 66, 62, 59),
        "phase": 0.7,
    },
    {
        "slug": "afterglow",
        "title": "Afterglow",
        "chords": ((41, 48, 52, 55), (48, 55, 59, 62),
                   (43, 50, 55, 57), (45, 52, 55, 60)),
        "melody": (69, 67, 64, 60, 67, 71, 67, 64,
                   62, 67, 69, 67, 64, 60, 59, 57),
        "phase": 1.4,
    },
)


def frequency(midi_note):
    return 440.0 * 2 ** ((midi_note - 69) / 12)


def add_voice(samples, start, duration, note, amplitude, voice, phase=0.0):
    """Mix a quiet sine-based pad, bass, or rounded electric-piano voice."""
    start_sample = int(start * SAMPLE_RATE)
    count = min(int(duration * SAMPLE_RATE), len(samples) - start_sample)
    pitch = frequency(note)
    for offset in range(max(0, count)):
        time = offset / SAMPLE_RATE
        angle = TAU * pitch * time
        if voice == "pad":
            attack = min(1.0, time / 1.6)
            release = min(1.0, max(0.0, duration - time) / 2.5)
            envelope = math.sin(attack * math.pi / 2) ** 2 * release ** 2
            # Quiet detuning and a restrained second harmonic make a warm pad.
            tone = (0.72 * math.sin(angle + phase)
                    + 0.22 * math.sin(angle * 1.0013 - phase)
                    + 0.06 * math.sin(angle * 2 + phase))
            tone *= 0.94 + 0.06 * math.sin(TAU * 0.14 * time + phase)
        elif voice == "bass":
            envelope = min(1.0, time / 0.5) * min(1.0, (duration - time) / 1.7)
            tone = math.sin(angle) + 0.07 * math.sin(angle * 2)
        else:
            envelope = (1 - math.exp(-time / 0.025)) * math.exp(-time / 1.35)
            envelope *= min(1.0, max(0.0, duration - time) / 0.45)
            tone = (math.sin(angle + 0.45 * math.exp(-time / 0.4)
                             * math.sin(angle * 2))
                    + 0.09 * math.exp(-time / 0.3) * math.sin(angle * 3))
        samples[start_sample + offset] += amplitude * envelope * tone


def generate_track(track):
    samples = [0.0] * (SAMPLE_RATE * DURATION)
    for index, chord in enumerate(track["chords"]):
        for voice_index, note in enumerate(chord):
            add_voice(samples, index * 8, 10, note, 0.065, "pad",
                      track["phase"] + voice_index * 0.9)
        add_voice(samples, index * 8, 8.7, chord[0] - 12, 0.075, "bass")
        for step in range(8):
            # An understated one-note-per-second pulse anchors the piece.
            note = chord[(step * 3 + index) % len(chord)] + 12
            add_voice(samples, index * 8 + step + 0.35, 3.7, note,
                      0.027 if step % 2 else 0.036, "keys")
    for index, note in enumerate(track["melody"]):
        start = index * 2 + 0.12
        add_voice(samples, start, 4.3, note, 0.093, "keys")
        add_voice(samples, start + 0.57, 4.3, note, 0.023, "keys")
        add_voice(samples, start + 1.14, 4.3, note, 0.009, "keys")
    for index, sample in enumerate(samples):
        time = index / SAMPLE_RATE
        fade_in = min(1.0, time / 1.4)
        fade_out = min(1.0, (DURATION - time) / 3.2)
        samples[index] = math.tanh(sample * 1.1) * fade_in * fade_out
    peak = max(abs(sample) for sample in samples)
    # Leave plenty of headroom for system volume and speaker amplification.
    pcm = array("h", (round(sample / peak * 0.72 * 32767) for sample in samples))
    if sys.byteorder != "little":
        pcm.byteswap()
    path = ROOT / "assets" / "audio" / (track["slug"] + ".wav")
    path.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(path), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(SAMPLE_RATE)
        output.writeframes(pcm.tobytes())
    rms = math.sqrt(sum(sample * sample for sample in samples) / len(samples))
    print(f'{track["title"]}: {DURATION:.1f}s, {SAMPLE_RATE} Hz, mono PCM16; '
          f'peak -2.85 dBFS, RMS {20 * math.log10(rms / peak * 0.72):.2f} dBFS')


def inside_polygon(x, y, vertices):
    inside = False
    previous = vertices[-1]
    for current in vertices:
        ax, ay = previous
        bx, by = current
        if (ay > y) != (by > y) and x < (bx - ax) * (y - ay) / (by - ay) + ax:
            inside = not inside
        previous = current
    return inside


def icon_pixel(x, y):
    # Four-pixel inset, smooth iOS-style rounded silhouette.
    dx = max(32 - x, x - 96, 0)
    dy = max(32 - y, y - 96, 0)
    if dx * dx + dy * dy > 28 * 28:
        return (0, 0, 0, 0)
    position = min(1, max(0, (y - 4) / 120))
    background = (255, round(91 - 32 * position), round(109 - 20 * position), 255)
    # White connected eighth notes; sloped beam, vertical stems, tilted heads.
    beam = ((57, 36), (91, 28), (91, 43), (64, 49), (64, 83),
            (57, 85))
    right_stem = ((84, 39), (91, 37), (91, 76), (84, 78))
    left_head = ((x - 52) / 12.5) ** 2 + ((y - 84 + 0.26 * (x - 52)) / 8.5) ** 2
    right_head = ((x - 79) / 12.5) ** 2 + ((y - 77 + 0.26 * (x - 79)) / 8.5) ** 2
    if (inside_polygon(x, y, beam) or inside_polygon(x, y, right_stem)
            or left_head <= 1 or right_head <= 1):
        return (255, 255, 255, 255)
    return background


def png_chunk(kind, content):
    return struct.pack(">I", len(content)) + kind + content + struct.pack(
        ">I", zlib.crc32(kind + content) & 0xffffffff)


def generate_icon():
    size, supersample = 128, 4
    scanlines = bytearray()
    for y in range(size):
        scanlines.append(0)
        for x in range(size):
            subpixels = [icon_pixel(x + (sx + 0.5) / supersample,
                                    y + (sy + 0.5) / supersample)
                         for sy in range(supersample) for sx in range(supersample)]
            alpha_sum = sum(pixel[3] for pixel in subpixels)
            for channel in range(3):
                scanlines.append(round(sum(p[channel] * p[3] for p in subpixels)
                                       / alpha_sum) if alpha_sum else 0)
            scanlines.append(round(alpha_sum / len(subpixels)))
    png = (b"\x89PNG\r\n\x1a\n"
           + png_chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0))
           + png_chunk(b"IDAT", zlib.compress(scanlines, 9))
           + png_chunk(b"IEND", b""))
    path = ROOT / "assets" / "app-icon.png"
    path.write_bytes(png)
    print("App icon: 128 × 128 RGBA PNG")


if __name__ == "__main__":
    for track in TRACKS:
        generate_track(track)
    generate_icon()
