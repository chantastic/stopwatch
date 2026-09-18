# Native reset coordinator checks

Run `python3 tests/factory-reset/run.py`. The runner extracts and compiles the
actual `persist`, `requestReset` and `pollReset` functions from native `main.cpp`
with the production settings/bookmark/unlock policies. NVS, service completion,
model refresh and board brightness are bounded host doubles. It never opens a
serial port or resets a device. Address and undefined-behavior sanitizers apply.

The NVS double writes each successful key immediately, matching the pinned IDF
implementation; it does not pretend the four keys are an atomic transaction.
Fixtures cover pending/running/failed service results, duplicate confirmation,
failed request, a failure at each preference write and at commit, unavailable
NVS, explicit preference-only retry, a new profile invalidating that shortcut,
resumption of older delayed saves after failure, and terminal idempotence. A
successful reset clears both the saved After Dark unlock and any pending unlock
save, so the old delayed save cannot unlock it again. Each failed preference
boundary retains the in-memory unlock and reports incomplete reset; a commit
failure can already have stored the locked byte. Clock and a legacy-data sentinel
remain unchanged.

The service transaction itself is exercised by `tests/factory-services` and the
native confirmation/modal behavior by `tests/factory-ui`. This fixture covers the
main coordinator between those boundaries, not physical flash power cuts.
