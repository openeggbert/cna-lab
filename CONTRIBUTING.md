# Contributing

1. Keep CNA, sharp-runtime, easy-gl, Mesh Craft, and Iron Shadows as separate repositories.
2. Do not copy proprietary game assets, scripts, dialogue, map layouts, names, or mission designs.
3. Add tests for non-rendering game logic.
4. Keep source assets and generated runtime assets separate.
5. Record every third-party asset and its license before committing it.
6. Reuse persistent `cmake-build-*` directories, keep ccache enabled, use at most four build jobs, and do not create build trees under `/tmp`, `/var/tmp`, or `/dev/shm`.
7. Update `plan.md` when a task is started or completed and document architectural changes in `docs/architecture.md`.
