# Iron Gang's MC3 authoring conventions

Mesh Craft's XSD (`mesh-craft/mc3/mc3.xsd`) validates MC3's *grammar*. It deliberately leaves
several things open that a **project** has to close, and until 2026-08-26 Iron Gang had closed them
only by habit. `scripts/check_mc3_conventions.py` enforces them now, on every `ctest` run
(`iron_gang_mc3_conventions_tests`) and inside `scripts/build-assets.sh`.

## Collision is a role here, not a shape

| Value | Meaning |
| --- | --- |
| `none` | Decoration. No collider — road paint, a lamp's arm, a bin lid. |
| `static` | An immovable world collider. Walls, posts, bench frames. |
| `trigger` | A volume that reports overlap but does not block. |

**This is not MC3's own vocabulary, and the divergence is deliberate.** MC3 documents
`none / box / sphere / capsule / mesh / convex` — those describe the collider's **shape**. Every
collider in this game is an axis-aligned box (`PrototypeWorld`'s `Aabb`), so shape carries no
information, while "is this a wall, a trigger, or decoration?" carries all of it.

The schema cannot catch the difference: it types `collision` as `xs:string` with a default of
`"none"`, so *any* spelling validates. Iron Gang had been writing `static` and `trigger` across six
files with nothing checking them.

The values survive export verbatim as `node.extras.collision` in the generated glTF — 51 nodes carry
one in `warehouse_block_props.glb` — so a future collision importer reads the role directly and does
not have to guess it back from geometry.

**The cost, recorded so nobody rediscovers it:** Mesh Craft's own Walk Mode reads the *shape*
vocabulary and will not understand `static`. A file authored to be walked in the editor as well as
shipped in the game would need revisiting.

## Every geometry object states its collision

MC3 defaults `collision` to `"none"`. That makes "nobody thought about it" and "deliberately no
collider" indistinguishable, and only one of those is an authoring decision. So an object that omits
`collision` fails the check, including geometry nested inside a `<definition>` or a `<group>` — a
rule that stopped at the top level would be trivially avoided by wrapping.

## Units and axes

Every document declares `unit="meter"` and `coordinate_system="right_handed_y_up"`, and a `model`
name. These match the game's own world units, so nothing rescales on import.

## Repeated props: define once, instance many times

Use MC3's `<definitions>` / `<instance>` rather than copying geometry (plan_09 `IG-09-008`). The XSD
allows a `<definition>` exactly **one** child, so a multi-part prop wraps its parts in a `<group>`.

```xml
<definitions>
  <definition id="street_lamp">
    <group name="StreetLamp">
      <box name="Post" size="0.18 3.7 0.18" position="0 2.0 0" material="iron" collision="static"/>
      <box name="Head" size="0.55 0.26 0.4" position="0.8 3.72 0" material="lamp_glass" collision="none"/>
    </group>
  </definition>
</definitions>
<objects>
  <instance name="LampWest0" definition="street_lamp" position="-9 0 0"/>
  <instance name="LampEast0" definition="street_lamp" position="9 0 0" rotation="0 180 0"/>
</objects>
```

Placement is then authored content: the renderer draws the whole set with one call and does not know
where the benches are. It pays in the asset too — `mc3togltf` shares one glTF mesh per definition, so
22 instances cost roughly 108 triangles of unique geometry rather than 984.

## Budgets

Every MC3 source needs an entry in `assets/content-budgets.json`, and its recorded `baseline` must
equal what `content_budget.py` measures — budgets are two-sided, because a ceiling alone cannot
catch a counter that has stopped counting.

## What is still open

`collision` reaches the glTF and **nothing reads it back into the physics world**: the benches and
bins are walk-through. Importing collision proxies is plan_14 `IG-14-011`/`IG-14-012`, and this prop
set is the content that will exercise it. Naming, pivots, tags and layer conventions
(plan_09 `IG-09-006`) are only partly closed here — units, axes and the model name are enforced;
naming and pivots are not.
