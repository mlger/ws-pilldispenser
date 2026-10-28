<!--
Acceptance checklist (spec C9). A PR cannot merge until every non-N/A item is provable
from the description or linked artifacts.
-->

## Linked issue

<!-- Link the issue this PR closes: "Closes #123". PRs without a linked issue need a justification line. -->

## Area

- [ ] firmware-main
- [ ] firmware-coproc
- [ ] cloud
- [ ] android-app
- [ ] web-flasher
- [ ] tools / CI
- [ ] docs

## Test evidence

<!-- Paste or link: host CTest output, QEMU JUnit summary, HIL logs (if applicable). -->

- [ ] Host tests (`ctest`) — N/A because: …
- [ ] QEMU tests (`pytest -m qemu`) — N/A because: …
- [ ] HIL (nightly / label `run-hil`) — N/A because: …

## Safety impact

- **Does this change dose timing or dose count?** yes / no
- **FMEA updated?** yes / no / N/A (required `y` when dose logic changes; see docs/fmea.md)

## Size impact

<!-- tools/ci size-report posts a table; add label `size-ok` if >64 KB app growth is intended. -->

## Screenshots / UI evidence

<!-- Before/after for anything user-visible. -->
