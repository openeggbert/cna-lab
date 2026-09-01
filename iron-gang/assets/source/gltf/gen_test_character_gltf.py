#!/usr/bin/env python3
"""Generates assets/source/gltf/test_character.gltf for Iron Gang gate M6.

Hand-authored (bypasses Mesh Craft/MC3, which has no rigging/skinning concept at all --
see plan/plan_10-gltf-cnj-mcb-and-runtime-packages.md IG-10-004b's sibling note in
plan_39/analysis.md for the equivalent M6 deviation record). A minimal blocky humanoid:
a torso+head box rigid to a Root bone, and two leg boxes each rigid to their own hip-pivot
bone, with an "Idle" (static) and "Walk" (alternating leg-swing) clip -- enough to prove
Iron Gang's skinned-character pipeline end to end (MC3-bypass glTF -> cna_tool_gltf_to_cnj
-> CNJ -> cna-extended's ModelAnimationComponentEXT/ModelAnimationSystem3DEXT) without
claiming to be final character art.

Run directly: python3 gen_test_character_gltf.py > test_character.gltf
"""
import base64
import json
import math
import struct

FLOAT = 5126
USHORT = 5123

buf = bytearray()


def add(fmt, *values):
    offset = len(buf)
    packed = struct.pack("<" + fmt, *values)
    buf.extend(packed)
    return offset, len(packed)


def add_vec3_array(vectors):
    start = len(buf)
    for v in vectors:
        buf.extend(struct.pack("<3f", *v))
    return start, len(buf) - start


def add_vec2_array(vectors):
    start = len(buf)
    for v in vectors:
        buf.extend(struct.pack("<2f", *v))
    return start, len(buf) - start


def add_ushort4_array(vectors):
    start = len(buf)
    for v in vectors:
        buf.extend(struct.pack("<4H", *v))
    return start, len(buf) - start


def add_vec4_array(vectors):
    start = len(buf)
    for v in vectors:
        buf.extend(struct.pack("<4f", *v))
    return start, len(buf) - start


def box_geometry(min_corner, max_corner):
    """8 corners is not enough for flat-shaded normals, so emit 24 verts (4 per face,
    6 faces), 36 indices (2 tris per face) -- a standard non-shared-normal box."""
    x0, y0, z0 = min_corner
    x1, y1, z1 = max_corner
    faces = [
        # (normal, 4 corners in CCW winding when viewed from outside along +normal)
        ((0, 0, 1), [(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)]),
        ((0, 0, -1), [(x1, y0, z0), (x0, y0, z0), (x0, y1, z0), (x1, y1, z0)]),
        ((0, 1, 0), [(x0, y1, z1), (x1, y1, z1), (x1, y1, z0), (x0, y1, z0)]),
        ((0, -1, 0), [(x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1)]),
        ((1, 0, 0), [(x1, y0, z1), (x1, y0, z0), (x1, y1, z0), (x1, y1, z1)]),
        ((-1, 0, 0), [(x0, y0, z0), (x0, y0, z1), (x0, y1, z1), (x0, y1, z0)]),
    ]
    positions = []
    normals = []
    uvs = []
    indices = []
    for normal, corners in faces:
        base = len(positions)
        for i, corner in enumerate(corners):
            positions.append(corner)
            normals.append(normal)
            uvs.append([(0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0)][i])
        indices.extend([base, base + 1, base + 2, base, base + 2, base + 3])
    return positions, normals, uvs, indices


def make_primitive(min_corner, max_corner, joint_index):
    positions, normals, uvs, indices = box_geometry(min_corner, max_corner)
    vertex_count = len(positions)
    joints = [[joint_index, 0, 0, 0]] * vertex_count
    weights = [[1.0, 0.0, 0.0, 0.0]] * vertex_count

    pos_off, pos_len = add_vec3_array(positions)
    norm_off, norm_len = add_vec3_array(normals)
    uv_off, uv_len = add_vec2_array(uvs)
    joints_off, joints_len = add_ushort4_array(joints)
    weights_off, weights_len = add_vec4_array(weights)

    idx_off = len(buf)
    for i in indices:
        buf.extend(struct.pack("<H", i))
    idx_len = len(buf) - idx_off

    xs = [p[0] for p in positions]
    ys = [p[1] for p in positions]
    zs = [p[2] for p in positions]

    return {
        "positions": (pos_off, pos_len, vertex_count, [min(xs), min(ys), min(zs)], [max(xs), max(ys), max(zs)]),
        "normals": (norm_off, norm_len, vertex_count),
        "uvs": (uv_off, uv_len, vertex_count),
        "joints": (joints_off, joints_len, vertex_count),
        "weights": (weights_off, weights_len, vertex_count),
        "indices": (idx_off, idx_len, len(indices)),
    }


# Bones: Root (hip pivot, y=0.9), LeftLeg (hip pivot, x=-0.2,y=0.9), RightLeg (x=0.2,y=0.9).
# Torso+head box (rigid to Root): y in [0.9, 1.7]. Legs: y in [0.0, 0.9] each side.
torso = make_primitive((-0.25, 0.9, -0.15), (0.25, 1.7, 0.15), 0)
left_leg = make_primitive((-0.3, 0.0, -0.15), (-0.1, 0.9, 0.15), 1)
right_leg = make_primitive((0.1, 0.0, -0.15), (0.3, 0.9, 0.15), 2)

# Inverse bind matrices (column-major 4x4, matching glTF convention): inverse of each
# joint's bind-pose GLOBAL translation. Root has no parent, so its global == its local.
def inverse_translation_matrix(t):
    x, y, z = t
    # Column-major 4x4 identity with translation -t in the last column.
    return [
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        -x, -y, -z, 1,
    ]


root_inv = inverse_translation_matrix((0.0, 0.9, 0.0))
left_leg_inv = inverse_translation_matrix((-0.2, 0.9, 0.0))
right_leg_inv = inverse_translation_matrix((0.2, 0.9, 0.0))
ibm_off, ibm_len = add_vec4_array(
    [tuple(root_inv[i:i + 4]) for i in range(0, 16, 4)]
    + [tuple(left_leg_inv[i:i + 4]) for i in range(0, 16, 4)]
    + [tuple(right_leg_inv[i:i + 4]) for i in range(0, 16, 4)]
)


def quat_x(angle_rad):
    return [math.sin(angle_rad / 2.0), 0.0, 0.0, math.cos(angle_rad / 2.0)]


def quat_z(angle_rad):
    return [0.0, 0.0, math.sin(angle_rad / 2.0), math.cos(angle_rad / 2.0)]


SWING = math.radians(25.0)
DIALOGUE_ANGLE = math.radians(8.0)

# "Idle": both legs held at rest (identity rotation) for a 1-second clip.
idle_times_off, idle_times_len = add("2f", 0.0, 1.0)
idle_identity_off, idle_identity_len = add_vec4_array([(0, 0, 0, 1), (0, 0, 0, 1)])

# "Walk": 1-second cycle, legs alternate phase (0 -> 0.5 -> 1.0, LINEAR, loops).
walk_times_off, walk_times_len = add("3f", 0.0, 0.5, 1.0)
walk_left_off, walk_left_len = add_vec4_array([quat_x(SWING), quat_x(-SWING), quat_x(SWING)])
walk_right_off, walk_right_len = add_vec4_array([quat_x(-SWING), quat_x(SWING), quat_x(-SWING)])

# "Turn": a shuffling pivot -- the legs alternate like Walk but with a shorter swing and twice the
# cadence (a half-second cycle), which is what turning on the spot looks like: quick small steps
# rather than a stride. Distinct from Walk on purpose, since a pedestrian reversing at the end of a
# pavement is not translating, and playing Walk there slides the cycle across the ground.
TURN_SWING = SWING * 0.45
turn_times_off, turn_times_len = add("3f", 0.0, 0.25, 0.5)
turn_left_off, turn_left_len = add_vec4_array(
    [quat_x(TURN_SWING), quat_x(-TURN_SWING), quat_x(TURN_SWING)])
turn_right_off, turn_right_len = add_vec4_array(
    [quat_x(-TURN_SWING), quat_x(TURN_SWING), quat_x(-TURN_SWING)])

# "Dialogue": a static "parade rest" stance -- legs angled slightly toward each other about the
# local Z axis (a different axis than Walk's X-axis swing, so it reads as a distinct pose, not
# just a frozen mid-walk frame), held for a 1-second clip like Idle.
dialogue_times_off, dialogue_times_len = add("2f", 0.0, 1.0)
dialogue_left_off, dialogue_left_len = add_vec4_array([quat_z(DIALOGUE_ANGLE), quat_z(DIALOGUE_ANGLE)])
dialogue_right_off, dialogue_right_len = add_vec4_array([quat_z(-DIALOGUE_ANGLE), quat_z(-DIALOGUE_ANGLE)])

# "EnterVehicle"/"ExitVehicle": both legs bend forward TOGETHER (unlike Walk's alternating
# phase), as if sitting into/standing up from a car seat. Authored as a 1-second clip but only
# ever played for IronGangGame::kVehicleTransitionSeconds (0.5s, half the clip) before the
# game switches away -- deliberately so the motion is still visibly *in progress* (not already
# holding its end pose) at the moment playerDriving_ flips, and so LoopEXT's default-true modulo
# wraparound never has a chance to trigger (see the "boundary" gotcha noted in
# ModelAnimationSystem3DEXTTests.cpp, cna-extended).
SIT_ANGLE = math.radians(60.0)
enter_times_off, enter_times_len = add("2f", 0.0, 1.0)
enter_legs_off, enter_legs_len = add_vec4_array([(0, 0, 0, 1), quat_x(SIT_ANGLE)])  # standing -> sitting

exit_times_off, exit_times_len = add("2f", 0.0, 1.0)
exit_legs_off, exit_legs_len = add_vec4_array([quat_x(SIT_ANGLE), (0, 0, 0, 1)])  # sitting -> standing

# A trivial 1x1 white PNG -- verbatim bytes from cna's own proven-good kTexturedSkinnedGltf/
# kSkinnedAnimatedGltf test fixtures. Needed because a skinned mesh with no material at all
# still gets TextureEnabled=true from cna's importer, which then requires a real bound
# texture -- confirmed by an actual runtime crash ("TextureEnabled=true but texture0 is
# null") when these primitives had no material/texture at all.
_white_1x1_png = base64.b64decode(
    "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAADElEQVR4nGP4z8AAAAMBAQDJ/pLvAAAAAElFTkSuQmCC"
)
png_off = len(buf)
buf.extend(_white_1x1_png)
png_len = len(buf) - png_off

# IMPORTANT: buffer_b64 must be computed only after every add_*()/buf.extend() call above --
# it snapshots buf's current bytes.
buffer_b64 = base64.b64encode(bytes(buf)).decode("ascii")

buffer_views = []


def bv(offset, length):
    buffer_views.append({"buffer": 0, "byteOffset": offset, "byteLength": length})
    return len(buffer_views) - 1


accessors = []


def acc(bv_index, component_type, count, atype, minmax=None):
    entry = {"bufferView": bv_index, "componentType": component_type, "count": count, "type": atype}
    if minmax:
        entry["min"], entry["max"] = minmax
    accessors.append(entry)
    return len(accessors) - 1


def add_primitive_accessors(part):
    pos_off, pos_len, count, pmin, pmax = part["positions"]
    norm_off, norm_len, _ = part["normals"]
    uv_off, uv_len, _ = part["uvs"]
    joints_off, joints_len, _ = part["joints"]
    weights_off, weights_len, _ = part["weights"]
    idx_off, idx_len, idx_count = part["indices"]

    pos_acc = acc(bv(pos_off, pos_len), FLOAT, count, "VEC3", (pmin, pmax))
    norm_acc = acc(bv(norm_off, norm_len), FLOAT, count, "VEC3")
    uv_acc = acc(bv(uv_off, uv_len), FLOAT, count, "VEC2")
    joints_acc = acc(bv(joints_off, joints_len), USHORT, count, "VEC4")
    weights_acc = acc(bv(weights_off, weights_len), FLOAT, count, "VEC4")
    idx_acc = acc(bv(idx_off, idx_len), USHORT, idx_count, "SCALAR")

    return {
        "attributes": {
            "POSITION": pos_acc,
            "NORMAL": norm_acc,
            "TEXCOORD_0": uv_acc,
            "JOINTS_0": joints_acc,
            "WEIGHTS_0": weights_acc,
        },
        "indices": idx_acc,
    }


torso_prim = add_primitive_accessors(torso)
left_leg_prim = add_primitive_accessors(left_leg)
right_leg_prim = add_primitive_accessors(right_leg)
for prim in (torso_prim, left_leg_prim, right_leg_prim):
    prim["material"] = 0

image_bv = bv(png_off, png_len)

ibm_acc = acc(bv(ibm_off, ibm_len), FLOAT, 3, "MAT4")

idle_times_acc = acc(bv(idle_times_off, idle_times_len), FLOAT, 2, "SCALAR", ([0.0], [1.0]))
idle_identity_acc = acc(bv(idle_identity_off, idle_identity_len), FLOAT, 2, "VEC4")

walk_times_acc = acc(bv(walk_times_off, walk_times_len), FLOAT, 3, "SCALAR", ([0.0], [1.0]))
walk_left_acc = acc(bv(walk_left_off, walk_left_len), FLOAT, 3, "VEC4")
walk_right_acc = acc(bv(walk_right_off, walk_right_len), FLOAT, 3, "VEC4")

turn_times_acc = acc(bv(turn_times_off, turn_times_len), FLOAT, 3, "SCALAR", ([0.0], [0.5]))
turn_left_acc = acc(bv(turn_left_off, turn_left_len), FLOAT, 3, "VEC4")
turn_right_acc = acc(bv(turn_right_off, turn_right_len), FLOAT, 3, "VEC4")

dialogue_times_acc = acc(bv(dialogue_times_off, dialogue_times_len), FLOAT, 2, "SCALAR", ([0.0], [1.0]))
dialogue_left_acc = acc(bv(dialogue_left_off, dialogue_left_len), FLOAT, 2, "VEC4")
dialogue_right_acc = acc(bv(dialogue_right_off, dialogue_right_len), FLOAT, 2, "VEC4")

enter_times_acc = acc(bv(enter_times_off, enter_times_len), FLOAT, 2, "SCALAR", ([0.0], [1.0]))
enter_legs_acc = acc(bv(enter_legs_off, enter_legs_len), FLOAT, 2, "VEC4")

exit_times_acc = acc(bv(exit_times_off, exit_times_len), FLOAT, 2, "SCALAR", ([0.0], [1.0]))
exit_legs_acc = acc(bv(exit_legs_off, exit_legs_len), FLOAT, 2, "VEC4")

gltf = {
    "asset": {"version": "2.0", "generator": "iron-gang gen_test_character_gltf.py (hand-authored, bypasses MC3 -- see plan_39/plan_13 M6 notes)"},
    "scene": 0,
    "scenes": [{"nodes": [0, 3]}],
    "nodes": [
        {"name": "Root", "children": [1, 2], "translation": [0.0, 0.9, 0.0]},
        {"name": "LeftLeg", "translation": [-0.2, 0.0, 0.0]},
        {"name": "RightLeg", "translation": [0.2, 0.0, 0.0]},
        {"name": "CharacterMesh", "mesh": 0, "skin": 0},
    ],
    "meshes": [
        {
            "name": "test_character",
            "primitives": [torso_prim, left_leg_prim, right_leg_prim],
        }
    ],
    "materials": [{"pbrMetallicRoughness": {"baseColorTexture": {"index": 0}}}],
    "textures": [{"source": 0}],
    "images": [{"bufferView": image_bv, "mimeType": "image/png"}],
    "skins": [{"joints": [0, 1, 2], "inverseBindMatrices": ibm_acc, "name": "TestCharacterSkeleton"}],
    "animations": [
        {
            "name": "Idle",
            "samplers": [
                {"input": idle_times_acc, "output": idle_identity_acc, "interpolation": "LINEAR"},
                {"input": idle_times_acc, "output": idle_identity_acc, "interpolation": "LINEAR"},
            ],
            "channels": [
                {"sampler": 0, "target": {"node": 1, "path": "rotation"}},
                {"sampler": 1, "target": {"node": 2, "path": "rotation"}},
            ],
        },
        {
            "name": "Walk",
            "samplers": [
                {"input": walk_times_acc, "output": walk_left_acc, "interpolation": "LINEAR"},
                {"input": walk_times_acc, "output": walk_right_acc, "interpolation": "LINEAR"},
            ],
            "channels": [
                {"sampler": 0, "target": {"node": 1, "path": "rotation"}},
                {"sampler": 1, "target": {"node": 2, "path": "rotation"}},
            ],
        },
        {
            "name": "Turn",
            "samplers": [
                {"input": turn_times_acc, "output": turn_left_acc, "interpolation": "LINEAR"},
                {"input": turn_times_acc, "output": turn_right_acc, "interpolation": "LINEAR"},
            ],
            "channels": [
                {"sampler": 0, "target": {"node": 1, "path": "rotation"}},
                {"sampler": 1, "target": {"node": 2, "path": "rotation"}},
            ],
        },
        {
            "name": "Dialogue",
            "samplers": [
                {"input": dialogue_times_acc, "output": dialogue_left_acc, "interpolation": "LINEAR"},
                {"input": dialogue_times_acc, "output": dialogue_right_acc, "interpolation": "LINEAR"},
            ],
            "channels": [
                {"sampler": 0, "target": {"node": 1, "path": "rotation"}},
                {"sampler": 1, "target": {"node": 2, "path": "rotation"}},
            ],
        },
        {
            "name": "EnterVehicle",
            "samplers": [
                {"input": enter_times_acc, "output": enter_legs_acc, "interpolation": "LINEAR"},
            ],
            "channels": [
                {"sampler": 0, "target": {"node": 1, "path": "rotation"}},
                {"sampler": 0, "target": {"node": 2, "path": "rotation"}},
            ],
        },
        {
            "name": "ExitVehicle",
            "samplers": [
                {"input": exit_times_acc, "output": exit_legs_acc, "interpolation": "LINEAR"},
            ],
            "channels": [
                {"sampler": 0, "target": {"node": 1, "path": "rotation"}},
                {"sampler": 0, "target": {"node": 2, "path": "rotation"}},
            ],
        },
    ],
    "buffers": [{"byteLength": len(buf), "uri": "data:application/octet-stream;base64," + buffer_b64}],
    "bufferViews": buffer_views,
    "accessors": accessors,
}

print(json.dumps(gltf, indent=2))
