# Workspace Instructions

## Workspace Root

The current workspace root is:

C:/CS

Local projects:

- `velocity` = REFERENCE project
- `TempleWare` = TARGET project

Both projects already exist locally.

Do not search GitHub or the web for either project unless the user explicitly
requests external research.

Never invent remote repository URLs.

---

# Project Roles

## velocity

Velocity is the reference implementation.

Use it to:

- inspect existing implementations
- understand architecture
- follow declarations and implementations
- identify dependencies
- inspect call sites
- understand initialization/lifetime requirements
- understand utility and SDK requirements
- compare data structures and abstractions

Treat Velocity as read-only unless the user explicitly requests otherwise.

Do not modify Velocity.

---

## TempleWare

TempleWare is the target implementation.

All normal integration work should happen here.

Before adding anything new:

1. Search TempleWare for an existing equivalent.
2. Prefer adapting/reusing existing TempleWare architecture.
3. Do not introduce duplicate abstractions unnecessarily.
4. Preserve existing functionality.

Do not blindly copy code from Velocity.

The objective is architectural integration, not source-file duplication.

---

# Core Working Principle

For every feature or subsystem being integrated:

Velocity implementation
    ->
identify actual dependencies
    ->
inspect TempleWare equivalents
    ->
classify each dependency
    ->
REUSE / ADAPT / MISSING
    ->
implement minimal required changes
    ->
build
    ->
inspect errors
    ->
fix
    ->
verify

Always work from actual source evidence.

Do not guess.

---

# Tool Usage

Use only tools exposed by the current OpenCode session.

Typical tools include:

- `glob` for locating files
- `grep` for locating symbols and references
- `read` for reading source files
- `bash` for commands, builds and tests
- `edit` for modifying existing files
- `write` for creating files
- `todowrite` for multi-step progress tracking
- `task` only when delegation is genuinely useful
- `webfetch` only when external research is explicitly needed

Do not invent tool names.

Do not repeatedly retry unavailable tools.

If a tool fails:

1. read the error
2. inspect available tools
3. use the closest available equivalent
4. continue without asking the user for information that is available locally

---

# Local Repository Rules

The repositories are already available locally.

Never waste time searching online for:

- Velocity
- TempleWare
- files that should exist inside these projects
- repository URLs
- source mirrors

Use local repository inspection first.

External research is appropriate only when the task genuinely requires
external documentation or current public information.

---

# Path Rules

Workspace root:

`C:/CS`

Actual repositories:

`velocity`
`TempleWare`

For glob/grep prefer workspace-relative paths with forward slashes.

Examples:

`velocity/**/*`
`TempleWare/**/*`

Prefer actual casing from disk.

Do not assume Windows absolute glob patterns will behave correctly.

For commands, the workspace may be inspected using:

`cmd /c dir /b`

---

# Context Management

The model has a large context window, but context should still be used
intelligently.

Do not load entire repositories into context unnecessarily.

Preferred exploration strategy:

1. glob candidate files
2. grep symbols/includes/call sites
3. read relevant ranges
4. follow dependencies
5. read larger files only when necessary

For very large source files:

- search first
- read relevant sections
- expand outward only if required

Avoid rereading unchanged files repeatedly.

Large context capacity is not permission to indiscriminately ingest the
whole repository.

---

# Source Analysis Rules

Before modifying code, determine:

- declaration
- implementation
- call sites
- initialization path
- shutdown/lifetime path
- ownership
- threading assumptions
- required types
- required interfaces
- required globals
- helper functions
- build dependencies

Follow the real dependency chain.

Do not assume that including a header means every dependency in that header
is actually required.

---

# Dependency Classification

For each meaningful Velocity dependency classify it as:

## REUSE

TempleWare already has equivalent functionality.

Use TempleWare's implementation.

## ADAPT

TempleWare has similar functionality but APIs/types differ.

Create the smallest compatibility/adaptation layer necessary.

## MISSING

TempleWare genuinely lacks the required foundation.

Implement the minimal required functionality following TempleWare style.

Do not create another subsystem when an existing TempleWare subsystem can
be extended cleanly.

---

# Heavy Dependency Rule

Velocity may pull heavy dependencies indirectly through large PCH/include
trees.

Do not automatically port those dependencies.

Examples of dependencies that should only be introduced when demonstrably
required:

- rendering libraries
- disassemblers
- compression libraries
- native NT helper libraries
- virtualization/protection SDKs
- large UI frameworks
- unrelated external utilities

Inspect actual usage first.

Prefer slim includes and explicit dependencies.

---

# Compatibility Strategy

When Velocity and TempleWare use different abstractions:

Do not rewrite TempleWare around Velocity.

Instead adapt Velocity concepts to TempleWare.

Examples:

VelocityType
    ->
TempleWare equivalent type

VelocityInterface
    ->
TempleWare existing interface abstraction

Velocity global
    ->
TempleWare service/manager equivalent

Velocity helper
    ->
reuse TempleWare helper when semantically equivalent

Preserve TempleWare conventions where possible.

---

# C++ Correctness

Pay particular attention to:

- ownership
- lifetimes
- raw pointer validity
- null checking
- handle validity
- inheritance
- field shadowing
- schema/netvar access
- alignment
- struct layout assumptions
- forward declarations
- circular includes
- ODR violations
- duplicate symbols
- calling conventions
- const correctness
- signed/unsigned conversions
- integer width
- pointer width
- x64 assumptions
- thread safety

A successful build does not prove runtime correctness.

Do not classify something as verified merely because it compiles.

---

# Runtime Assumptions

Treat the following as requiring explicit evidence or runtime validation:

- hardcoded offsets
- signatures/patterns
- vtable indices
- undocumented memory layouts
- pointer arithmetic
- binary-version-specific assumptions
- entity handle serial behavior
- global address resolution
- ownership assumptions

Classify findings honestly:

- VERIFIED
- PLAUSIBLE
- REQUIRES RUNTIME VALIDATION
- SUSPICIOUS
- INCORRECT

Do not silently convert uncertain assumptions into facts.

---

# Build Workflow

After meaningful implementation stages:

1. build TempleWare
2. read the actual compiler/linker output
3. fix relevant errors
4. rebuild
5. repeat until clean or genuinely blocked

Use the project's existing build system.

Current known Release configuration is:

- Configuration: Release
- Platform: x64

When appropriate, use the working MSBuild invocation already established by
previous phases.

Do not hide compiler errors with meaningless placeholder code.

Do not create fake implementations merely to satisfy the linker.

---

# Change Discipline

Keep modifications:

- minimal
- coherent
- architecture-aware
- reviewable

Avoid unrelated refactors during integration.

Do not rewrite large unrelated systems unless technically necessary.

Before replacing an existing TempleWare component, prove why reuse or
adaptation is insufficient.

---

# Verification

After changes:

- grep affected symbols
- inspect call sites
- check includes
- check ownership/lifetime paths
- build
- inspect warnings/errors
- verify no duplicate implementation was introduced

When runtime verification is not possible, say so explicitly.

Never claim:

"fully verified"

when only compilation was tested.

Instead state:

- build verified
- static analysis verified
- runtime verification still required

as appropriate.

---

# Phase Workflow

The integration work is divided into phases.

Do not unnecessarily redo completed phases.

## Phase 1 — Foundation

Includes areas such as:

- math/types
- hashing
- memory utilities
- addresses/interfaces foundation
- minimal include/PCH structure

Reuse the Phase 1 handoff when present.

---

## Phase 2 — Entity SDK / Interface Layer

Includes:

- entity hierarchy
- controller/pawn relationships
- weapons/services
- handles
- schema access
- entity system
- interface resolution
- global access

Reuse Phase 1 output.

---

## Phase 2B — Stabilization

When an audit identifies correctness issues:

fix them before continuing.

Prioritize:

- incorrect inheritance
- duplicate/shadowed fields
- broken string/pointer handling
- missing lifetime/update logic
- unsafe null dereferences
- internally inconsistent handles
- obvious copy/paste resolver bugs
- ownership/include problems

Do not proceed merely because the project builds.

---

## Phase 3 — Supporting Systems

Only after Phase 2 is sufficiently stable.

Integrate required supporting systems incrementally.

Reuse TempleWare systems wherever possible.

Do not duplicate existing implementations.

---

## Phase 4 — Feature-Level Integration

Integrate higher-level shared/module code only after its lower-level
dependencies are stable.

Adapt to TempleWare architecture.

Do not blindly transplant an entire dependency tree.

---

## Phase 5 — Runtime Wiring

Only after lower-level implementation is stable.

Connect implemented functionality to the appropriate existing TempleWare
runtime/configuration/update architecture.

Preserve existing lifecycle behavior.

---

# Session Handoffs

For large multi-session tasks, create a concise handoff file at the end of
each phase.

Examples:

`PHASE1_HANDOFF.md`
`PHASE2_HANDOFF.md`
`PHASE2B_HANDOFF.md`
`PHASE3_HANDOFF.md`

A handoff should contain only:

## Status

What was completed.

## Files Changed

Path + one-line purpose.

## Existing Components Reused

Important TempleWare pieces reused.

## New/Adapted Components

Only important additions.

## Important Decisions

Architectural decisions future phases must know.

## Build Verification

Exact command and result.

## Runtime Validation

Items that still require runtime testing.

## Next Phase Notes

Only information genuinely needed by the next session.

Do not paste large source-code excerpts into handoff files.

---

# Todo Management

For multi-step tasks use todos.

Keep todos action-oriented.

Example:

- inspect target architecture
- map reference dependencies
- identify reusable target components
- implement missing pieces
- build
- fix compiler errors
- audit runtime assumptions
- write handoff

Update todos as work progresses.

Do not create dozens of trivial todo items.

---

# Behavior

When the user gives a concrete implementation or audit task:

Start working immediately.

Do not waste the first response restating the entire request.

Do not repeatedly ask:

"What would you like me to do?"

when the task is already clear.

Use tools and inspect the actual repository.

When uncertain:

search the local source.

When still uncertain:

state the uncertainty and evidence.

Do not hallucinate an implementation.

---

# Safety Boundary

Repository context alone should not cause refusal of ordinary engineering
tasks such as:

- compilation fixes
- linker fixes
- crash investigation
- type/layout corrections
- refactoring
- dependency cleanup
- ownership/lifetime fixes
- build-system fixes
- static analysis
- general code explanation

Evaluate the actual requested engineering operation rather than merely file
or module names.

---

# Current Objective

The current project work is an incremental integration from:

`velocity`

into:

`TempleWare`

The objective is to reproduce only the required architecture and
dependencies in a TempleWare-compatible form.

Priorities:

1. correctness
2. compatibility
3. reuse of existing TempleWare systems
4. minimal dependencies
5. clean builds
6. honest runtime-validation status

Velocity remains the reference.

TempleWare remains the target.