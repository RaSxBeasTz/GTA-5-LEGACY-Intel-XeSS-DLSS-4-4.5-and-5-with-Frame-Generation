# R628 — visible Neural Rendering controls

The R627 NR wrapper passed successful calls to the supplied runtime but hard-coded tone, local structure and skin structure strengths to 0, with overall intensity 0.5. The real-game comparison confirmed almost no change: mean absolute RGB difference 0.000468; only 0.699% of pixels differed by more than 1/255 in any RGB channel.

R628 uses intensity/tone/structure/skin defaults of 100%, with UI correction retained. Controls can be set as integer percentages in the NVIDIA INI section: NRIntensity, NRTone, NRStructure, NRSkin. NRUICorrection remains a boolean. Changed controls reset NR history once and are logged. The three strength controls no longer remain silently disabled.

Opt-in diagnostic: Diagnostics/NRComparison=N captures paired FP16 SR input and NR output from the exact same GPU frame, after 120 NR evaluations. Captures are fenced before CPU mapping and preserve original resource states. Normal startup with NRComparison=0 allocates no readback resources. It does not inject a visual filter: images are the actual pre-NR/post-NR GPU textures, saved as clipped 8-bit BMPs, and numerical measurements use their original FP16 samples.

Real GTA validation at 1440p, Performance input 1280x720, DLSS5 and x6:
- Original controls: changed-over-1/255 = 0.699382%; mean RGB delta = 0.000468007.
- Enabled strengths, UI correction off: 98.8961%; mean = 0.0302634.
- Enabled strengths, UI correction on (release default): 94.5717%; mean = 0.0168193; zero invalid channels.
- Inspected paired captures: visible changes in the lamp illumination/material, wooden door, walls, clothing and skin. This establishes actual NR visual output, rather than relying on successful API returns.
- Over 1,400 successful NR evaluations; NVIDIA present status continued actual=6, requested=6, status=0 through submission 1800. No crash during this run. This is a short scene test, not comprehensive visual or temporal validation.
- Native build with protected-source integrity checks passed; WinUI build passed with no warnings/errors. Camera, ghosting and freezing code was not changed.

Evidence: profiling/R628-NR-comparison/. Each numbered before/after pair is a single identical frame; different numbered pairs were captured at different times and should not be used as identical-scene pixel comparisons.

Installed app and GTA runtime updated together. App preferences retain DLSS5, 1440p Performance and x6. NRComparison is disabled in the final installed INI. The experimental GTA integration does not establish NVIDIA support/certification or guarantee the look of another game's authored integration.
