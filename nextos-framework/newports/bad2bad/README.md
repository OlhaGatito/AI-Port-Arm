# Bad 2 Bad Delta — NextOS port (source only)

Android `com.dawinstone.b2b` 1.6.4 → ARM Linux (Rocknix / PortMaster).

- Engine: Unity **2022.3.62f3** IL2CPP, arm64-v8a only
- Loader base: `ports/sallyface-nextos` (same Unity version), GPL-3.0-only
- Status: native load + Unity init + MobileAds/UMP + title frame rendering on device
- Not in this tree: owner APK, game `lib/`/`assets/`, saves, debug logs/screenshots

Build (Docker `portmaster-builder:aarch64-latest`):

```bash
cd port && bash build.sh
```

Device notes and pipeline: [docs/ANALYSIS.md](docs/ANALYSIS.md).

Game data stays local under `gamedata/` / `apk/` (gitignored). Owner-supplied only.
