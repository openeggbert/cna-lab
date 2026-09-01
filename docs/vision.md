# MeshWorld — Vision and Product Decisions

## What MeshWorld is

MeshWorld is a free, open-source, procedural city exploration demo. It uses MeshCraft formats (mc3.xml source, mcb runtime) to generate and stream city chunks as a player walks through a virtual city.

It is a technology demo, portfolio project, and open-source showcase — not a commercial product.

---

## Why MeshWorld is free and open-source

The core insight: the value of MeshWorld is in the technology, the formats, and the ecosystem — not in selling individual city chunks.

An open, reproducible, deterministic world generator demonstrates what MeshCraft can do. Charging per chunk would:

- make the demo inaccessible to most users
- introduce payment infrastructure complexity before the tech is ready
- create liability when the AI generator produces bad output
- undermine the portfolio/open-source purpose

---

## Why paid AI chunk generation was rejected for the first version

The original concept: users pay credits each time they enter an ungenerated chunk, funding the AI API call.

This was rejected for the following reasons:

**Technical risk.** AI output is non-deterministic and may fail validation. A paid product must deliver reliable output. The validator, retry logic, and fallback procedural generator must all be built first — before money changes hands.

**Infrastructure burden.** A public paid server requires payment processing, user accounts, credit balance tracking, fraud prevention, abuse rate limiting, and refund handling. This is a large engineering scope before the core generator even works.

**Cost unpredictability.** If the AI API is called for every new chunk and the world is large, the operator's API costs could exceed revenue quickly, especially during demos, bot traffic, or abuse.

**Lock-in to a specific AI provider.** Tying the product to a specific paid API before evaluating alternatives (local models, other providers) is premature.

**Fragility.** If the AI provider goes down or changes pricing, the entire product breaks. Procedural generation must be the robust foundation first.

---

## Why the project is still valuable without monetization

- Demonstrates procedural city generation using MeshCraft formats.
- Shows MC3 → MCB compilation and runtime streaming in practice.
- Provides an open reference implementation for MeshCraft consumers.
- Functions as a portfolio piece for the MeshCraft ecosystem.
- Can be used as a basis for future commercial or hosted offerings once stable.
- Enables community contributions — others can write chunk generators, styles, and tools.

---

## Future indirect monetization (explicitly out of scope now)

These options may be explored later. None are implemented or planned for the first version:

| Option | Notes |
|--------|-------|
| GitHub Sponsors / Patreon | Voluntary support from users who find the project valuable. |
| Paid hosted server | A managed instance where chunks are pre-generated; users pay for convenience, not for the generator itself. |
| Paid asset packs | Additional city styles, district types, or high-quality MCB chunk libraries. |
| MeshCraft Pro | A commercial version of the MeshCraft editor with advanced features. |
| BYOK AI tier | Premium tooling for users who supply their own AI API keys. |

**None of these are implemented. Do not add billing, Stripe, subscriptions, credits, virtual currency, or marketplace code.**

---

## Governing constraints

- No API keys in source code.
- No public server calling paid AI on behalf of anonymous users.
- No credit system.
- No virtual currency ("virtual dollars" or equivalent).
- No in-game monetized actions in the first version.
- AI must be optional and disabled by default.
- Procedural generation must work without any external API.
