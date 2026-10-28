# android-app

Companion app (Phase 5, spec Section F). Placeholder until the Gradle project
lands — `ci-android.yml` self-skips until `android-app/settings.gradle*`
exists.

## Phase 5 checklist

- [ ] Gradle wrapper + Kotlin project (Temurin 21)
- [ ] ktlint + detekt with committed baselines
- [ ] Unit tests; optional nightly instrumented tests on an emulator
- [ ] Release signing via the `android-release` environment (upload keystore +
      Play App Signing; spec G2)
- [ ] Pin `gradle/actions/setup-gradle` to a full SHA (docs/ci-cd.md appendix)
