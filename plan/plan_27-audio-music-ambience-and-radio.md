# 27. Audio, music, ambience, and radio

[Back to master plan](../plan.md)

Build a game-level audio graph above CNA audio/media capabilities. The vehicle radio
ships as one or two simple looping stations for v1 (fixed track list, no scheduling,
no ads, no announcer/interruption rules) — full station scheduling is explicit later
polish, not a v1 dependency.

### Gate M10 status (first pass, vertical slice)

Iron Gang now plays real (not synthesized) CC0 sound: a looped vehicle engine idle
(`assets/audio/engine_loop.wav`, volume/pitch scaled by `VehicleController::GetSpeedKph()`, tied
to `playerDriving_`) and two one-shots (`horn.wav` on the H key while driving, `footstep.wav` on a
fixed-interval timer while walking on foot) via `SoundEffect`/`SoundEffectInstance`, loaded
directly from WAV files with the same optional-asset try/catch convention used for Models/mission/
cutscene files. No bus graph (master/music/dialogue/ambience/vehicle/effects/UI), no real spatial
3D positioning (`Apply3D` is not called -- volume/pitch are set directly, not derived from
listener/emitter position), no ambient zones, no radio, and no ambience/siren content at all (the
one CC0 pack sourced this pass has no matching category -- see `IG-27-009`/`IG-27-016` below).
Everything else in this file remains unstarted.

## Core audio data and flow

- [x] **IG-27-001 P0** — Create master, music, dialogue, ambience, vehicle, effects, and UI buses. *(`AudioBus` + `AudioBusGraph` (`include/`/`src/Audio/AudioBuses.hpp/.cpp`): all seven buses, per-bus volume and mute, and `GetEffectiveVolume(bus, requested)` = requested x bus x Master x duck. Deliberately a **one-level** graph -- every bus routes to Master and the enum is the graph; a tree of sub-buses is a mixing-desk feature and this game has seven categories and one output. Stable string ids (`AudioBusId`/`ParseAudioBusId`) so a saved mix survives renaming the enum. `IronGangGame` routes the engine loop and horn to Vehicle and footsteps to Effects, and feeds `settings_.masterVolume` into the Master bus each update, replacing the single global multiplier `EffectiveVolume()` used to be. Not done: `UserSettings` still stores only a master volume, so per-bus levels are runtime-only; no Music or Ambience source exists yet to route.)*
- [ ] **IG-27-002 P0** — Play one spatial ambient emitter and one vehicle sound through CNA. *(Partial: a vehicle sound plays (looped engine idle, volume/pitch tied to speed), but not through a spatial 3D emitter -- `Apply3D` is not used, and there is no ambient emitter at all this pass; see the Gate M10 status note above.)*
- [ ] **IG-27-003 P0** — Create listener updates from active camera/player mode.
- [x] **IG-27-004 P0** — Create dialogue ducking and subtitle synchronization. *(Ducking: while a dialogue line is showing, every bus except Dialogue, UI and Master drops to `kDialogueDuckGain` (0.35) over a ramp -- 0.15 s down, 0.40 s back. Asymmetric on purpose: the drop has to be in place before the first syllable, and coming back as fast as it left is what makes a run of short lines pump the mix. The UI bus is exempt because a menu click going quiet while someone talks is a bug, not a mix. Subtitle synchronization: the duck is driven by exactly the same condition the subtitle is -- `dialogue_.GetCurrentLine() != nullptr` -- so the two cannot disagree. The engine loop's volume is re-applied outside the "world is advancing" gate, because it is only recomputed while driving and the duck would otherwise never reach it during the one situation it exists for.)*
- [ ] **IG-27-005 P1** — Create audio event definitions separate from raw file paths.
- [ ] **IG-27-006 P1** — Create voice limits and priority/stealing policy.
- [ ] **IG-27-007 P1** — Create distance attenuation and spatialization presets.
- [ ] **IG-27-008 P1** — Create simple obstruction/occlusion queries (a volume check against level geometry, not a dedicated occlusion subsystem).
- [ ] **IG-27-009 P1** — Create ambient zones for streets, interiors, industrial sites, countryside, and waterfront across the campaign's districts.
- [ ] **IG-27-010 P1** — Create smooth zone blending.
- [ ] **IG-27-011 P1** — Create footstep surface-to-event mapping.
- [ ] **IG-27-012 P1** — Create vehicle idle, load, RPM, exhaust, tire, suspension, impact, and cabin layers.
- [ ] **IG-27-013 P1** — Create engine parameter smoothing and gear events.
- [ ] **IG-27-014 P1** — Create music state and transition system, including mission-specific music states.
- [ ] **IG-27-015 P1** — Create one or two simple looping vehicle-radio stations (fixed track list, no scheduling/ads/interruption rules).
- [ ] **IG-27-016 P1** — Create voice/music streaming and preload budgets.
- [ ] **IG-27-017 P1** — Create pause, focus-loss, and suspend behavior.
- [ ] **IG-27-018 P1** — Create per-bus volume settings and dynamic-range presets.
- [ ] **IG-27-019 P1** — Create audio debug meters and active-voice display.
- [ ] **IG-27-020 P1** — Create missing-audio fallback and validation.
- [ ] **IG-27-021 P2** — Create reverberation zones if backend/library support is adequate.
- [ ] **IG-27-022 P2** — Create weather-dependent ambience.
- [ ] **IG-27-023 P2** — Create time-of-day ambience variation.
- [ ] **IG-27-024 P3** — Design full radio-station scheduling (multiple stations, tracks, announcer, ads, interruption rules) only as later polish once the simple station from IG-27-015 ships.
- [ ] **IG-27-025 P2** — Create visual sound indicators as an accessibility option.

## Audio bus graph

- [x] **IG-27-026 P0** — Define the scope and public API of the audio bus graph (master/music/dialogue/ambience/vehicle/effects/UI from IG-27-001), including audio event definitions (IG-27-005) and per-bus volume (IG-27-018). *(`include/IronGang/Audio/AudioBuses.hpp` is the API and states the scope in its header comment: seven buses, one level, per-bus volume/mute, a ducking ramp, and stable ids. Explicit non-goals recorded there: no sub-bus tree, no DSP effects, no per-voice routing. Audio **event** definitions separate from file paths remain open under `IG-27-005`.)*
- [x] **IG-27-027 P0** — Implement the smallest deterministic reference path: route one sound through one bus at one volume. *(`GetEffectiveVolume(AudioBus::Vehicle, 0.4)` through a Master at the settings volume is the reference path, exercised end to end by the running game's engine loop and covered by `TestAudioBusGraphMixing`, which asserts a fresh graph is transparent and that Master scales itself exactly once rather than twice.)*
- [ ] **IG-27-028 P1** — Add input validation and actionable failure reporting for malformed audio event/bus data.
- [ ] **IG-27-029 P1** — Add unit tests and one integration scenario covering dialogue ducking (IG-27-004) and voice priority/stealing (IG-27-006).
- [ ] **IG-27-030 P1** — Define save/checkpoint serialization and restoration for per-bus volume settings.
- [ ] **IG-27-031 P2** — Add debug logging/inspection (meters, active-voice display from IG-27-019) and document usage.

## Spatial audio emitter

- [ ] **IG-27-032 P1** — Define the scope and public API of spatial audio emitters (attenuation/spatialization from IG-27-007, occlusion check from IG-27-008).
- [ ] **IG-27-033 P1** — Implement the smallest deterministic reference path: one emitter attenuates correctly as the listener moves.
- [ ] **IG-27-034 P1** — Add input validation and actionable failure reporting for malformed emitter data.
- [ ] **IG-27-035 P1** — Add unit tests and one integration scenario covering listener updates (IG-27-003) across camera/player modes.
- [ ] **IG-27-036 P2** — Add debug logging/inspection and document usage.

## Ambient zone manager

- [ ] **IG-27-037 P1** — Define the scope and public API of the ambient zone manager (zones and blending from IG-27-009/IG-27-010).
- [ ] **IG-27-038 P1** — Implement the smallest deterministic reference path: cross a zone boundary and hear a smooth ambience blend.
- [ ] **IG-27-039 P1** — Add input validation and actionable failure reporting for malformed zone data.
- [ ] **IG-27-040 P1** — Add unit tests and one integration scenario covering weather/time-of-day variation (IG-27-022/IG-27-023) once those exist.
- [ ] **IG-27-041 P2** — Add debug logging/inspection and document usage.

## Footstep resolver

- [ ] **IG-27-042 P1** — Define the scope and public API of the footstep resolver (surface mapping from IG-27-011).
- [ ] **IG-27-043 P1** — Implement the smallest deterministic reference path: one footstep event resolves to the correct surface sound.
- [ ] **IG-27-044 P1** — Add input validation and actionable failure reporting for unmapped surfaces (fallback per IG-27-020).
- [ ] **IG-27-045 P1** — Add unit tests and one integration scenario covering all district surface types.
- [ ] **IG-27-046 P2** — Add debug logging/inspection and document usage.

## Vehicle audio controller

- [ ] **IG-27-047 P1** — Define the scope and public API of the vehicle audio controller (layers from IG-27-012, engine smoothing from IG-27-013).
- [ ] **IG-27-048 P1** — Implement the smallest deterministic reference path: drive the sedan through idle, acceleration, and braking with correct layered sound.
- [ ] **IG-27-049 P1** — Add input validation and actionable failure reporting for malformed vehicle audio data.
- [ ] **IG-27-050 P1** — Add unit tests and one integration scenario covering the radio stations from IG-27-015 playing through the cabin bus.
- [ ] **IG-27-051 P2** — Add debug logging/inspection and document usage.

## Music director

- [ ] **IG-27-052 P1** — Define the scope and public API of the music director (states/transitions from IG-27-014, including mission-specific states).
- [ ] **IG-27-053 P1** — Implement the smallest deterministic reference path: transition from exploration music to one mission's combat/tension state and back.
- [ ] **IG-27-054 P1** — Add input validation and actionable failure reporting for malformed music-state data.
- [ ] **IG-27-055 P1** — Add unit tests and one integration scenario covering a full mission's music-state sequence.
- [ ] **IG-27-056 P1** — Define save/checkpoint serialization and restoration for the active music state.
- [ ] **IG-27-057 P2** — Add debug logging/inspection and document usage.
