# DSOBD instrument graphics and sound

`Barlow-SemiBold.ttf` and `BarlowCondensed-SemiBold.ttf` are from [Google Fonts' Barlow family](https://github.com/google/fonts/tree/main/ofl/barlow) and [Barlow Condensed family](https://github.com/google/fonts/tree/main/ofl/barlowcondensed), by Jeremy Tribby, under the SIL Open Font License in `FONT-LICENSE.txt`. The normal face supplies labels; the condensed face supplies large numeral sprites. No system fonts are required.

`tools/build_art.py` draws a metal dial and subtle instrument-deck texture, and rasterizes font masks. All resulting data lives in `source/dtec_assets.c` with declarations in `include/dtec_assets.h`. Regenerate with Python 3 and Pillow; generated data is checked in for reproducible ROM builds without external artwork.

`source/dtec_audio.c` synthesizes four original, short enveloped PCM cues once at startup. There are no third-party sound samples or playback dependencies beyond the existing libnds ARM7 sound service. Playback uses a single reserved channel and no blocking delays. Implementation references: [BlocksDS audio guide](https://blocksds.skylyrac.net/tutorial/basic/audio/) and [filesystem guide](https://blocksds.skylyrac.net/docs/guides/filesystem/).
