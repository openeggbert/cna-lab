# P1 icon atlases

`p1-icon-atlas.png` is the earlier transparent, one-colour atlas. It is kept
only as a legacy comparison asset and is not loaded at runtime.

`p1-icon-atlas-smooth.png` is the runtime atlas of the eight face pictograms
in the user-supplied P1 visual reference: Food, Light, Game, Medicine; Toilet,
Health, Discipline, and Attention. Each pictogram was separately cropped from
the reference, isolated as a transparent black mask, scaled with Lanczos
filtering, and centred in an equal 152 x 144 cell. The upper-row crops start
below the photographed top bezel and the bottom-row crops stop above the lower
bezel, so no border pixels enter the icons. The
4 x 2 atlas is 608 x 288 pixels.

Transparent pixels contain no white backing, so the independently coloured
upper and lower LCD bands remain visible. CNA draws every icon at the same
destination size. A selected or urgent icon uses a separate small cursor; the
icon bitmap itself is not given a coloured cell background.
