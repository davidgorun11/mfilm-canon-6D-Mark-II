# M-Film on the Canon EOS 6D Mark II (firmware 1.1.1)

This branch (`6d2`) is a fork of [HNGSeries/mfilm-canon-film-simulation](https://github.com/HNGSeries/mfilm-canon-film-simulation)
for one camera: the 6D Mark II, a DIGIC 7 body that Magic Lantern runs on only experimentally. The film OS
sources in `ml-dev/filmos` and `ml-dev/fork/src` are the ones that run on the camera. They come from the
0.1.x line, with parts of 0.2.0 ported in (see below). They aren't tested on any other camera.

## Build

```
docker build -t mfilm-build ml-dev/fork/ml-patches/6D2.111
docker run --rm -v "$PWD:/w" -w /w mfilm-build bash ml-dev/build_fork.sh 6D2.111
```

The card files end up in `ml-dev/out/fork-6D2.111/`. Install them as `ml-dev/CAMERA-README.txt` says.

| File | What it is |
|---|---|
| `0001-6D2-film-mode.patch` | the Magic Lantern changes for the 6D Mark II, on the base `ed3e7c0`: hooks, an RGBA bitmap palette of 256 entries, symbols the module needs, property whitelist |
| `module.mk` | the film OS module's Makefile for the Cortex-A9 (ARM mode, `-mword-relocations`); the maths come from `autoexec.bin` through `6D2_111.sym` |
| `Dockerfile` | everything `build_fork.sh` needs |

## What's different from upstream

- **Speed.**
  - RAW is read through a 256 KB buffer with cache invalidation: 11.5 s down to about 1.6 s.
  - Big prints store portrait frames transposed.
  - LiveView pauses while a roll develops.
  - On the camera, frames develop in Small about 10–12 s, Medium 21–25 s, Large 36–39 s.
- **Memory at boot.** The film OS is a module, so boot memory drops from 549 KB to 380 KB.
- **From 0.2.0.**
  - Three print sizes (Small, Medium, Large) with zoom in Prints.
  - A date stamp.
  - The darkroom engine (`fos_dev.c`) itself.
- **Without LiveView.** M-Film opens without LiveView (PLAY mode).
- **LiveView in the darkroom.** LiveView pauses for the darkroom and is left off afterwards.
- **Darkroom clock.** The second hand ticks once a second.
- **Buttons.** They do the same thing on every screen:

  | Button | Action |
  |---|---|
  | MENU | back |
  | PLAY | Prints |
  | Trash | camera (it deletes marked rolls only in the Darkroom list) |
  | INFO | the screen's second action |
  | SET | select |
  | Half-shutter, LV | camera |

- **Fixes.**
  - The roll list shows dates.
  - No leftovers on screen after leaving Prints with the shutter.
