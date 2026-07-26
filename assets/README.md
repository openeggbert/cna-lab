# P1 icon atlas

`p1-icon-atlas.png` is a transparent, one-colour RGBA mask of the eight face
pictograms in the user-supplied `tamagotchi.png` reference image. It contains
four top-band icons followed by four bottom-band icons.  It deliberately has
no white backing pixels, including around the anti-aliased source outlines.

The application does not use the reference photograph or its LCD background at
runtime. CNA tints the same mask muted grey for inactive icons and near-black
for a selected or urgent icon.
