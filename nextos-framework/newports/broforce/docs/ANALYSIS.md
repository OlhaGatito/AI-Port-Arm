# Broforce — Android → ARM Linux (NextOS) porting analysis

Game: Broforce (Free Lives), APK repacked by APKVISION (site shell present).
Target: EE4.7 aarch64 handheld (Mali-G52, KMSDRM display).

## 1. APK facts (verified, not guessed)

- Engine: **Unity 2017.4.40f1** (6e14067f8a9a), IL2CPP backend
  (`Managed/Metadata/global-metadata.dat`), scripting via `__Internal` only.
- Package (repack): `com.vocies61.broforce`; Activity: standard
  `com.unity3d/player/UnityPlayerActivity`.
- `lib/arm64-v8a` contents:
  - `libmain.so` (7 KB) — **Unity official 2017.4 split loader** (`.note.unity`,
    exports `JNI_OnLoad`, dlopens `libunity.so`, registers
    `com/unity3d/player/NativeLoader.load`). Same chain as Greak 2022.
  - `libunity.so` (19 MB) — exports `JNI_OnLoad` + `UnitySendMessage`; imports
    EGL (**23** `egl*` dynsyms, corrected from an earlier count of 19), no
    direct GL/OpenSL/Java dynsym (all via eglGetProcAddress /
    dlsym at runtime). NEEDED: libmain/libc/libm/libandroid/libdl/liblog/
    libGLESv2/libstdc++/libz/libEGL.
    Notable imports: `eglGetConfigAttrib` and `eglQueryString` **are** imported;
    `eglQueryContext` is **not** (see §5.1).
  - `libil2cpp.so` (39 MB) — 403 exports (181 `il2cpp_*`), 206 imports, no
    Java/GL dynsym. NEEDED: libc/libm/liblog/libdl/libstdc++.
  - `libstub.so` (700 KB) — **repack-site shell**: signature-check layer, only
    exports `JNI_OnLoad`; carries 173 `com/unity3d/player/*` Java signatures.
    NOT loaded by the port (same decision as Greak).
  - `libAPKVISION.so` (1.2 MB) — repack-site helper statically linking
    **Dobby** (DobbyHook/DobbyCodePatch) + libc++. NOT loaded by the port.
- `assets/bin/Data` standard layout: `globalgamemanagers`, `boot.config`
  (`wait-for-native-debugger=0`), `Managed/Metadata/global-metadata.dat`,
  `Managed/Resources`, `SymbolMap-ARM64` (binary hash map, 2017.4 IL2CPP).
- Assets are classic AssetBundles (`chunks/levels/scenes*/cutscenes/
  expendabros/networkobjects`) + a `StandaloneWindows` folder. **No mp4, no
  StreamingAssets, no plugins dir** → the Greak media (AMediaCodec) chain is
  not needed for the first bring-up.
- Input: **Rewired** (metadata strings: "Rewired Input Manager prefab",
  "CheckIsRewiredReady", DataFiles warnings) over Unity legacy Input.
- Audio: **FMOD** (corrected — an earlier pass searched only the asset
  metadata and wrongly concluded "Unity built-in"). `libunity.so` holds 304
  `fmod` symbol/string hits and the running process shows three live FMOD
  threads (`FMOD file threa`, `FMOD nonblockin`, `FMOD stream thr`, plus
  `fmod_pump()` frames), so the audio backend is FMOD Studio, not Unity's
  built-in streamer. It runs through the SDL audio path, so the Stage 2 audio
  work is SDL/ALSA tuning rather than a new shim.

## 2. Loading chain (Android ground truth from dex + libmain)

`System.loadLibrary("APKVISION"|"stub"|"main")` — dex contains **no**
loadLibrary("unity"/"il2cpp"): `libmain` (official) does
`dlopen("libunity.so")` inside `NativeLoader.load`, which also brings
`libil2cpp.so`. Therefore the NextOS loader keeps the Greak sequence:

1. relocate + `nx_run_init(libmain)`, call `JNI_OnLoad(libmain)`.
2. call the registered `NativeLoader.load` — our handle-aware dlopen bridge
   runs libunity's init array + `JNI_OnLoad(libunity)`, then libil2cpp.
3. drive the fake-JNI surface and enter `run_unity()`.

The loader skeleton is a direct copy of Greak's (`port/src`), with only
identity changes (cmdline "broforce", GPTK port name, package
`com.vocies61.broforce`, log prefixes, banner). First build passed the
GLIBC/DT_NEEDED gate unchanged (BuildID 0fc4b623).

## 3. Device (EE4.7 test unit)

- aarch64, ~2 GB RAM free at boot, `/storage/roms` on SD (97 GB, 56 GB free).
- GL stack: **Mesa is NOT in use here** (an earlier pass said so from a
  library-name probe — corrected). The live renderer string is
  `Mali-G52`, GL version `OpenGL ES 3.2 v1.r49p0-00eac0.7efffa7bc692eb26bdd9ce424d2afbbc`,
  i.e. the vendor blob driver, not llvmpipe. `/usr/lib/libmali` is **absent**
  on this unit, so the `LD_LIBRARY_PATH` prefix in the launch script is a
  no-op and was never needed.
- Display: **no `/dev/fb0`** on this unit. The launch script detects that and
  exports `SDL_VIDEODRIVER=kmsdrm`; the log confirms
  `PLATFORM=emuelec SDL_VIDEODRIVER=kmsdrm`. Greak takes the same path, so
  the shared `egl_sdl.c` KMSDRM page-flip route is kept. DRM state was
  verified live: `card0-DSI-1 status=connected`, `dpms=On`.

## 4. Plan

Stage 1: loader loads libmain→libunity→libil2cpp, JNI_OnLoad ok, Unity init
reaches the player loop with a visible framebuffer.
Stage 2: input (SDL→Android keycodes, gamepad passthrough per Greak §14 —
Rewired reads Unity legacy Input), audio, saves.
Stage 3: resolution/perf tuning (Broforce is a 2D pixel game; GLES2 target).

Payload layout: `roms/ports/broforce/{broforce-nextos, lib/{libmain,libunity,
libil2cpp}.so, assets/, fonts/, home/}`, script
`roms/ports_scripts/Broforce.sh`.

## 5. Black screen on first run — three stacked root causes (resolved)

Symptom: the port reached the player loop and kept swapping frames (3300+
frames, clean exit, live input receipts) but the screen showed nothing. The
frame proof adapter reported `frame_proof=100% verdict=OK`, which was
misleading — it samples the currently bound FBO before the swap dispatch, so
"non-black" only proved that *something* had been drawn into that FBO, not
that a presentable image reached the panel. Greak on the same device, same
KMSDRM path, showed a picture, so the display pipeline was exonerated early.

Ground truth first: UnityPy over `assets/bin/Data` shows **every** hash-asset
`Shader` carries `platforms=[9]` (`#version 300 es`, GLES3-only) — the blob has
**no platform 5 (GLES2) variant at all**. `globalgamemanagers` already has
`BuildSettings.m_GraphicsAPIs=[11]` (OpenGLES3), so the *data* contract was
correct; the fault was in what the runtime reported at startup.

### 5.1 Layer 1 — magenta screen (fake EGL under-reported ES2)

Unity 2017.4 logged
`OpenGLES2 OpenGL ES 3.2 v1.r49p0-...`: device type `OpenGLES2` while the
passthrough GL_VERSION said ES 3.2. With `GfxDevice=OpenGLES2` it requested
platform 5 variants → 35× `Desired shader compiler platform 5 is not
available in shader blob` (`shader.cpp:536`) → Unity's magenta error
shader → FBO0 sampled 3072/3072 points at `(255,0,255)`.

The GfxDevice choice is made from the **EGL config attributes**, not from
`glGetString`. `sdl_eglGetConfigAttrib` answered `EGL_RENDERABLE_TYPE` and
`EGL_CONFORMANT` with `EGL_OPENGL_ES2_BIT` (0x4) only, so Unity saw an
ES2-only config and gave up on ES3.

Ruled out by experiment before touching the config path (all three are dead
ends worth recording):

- **`-force-gles30` in `/proc/self/cmdline`** — the `bionic.c` hook that
  rewrites the cmdline never fired; the `[sf/gfx] Unity cmdline` log line
  appears in **no** run. It was dead code inherited from the skeleton.
- **`ro.opengles.version=196608`** — `libunity.so` contains no such string.
  With `SF_VERBOSE` it only ever queries `cpu.abi`-style properties (×6).
- **JNI `getDeviceConfigurationInfo`** — no GLES/Configuration JNI call
  appears anywhere in the `SF_JNILOG` trace.

Also note `libunity.so` does **not** import `eglQueryContext`, so the
`EGL_CONTEXT_CLIENT_VERSION` hardcode in `sdl_eglQueryContext` was a red
herring; it is still fixed (it now returns the real major version) but it
was not the cause.

Fix: report the real capability — `EGL_OPENGL_ES2_BIT | EGL_OPENGL_ES3_BIT`
(0x44) for both attributes. Result: Unity self-reports `OpenGLES3`, shader
errors 35 → **0**.

### 5.2 Layer 2 — black screen (jar:file:// AssetBundles unreachable)

With shaders fixed, the screen stayed black and the real blocker surfaced:

```
Unable to open archive file: jar:file://!/assets/scenesintro.assetbundle
Scene 'Intro' couldn't be loaded because it has not been added to the build
settings or the AssetBundle has not been loaded.
```

Every AssetBundle is addressed through a `jar:file://<apk>!/assets/...` URI.
On Android the APK path part is populated; here it is empty, so the URI
resolves to nothing, no bundle ever loads, `Intro` never starts and the
frame loop just keeps clearing to black. Metrics before the fix: 0 archives
opened, scene-load failures, **18 674** `ArgumentException` lines.

Fix follows the established `ports/fp2-nextos/upstream/src/bionic.c` pattern
(`fp2_traduz_jar`): one translator at the libc boundary that rewrites
`jar:file://…!/assets/…` to the real file under `sf_apkdir`, shared by all
four entry points that touch it — `open`, `openat`, `fopen`, plus
`access`/`faccessat`, which were previously passed straight through in the
import table. After the fix: 0 archive failures, 0 scene failures, **0**
`ArgumentException`, and `VIDEO: … frame_proof=21.0% verdict=OK`.

### 5.3 Layer 3 — black screen after the intro (semaphore pool exhaustion)

The intro played and then the picture froze on the last presented frame. The
stop was **deterministic, not random**: two independent runs both died at
swap=1200 (≈20 s), with the render thread and `UnityPreload` parked in
`b_sem_wait` and zero utime growth. The owner confirmed the symptom as
"intro finishes, then black screen".

The first hypothesis was wrong and is recorded so it is not retried: the
stack showed a thread named `UnityMain` sitting in `fmod_pump` →
`sf_audio_device_write`, which looked like the managed main thread stuck on
audio backpressure. Two corrections:

- Unity 2017.4 names the **most recently created** thread `UnityMain`, so
  `/proc/<pid>/task/*/comm` does **not** identify the managed main thread.
  That thread was in fact our own FMOD mixer, running normally.
- `b_sem_wait` in two threads plus a frozen picture is a signal that will
  never be posted, not a slow frame rate — so the wait itself was the clue.

The real evidence was in the verbose log: **64× `semaphore table full`**,
starting exactly where the scene/save initialisation began
(`[FZ] Disconnect` → `home/Levels` → `home/Saves/progress.sav` →
`semaphore table full`). `pthread_bridge.c` implements bionic's `sem_t`
(which carries no handle) as a **fixed 512-entry table keyed by the address
the game passes**, and `b_sem_destroy` was a deliberate no-op ("keep the
mapping alive"). So entries only ever accumulated. Unity creates and
destroys semaphores constantly while switching scenes and streaming assets;
after ~20 s the table was full, every later semaphore came from a bare
`calloc()` **outside the table**, and producers' `sem_post` and consumers'
`sem_wait` landed on different objects. The render thread and UnityPreload
waited forever.

Fix: reclaim the slot in `b_sem_destroy` (compacting the table), and raise
`SEM_MAX` from 512 to 4096 for headroom. The orphaned `sem_t` is
intentionally **not** destroyed — as in bionic, someone may still be blocked
in `sem_wait` on it, and freeing it would be a use-after-free; 32 bytes per
destroyed semaphore is irrelevant. The overflow diagnostic is now rate-
limited (once per 4096) so a future overflow cannot flood the log and hide
its own cause.

Result on hardware: `semaphore table full` 64 → **0**, swap advanced from a
frozen 1200 to **11400+**, and a frame captured at swap 2400 shows the
**main menu fully rendered** — title art, START/OPTIONS/LEVEL EDITOR,
version string. Zero magenta, 199 distinct colours.

Two other defects were fixed on the way and are worth keeping:

- `choreographer_driver` (jni.c) did `if (!deliver_handle_message()) return
  NULL;` — if the Choreographer proxy was not captured yet, the frame driver
  **exited silently, with no log line**, and since that thread is what calls
  `doFrame`, the screen froze forever. It now waits for the proxy, logs every
  2 s, and only exits on an explicit `choreo_shutdown`.
- `sf_audio_device_write` spun up to **500 × 1 ms = 0.5 s per block** when
  the SDL queue stayed full. Now 20 × 5 ms `nanosleep`, and if the queue is
  still full the block is dropped and counted (`dropped_blocks`): late audio
  beats a frozen screen.

### 5.4 Verification performed

Built and measured on the real device, not inferred:

- build: `bash build.sh` (BuildIDs `f96ffaf3` after §5.1, `326b5082` after §5.2)
- `OpenGLES3` self-report, 0 `Desired shader compiler` errors, 0
  `Unable to open archive`, 0 `ArgumentException`, `verdict=OK`
- screenshots pulled over SSH and pixel-sampled: magenta **0.00 %** (was
  100 %), non-black content present
- **hardware-confirmed by the owner: the game reaches a playable picture**
  (this supersedes any earlier claim based only on logs), and after the
  semaphore fix a frame captured at swap 2400 shows the **main menu fully
  rendered** (title art, START/OPTIONS/LEVEL EDITOR, version string), with
  swap advancing past 11400 instead of freezing at 1200.

Still not verified: gameplay past the menu, Rewired gamepad mapping, FMOD
audio mix, save paths.

### 5.5 In-game magenta (open, not solved)

Symptom: the menu and the map screen render normally, but after START the
in-game view is almost entirely magenta (measured 99.9 % of sampled pixels
at 1024x768, only 8 distinct colour buckets) — the ocean/sky that should be
blue is magenta.

What is established:

- This is **not** the §5.1 magenta. That one was a missing shader variant in
  the blob and is fixed (Unity self-reports OpenGLES3, 0 blob errors). This
  one happens after the scene is running.
- It is Unity's magenta **error shader**: when a shader program fails to
  link, Unity substitutes it. With `SF_SHADERLOG=1 SF_GLSOURCE=1
  SF_GLSOURCE_OUT=1` the log shows 20–30 fragment shaders with `ok=0`:
  `L0002: Undeclared variable 'u'` and `S0001: No matching overload for
  function 'texture2D'`. `SF_GLSOURCE` is gated behind `shader_log_enabled()`,
  so `SF_SHADERLOG=1` is required as well or nothing is dumped.
- One real translator bug was found and fixed: `essl1.c` rewrote
  `precision highp float;` into `precision SF_HP float;`. GLSL ES only
  accepts literal qualifiers after `precision`, never a macro, so the line
  was a syntax error. A `precision` line is now passed through untouched.
- The remaining `L0002: Undeclared variable 'u'` is triggered by the
  `#ifdef UNITY_ADRENO_ES3` dead branches Unity 2017.4 emits; the Mali
  compiler appears to truncate the identifier `u_xlatb0` and report the
  leading `u` as an undeclared macro.

What was tried and reverted (do not repeat): a preprocessor that deletes the
dead `#ifdef` branches. It first overflowed a heap buffer, then produced
orphaned `#else`/`#endif` (`P0001: #else directive found outside
if-section`) and made the failure count worse — the Unity source's
`#else`/`#endif` pairs are relative to an `#ifdef` that the translator has
already rewritten, so any structural edit breaks them. Defining the vendor
macros to 0 in the emitted header is the safe equivalent and raised the
number of shaders that compile, but it did not clear the symptom.

**Measurement caveat that cost real time:** a run showing "only 32 % magenta"
was recorded as a breakthrough. It was wrong — the screenshot had been taken
on the **map** screen, which is UI- and sprite-heavy and renders mostly fine
anyway. Inside an actual level the screen is still ~100 % magenta. The only
valid acceptance criterion for this bug is a screenshot taken **after START,
inside gameplay**. Every future round must record which layer the capture was
taken on. Note also that the shader-link failures above are **not yet proven**
to be the cause of the in-game magenta: they may only account for the map
layer, and the gameplay magenta may have a separate, still unidentified
cause.

**Update 2026-10-02 — SOLVED.** The magenta did not come from a broken shader
at all: it came from this runtime *translating* the shader. See §5.6.

## 5.6 — The magenta was the ESSL 300→100 downgrade translator (fixed)

**Decisive experiment.** The translator was bypassed with `SF_NO_ESSL1=1`, so
the game's own ESSL 300 source is handed to the driver untouched:

| | translator ON | translator OFF (`SF_NO_ESSL1=1`) |
|---|---|---|
| shaders compiled (`ok=1`) | 33 | 21 / 42 compiles, **0 failures** |
| `ok=0` (compile failed) | dozens | **0** |
| in-game magenta (device-side pixel count) | ~100 % | **0.00 %** |

With the translator off the gameplay capture measures `magenta=0.00%` and the
frame has the expected structure: blue ocean, brown terrain, no magenta run.
Same binary, same device, same level — the only variable is the translation.

**Why this is the cause.** Broforce ships GLES3 (ESSL 300) shaders. The
translator exists for Utgard-class GPUs (Mali-450) that cannot compile ESSL
300 at all. On a GPU that *does* speak ESSL 300 the downgrade is pure loss:
it rewrites constructs the Mali compiler then rejects (`Undeclared variable
'u'`, no matching `texture2D` overload). Every shader that fails to compile
makes Unity fall back to its error shader — which is magenta. That is why the
menu, intro and map (few/simple shaders) survived while gameplay (the full
shader set) turned entirely magenta.

**Fix (capability-gated, no env var needed).**
- `sf_essl1_note_gles_version(int major)` — new. `egl_sdl.c` parses
  `GL_VERSION` right after the context is created (`"OpenGL ES 3.2 v1.r49p0…"`
  → 3; falls back to the requested `es_major_version`) and reports it.
- `sf_essl1_disabled()` now returns 1 when the live context is ES3+, so the
  ESSL 300 source goes to the driver untouched. ES2-only devices keep the old
  path. `SF_NO_ESSL1=1` forces off and `SF_ESSL1=1` forces on, kept for A/B
  comparisons.
- `patch_fragment_external_alpha()` (ETC1 external alpha) now picks the
  sampling function by dialect — `texture` for ESSL 300, `texture2D` for
  ESSL 100 — because that patch also runs on the untouched ESSL 300 path.

**Deployment.** Built binary `broforce-nextos`, md5 `c7b07c583afdc2fd9998f36c7febcb39`
(548 632 bytes, GLIBC 2.30), uploaded to the EmuELEC 4.7 test unit and
atomically swapped in; previous binary kept as `broforce-nextos.bak.<ts>`
(md5 `8825faf6…`). Device `ldd` reports zero unresolved libraries.

**Status: confirmed on the device (2026-10-03).** Started normally from
PortMaster — no environment override, no debug flags — the level renders
correctly. The earlier manual runs were launching without the override, which
is exactly why this fix had to be moved from an environment variable into the
code. Nothing else in the chain was changed; the translator gate is the whole
fix.

The open item from §5.5 — "in-game rendering is not correct" — is closed by
this. Any remaining rendering complaint must be judged against a *new*
baseline: capture the layer (menu / map / gameplay) before assuming a
regression, because the three never failed alike.

## 6. Analog sticks: each stick only drives one direction (open — device side)

**Symptom, as reported from the unit (2026-10-03, gameplay).**

- Left stick: only **left** works. Right does nothing. Up and down are also
  interpreted as left.
- Right stick: only **right** works.
- The D-pad / direction buttons work correctly in all four directions.

Note how the two failures mirror each other: one stick yields only the
negative half of an axis, the other only the positive half. That symmetry is
the useful part — it is not what a sign flip or a deadzone in our code would
produce, because our left and right sticks are handled by identical,
symmetric code (`input.c:1253-1281`).

**Runtime evidence.**

```
[bc/input] layout: gamepad nativo (cursor=on swipe=off)
[bc/input] controller 0: gamepad (0000:0000)
[?/Unity] [Broforce] joysticks (1): Microsoft X-Box 360 pad
[?/Unity] [Gamepad] systemPads=1 rewiredPads=1 playersWithPad=1 :: [0]Microsoft X-Box 360 pad
```

The GUID is `0000:0000`. That is not a real device identity — it is what SDL
reports when it cannot build one.

**Device evidence (decisive).** `/proc/bus/input/devices` on the unit:

```
I: Bus=0019 Vendor=0000 Product=0000 Version=0000
N: Name="gamepad"
P: Phys=
S: Sysfs=/devices/platform/gamepad/input/input3
U: Uniq=
H: Handlers=kbd js0 event3 dmcfreq
B: PROP=0
B: EV=30000b
B: KEY=f00000000 0 0 0 7fdb000000000000 0 0 10 0
B: ABS=1b
B: FF=107030000 0

I: Bus=0019 Vendor=0001 Product=0001 Version=0100
N: Name="GPIO Joystick Mouse"
H: Handlers=kbd event5 dmcfreq
B: EV=7
B: REL=103
```

Reading it out:

- There is exactly **one** real gamepad device, and it is a platform driver
  (`/devices/platform/gamepad/input/input3`), not a USB device.
- **Vendor = 0000, Product = 0000, Version = 0000, Uniq is empty.** With no
  bus identity and no unique string, SDL has nothing to build a GUID from —
  this is the direct cause of `0000:0000`, and it means SDL cannot match any
  `gamecontrollerdb` entry and falls back to guessing a mapping.
- `B: ABS=1b` = bits 0,1,3,4 = `ABS_X`, `ABS_Y`, `ABS_RX`, `ABS_RY`. Four axes
  exist, so the kernel side does expose two sticks.
- **`ABS_HAT0X` / `ABS_HAT0Y` (0x10 / 0x11) are absent.** The D-pad is a
  *button* device, never a hat. That is consistent with the observation that
  the D-pad works everywhere while both sticks are half-broken: the D-pad
  travels a completely different path — button codes 19/20/21/22
  (`input.c:457-460`, `jni.c` marks them `SOURCE_DPAD`) — and never touches
  the axis mapping at all.

**Our chain, for reference (unchanged).**

- `input.c:1253-1256` — `SDL_GameControllerGetAxis` for LEFTX/LEFTY/
  RIGHTX/RIGHTY (with a virtual override on top, off by default).
- `input.c:1294` — `sf_jni_motion_event(lx, ly, rx, ry, lt, rt, hx, hy)`.
- `jni.c` — packs them as `axis[0]=lx`, `axis[1]=ly`, `axis[11]=rx`,
  `axis[14]=ry`, `axis[15]/[16]=hat`, `source=0x01000010` (JOYSTICK).

So the value we hand to Unity is whatever SDL says the axis is. If SDL's
fallback mapping for this identity-less device folds both sticks onto fewer
axes than it actually has, one direction per stick survives — which is
exactly the reported pattern.

**Assessment.** All available evidence points at the device / SDL mapping
layer, not at the game and not at our injection code. The fact that the
device reports no vendor, product or unique ID — and therefore cannot be
matched to a controller mapping — is the strongest single indicator. This
matches the operator's own judgement ("可能是设备的原因").

**Not yet done (deliberately).** No code was changed. To close this, the next
step is to read the raw axis traffic instead of SDL's interpretation: dump
`/dev/input/event3` while pushing each stick in turn and check which of
`ABS_X / ABS_Y / ABS_RX / ABS_RY` actually moves. If the kernel reports them
correctly, the fix belongs in the launch environment
(`SDL_GAMECONTROLLERCONFIG` with an explicit mapping, or
`SDL_JOYSTICK_DEVICE`/hint overrides) rather than in the port. If the kernel
itself reports only one direction per stick, it is a hardware/driver limit
and no port-side change can recover it.

