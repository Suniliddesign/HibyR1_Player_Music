# Sonix Lumia interface

Run from Ubuntu/WSL, in the repository root:

```sh
bash tools/run-lumia-host.sh
```

The launcher builds the host target, copies the bundled R1 resources (including
languages, fonts and model information), and uses `test-music/` as the simulated
card. Settings persist in `test-music/.local/device_config.ini`. Complete the
language/time setup on the first launch. Put your music in `test-music/`, then
open Music > scan music. To use an existing folder:

```sh
SONIX_SD_ROOT="$HOME/Music" bash tools/run-lumia-host.sh
```

The Lumia presentation includes Start, Music (albums/artists/songs), Now Playing
and Settings. Artwork and metadata come from the real library. Play/pause,
previous/next, seeking and the Off/15/30/60-minute music sleep timer use Sonix's
existing services. Tap Settings > Sleep timer to cycle the timer.

Advanced audio settings, album track lists, streaming, Bluetooth and other tools
retain their existing Sonix pages. The new primary screens use black/cyan and
the bundled UI font; this is a Lumia-inspired interface, not Windows Phone or
Segoe UI. Device-only radio, power and audio hardware are not emulated by SDL.

Source: `sonix-player/src/gui/shell/lumia.c`.
