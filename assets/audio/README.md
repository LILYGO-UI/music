# Little Moments — demo audio

The three Little Moments files are original, procedurally composed and
synthesized examples for this music player. The Espressif ADF files below are
third-party test samples downloaded from the linked Espressif documentation.

| File | Title | Artist | Album | Duration |
| --- | --- | --- | --- | --- |
| `soft-landing.wav` | Soft Landing | LILYGO Sessions | Little Moments | 0:32 |
| `blue-hour.wav` | Blue Hour | LILYGO Sessions | Little Moments | 0:32 |
| `afterglow.wav` | Afterglow | LILYGO Sessions | Little Moments | 0:32 |

All files use uncompressed 16-bit little-endian mono PCM at 22,050 Hz. Their warm
sine-based chord pads, quiet bass, and electric-piano melodies have gentle
attacks and a final fade. Each track peaks at approximately −2.85 dBFS, leaving
headroom without clipping.

To reproduce all three WAV files and the 128 × 128 RGBA app icon, run:

```sh
python3 tools/generate_samples.py
```

The generator uses only the Python standard library. Track titles, artist, and
album are provided by the player's built-in library; WAV files do not contain
embedded cover art or metadata tags.

## Espressif ADF two-channel samples

The `ff-16b-2c-44100hz.*` files are the long two-channel samples listed in the
[Espressif ADF Audio Samples](https://docs.espressif.com/projects/esp-adf/en/latest/audio-samples.html#two-channel-audio)
table. They are kept in `assets/audio/` for development and are intentionally
not installed by the Debian package. For device playback tests, upload them to
the target user's `Music` directory.

The source page identifies the original music as “Furious Freak” and “Galway”
by Kevin MacLeod, licensed under [Creative Commons Attribution 3.0]
(http://creativecommons.org/licenses/by/3.0/).
