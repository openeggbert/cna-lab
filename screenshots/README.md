# MeshWorld — Sample Output

This directory contains sample MC3 XML chunk files generated from `examples/world.json`.
Load them in MeshCraft viewer to render the 3D scene.

## Included samples

| File | Region | Coordinates |
|------|--------|-------------|
| `chunk_park_2_9.mc3.xml`       | Park (fountain, benches, trees, flower beds) | (2, 9) |
| `chunk_road_3_3.mc3.xml`       | Road (asphalt, sidewalks, lamp posts, curbs) | (3, 3) |
| `chunk_square_9_9.mc3.xml`     | Town square (cobblestones, fountain, seating) | (9, 9) |
| `chunk_river_bank_0_0.mc3.xml` | River bank (water, embankment, railing)       | (0, 0) |

## How to view

```bash
# Export all 400 chunks to a directory
./build/MeshWorldExport examples/world.json output/

# Open any .mc3.xml file in the MeshCraft viewer
# (openeggbert/mesh-craft must be built and installed separately)
meshcraft output/2_9.mc3.xml
```

## World map (ASCII)

```
./build/MeshWorldMap examples/world.json
```
