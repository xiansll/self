# P6 Rage Dry-Run Foundation

This batch intentionally builds the Rage feature surface without wiring live execution.

## Added

- `RageDryConfig`
- `CommandContext` owner slot (`CUserCmd*`, never dereferenced here)
- `CombatFrameSnapshot`
- `WeaponSnapshot`
- `PredictionSnapshot`
- `CandidateSnapshot`
- `LagRecordSnapshot`
- `ShootHistorySnapshot`
- `HitchanceResult`
- `PenetrationResult`
- `DamageResult`
- `StopPredictionResult`
- `ExtrapolationResult`
- `DoubletapEligibility`
- `AntiAimPlan`
- `QuickPeekPlan`
- `DryRunActionPlan`
- `ReadinessMatrix`
- `DecisionEngine`
- Built-in synthetic A/B/C self-test
- Aimbot UI controls for dry-run feature configuration
- Config persistence for the dry-run config

## Explicitly not wired

- No CreateMove hook
- No command acquisition
- No command mutation
- No attack button writes
- No view-angle writes
- No movement writes
- No tick shifting
- No live Entity/Bone/Hitbox/Trace provider binding

## Built-in self-test

The test creates:

- A: fov 4.2, distance 300, visible
- B: fov 1.8, distance 800, visible
- C: fov 0.9, distance 250, invisible

Expected:

- FOV selection => B
- Distance selection => A
- Invisible C is rejected when visibility is required

## Next runtime foundation batch

Without binding execution, the next batch can add owner/provider adapters for:

1. Combat frame snapshot publication
2. Weapon snapshot publication
3. Entity candidate publication
4. Bone/hitbox snapshot publication
5. Read-only lag record collection
6. Readiness/debug panel publication
7. Synthetic trace / penetration evaluator tests

Runtime trace remains blocked until a separately validated trace backend exists.
