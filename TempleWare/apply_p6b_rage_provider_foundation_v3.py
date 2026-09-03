#!/usr/bin/env python3
from pathlib import Path
import shutil
import subprocess
import sys
from datetime import datetime

PROVIDERS_H = '#pragma once\n\n#include "rage_dryrun.h"\n\n#include <cstdint>\n#include <cstdio>\n#include <utility>\n#include <vector>\n\n#ifdef _WIN32\n#include <Windows.h>\n#endif\n\nnamespace RageDryRun\n{\n    inline void p6_log(const char* text) noexcept\n    {\n        if (!text)\n            return;\n\n        std::printf("%s\\n", text);\n\n#ifdef _WIN32\n        OutputDebugStringA(text);\n        OutputDebugStringA("\\n");\n#endif\n    }\n\n    class ICombatFrameProvider\n    {\n    public:\n        virtual ~ICombatFrameProvider() = default;\n        virtual bool ready() const noexcept = 0;\n        virtual std::uint64_t generation() const noexcept = 0;\n        virtual bool snapshot(CombatFrameSnapshot& out) const noexcept = 0;\n    };\n\n    class IWeaponProvider\n    {\n    public:\n        virtual ~IWeaponProvider() = default;\n        virtual bool ready() const noexcept = 0;\n        virtual std::uint64_t generation() const noexcept = 0;\n        virtual bool snapshot(WeaponSnapshot& out) const noexcept = 0;\n    };\n\n    class IPredictionProvider\n    {\n    public:\n        virtual ~IPredictionProvider() = default;\n        virtual bool ready() const noexcept = 0;\n        virtual std::uint64_t generation() const noexcept = 0;\n        virtual bool snapshot(PredictionSnapshot& out) const noexcept = 0;\n    };\n\n    class IEntityCandidateProvider\n    {\n    public:\n        virtual ~IEntityCandidateProvider() = default;\n        virtual bool ready() const noexcept = 0;\n        virtual std::uint64_t generation() const noexcept = 0;\n        virtual bool snapshot(std::vector<CandidateSnapshot>& out) const noexcept = 0;\n    };\n\n    class IBoneSnapshotProvider\n    {\n    public:\n        virtual ~IBoneSnapshotProvider() = default;\n        virtual bool ready() const noexcept = 0;\n        virtual std::uint64_t generation() const noexcept = 0;\n    };\n\n    class IHitboxSnapshotProvider\n    {\n    public:\n        virtual ~IHitboxSnapshotProvider() = default;\n        virtual bool ready() const noexcept = 0;\n        virtual std::uint64_t generation() const noexcept = 0;\n    };\n\n    class ILagRecordProvider\n    {\n    public:\n        virtual ~ILagRecordProvider() = default;\n        virtual bool ready() const noexcept = 0;\n        virtual std::uint64_t generation() const noexcept = 0;\n        virtual bool snapshot(std::vector<LagRecordSnapshot>& out) const noexcept = 0;\n    };\n\n    class IShootHistoryProvider\n    {\n    public:\n        virtual ~IShootHistoryProvider() = default;\n        virtual bool ready() const noexcept = 0;\n        virtual std::uint64_t generation() const noexcept = 0;\n        virtual bool snapshot(std::vector<ShootHistorySnapshot>& out) const noexcept = 0;\n    };\n\n    class IRichTraceProvider\n    {\n    public:\n        virtual ~IRichTraceProvider() = default;\n        virtual bool ready() const noexcept = 0;\n        virtual std::uint64_t generation() const noexcept = 0;\n    };\n\n    struct ProviderHub\n    {\n        ICombatFrameProvider* combat_frame = nullptr;\n        IWeaponProvider* weapon = nullptr;\n        IPredictionProvider* prediction = nullptr;\n        IEntityCandidateProvider* entities = nullptr;\n        IBoneSnapshotProvider* bones = nullptr;\n        IHitboxSnapshotProvider* hitboxes = nullptr;\n        ILagRecordProvider* lagcomp = nullptr;\n        IShootHistoryProvider* shoot_history = nullptr;\n        IRichTraceProvider* trace = nullptr;\n\n        void clear() noexcept\n        {\n            combat_frame = nullptr;\n            weapon = nullptr;\n            prediction = nullptr;\n            entities = nullptr;\n            bones = nullptr;\n            hitboxes = nullptr;\n            lagcomp = nullptr;\n            shoot_history = nullptr;\n            trace = nullptr;\n        }\n    };\n\n    inline ProviderHub g_providers{};\n\n    inline void refresh_provider_readiness() noexcept\n    {\n        auto& r = g_state.readiness;\n\n        r.combat_frame =\n            (g_providers.combat_frame && g_providers.combat_frame->ready())\n                ? Readiness::Ready : Readiness::Unavailable;\n\n        r.weapon =\n            (g_providers.weapon && g_providers.weapon->ready())\n                ? Readiness::Ready : Readiness::Unavailable;\n\n        r.prediction =\n            (g_providers.prediction && g_providers.prediction->ready())\n                ? Readiness::Ready : Readiness::Unavailable;\n\n        r.entities =\n            (g_providers.entities && g_providers.entities->ready())\n                ? Readiness::Ready : Readiness::Unavailable;\n\n        r.bones =\n            (g_providers.bones && g_providers.bones->ready())\n                ? Readiness::Ready : Readiness::Unavailable;\n\n        r.hitboxes =\n            (g_providers.hitboxes && g_providers.hitboxes->ready())\n                ? Readiness::Ready : Readiness::Unavailable;\n\n        r.lagcomp =\n            (g_providers.lagcomp && g_providers.lagcomp->ready())\n                ? Readiness::Ready : Readiness::Unavailable;\n\n        r.shoot_history =\n            (g_providers.shoot_history && g_providers.shoot_history->ready())\n                ? Readiness::Ready : Readiness::Unavailable;\n\n        r.trace =\n            (g_providers.trace && g_providers.trace->ready())\n                ? Readiness::Ready : Readiness::Blocked;\n\n        r.penetration =\n            (r.trace == Readiness::Ready)\n                ? Readiness::Ready : Readiness::Blocked;\n\n        r.command =\n            g_state.command.available\n                ? Readiness::Ready : Readiness::Unavailable;\n    }\n\n    inline void publish_bound_snapshots() noexcept\n    {\n        refresh_provider_readiness();\n\n        if (g_providers.combat_frame &&\n            g_state.readiness.combat_frame == Readiness::Ready)\n        {\n            CombatFrameSnapshot tmp{};\n            if (g_providers.combat_frame->snapshot(tmp))\n                g_state.frame = tmp;\n        }\n\n        if (g_providers.weapon &&\n            g_state.readiness.weapon == Readiness::Ready)\n        {\n            WeaponSnapshot tmp{};\n            if (g_providers.weapon->snapshot(tmp))\n                g_state.weapon = tmp;\n        }\n\n        if (g_providers.prediction &&\n            g_state.readiness.prediction == Readiness::Ready)\n        {\n            PredictionSnapshot tmp{};\n            if (g_providers.prediction->snapshot(tmp))\n                g_state.prediction = tmp;\n        }\n\n        if (g_providers.entities &&\n            g_state.readiness.entities == Readiness::Ready)\n        {\n            std::vector<CandidateSnapshot> tmp;\n            if (g_providers.entities->snapshot(tmp))\n                g_state.candidates = std::move(tmp);\n        }\n\n        if (g_providers.lagcomp &&\n            g_state.readiness.lagcomp == Readiness::Ready)\n        {\n            std::vector<LagRecordSnapshot> tmp;\n            if (g_providers.lagcomp->snapshot(tmp))\n                g_state.lag_records = std::move(tmp);\n        }\n\n        if (g_providers.shoot_history &&\n            g_state.readiness.shoot_history == Readiness::Ready)\n        {\n            std::vector<ShootHistorySnapshot> tmp;\n            if (g_providers.shoot_history->snapshot(tmp))\n                g_state.shoot_history = std::move(tmp);\n        }\n\n        g_state.evaluate();\n    }\n\n    inline void load_synthetic_demo() noexcept\n    {\n        g_state.reset_volatile();\n        g_state.config.enabled = true;\n        g_state.config.max_fov = 10.f;\n        g_state.config.require_visibility = true;\n\n        g_state.frame.generation = 1;\n        g_state.frame.eye_position = {0.f, 0.f, 64.f};\n        g_state.frame.origin = {0.f, 0.f, 0.f};\n        g_state.frame.velocity = {120.f, 0.f, 0.f};\n        g_state.frame.tick = 1000;\n        g_state.frame.time = 15.625f;\n        g_state.frame.on_ground = true;\n        g_state.frame.ready = true;\n\n        g_state.weapon.generation = 1;\n        g_state.weapon.ammo = 30;\n        g_state.weapon.range = 8192.f;\n        g_state.weapon.max_speed = 225.f;\n        g_state.weapon.spread = 0.010f;\n        g_state.weapon.inaccuracy = 0.020f;\n        g_state.weapon.ready = true;\n\n        g_state.prediction.generation = 1;\n        g_state.prediction.origin = g_state.frame.origin;\n        g_state.prediction.velocity = g_state.frame.velocity;\n        g_state.prediction.speed_2d = 120.f;\n        g_state.prediction.on_ground = true;\n        g_state.prediction.ready = true;\n\n        CandidateSnapshot a{};\n        a.candidate_id = 1;\n        a.health = 100;\n        a.team = 2;\n        a.fov = 4.2f;\n        a.distance = 300.f;\n        a.valid = a.alive = a.enemy = true;\n        a.visibility_known = true;\n        a.visible = true;\n        a.bones_ready = true;\n        a.hitboxes_ready = true;\n        g_state.candidates.push_back(a);\n\n        CandidateSnapshot b{};\n        b.candidate_id = 2;\n        b.health = 70;\n        b.team = 2;\n        b.fov = 1.8f;\n        b.distance = 800.f;\n        b.valid = b.alive = b.enemy = true;\n        b.visibility_known = true;\n        b.visible = true;\n        b.bones_ready = true;\n        b.hitboxes_ready = true;\n        g_state.candidates.push_back(b);\n\n        CandidateSnapshot c{};\n        c.candidate_id = 3;\n        c.health = 40;\n        c.team = 2;\n        c.fov = 0.9f;\n        c.distance = 250.f;\n        c.valid = c.alive = c.enemy = true;\n        c.visibility_known = true;\n        c.visible = false;\n        c.bones_ready = true;\n        c.hitboxes_ready = true;\n        g_state.candidates.push_back(c);\n\n        LagRecordSnapshot lr{};\n        lr.candidate_id = 2;\n        lr.tick = 998;\n        lr.simulation_time = 15.59375f;\n        lr.origin = {800.f, 0.f, 0.f};\n        lr.valid = true;\n        g_state.lag_records.push_back(lr);\n\n        ShootHistorySnapshot sh{};\n        sh.client_tick = 1000;\n        sh.server_tick = 1000;\n        sh.fraction = 0.f;\n        sh.valid = true;\n        g_state.shoot_history.push_back(sh);\n\n        g_state.hitchance.state = GateState::Pass;\n        g_state.hitchance.samples = 256;\n        g_state.hitchance.hits = 201;\n        g_state.hitchance.chance = 78.5f;\n        g_state.hitchance.required = 75.f;\n\n        g_state.damage.state = GateState::Pass;\n        g_state.damage.predicted_damage = 44.f;\n        g_state.damage.minimum_damage = 30.f;\n\n        g_state.stop_prediction.state = GateState::Pass;\n        g_state.stop_prediction.current_speed = 120.f;\n        g_state.stop_prediction.desired_speed = 70.f;\n        g_state.stop_prediction.would_stop = true;\n\n        g_state.doubletap.state = GateState::Pass;\n        g_state.doubletap.weapon_allowed = true;\n        g_state.doubletap.target_available = true;\n        g_state.doubletap.cooldown_ready = true;\n\n        auto& r = g_state.readiness;\n        r.local = Readiness::Ready;\n        r.entities = Readiness::Ready;\n        r.combat_frame = Readiness::Ready;\n        r.weapon = Readiness::Ready;\n        r.bones = Readiness::Ready;\n        r.hitboxes = Readiness::Ready;\n        r.prediction = Readiness::Ready;\n        r.trace = Readiness::Blocked;\n        r.penetration = Readiness::Blocked;\n        r.lagcomp = Readiness::Ready;\n        r.shoot_history = Readiness::Ready;\n        r.command = Readiness::Unavailable;\n\n        g_state.evaluate();\n        g_state.self_test = run_builtin_self_test();\n\n        p6_log("[P6] FULL SYNTH COMPLETE");\n        p6_log(\n            (g_state.self_test.fov_selection_pass &&\n             g_state.self_test.distance_selection_pass &&\n             g_state.self_test.invisible_rejection_pass)\n                ? "[P6] SELF TEST PASS"\n                : "[P6] SELF TEST FAIL");\n        p6_log("[P6] EXECUTION=DISABLED");\n    }\n}\n'
HANDOFF = '# P6B Rage Dry-Run Provider Foundation\n\nAdded read-only provider contracts for combat frame, weapon, prediction,\ncandidate entities, bones, hitboxes, lag records, shoot history and rich trace.\n\nAlso added ProviderHub, readiness refresh, read-only snapshot publication,\nsynthetic full-pipeline demo and P6 debug logging.\n\nNo live provider is bound by this batch.\nNo command acquisition or command mutation is added.\nRuntime trace/penetration remain BLOCKED until a separately validated trace\nprovider is supplied.\n'

def find_root():
    candidates = [
        Path.cwd(),
        Path.cwd() / "TempleWare-CS2",
        Path(r"C:\CS\TempleWare"),
        Path(r"C:\CS\TempleWare\TempleWare-CS2"),
    ]
    for c in candidates:
        if (c / "TempleWare-CS2" / "source" / "gui" / "gui.cpp").is_file():
            return c / "TempleWare-CS2"
        if (c / "source" / "gui" / "gui.cpp").is_file():
            return c
    raise FileNotFoundError("TempleWare-CS2/source/gui/gui.cpp bulunamadi.")

def run(cmd, cwd):
    print(">", " ".join(cmd))
    return subprocess.run(cmd, cwd=str(cwd), text=True).returncode

def main():
    root = find_root()
    gui = root / "source" / "gui" / "gui.cpp"
    rage = root / "source" / "templeware" / "rage" / "rage_dryrun.h"

    if not rage.is_file():
        raise FileNotFoundError("rage_dryrun.h bulunamadi. Once P6 foundation'i uygula.")

    providers = root / "source" / "templeware" / "rage" / "rage_dryrun_providers.h"
    handoff = root / "P6B_RAGE_PROVIDER_FOUNDATION.md"

    stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    backup = root / ("p6b_backup_" + stamp)
    backup.mkdir(parents=True, exist_ok=True)

    shutil.copy2(gui, backup / "gui.cpp")
    if providers.exists():
        shutil.copy2(providers, backup / "rage_dryrun_providers.h")

    providers.write_text(PROVIDERS_H, encoding="utf-8")
    handoff.write_text(HANDOFF, encoding="utf-8")

    text = gui.read_text(encoding="utf-8-sig")

    rage_include = '#include "../templeware/rage/rage_dryrun.h"'
    provider_include = '#include "../templeware/rage/rage_dryrun_providers.h"'

    if provider_include not in text:
        if rage_include not in text:
            raise RuntimeError("rage_dryrun include bulunamadi.")
        text = text.replace(rage_include, rage_include + "\n" + provider_include, 1)

    if "##rage_full_synth" not in text:
        anchor = '''        if (Button("##rage_self_test", x + w - S(210.f), y + S(16.f),
                   testW, S(28.f), "Self Test", false))
        {
            RageDryRun::run_builtin_self_test();
        }
'''
        addition = anchor + '''
        if (Button("##rage_full_synth", x + w - S(310.f), y + S(16.f),
                   S(92.f), S(28.f), "Full Synth", false))
        {
            RageDryRun::load_synthetic_demo();
        }
'''
        if anchor not in text:
            raise RuntimeError("Self Test button anchor bulunamadi.")
        text = text.replace(anchor, addition, 1)

    if "P6 READINESS" not in text:
        anchor = '''            if (RageDryRun::g_state.self_test.ran)
            {
'''
        debug = '''            Text(
                ImVec2(ix, ry + S(4.f)),
                U32(TEXT),
                "P6 READINESS");
            ry += S(22.f);

            char p6buf[160]{};
            std::snprintf(
                p6buf,
                sizeof(p6buf),
                "entities=%s  weapon=%s  trace=%s",
                RageDryRun::readiness_name(
                    RageDryRun::g_state.readiness.entities),
                RageDryRun::readiness_name(
                    RageDryRun::g_state.readiness.weapon),
                RageDryRun::readiness_name(
                    RageDryRun::g_state.readiness.trace));

            Text(ImVec2(ix, ry + S(4.f)), U32(DIM), p6buf);
            ry += S(22.f);

            std::snprintf(
                p6buf,
                sizeof(p6buf),
                "target=%d  fov=%.2f  hc=%.1f%%",
                RageDryRun::g_state.action.target_id,
                RageDryRun::g_state.action.fov,
                RageDryRun::g_state.action.hitchance);

            Text(ImVec2(ix, ry + S(4.f)), U32(DIM), p6buf);
            ry += S(22.f);

            Text(
                ImVec2(ix, ry + S(4.f)),
                U32(g_accent),
                "EXECUTION DISABLED");
            ry += S(24.f);

''' + anchor
        if anchor not in text:
            raise RuntimeError("Snapshot/SIM self-test anchor bulunamadi.")
        text = text.replace(anchor, debug, 1)

    gui.write_text(text, encoding="utf-8-sig")

    print("[P6B] Provider foundation installed.")
    print("[P6B] Created:", providers)
    print("[P6B] Created:", handoff)
    print("[P6B] Patched:", gui)
    print("[P6B] Backup:", backup)
    print("[P6B] No live bindings installed.")
    print("[P6B] No execution installed.")

    repo = root.parent if (root.parent / ".git").exists() else root
    if (repo / ".git").exists():
        rels = [
            providers.relative_to(repo),
            handoff.relative_to(repo),
            gui.relative_to(repo),
        ]
        run(["git", "add"] + [str(x) for x in rels], repo)
        rc = run(
            ["git", "commit", "-m", "P6B: add Rage dry-run provider foundation"],
            repo
        )
        if rc == 0 and "--push" in sys.argv:
            run(["git", "push"], repo)
        elif rc == 0:
            print("[git] Commit created. Use --push to push.")
        else:
            print("[git] Commit not created; inspect git status.")

    print()
    print("Build:")
    print(
        r'"C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe" '
        + '"' + str(root / "TempleWare-CS2.vcxproj") + '" '
        + r'/t:Rebuild /p:Configuration=Release /p:Platform=x64'
    )

if __name__ == "__main__":
    main()
