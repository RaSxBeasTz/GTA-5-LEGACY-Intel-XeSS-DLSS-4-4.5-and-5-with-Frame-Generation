# R610 comparison status correction

R609 gameplay log 20261020-070710 revealed two independent problems. The existing expectedStatus check only accepted four-frame success and rejected valid three-frame success during x3, causing status_errors=1 and fallback. Camera rebase/reset bursts were present before COMPARISON_BEGIN and continued through the x2 sample; 125/125 is not valid x2 scaling evidence. The repeated reset cause remains unresolved; this build does not alter protected camera selection/reset rules.

Status validation, hidden reveal eligibility, diagnostic skipped-status fallback and backlog classification now use worker-applied interpolation count + 1. Normal x4 remains four frames. Comparison always queries SDK status instead of relying on assumed output. Counters/CSV add reset_frames, generated_frames and valid_interpolation_sample. Samples producing under 90 percent of requested generation are marked invalid, terminate with explicit reason, and request x4 restoration. No suppressing genuine camera resets to make benchmarks pass.

Release build succeeds (existing C4244 warnings). Production comparison regression covers all modes, worker acknowledgement, F8/F9/focus/timeout restoration and reset-dominated sample rejection. Protected freezing source hashes unchanged. Installation backed up and four runtime hashes verified. Gameplay validation pending.

Run GTA, F9, wait for normal gameplay camera/output to settle, keep same scene, F8, wait 60 seconds. Send newest Comparison CSV and ScriptHook log, including if it reports invalid_sample_history_resets_or_missing_generation. That outcome is diagnostic evidence rather than a performance result.
