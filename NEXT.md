# Black Pine next steps

Start a new context by reading `plan.md`, then
`docs/PLAYABILITY_AUDIT.md`, `docs/GAME_DESIGN.md` region I, and
`VERIFICATION.md`. Do not reimplement audited screens 1-103.

The immediate milestone is the Nightjar bunker (screens 104-115):

1. Replace its linear catalogue route with the designed decontamination,
   corridor, laboratory, machine-shop, cooling, archive and rescue branches.
2. Add individual 16-colour scenes and persistent puzzle states.
3. Make F1 name visible doors and mechanisms one action at a time.
4. Convert the full scenario to the same physical route used by a player.
5. Render and inspect before/after states, run native and sanitizer tests, then
   update the audit and commit the region separately.

After that, author the summit and transmitter (screens 116-124), exercise all
hazard restarts and endings, perform a bilingual human keyboard playthrough,
and add a content/save-schema version. Detailed constraints and verification
commands live in `plan.md`.
