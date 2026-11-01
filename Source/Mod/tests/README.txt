B546 current temporal-policy validation: B539Differential.vcxproj compares the
new DLL against the preserved B539 binary on b539-camera-fixture.txt. Copy the
reference binary as B539Reference.dll beside the executable (test-only).
ResetRegression.cpp contains later B541 continuity expectations and is historical
for this B539-policy restoration; it is not the B546 acceptance test.

Tests for B529

- audit_relocation.py compares extracted camera code and shader strings with the
  original B499 files in baseline/. It also checks the locked B423 source hash.
- TemporalReplay.vcxproj builds a console executable. Build after GhostingFix,
  using the same Release/x64/v143 configuration. Put GTAXeSS.GhostingFix.dll
  beside the executable and run from a writable directory. It compares 2,400
  frames with code copied directly from the original B499 temporal path and
  compiles all four motion shaders. It creates diagnostic logs locally.
- check_integrity.py creates isolated copies below tests/integrity and checks
  matching, modified-hash, legacy-API and missing DLL cases. It needs the cached
  XeSS runtime bin directory used by the build. fixtures/GhostingFix-v1.dll is
  solely a negative-test fixture and must never be installed into GTA.

These tests do not emulate GTA/D3D scheduling or verify visible ghosting.
