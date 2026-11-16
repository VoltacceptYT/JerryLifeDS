# Jerry Life (Nintendo DS)

Port of Jerry Life (the island-life sim in `JerryLife/`) to the Nintendo DS using devkitARM + libnds,
built and converted with the same workflow as JerryClickerDS.

Top screen: the scrolling island (day/night + seasons). Bottom screen: touch panels (residents, items, shop, town hall, news, menu).
Both screens are software-rendered (portable C), so the simulation, world and UI can be tested on a PC.

## Controls
| Input | Action |
|---|---|
| D-pad (hold **Y** = fast) | Scroll the island / move the focus in panels |
| **A** | Use whatever is under the crosshair / press the focused button |
| **B** | Deselect / back |
| **L** / **R** / **X** | News / Items / Residents |
| **Start** | Menu (or back to the map) |
| **Select** | Music on/off |
| Stylus | Everything: minimap jump/drag, tap residents and buildings, buttons |

Saves go to `JerryLife.sav` next to the ROM (needs DLDI/FAT, autosaves every minute). Without storage the game still runs.

## Building
Push to GitHub: `.github/workflows/Build.yml` converts the graphics and music, builds `JerryLife.nds` and uploads it as an artifact.
Locally: `python3 tools/convert_assets.py` (Pillow), `python3 tools/convert_music.py --rate 22050` (ffmpeg + numpy), then `make`.

- `tools/convert_assets.py` regenerates `source/gfxdata.*`, `source/atlasdata.*`, `source/itemdata.*`, `assets/gen/atlas.bin` and `icon.bmp` from `assets/` (sprites come from `assets/jerrylife/atlas_*.png` + `manifest.json`).
- `tools/convert_music.py` turns `assets/music/JerryUnited.mp3` into DS IMA-ADPCM (`source/music_data.s` embeds it).

## Host tests (no DS needed)
```
sh tools/host/build_host.sh tools/host/world_test.c /tmp/wt && /tmp/wt   # world render -> /tmp/world.raw
sh tools/host/build_host.sh tools/host/ui_test.c    /tmp/ut && /tmp/ut   # UI + save round-trip -> /tmp/ui.raw
python3 tools/host/dump2png.py /tmp/ui.raw /tmp/ui.png
```

## Layout
`jl.c` simulation + save format | `map.c` terrain/buildings | `world.c` world renderer | `ui.c` panels | `sprites.c`/`gfx.c` drawing | `main.c` DS glue (video, input, saving) | `sfx.c` PSG sound effects | `audio.c` music

## Credits
Game: VoltacceptYT | Music: VO!DWiNG | Built on the JerryClickerDS toolchain
