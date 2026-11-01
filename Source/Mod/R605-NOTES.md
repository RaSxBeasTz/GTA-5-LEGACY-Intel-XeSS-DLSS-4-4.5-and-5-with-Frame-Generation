# R605 stable driver latency mode

R604 still reported DEVICE_HUNG during XeFG Present. Its healthy late telemetry was51.40sourceFPS and147.63outputFPS. Nonblocking native GPU capture samples show depth packing around0.02ms and colour capture around0.04ms: these samples do not support capture copies as the primary multi-millisecond bottleneck. They do not measure full XeFG interpolation, complete GTA GPU time or all stalls.

R605 removes the per-backlogged-frame low-latency disable/enable workaround. SetSleepMode remains configured once during initialization; every Present ID still gets exactly one xellSleep before refresh, markers and preparation. All six markers and error handling remain. XeFG stays attached to XeLL. No source cap, Present flags, interpolation count, resource ownership, shader or camera/history policy changes.

This removes repeated driver-mode churn; it is not proof that mode churn caused the GPU hang. Build passes with existing C4244 warnings. Source audit confirms one sleep, no mode mutations in the live worker and preserved marker order. DLL integrity passes and locked FreezingFix hash is unchanged. Installed runtime files SHA256 verified. Full gameplay stability/performance require confirmation.
