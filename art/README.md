# Artwork

## `boeing-737-800-silhouette.png`

The boot splash's logo: a top view of a Boeing 737-800.

- **Work:** "Boeing 737-800 silhouette", by Peter James Lowden, 2016
- **Source:** <https://commons.wikimedia.org/wiki/File:Boeing_737-800_silhouette.svg>. This
  file is Wikimedia Commons' 960 px rendering of that SVG, saved as PNG.
- **Licence:** [Creative Commons Attribution-ShareAlike 4.0 International (CC BY-SA 4.0)](https://creativecommons.org/licenses/by-sa/4.0/)

The firmware carries an adaptation of it. `tools/make_logo.py` crops the image to the
aircraft, scales it to each screen, flattens it onto the panel background and reduces it to
RGB565. The result is in `Annunciator/Splash/Logo320.h` and `Logo480.h`, and those headers
are licensed CC BY-SA 4.0 as well. The splash credits the author and the licence on screen.

The share-alike condition covers the image and the adaptations of it, not the firmware
around it. The rest of this repository is under its own [MIT licence](../LICENSE).
