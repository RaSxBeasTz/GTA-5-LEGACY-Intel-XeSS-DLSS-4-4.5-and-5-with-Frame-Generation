# R626 DLSS version selector

NVIDIA Upscaling now contains one version dropdown instead of the DLSS5 toggle: DLSS 4, DLSS 4.5, DLSS 5 (experimental). Quality and FG multiplier stay independent.

DLSS 4 explicitly sets preset K for every SR quality mode. DLSS 4.5 explicitly sets preset M for DLAA/Quality/Balanced/Performance and preset L for Ultra Performance. NVIDIA documents M/L as supported in Quality/Balanced/DLAA too, although Performance/Ultra Performance offer the best intended tradeoff. DLSS5 uses the same M/L SR base plus the existing separate NR pass. These are model-family selections using the installed SDK, not claims of switching three separate DLL versions. NVIDIA App/driver overrides can supersede app requests.

Settings write NVIDIA.DLSSVersion and derive NVIDIA.DLSS5 from the selected version. Legacy preferences/profile files with only NvidiaDlss5 migrate to 5 when true, otherwise 4.5. Invalid versions are rejected. Live version changes remain pending until GTA restarts. Existing Intel mode and current 1440p Quality/x2 DLSS5 selection preserved. Runtime reads version once per session, avoiding per-frame INI reads.

Validation: backend regressions passed for all three settings/INI/readback paths, invalid versions, deferred live changes and prior tests. Release app/native builds passed. GPU smoke: each selection completed 100/100 x2 multiplier frames and 200 actual presentations. DLSS5 additionally evaluated 100/100 NR frames. Actual backend logs showed preset=11 NR=0; preset=13 NR=0; preset=13 NR=1 respectively. No new full GTA gameplay test performed.

App and game manifests verified. Runtime backup: %LOCALAPPDATA%/ZinkGTAUI/backups/20261028-013640-402-runtime-54e22a. Settings backup: %LOCALAPPDATA%/ZinkGTAUI/backups/20261028-013641-785-settings-4c3fc6. Sources: backups/pre-R626-*.

Reference: https://www.nvidia.com/en-us/geforce/forums/geforce-graphics-cards/5/580689/dlss-45-super-resolution-faq/
