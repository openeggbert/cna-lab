# MeshWorld — Optional AI Generation

**Status as of 2026-07-11: entirely unimplemented.** None of `config/ai.local.json`,
a Claude API adapter, or `ai_requested`/`ai_assisted` wiring exist in this codebase
(confirmed during the procedural-model-generator-roadmap infra pass — see
`docs/procedural-model-generator-roadmap.md`). Whether to build this BYOK-optional
*runtime* generation feature at all remains a scope decision for the user
(NEXT.md §8/§9), not something to self-start.

This document describes that unbuilt runtime feature specifically — a live API
call made WHILE generating a chunk. It is unrelated to, and does not conflict
with, an AI coding assistant (e.g. Claude Code) writing new `.lua` generator
FILES offline, ahead of time, as ordinary development work (exactly how every
generator added in this session was written) — those files are then reviewed,
tested, and committed like any other source change, with no runtime API
dependency at all. The project's standing rule ("no new runtime AI/Claude API
calls, permanently") is about the feature this document describes, not about
how the generator source code itself gets written.

## Default state: disabled

AI-assisted chunk generation is **disabled by default**. MeshWorld works fully without it. The C++ procedural generator is the primary implementation and guaranteed fallback.

---

## When AI is and is NOT used

**AI is appropriate for:**
- Landmark buildings (cathedral, castle, skyscraper, monument) — one-of-a-kind structures that are impractical to template
- Creative one-off layouts (unusual plazas, festival areas, special quest locations)
- Cases where a specific chunk is marked `ai_requested = true` by a game designer

**AI is NOT appropriate for:**
- Normal houses — use `HouseGenerator` (C++ procedural)
- Roads — use `RoadGenerator`
- Trees, benches, lamp posts, fences — use the corresponding object generators
- Every chunk in a city — the vast majority of chunks should be pure C++

> The procedural generator must produce visually complete, high-quality output for every normal chunk type. AI is a special-case enhancer, not a crutch.

---

## BYOK — Bring Your Own Key

To enable AI generation, a user must:

1. Obtain an API key from Anthropic (Claude API).
2. Create a local config file: `config/ai.local.json` (gitignored).
3. Set `"enabled": true` in that file.

Example `config/ai.local.json`:

```json
{
  "enabled": true,
  "provider": "claude",
  "api_key": "sk-ant-...",
  "model": "claude-sonnet-4-6",
  "max_retries": 3,
  "fallback_to_procedural": true
}
```

**Never commit API keys. Never hardcode them. `config/` is gitignored.**

---

## No public AI server

MeshWorld does not provide a public server that calls a paid AI API on behalf of anonymous users. If a future hosted server is built, it will use pre-generated chunk caches — not real-time AI calls.

---

## AI generation pipeline

When AI generation is enabled and a chunk with `ai_requested = true` is needed:

```
ChunkContext (zone, region, style, seed, neighbors, constraints, ai_requested=true)
     |
     v
C++ procedural generator  → base mc3.xml  (always runs first)
     |
     v
PromptBuilder.build(context, base_mc3_xml) → structured prompt
  "Here is the procedural base. Modify or enhance it for a landmark X."
     |
     v
ClaudeAdapter.generate(prompt) → modified mc3.xml
     |
     v
MC3Validator.validate(result) → ok or error
     |
     v
  [success] -> store to cache
  [failure] -> retry (up to max_retries, with correction hint appended)
  [all retries failed] -> use the C++ procedural base, log failure
```

AI always receives the C++ base MC3 as context. It modifies or enhances it — it does not generate from scratch.

---

## Prompt structure

Prompts are structured and constrained. AI modifies the existing procedural scene.

Example prompt:

```
Below is a procedural MC3 chunk for a city park (64m×64m).
Enhance it to be a notable landmark park with a decorative arch entrance.
Do NOT remove existing objects. ADD new objects only.

Constraints:
- Keep all existing object IDs unchanged.
- Do not place objects outside chunk bounds (0..64m on X and Z).
- Maximum 120 objects total.
- No characters or NPCs.
- Return valid mc3.xml only.

Existing procedural output:
[base mc3.xml inserted here]
```

---

## MC3 validation

Every AI-generated MC3 must pass:

1. XML well-formedness.
2. `<mc3>` root present.
3. All objects within `[0, chunk_size_m]` on X and Z.
4. Object count ≤ `GeneratorConstraints::max_objects`.
5. `<metadata>` tag present with `ai_assisted: true`.
6. No character/NPC object types.

On failure: append error description to the next retry prompt. After `max_retries`, fall back to the C++ procedural base.

---

## What AI is and is not

AI is an **optional, special-case enhancer**. It is not:

- Required for basic functionality
- A revenue source
- A replacement for C++ generators
- The right tool for normal city blocks, roads, parks, or houses

The C++ generator is always the foundation. AI makes one-of-a-kind structures look unique when it works.
