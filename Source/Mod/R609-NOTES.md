# R609 x2/x3/x4 scaling diagnostic

F8 runs full_x2, full_x3, full_x4 in order. Each phase waits for worker acknowledgement and visible output, settles 3 seconds, then measures 12 seconds. Normal operation remains x4. Completion, F8 cancellation, F9 cancellation, focus loss and transition timeout request x4 restoration; F9 off remains off.

The native SDK SetNumInterpolatedFrames call runs exclusively on the existing FG worker, outside concurrent init/destroy. Only changed counts are applied. The SDK warns that a change may compile shaders/reallocate resources, so transition/settling time is excluded. A failed SDK change returns failure to the existing worker recovery path instead of continuing on a disabled/error SDK. Changing count requests fresh temporal history. No resolution, source cap, capture, protected camera/motion shaders or freezing changes.

CSV retains source/output FPS, native capture GPU timings and FG queue interval. The queue interval includes waits and cannot alone distinguish shader work from presentation pacing. Use the results to establish scaling before choosing an optimization.

Validation: production runner regression covers delayed acknowledgement, three labelled results, completion, F8/F9/focus cancellation and timeout. Release stage succeeded with existing C4244 warnings. Both freezing sources retain expected SHA256 528d8f37e0a46770bc689924bfbc2a10d2e17c01b3998678f8b6cb8e3c362448. Four installed runtime files hash verified, existing freezing DLL and INI preserved. Gameplay validation pending.

Test: start GTA, enable F9, let visible output settle, remain stationary in a representative scene, press F8 and wait about 60 seconds. Send newest Comparison CSV and ScriptHook log. No recorder/admin command required. Mode switching can briefly disturb output during unmeasured settling.
