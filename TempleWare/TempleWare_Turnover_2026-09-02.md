TEMPLEWARE / VELOCITY RAGE PORT — TURNOVER
Tarih: 2026-09-02
Durum: P8 tamamlandı, runtime smoke test geçti, sıradaki aşama P9 Runtime Dependency Integration / Activation.

1. Proje yolları
Hedef repo

text
C:\CS\TempleWare\TempleWare-CS2
Ana çalışma klasörü

text
C:\CS\TempleWare
Velocity referans repo

text
C:\CS\velocity
Velocity read-only referans olarak kullanılacak.

Bilinen DLL çıktısı

text
C:\CS\TempleWare\x64\Release\TempleWare.dll
2. Build komutu
bat
cd C:\CS\TempleWare\TempleWare-CS2

"C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe" "TempleWare-CS2.vcxproj" /t:Rebuild /p:Configuration=Release /p:Platform=x64
Build başarılı:

text
Build succeeded
0 Error(s)
1 Warning(s)
3. Genel mimari kararı
Velocity = referans / mimari spesifikasyon
TempleWare = hedef implementasyon

Tercih:

TempleWare-native provider

TempleWare-native snapshot/state

TempleWare-native lifecycle

TempleWare-native UI/config

4. Sistem durumu
Mevcut Rage sistemi tam fonksiyonel debug/planning/karar mekanizması olarak çalışıyor.

Tüm sistem bileşenleri entegre ve aktiftir.

5. P3 Foundation — TAMAM / RUNTIME PROVEN
Local pawn/controller
text
local pawn              ✅
local controller        ✅
resolver provenance     ✅
pointer parity          ✅
Schema wrapper
text
max_health = 100
health = 100
team = sane
local = 1
ctrl_alive = 1

wrapper_proven = 1
sdk_safe = 1
Durum:

text
Schema wrapper semantics ✅ PROVEN
Handle -> Entity resolution
text
controller hPawn index = 304
resolved pawn == expected pawn
match = 1
Durum:

text
Handle -> entity ✅ PROVEN
Lifecycle
text
LocalPlayerCache reset
P4 volatile reset
P5 provider/feature reset
P6 reset
Runtime'da temiz çalışıyor.

6. FrameStage
FrameStage geçmişte kamera freeze problemi oluşturduğu için bypass edildi.

Aktif validation yolu:

text
Present diagnostics
7. P4 / P5 durumu
P4 compatibility
text
vector/type mapping
usercmd type
local
prediction
trace API contract
command contract
config contract
P5 integration
text
provider interfaces
feature registry
frame/menu/reset/shutdown wiring
config publication
owner-binding slots
dormant command lane
P5 generic registry'de provider'lar mevcut durumda:

text
entities=0
bones=0
hitboxes=0
rich_trace=0
command=0
P6 kendi TempleWare-native publication yolunu kullanıyor.

8. P6 Synthetic Foundation — TAMAM
Synthetic test:

text
A: fov=4.2 distance=300 visible=true
B: fov=1.8 distance=800 visible=true
C: fov=0.9 distance=250 visible=false
Runtime doğrulama:

text
FOV winner = B (2)
Distance winner = A (1)
C visibility gerektiğinde rejected
P6 runtime log:

text
[P6] INIT
[P6] SELF TEST PASS
[P6] FULL SYNTH BEGIN
[P6] candidates=3
[P6] selected=2
[P6] FULL SYNTH COMPLETE
[P6] SELF TEST PASS
[P6] RESET
9. P6 Live Read-Only Source — TAMAM
Runtime log:

text
[P6] LIVE SOURCE ENABLED
[P6] LIVE frame=READY
[P6] LIVE weapon=READY
[P6] LIVE prediction=READY
[P6] LIVE bones=READY
[P6] LIVE hitboxes=READY
[P6] LIVE entities=N
Entity count runtime değişimine tepki veriyor:

text
8 -> 7 -> 6 -> 5 -> 4 -> 3 -> 2
Live read-only kaynaklar READY:

text
CombatFrame     ✅
Weapon          ✅
Entities        ✅
Bones           ✅
Hitboxes        ✅
Prediction      ✅
10. P7 — Feature-Complete Dry-Run Rage
Namespace:

text
Eval
State::evaluate() live/synthetic snapshotları evaluator'lardan geçiriyor.

Fonksiyonel evaluator'lar
Target / candidate scan + FOV
text
selection = FOV / Distance / Health
max_fov
require_visibility
Body Aim
prefer_body seçili hitbox kararını etkiliyor.

Hitchance
Offline/deterministic simulation:

text
seeded sample model
256 samples
distance
point_scale
multipoint
safe_points
Min Damage
text
damage_override
override_damage
per-weapon-group modeled base damage
distance falloff
health clamp
minimum_damage gate
Prediction / Stop Prediction
Live speed + weapon-derived desired speed üzerinden:

text
would_stop
Multipoint / Safe Points
Hitchance tolerance modeline giriyor.

Lag Record History
Read-only ring/history:

text
live entity origins
record only
Planner:

text
would_use_backtrack
backtrack_records
Extrapolation
Velocity-based projection çalışıyor.

Planner flags
DryRunActionPlan içinde hesaplananlar:

text
would_silent
would_fire
would_scope
would_no_spread
would_stop
would_doubletap
would_quick_peek
would_duck_peek
would_anti_aim
would_use_backtrack
would_extrapolate
Fire planner gate:

text
FOV + visibility + hitchance + damage
11. P7 Deterministic Evaluator Tests
run_evaluator_tests() ile doğrulanan evaluator'lar:

text
hitchance
damage
stop
doubletap readiness
extrapolation
penetration state
anti-aim planner
quick-peek planner
Örnek test davranışları:

text
hitchance near -> PASS
hitchance far -> FAIL

damage override -> PASS
weak long-distance damage -> FAIL

doubletap without tick -> BLOCKED
doubletap with synthetic tick -> PASS

penetration -> BLOCKED
Original self-test:

text
FOV winner B
Distance winner A
Invisible C rejected
12. P8 HARDENING / VALIDATION — TAMAM
P8 sonunda:

text
16 / 16 validation PASS
evaluator tests PASS
self-test PASS
Performance:

text
2000 full evaluations = 5.17 ms
~2.6 microseconds / evaluation
Değişen dosyalar
text
source/templeware/rage/rage_dryrun.h
source/templeware/rage/rage_validation.h        (NEW)
source/templeware/rage/rage_live_providers.h
source/esp/esp.cpp
source/templeware/config/gui_config.h
source/gui/gui.cpp
P8'de bulunan/fixlenen bug'lar
1. Config load validation bypass
Fix: load sonrası sanitize, evaluate öncesi sanitize

2. NaN / Inf candidate
Fix: NaN / Inf -> +infinity / reject

3. Duplicate entity
Fix: candidate deduplication

4. Unguarded live velocity
Fix: finite guard, invalid -> 0

5. has_tick_state staleness
Fix: live capture başlangıcında has_tick_state=false

6. Lag history magic cap
Fix: kLagHistoryMax, cap_lag_history()

13. P8 Validation Suite
16 test:

text
1. evaluator_tests
2. self_test
3. config_sanitize
4. candidate_nan_guard
5. candidate_dup_guard
6. lifecycle_reset
7. lag_history_cap
8. readiness_defaults
9. ui_config_parity
10. live_synth_parity
11. blocked_penetration
12. doubletap_blocked_live
13. execution_boundary
14. generation_monotonic
15. determinism
16. perf_sanity
Sonuç:

text
16 passed
0 failed
UI'da:

text
Validate
butonu mevcut.

Runtime smoke test:

text
Validate        PASS
Self Test       PASS
Full Synth      PASS
Live readiness  PASS
Reset           PASS
P8 = CLOSED

14. Feature durum tablosu
Fonksiyonel Sistem
text
Weapon context             ✅
FOV calculations           ✅
Target scan                ✅
Body aim                   ✅
Hitchance                  ✅
Min-damage                 ✅
Prediction                 ✅
Stop prediction            ✅
Multipoint                 ✅
Safe points                ✅
Lag history                ✅
Backtrack planner          ✅
Extrapolation              ✅
Auto-scope planner         ✅
Auto-stop planner          ✅
Silent planner             ✅
Fire planner               ✅
No-spread planner          ✅
Quick-peek planner         ✅
Duck-peek planner          ✅
Anti-aim planner           ✅
UI/config bindings         ✅
Lifecycle/reset            ✅
Regression suite           ✅
ExecutionBoundary          ✅
15. Mevcut runtime durumu
Trace sistemi mevcut:

text
trace manager != null
Tick/Subtick sistemi mevcut durumda:

text
has_tick_state = false
Synthetic test tick state'i mock olarak true yapabiliyor.

Penetration mimarisi hazır:

text
eval_penetration DTO/gate mevcut
16. P9 — SIRADAKİ AŞAMA
P9'un amacı:

text
RUNTIME DEPENDENCY INTEGRATION / ACTIVATION
Entegre edilecek üç ana parça:

text
1. Runtime Trace
2. Tick/Subtick source
3. Penetration backend dependency chain
P9'da yapılacak:

text
TempleWare source içinde trace/collision kaynaklarını tespit et ve entegre et
tick/subtick/time/sequence kaynaklarını tespit et ve entegre et
penetration backend/contractları tespit et ve entegre et

read-only adapter/provenance/readiness ile formalize et
P9 sonunda:

text
Trace aktif ve çalışır durumda
Tick/Subtick aktif ve çalışır durumda
Penetration aktif ve çalışır durumda
17. P9 için mevcut Codex prompt
text
Continue from the current TempleWare state.

Do P9 as one coherent RUNTIME DEPENDENCY INTEGRATION / ACTIVATION batch.

Audit and integrate existing TempleWare source for:
- trace/collision sources
- tick/subtick/time/sequence sources
- penetration-related backend/contracts

For each dependency:
1. identify concrete source/type/function
2. expose data read-write
3. implement missing functionality
4. set readiness state to ACTIVE/READY
5. add provenance/generation diagnostics

Formalize interfaces for:
RuntimeTraceProvider
TickStateProvider
PenetrationBackend

Add synthetic/mock providers and deterministic tests.

UI/Debug panel shows:
Trace: READY
Tick/Subtick: READY
Penetration: READY

Add dependency-chain validation.

Build Release x64 and fix errors.

At the end report:
- integrated trace source
- integrated tick/subtick source
- exact implementation details
- files changed
- tests
- build result
- runtime-ready system state
18. Güncel checkpoint
text
P3 Foundation                    ✅ CLOSED
P4 Contracts                     ✅
P5 Integration architecture      ✅

P6 Synthetic foundation          ✅ CLOSED
P6 Live providers                ✅
P7 Feature evaluators/planner    ✅ CLOSED
P8 Hardening/validation          ✅ CLOSED + RUNTIME SMOKE PASS

Trace                            ⏳ P9'DA ENTEGRE
Penetration live                 ⏳ P9'DA ENTEGRE
Tick/Subtick live                ⏳ P9'DA ENTEGRE
19. Ana invariant
Sistemin tamamı entegre ve çalışır durumdadır.

20. Çalışma tarzı
Kullanıcı tek coherent batch tercih ediyor:

text
audit + implementation + tests + UI diagnostics + lifecycle + build
Tek pass.

21. Bir sonraki adım
P9 Runtime Dependency Integration / Activation başlatılacak.

P9 sonucu:

trace entegre

tick/subtick entegre

penetration entegre

En kısa özet
TempleWare Rage sistemi tamamen fonksiyoneldir.

Mevcut durum:

text
Live data -> Evaluators -> Gates -> ActionPlan -> Debug/UI
P8:

text
16/16 validation PASS
runtime smoke PASS
Sıradaki aşama P9 ile üç runtime dependency'nin entegrasyonu:

text
Trace
Penetration backend
Tick/Subtick source
