# R632 — NVIDIA undilated motion contract correction

**Superseded by R633 investigation:** the active full-resolution worker uses GhostingMotionShader1, which already dilates motion in a 3x3 neighborhood. Only the optional reduced-grid shader is undilated. R632 incorrectly generalized the reduced-grid behavior to the active path. Its false dilation flag must not be retained for full-resolution inputs. Synthetic generation success did not establish visual correctness.

The source and reduced-resolution motion shaders output undilated per-pixel vectors. NVIDIA Constants incorrectly advertised motionVectorsDilated=true. Corrected to false so SDK processing receives the actual input contract. Existing camera reconstruction and protected ghosting/freezing modules were preserved.

Native integrity build and app build passed. MotionGpuRegression passed, including foreground-edge and reduced-grid undilated checks. NVIDIA 4K Quality + NR x6 smoke: 100/100 NR evaluations, 100/100 full-multiplier frames, 600 actual presents. Evidence: profiling/R632-smoke.txt. This synthetic test verifies generation, not visual correctness in GTA movement. Camera-only motion still lacks independent object velocities; no claim that all trails are eliminated.

Updated NVIDIA DLL in staged payload, source app bundle/manifest, installed app bundle and GTA. Base bridge remains R631; NVIDIA constants log identifies R632 correction. Existing 4K Quality/DLSS5 selection retained, FG re-enabled at saved x6. Fluent Acrylic UI retained, installed build passes without warnings/errors.
