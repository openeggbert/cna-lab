# 27. Audio, music, ambience, and radio

[Back to master plan](../plan.md)

Build a game-level audio graph above CNA audio/media capabilities. The vehicle radio
ships as one or two simple looping stations for v1 (fixed track list, no scheduling,
no ads, no announcer/interruption rules) — full station scheduling is explicit later
polish, not a v1 dependency.

### Gate M10 status (first pass, vertical slice)

Iron Shadows now plays real (not synthesized) CC0 sound: a looped vehicle engine idle
(`assets/audio/engine_loop.wav`, volume/pitch scaled by `VehicleController::GetSpeedKph()`, tied
to `playerDriving_`) and two one-shots (`horn.wav` on the H key while driving, `footstep.wav` on a
fixed-interval timer while walking on foot) via `SoundEffect`/`SoundEffectInstance`, loaded
directly from WAV files with the same optional-asset try/catch convention used for Models/mission/
cutscene files. No bus graph (master/music/dialogue/ambience/vehicle/effects/UI), no real spatial
3D positioning (`Apply3D` is not called -- volume/pitch are set directly, not derived from
listener/emitter position), no ambient zones, no radio, and no ambience/siren content at all (the
one CC0 pack sourced this pass has no matching category -- see `IS-27-009`/`IS-27-016` below).
Everything else in this file remains unstarted.

## Core audio data and flow

- [ ] **IS-27-001 P0** — Create master, music, dialogue, ambience, vehicle, effects, and UI buses.
- [ ] **IS-27-002 P0** — Play one spatial ambient emitter and one vehicle sound through CNA. *(Partial: a vehicle sound plays (looped engine idle, volume/pitch tied to speed), but not through a spatial 3D emitter -- `Apply3D` is not used, and there is no ambient emitter at all this pass; see the Gate M10 status note above.)*
- [ ] **IS-27-003 P0** — Create listener updates from active camera/player mode.
- [ ] **IS-27-004 P0** — Create dialogue ducking and subtitle synchronization.
- [ ] **IS-27-005 P1** — Create audio event definitions separate from raw file paths.
- [ ] **IS-27-006 P1** — Create voice limits and priority/stealing policy.
- [ ] **IS-27-007 P1** — Create distance attenuation and spatialization presets.
- [ ] **IS-27-008 P1** — Create simple obstruction/occlusion queries (a volume check against level geometry, not a dedicated occlusion subsystem).
- [ ] **IS-27-009 P1** — Create ambient zones for streets, interiors, industrial sites, countryside, and waterfront across the campaign's districts.
- [ ] **IS-27-010 P1** — Create smooth zone blending.
- [ ] **IS-27-011 P1** — Create footstep surface-to-event mapping.
- [ ] **IS-27-012 P1** — Create vehicle idle, load, RPM, exhaust, tire, suspension, impact, and cabin layers.
- [ ] **IS-27-013 P1** — Create engine parameter smoothing and gear events.
- [ ] **IS-27-014 P1** — Create music state and transition system, including mission-specific music states.
- [ ] **IS-27-015 P1** — Create one or two simple looping vehicle-radio stations (fixed track list, no scheduling/ads/interruption rules).
- [ ] **IS-27-016 P1** — Create voice/music streaming and preload budgets.
- [ ] **IS-27-017 P1** — Create pause, focus-loss, and suspend behavior.
- [ ] **IS-27-018 P1** — Create per-bus volume settings and dynamic-range presets.
- [ ] **IS-27-019 P1** — Create audio debug meters and active-voice display.
- [ ] **IS-27-020 P1** — Create missing-audio fallback and validation.
- [ ] **IS-27-021 P2** — Create reverberation zones if backend/library support is adequate.
- [ ] **IS-27-022 P2** — Create weather-dependent ambience.
- [ ] **IS-27-023 P2** — Create time-of-day ambience variation.
- [ ] **IS-27-024 P3** — Design full radio-station scheduling (multiple stations, tracks, announcer, ads, interruption rules) only as later polish once the simple station from IS-27-015 ships.
- [ ] **IS-27-025 P2** — Create visual sound indicators as an accessibility option.

## Audio bus graph

- [ ] **IS-27-026 P0** — Define the scope and public API of the audio bus graph (master/music/dialogue/ambience/vehicle/effects/UI from IS-27-001), including audio event definitions (IS-27-005) and per-bus volume (IS-27-018).
- [ ] **IS-27-027 P0** — Implement the smallest deterministic reference path: route one sound through one bus at one volume.
- [ ] **IS-27-028 P1** — Add input validation and actionable failure reporting for malformed audio event/bus data.
- [ ] **IS-27-029 P1** — Add unit tests and one integration scenario covering dialogue ducking (IS-27-004) and voice priority/stealing (IS-27-006).
- [ ] **IS-27-030 P1** — Define save/checkpoint serialization and restoration for per-bus volume settings.
- [ ] **IS-27-031 P2** — Add debug logging/inspection (meters, active-voice display from IS-27-019) and document usage.

## Spatial audio emitter

- [ ] **IS-27-032 P1** — Define the scope and public API of spatial audio emitters (attenuation/spatialization from IS-27-007, occlusion check from IS-27-008).
- [ ] **IS-27-033 P1** — Implement the smallest deterministic reference path: one emitter attenuates correctly as the listener moves.
- [ ] **IS-27-034 P1** — Add input validation and actionable failure reporting for malformed emitter data.
- [ ] **IS-27-035 P1** — Add unit tests and one integration scenario covering listener updates (IS-27-003) across camera/player modes.
- [ ] **IS-27-036 P2** — Add debug logging/inspection and document usage.

## Ambient zone manager

- [ ] **IS-27-037 P1** — Define the scope and public API of the ambient zone manager (zones and blending from IS-27-009/IS-27-010).
- [ ] **IS-27-038 P1** — Implement the smallest deterministic reference path: cross a zone boundary and hear a smooth ambience blend.
- [ ] **IS-27-039 P1** — Add input validation and actionable failure reporting for malformed zone data.
- [ ] **IS-27-040 P1** — Add unit tests and one integration scenario covering weather/time-of-day variation (IS-27-022/IS-27-023) once those exist.
- [ ] **IS-27-041 P2** — Add debug logging/inspection and document usage.

## Footstep resolver

- [ ] **IS-27-042 P1** — Define the scope and public API of the footstep resolver (surface mapping from IS-27-011).
- [ ] **IS-27-043 P1** — Implement the smallest deterministic reference path: one footstep event resolves to the correct surface sound.
- [ ] **IS-27-044 P1** — Add input validation and actionable failure reporting for unmapped surfaces (fallback per IS-27-020).
- [ ] **IS-27-045 P1** — Add unit tests and one integration scenario covering all district surface types.
- [ ] **IS-27-046 P2** — Add debug logging/inspection and document usage.

## Vehicle audio controller

- [ ] **IS-27-047 P1** — Define the scope and public API of the vehicle audio controller (layers from IS-27-012, engine smoothing from IS-27-013).
- [ ] **IS-27-048 P1** — Implement the smallest deterministic reference path: drive the sedan through idle, acceleration, and braking with correct layered sound.
- [ ] **IS-27-049 P1** — Add input validation and actionable failure reporting for malformed vehicle audio data.
- [ ] **IS-27-050 P1** — Add unit tests and one integration scenario covering the radio stations from IS-27-015 playing through the cabin bus.
- [ ] **IS-27-051 P2** — Add debug logging/inspection and document usage.

## Music director

- [ ] **IS-27-052 P1** — Define the scope and public API of the music director (states/transitions from IS-27-014, including mission-specific states).
- [ ] **IS-27-053 P1** — Implement the smallest deterministic reference path: transition from exploration music to one mission's combat/tension state and back.
- [ ] **IS-27-054 P1** — Add input validation and actionable failure reporting for malformed music-state data.
- [ ] **IS-27-055 P1** — Add unit tests and one integration scenario covering a full mission's music-state sequence.
- [ ] **IS-27-056 P1** — Define save/checkpoint serialization and restoration for the active music state.
- [ ] **IS-27-057 P2** — Add debug logging/inspection and document usage.
