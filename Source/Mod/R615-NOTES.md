# R615 smoother default

User confirms R613 one-frame queue felt smoother; R614 one/two/one comparison confirms one-frame source67.9/70.3 output172.7 vs two-frame53.5 output184, higher queue age/variation with2. User authorized default-one while FG enabled and original setting on disable.

Apply B614SetNativeQueue(1) once during each successful XeFG activation after native-device/resource prewarm and before live capture enabled. Retained sessions and F9 reactivation follow same activation path. Existing exact original-state scope and worker-stop restoration remain. Original driver setting is never overwritten by the benchmark one/two switches. Failed queue setting is logged; the optional scheduling control does not independently break an otherwise ready FG session.

New B615RestoreNativeDefault chooses1 only while live FG is ready, active, without shutdown/stop request; otherwise restores original. Comparison completion/cancel/focus/timeout returns to1 when FG remains enabled. Explicit user F9-off restores original, and worker shutdown independently restores original on stop/error. This avoids restoring default1 into a pipeline already draining after failure. Full motion/depth inputs, R611 ghosting fix, protected temporal/freezing and uncapped x4 unchanged. Reduced-input experiment remains disabled.

Validation: production comparison regression passes default-one restoration after completion/F8/focus/timeout/invalid sample/failed change and exact original request on F9-off. Existing scope regressions and prior Arc A7703->1->2->1->3 hardware validation remain applicable; no driver control implementation change. Release stage succeeds with existing C4244 warnings. Four installed runtime files SHA256 verified, INI and freezing DLL preserved. Backup pre-R615-* contains Restore.ps1. New default lifecycle needs gameplay confirmation.

Use: restart GTA and F9, play normally. No F8 comparison required. R615 NATIVE_QUEUE_DEFAULT should report frames=1, applied=1; bridge records saved original and restoration when F9 disabled. Send newest ScriptHook/bridge logs if anything regresses.
