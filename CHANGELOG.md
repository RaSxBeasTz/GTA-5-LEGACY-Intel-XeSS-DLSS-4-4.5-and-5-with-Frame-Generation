# Zink GTA Enhancement release history

This file is the source for the app's About and release-history window. New release notes added here are copied into every build and appear in the app automatically.

## App 3.2.0 / Mod R655 - 1 November 2026

- Added background, resumable NaturalVision Evolved package downloading when the Visual Mod switch is enabled.
- Added live percentage and downloaded/total GB progress to the existing centred progress panel.
- Added an authorised package-link field for the user-specific Razed Mods / Cfx Marketplace download.
- Kept SHA-256 verification before installation and corrected NVE removal when the cached archive is no longer present.

## App 3.1.1 / Mod R654 - 1 November 2026

- Fixed GPU selection showing duplicate Intel entries for individual Windows display paths.
- GPU discovery now uses DXGI's real adapter list and dedicated-memory values, so the RTX 4090 and Intel Arc appear once each.
- Both detected GPUs are shown in the selector for either provider, allowing an explicit cross-provider rendering choice.

## App 3.1.0 / Mod R653 - 1 November 2026

- Added a Rendering GPU selector populated from the graphics adapters detected on the PC.
- NVIDIA and Intel now remember separate manual GPU choices, with Automatic retained as the recommended default.
- A manual choice is written to GTA's `AdapterIndex` and launch options, and GPU scans now report the exact adapter selected.

## App 3.0.2 / Mod R652 - 1 November 2026

- Removed the GTA-folder picker from both NVE installation and removal.
- The app now supplies the auto-detected GTA Legacy path before every helper command and explicitly accepts NVE's missing game-version metadata.
- NVE removal now rolls back the registered package names directly in reverse order, avoiding metadata parsing and its folder prompt entirely.

## App 3.0.1 / Mod R651 - 1 November 2026

- Fixed the Visual Mod off/on action failing before NVE progress began because `VISUAL-MOD-NOTICE.txt` retained its obsolete VisualV manifest hash.
- Replaced the bundled notice's obsolete VisualV description with the current NVE and RageOpenV provenance.
- Revalidated every backend payload hash so changing the NVE switch can reach the centered staged progress workflow.

## App 3.0.0 / Mod R650 - 1 November 2026

- Replaced the VisualV workflow with a one-button NaturalVision Evolved Legacy installer using the completed 16 GB Single Player package.
- Added SHA-256 verification for the NVE archive and the publisher-downloaded RageOpenV loader.
- Added headless installation and reversal of NVE's three main OIV packages, using an isolated `mods` folder so original Rockstar RPF archives remain untouched.
- Added centered progress stages for verification, extraction, loader setup, each package and final validation.
- Excluded NVE's optional DLSS/DLAA/FSR plug-in to avoid collisions with Zink's XeSS/DLSS pipeline.
- Disabling Visual Mod enhancement reverses the packages in the opposite order and disables the mods-folder loader.

## App 2.9.0 / Mod R649 - 1 November 2026

- Added an experimental Intel Arc DLSS 5 Neural Rendering setting using the vendor-neutral DirectML port from `janblade/OptiScaler-F5-DLSSNR-Multipass` v0.1.35.1.
- Intel mode keeps XeSS as the upscaler and runs the neural model over the finished picture; it does not claim that Intel hardware has native NVIDIA NGX support.
- Downloads are pinned to exact release URLs and SHA-256 hashes. Existing unmanaged `dxgi.dll` or OptiScaler installs are never overwritten.
- Frame generation now defaults to Off. It is forced Off and disabled in the UI while the initial Intel DLSS 5 test route is selected.
- The performance panel reports Intel DLSS 5 as active only after OptiScaler logs identify the vendor-neutral DirectML neural path; configured state alone is no longer shown as a successful run.
- Disabling the option removes only Zink-managed OptiScaler files after creating a safety backup.

## App 2.8.0 / Mod R648 - 1 November 2026

- Restored the complete VisualV weather and lighting package instead of leaving its timecycles disabled.
- Replaced the incompatible Enhanced GitHub payload with the official VisualV **1.0.630 Legacy** package used by GTA V Legacy.
- The app now verifies the 196 MB release against SHA-256 `96E666AD5DCFB9840317E9DD15A0151C358C51FEB2A85265494B190D070A112C`, extracts its OIV, validates every required file and installs all 22 Legacy timecycles plus `visualsettings.dat`.
- Kept the incompatible Enhanced files in the existing quarantine backup; this release does not delete them.
- Updated the settings UI and provenance marker so they identify the full Legacy graphics overhaul accurately.

## App 2.7.1 / Mod R647 - 1 November 2026

- Fixed the first real VisualV test crash: fxReloader recorded `0xC0000005` at `GTA5.exe+0x16AE054` inside the Legacy timecycle path after approximately four and a half minutes.
- Removed and quarantined all 22 Enhanced-targeted VisualV timecycle overrides; they are no longer installed on GTA V Legacy.
- Retained the VisualV `visualsettings.dat` redirect, which loaded successfully, together with Zink's native post-reconstruction filmic enhancement.
- Updated the UI and provenance marker to report **Legacy-safe mode** honestly instead of claiming that incompatible weather/timecycle overrides are active.

## App 2.7.0 / Mod R646 - 31 October 2026

- Upgraded **Visual Mod enhancement** from a presentation-grade shader alone to a real, reversible VisualV graphics-data integration for GTA V Story Mode.
- Added the GPL-licensed fxReloader loose-file loader, built from pinned source commit `063ac3`; its complete corresponding source and licence ship with the app.
- Enabling the switch downloads VisualV directly from its creator's pinned GitHub commit, verifies the archive SHA-256, and installs its weather, lighting, timecycle and visual settings without editing Rockstar's `update.rpf`.
- Disabling the switch turns off loose-file redirection so the next GTA launch returns to stock weather and lighting while preserving a reinstallable local copy.
- Added collision protection: the app refuses to overwrite an independently managed fxReloader installation.
- **Restore GTA folder** now safety-backs up and removes the Zink-managed VisualV loose files, configuration and provenance marker as well as restoring the original root files.

## App 2.6.0 / Mod R645 - 31 October 2026

- Added a reversible **Enable Visual Mod enhancement** switch, applied live and disabled by default.
- Added a native realistic presentation profile for NVIDIA DLSS and Intel XeSS: restrained filmic contrast, natural warmth, controlled saturation, highlight roll-off, subtle vignette and edge-aware detail sharpening.
- The effect runs after reconstruction and Neural Rendering but before the HUD and optional frame-generation presentation path; turning it off returns to the original reconstructed image without installing a second graphics injector.
- Visual direction was informed by the open-source VisualV, fxReloader, BlancoVision and ReShade/iMMERSE approaches while retaining the existing Zink render integration.

## App 2.5.1 / Mod R644 - 31 October 2026

- Alt+Tab now suspends the replacement presentation surface and invalidates queued output while GTA is not foreground, then resumes only with fresh temporal history.
- Enlarged the in-game 4K performance panel and added the active NVIDIA DLSS 5 Neural Rendering or Intel XeSS runtime file version.
- The panel now distinguishes requested frame generation from measured extra presented frames, so inactive generation is no longer shown as active.

## App 2.5.0 / Mod R643 - 31 October 2026

- Researched current open-source RTX 40 Neural Rendering implementations, including the MIT-licensed RenoDX DLSSNR Nexus Feature-18 path.
- Completed the native NGX Feature-18 contract with standard input/output dimensions, performance-quality mode, and low-resolution motion/reverse-depth creation flags.
- Kept the existing direct D3D12 GTA integration instead of stacking a second ReShade Present hook over the mod.
- Retained R642 input transparency and automatic return to normal GTA whenever NVIDIA produces no generated frames.

## App 2.4.1 / Mod R642 - 31 October 2026

- Made the fullscreen presentation proxy explicitly mouse-transparent and non-activating so normal GTA menus, keyboard controls and mouse input remain owned by GTA.
- Added an RTX fail-open watchdog: three consecutive NVIDIA Presents with no generated frame now hide and cleanly stop the proxy rather than leaving GTA behind a slow source-only surface.
- Preserved DLSS Super Resolution and the RTX 40 experimental neural-rendering test while reporting unsupported DLSS 5 neural rendering honestly.
- RTX 40 continues to use officially supported single-frame DLSS Frame Generation when the NVIDIA runtime accepts it; normal GTA is the guaranteed fallback when it does not.

## App 2.4.0 / Mod R641 - 31 October 2026

- Replaced the old single black performance strip with a larger glass-style 4K HUD panel, cyan accent rail and grouped FPS, FG, CPU, GPU, VRAM and runtime metrics.
- NVIDIA status now distinguishes the experimental RTX 40 neural-rendering test from working frame generation.
- Fixed live App Off commands being ignored whenever NVIDIA upscaling was active; disabling now drops the persistent latch, hides the proxy immediately and returns presentation to normal GTA.
- Retained GTA as the keyboard and mouse input owner while the non-activating presentation proxy is visible.

## App 2.3.0 / Mod R640 - 31 October 2026

### Centred scanning progress

- Provider/environment scans now use the same prominent centred glass overlay as automatic settings application.
- The overlay reports the selected GPU, display, GTA installation, settings and runtime scan while it runs.
- Scan success, warnings and failures are shown in the centre before the overlay dismisses itself.
- Automatic settings application waits for the scan to finish, preventing overlapping operations and confusing status messages.
- The mod remains R640 because this is an app workflow feature.

## App 2.2.3 / Mod R640 - 31 October 2026

### Provider-card order

- Moved NVIDIA RTX to the left provider card and Intel XeSS to the right.
- Provider selection, automatic scanning and saved settings behaviour are unchanged.
- The mod remains R640 because native rendering code is unchanged.

## App 2.2.2 / Mod R640 - 31 October 2026

### DLSS Quality default

- New configurations and Reset to defaults now select NVIDIA with DLSS Quality instead of Native/Intel defaults.
- Selecting the NVIDIA provider automatically selects DLSS Quality rather than switching back to Native.
- Other DLSS modes remain available if explicitly selected, but Quality is the standard starting mode.
- Added regression coverage for the NVIDIA DLSS Quality default.
- The mod remains R640 because this changes app defaults rather than native rendering code.

## App 2.2.1 / Mod R640 - 31 October 2026

### Maximized startup

- The main Zink window now opens maximized so the complete 4K-oriented interface and centred settings overlay are immediately visible.
- The standard Windows restore and minimize controls remain available.
- The mod remains R640 because native rendering code is unchanged.

## App 2.2.0 / Mod R640 - 31 October 2026

### Centred settings progress overlay

- Moved automatic-setting progress from the bottom command bar to a prominent glass panel in the centre of the app.
- Increased the spinner and status-text size so waiting, applying, success and failure states are clearly visible at 4K desktop scaling.
- The overlay floats above the interface without intercepting mouse or keyboard input and disappears automatically after completion.
- The mod remains R640 because this is an app presentation improvement.

## App 2.1.0 / Mod R640 - 31 October 2026

### Automatic settings application

- Settings now apply automatically after a short debounce instead of requiring the Apply settings button for every change.
- A Fluent progress ring reports `Waiting to apply changes`, `Applying changes to GTA and mod` and the final result.
- Live-compatible controls, including frame generation, HUD, FPS limit and NVIDIA multiplier, update the running mod immediately.
- Restart-required render changes are saved automatically and clearly reported as pending for the next GTA launch.
- Rapid changes are grouped into one write to avoid repeated mod updates and unnecessary backups.
- The Apply settings button remains available as a manual retry and fallback.
- Automatic application is enabled by default and can be disabled from Advanced settings.
- The mod remains R640 because this feature changes app workflow rather than native rendering code.

## App 2.0.1 / Mod R640 - 31 October 2026

### Live settings permissions

- Fixed the access-denied error shown when enabling or disabling frame generation while GTA is installed under a protected Windows folder.
- The app now declares that it requires administrator access, so Windows presents the normal permission prompt when Zink starts instead of failing later during a settings write.
- Atomic settings-write errors now identify the exact blocked path and explain how to recover.
- The mod remains R640 because its native binaries and rendering behaviour are unchanged in this app-only patch.

## App 2.0.0 / Mod R640 - 31 October 2026

This is the cumulative major release for the work completed throughout this development chat. Earlier in-progress binaries were development snapshots rather than separate published releases, so they are represented honestly here as one completed 2.0 release instead of inventing version numbers for builds that were never shipped.

### Why this is version 2.0

- Replaced the original app presentation with a Fluent glass Windows 11 interface, scalable high-resolution layout, imagery, updated controls and Intel/NVIDIA branding.
- Added complete GPU-provider selection and provider-specific PC scanning for GPU, VRAM, RAM, desktop resolution, refresh rate, GTA paths and compatible settings.
- Added automatic GTA discovery, mod installation/update, default automatic activation, install backups and one-click restoration of the pre-mod GTA folder.
- Added live runtime reporting for the selected and observed DLSS/XeSS versions, frame generation, neural pipeline, provider, resolution, performance and status.
- Added NVIDIA DLSS 4, 4.5 and experimental DLSS 5 selection, RTX 40 validation/fallback behaviour, NVIDIA frame generation and Intel XeSS/XeFG support paths.
- Added fullscreen mod presentation, GTA launch configuration, brightness/colour-path work, VRAM corrections and correct main-GPU reporting.
- Added native activation, DPI-scaled swap-chain discovery and hybrid Intel/NVIDIA adapter corrections required for the mod to reach its rendering pipeline.
- Added semantic release tracking, exact automatic build identities and an in-app About/change-history panel.

### Version and progress tracking

- The sidebar and About panel now show App 2.0.0, Mod R640 and the exact automatic build identity.
- Every future compile receives a unique UTC build identifier in the form `2.0.0+R640.build.YYYYMMDD-HHMMSS`.
- The bundled changelog remains the readable source for features, fixes and release progress, and is copied into every app build automatically.

### Current activation and display fixes

- Fixed the DPI-scaled GTA swap-chain check that prevented the mod from reaching live render-target discovery.
- Fixed NVIDIA hybrid-laptop launching so GTA and the DLSS bridge select the RTX adapter instead of splitting rendering between Intel and NVIDIA.
- Added regression coverage for the RTX adapter selection and deployed the corrected launch configuration.

### NVIDIA hybrid-GPU launch correction

- Selects GTA adapter 0 for NVIDIA mode on Intel/NVIDIA hybrid PCs instead of forcing adapter 1 (the Intel adapter on the affected system).
- Keeps GTA's D3D11 renderer and the NVIDIA D3D12/DLSS bridge on the same RTX GPU, allowing shared colour resources and the fullscreen mod surface to start.
- Accepts DPI-scaled outer GTA swap-chain dimensions and discovers the real internal composited render target dynamically.

### Installation and recovery

- Automatically discovers Steam, Rockstar and Epic GTA V Legacy installations.
- Automatically discovers the active GTA `settings.xml`, including OneDrive Documents.
- Installs or updates the bundled mod before applying settings when required.
- Enables automatic mod activation by default; F9 remains available as a manual toggle.
- Creates safety backups before modifying GTA files or settings.
- Adds Restore GTA folder, restoring the state captured before the most recent mod installation.
- Adds manifest and SHA-256 validation so mismatched native files are rejected.

### Intel and NVIDIA provider scans

- Selecting Intel or NVIDIA scans the requested GPU, dedicated VRAM, desktop resolution and refresh rate.
- Scans GTA version, installation path, settings path, adapter index, display mode and saved resolution.
- Checks every provider-specific runtime file before launch.
- Queries NVIDIA DLSS and frame-generation capability from the installed driver.
- Checks the Intel Arc requirement and Intel XeSS, XeSS-FG and XeLL payloads.
- Queries the selected SDK for the exact input render size and shows the render-to-output contract.
- The System panel now follows the selected provider instead of always displaying the NVIDIA GPU.

### NVIDIA and DLSS

- Adds selectable DLSS 4, DLSS 4.5 and experimental DLSS 5 modes.
- Adds the explicit RTX 40 experimental DLSS 5 option and RTX 40 2x frame-generation policy.
- Adds RTX 50 2x, 4x and 6x capability-driven frame-generation choices where supported.
- Adds DLAA/native-AA and DLSS quality selection.
- Adds NVIDIA Streamline, DLSS Super Resolution, DLSS-G and Reflex runtime reporting.
- Corrects the B354 bridge bug that inherited GTA's Intel D3D11 adapter. NVIDIA mode now enumerates and validates vendor `0x10DE` for the D3D12/Streamline bridge.
- Uses the selected NVIDIA bridge adapter for VRAM telemetry.

### Intel XeSS

- Adds Intel XeSS upscaling and native-AA choices.
- Adds Intel XeSS Frame Generation and XeLL integration.
- Adds Intel-specific Ultra Quality and Ultra Quality Plus modes.
- Keeps Intel and NVIDIA provider settings isolated when switching providers.

### Display and launch

- Detects the physical desktop resolution and refresh rate instead of using scaled UI dimensions.
- Adds fullscreen launch configuration and Windows high-performance GPU preferences for GTA executables.
- Calculates GTA's source render resolution from the chosen output resolution and SDK quality mode.
- Supports 1080p, 1440p and 4K output profiles with saved GTA XML/INI agreement.
- Starts the desktop app at a compact 1200 x 760 size.

### Interface and information

- Rebuilt the desktop app with a Windows 11 Fluent glass/Mica interface, acrylic cards, imagery and updated buttons.
- Added Intel and NVIDIA branding.
- Corrected installed RAM, usable RAM, physical desktop resolution, main GPU selection and dedicated GPU memory reporting.
- Added a live graphics-runtime information panel showing the configured or observed upscaler, runtime versions, neural pipeline and frame generation state.
- Added provider readiness results and a Scan again action.
- Added profiles, automatic apply, source-FPS limits and navigation for Graphics, Upscaling, Frame Generation and Advanced settings.

### In-game HUD and diagnostics

- Adds an in-game performance HUD configuration for source FPS, output FPS, multiplier, CPU, render-thread CPU, GPU, VRAM, runtime and status.
- Adds switches for FPS, frame intervals, usage and runtime status.
- Adds F11 artifact marking and F8 comparison diagnostics; F9 toggles the mod.
- Adds live telemetry and runtime-log parsing to distinguish configured values from verified active behavior.
- Adds clearer pending, active, fallback and failure reporting.

### Reliability fixes

- Allows DPI-scaled GTA window swap chains to start the pipeline while the exact SDK input is discovered from the internal render target, fixing startup at Windows display scaling above 100%.
- Corrected scaled fullscreen launch: GTA now stays at the SDK input size while the mod owns the full-monitor presentation surface, preventing desktop-resolution source rejection and exclusive-fullscreen gamma darkening.
- Fixed application startup/deployment failures by shipping matching XBF, PRI, runtime configuration and app binaries.
- Preserves unknown INI/XML settings and duplicate known keys while applying supported settings.
- Rejects changes while GTA is running when a safe live update is unavailable.
- Adds rollback when GTA starts during a settings transaction.
- Adds versioned native runtime backups and an application bridge marker.

### Current limitations

- DLSS 5 on RTX 40 remains an experimental community path and may be rejected by the supplied NVIDIA neural runtime or driver.
- The in-game HUD appears only after the native rendering pipeline reaches an active presentation state.
- GTA V Legacy and a compatible ScriptHookV installation are required.

# Unreleased

- Fixed NVIDIA launches on Intel/NVIDIA hybrid PCs by selecting GTA's discrete RTX adapter (adapter 0) instead of forcing the Intel adapter. This keeps GTA's D3D11 renderer and the NVIDIA D3D12/DLSS bridge on the same GPU so the mod can create its shared colour resources and fullscreen output.
- Allowed GTA's DPI-scaled outer swap chain to differ from the configured internal render input; live discovery now selects the actual internal composited render target instead of rejecting the game window before activation.

