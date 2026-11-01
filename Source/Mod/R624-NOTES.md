# R624 — upscaling choices, live settings, scrolling, automatic game activation

Installed app: `%LOCALAPPDATA%\Programs\Zink GTA Enhancement\ZinkGTAUI.exe`.
Desktop/Start menu shortcut: **Zink GTA Enhancement**.

## Controls

Both providers offer Native, Native AA, Quality, Balanced, Performance and Ultra Performance. Intel also offers Ultra Quality and Ultra Quality Plus. Native AA invokes XeSS AA or DLSS DLAA. Real upscaling renders GTA below the selected output size, then reconstructs through the selected SDK; it is not a downsample of an already full-size backbuffer. The app queries the pinned SDK for input dimensions before applying a render profile to the closed game.

Intel sizing prefers an available Arc adapter and the D3D11 SDK. When that context is unavailable, the standalone sizing probe uses the Intel D3D12 SDK to obtain a compatible recommended input size. This fallback is only a sizing query, not an alternative runtime backend. Intel D3D11 execution and image quality still need testing on an available Arc adapter.

The main content area and system column have visible vertical scroll bars. Sidebar Scroll up/down and Page Up/Page Down also move the main content area.

Save profile applies settings as well as saving the profile. Apply while GTA is running updates HUD fields, source FPS limit, FG enablement and NVIDIA multiplier through the atomic INI bridge. The render thread/worker poll live controls at a bounded 250 ms cadence. Renderer/provider/output resolution/quality and diagnostics changes are saved for the next game launch. The live save does not rewrite GTA's active resolution or mutate the active SDK input contract. GTA must rebuild its rendering resources for those changes; no forced ResizeBuffers or game termination is used.

Upscaling can remain active with FG disabled. In that mode the SDK presents one reconstructed original frame, with zero generated frames. F9 toggles the entire mod pipeline.

## Load GTA + mod

Load GTA applies the selected configuration before launching Steam's GTA V Legacy entry. The mod then enables itself after a playable scene is ready; no F9 activation is required. A read-only ScriptHookV cooperative script checks player availability, the loading screen and pause state, publishing a timestamped readiness flag to the Present callback. Native game calls never run on the D3D worker. Automatic activation waits for foreground gameplay and a fresh readiness sample. Manual F9 off remains off; the automatic startup is consumed once.

If GTA is already running, Load GTA saves supported live controls and requests activation of the current configuration. The request waits for foreground gameplay. Pending render changes still require closing and relaunching GTA. User-disabled native-only effects remain disabled.

Native hash references (verified against CitizenFX's primary native documentation):
- PLAYER_ID: https://github.com/citizenfx/natives/blob/master/PLAYER/PlayerId.md
- IS_PLAYER_PLAYING: https://github.com/citizenfx/natives/blob/master/PLAYER/IsPlayerPlaying.md
- GET_IS_LOADING_SCREEN_ACTIVE: https://github.com/citizenfx/natives/blob/master/DLC/GetIsLoadingScreenActive.md
- IS_PAUSE_MENU_ACTIVE: https://github.com/citizenfx/natives/blob/master/HUD/IsPauseMenuActive.md

## Validation

- WinUI Release build: zero warnings/errors. Native build passes, with the existing helper C4244 warnings only. Protected source hash lock passes.
- Native live-settings regression passes: HUD reads refresh after atomic file replacement; a 1280x720 configured input is accepted independently of 2560x1440 output.
- Backend regressions pass for true render/output dimension separation, deferred render changes without touching active XML/INI dimensions, live flags/caps/multiplier, explicit FG/activation commands, profiles, native resolutions, unknown INI/XML preservation, backups, install hashes and rollback guards.
- RTX 5070 standalone GPU: DLSS Quality 1280x720 -> 1920x1080 with x6, 100/100 complete frames, 600 actual presentations.
- RTX 5070 standalone GPU: DLSS Ultra Performance 640x360 -> 1920x1080 with x6, 100/100 complete frames, 600 actual presentations.
- Live x2 -> x4 -> x6 while upscaling: 100/100 matching frames, 454 actual presentations, matching the scheduled multiplier changes.
- Live FG on/off/on while upscaling: 100/100 matching frames, 425 actual presentations, including 35 reconstructed frames without FG.
- App UI: Intel quality list verified; Scroll up/down verified against actual scroll offsets. NVIDIA quality list and final installation verified separately during UI QA.

These tests exercise the SDK and app, not a full GTA gameplay session. Actual GTA render-size acceptance, automatic story-mode activation, camera/object/HUD image quality, pacing, and Intel Arc runtime behavior require an in-game test. No promise of a specific in-game FPS gain is made. Current saved provider/resolution/other preferences were preserved during deployment; no quality preset was forced.

## Recovery

Pre-R624 sources: `backups/pre-R624-20261022-090243` (also contains the previous app sources).
Previous runtime: `%LOCALAPPDATA%\ZinkGTAUI\backups\20261022-093438-150-runtime-5c6a31`.
Previous settings: `%LOCALAPPDATA%\ZinkGTAUI\backups\20261022-093438-787-settings-e88910`.

See `profiling/R624-*.txt` for build and GPU checks, and `redesign/tests/AppBackendRegression` / `redesign/tests/LiveSettings.cpp` for the regression checks. Existing protected camera, temporal, ghosting and freezing sources were not edited.
