# Explore2D visual system

Explore2D deliberately behaves like a small adventure-game console. Games may
draw different places and choose different combinations of the palette, but do
not replace the display model.

## Fixed display

- Logical framebuffer: 640×350 pixels.
- Room-local coordinate space: 492×262 pixels.
- Room origin on the framebuffer: `(8,8)`.
- Integer point-sampled presentation through CNA.
- No scrolling camera and no arbitrary-resolution game art.

`ScreenMetrics` exposes these values. Room geometry, collisions, hotspots and
decorations use room-local coordinates; `AdventureRenderer` applies the screen
offset.

## EGA palette

All game and interface drawing uses `PaletteColor`, whose indices reproduce the
16 default EGA colours used by QBasic `SCREEN 9`:

```text
0 black        4 red          8 dark gray      12 bright red
1 blue         5 magenta      9 bright blue    13 bright magenta
2 green        6 brown       10 bright green   14 bright yellow
3 cyan         7 light gray  11 bright cyan    15 white
```

The RGBA conversion exists only at the final framebuffer boundary.

## Code-drawn art

Room hotspot artwork, room decoration and title artwork use the same `Visual`
variant:

```cpp
using namespace explore2d;

room.decorations = {
    RectVisual{{0, 190, 492, 72}, PaletteColor::green, true},
    LineVisual{{20, 190}, {180, 70}, PaletteColor::white},
    CircleVisual{{420, 45}, 18, PaletteColor::brightYellow, true},
    EllipseVisual{{80, 170}, {25, 50}, PaletteColor::brightGreen, true},
    TextVisual{{205, 220}, "TRAIL", PaletteColor::brightYellow, 1},
};
```

`Canvas` additionally exposes `pixel()` and `paint()` for direct QBasic-like
plotting and boundary fill. Visuals have `PixelVisual` and `PaintVisual`
equivalents, so those operations can remain declarative in a world definition.

## Title configuration

`WorldDefinition::presentation.title` contains:

- background and border palette indices;
- the repeating colours used to draw the game title;
- subtitle and byline;
- NEW GAME, LOAD GAME and QUIT wording;
- procedural title artwork.

The host opens on this screen. Arrow keys move through its three choices and
Enter confirms. Starting or loading transitions into the shared game layout.
