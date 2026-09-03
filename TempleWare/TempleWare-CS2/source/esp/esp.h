#pragma once
#include <cstdint>

namespace Esp
{
    struct Config
    {
        bool enabled = true;

        // Box
        bool  box = true;
        int   boxType = 0;          // 0 = Full, 1 = Corner, 2 = 3D
        float cornerFrac = 0.25f;   // corner length as fraction of side (Corner style)
        float boxColor[4] = { 0.85f, 0.35f, 0.35f, 1.0f };
        float boxThickness = 1.5f;

        // Visibility-based box color (uses spotted state as a lightweight LOS proxy)
        bool  visColor = false;
        float visibleColor[4]  = { 0.35f, 0.85f, 0.35f, 1.0f };
        float occludedColor[4] = { 0.85f, 0.35f, 0.35f, 1.0f };

        // Filled box (uses boxColor with its own opacity)
        bool  boxFill = false;
        float boxFillAlpha = 0.15f;

        // Name
        bool name = true;

        // Health bar
        bool healthBar = true;
        bool healthText = false;   // numeric HP on the bar
        bool ammoText = false;     // numeric clip/max on the ammo bar
        bool showPing = false;     // ping (ms) in flags

        // Ammo bar (below the box)
        bool  ammoBar = true;
        float ammoColor[4] = { 1.0f, 0.72f, 0.20f, 1.0f };

        // Distance (meters)
        bool distance = false;

        // Status flags (scoped / flashed / defusing)
        bool flags = true;

        // Active weapon name
        bool weapon = true;
        int  weaponDisplay = 0;   // 0 text, 1 icon, 2 text+icon

        // Skeleton (bones)
        bool  skeleton = true;
        float skeletonColor[4] = { 0.95f, 0.95f, 0.95f, 1.0f };
        float skeletonThickness = 1.2f;
        bool  skeletonVisColor = false;                        // separate occluded color
        float skeletonOccludedColor[4] = { 0.55f, 0.55f, 0.60f, 1.0f };

        // Real line-of-sight visibility (eye->head trace instead of spotted-state)
        bool  realVis = false;

        // Glow (memory-write on the pawn's glow property)
        bool  glow = false;
        float glowColor[4] = { 1.0f, 0.20f, 0.20f, 1.0f };
        bool  glowTeam = false;
        float glowTeamColor[4] = { 0.30f, 0.75f, 1.0f, 1.0f };
        bool  glowLocal = false;
        float glowLocalColor[4] = { 0.90f, 0.85f, 0.30f, 1.0f };

        // Chams (scenesystem DrawArray hook; enemies only)
        bool  chams = false;                                  // visible (z-tested) layer
        float chamsColor[4] = { 0.60f, 0.20f, 1.0f, 1.0f };
        int   chamsType = 0;                                  // 0 Flat 1 Illum 2 Glow 3 Matte 4 Outline 5 Holo
        bool  chamsXqz = false;                               // through-wall (ignorez) layer
        float chamsXqzColor[4] = { 1.0f, 0.30f, 0.30f, 1.0f };
        bool  chamsTeam = false;
        float chamsTeamColor[4] = { 0.30f, 0.75f, 1.0f, 1.0f };
        bool  chamsLocal = false;
        float chamsLocalColor[4] = { 0.90f, 0.85f, 0.30f, 1.0f };

        // Item ESP (dropped weapons / utility on the ground)
        bool  itemEsp = false;
        float itemColor[4] = { 0.85f, 0.85f, 0.90f, 1.0f };
        bool  itemDistance = false;
        bool  itemIcon = true;    // draw the weapon icon for dropped items
        // per-group filter: 0 pistol,1 smg,2 rifle,3 shotgun,4 sniper,5 utility
        bool  itemGroup[6] = { true, true, true, true, true, true };
        bool  itemGlow = false;
        float itemGlowColor[4] = { 0.85f, 0.85f, 0.30f, 1.0f };
        bool  itemChams = false;
        float itemChamsColor[4] = { 0.85f, 0.85f, 0.30f, 1.0f };

        // Extras
        bool  headCircle = false;
        float headCircleColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        bool  snapline = false;
        int   snaplinePos = 0;                                // 0 bottom, 1 top, 2 center
        float snaplineColor[4] = { 1.0f, 1.0f, 1.0f, 0.7f };
        bool  offArrows = false;
        float offArrowColor[4] = { 1.0f, 0.30f, 0.30f, 1.0f };
        float offArrowSize = 12.0f;
        float offArrowRadius = 0.30f;   // fraction of the smaller screen dim
        bool  offArrowGlow = false;

        // Health bar position (0 left, 1 top, 2 bottom)
        int   healthBarPos = 0;
        bool  showArmor = true;
        bool  showMoney = false;
        bool  showDefuser = true;
        bool  bombEsp = true;
        float bombColor[4] = { 1.0f, 0.60f, 0.10f, 1.0f };
        bool  teamEsp = false;

        // Misc (memory-write QoL)
        bool  antiFlash = false;
        bool  fovChanger = false;
        int   fovValue = 100;
        bool  localOpacity = false;      // lower the local player's model opacity
        float localOpacityVal = 0.5f;
        bool  localOnlyScoped = true;    // only while scoped

        // HUD overlay
        bool  watermark = false;
        float watermarkColor[4] = { 0.96f, 0.51f, 0.12f, 1.0f };   // accent orange
        bool  crosshair = false;
        float crosshairColor[4] = { 0.10f, 1.0f, 0.90f, 1.0f };
        float crosshairSize = 6.0f;
        float crosshairGap = 3.0f;
        float crosshairThickness = 1.5f;
        bool  crosshairDot = false;

        // Velocity bar (local speed HUD)
        bool  velBar = false;
        int   velBarPos = 0;                                   // 0 bottom, 1 top
        bool  velGradient = true;                              // fill blends velColor -> velColor2
        bool  velGlow = false;                                 // soft glow behind the bar
        bool  velText = true;                                  // numeric speed readout
        int   velMax = 400;                                    // speed mapped to a full bar
        float velColor[4]  = { 0.20f, 0.80f, 1.00f, 1.0f };    // low speed / left
        float velColor2[4] = { 1.00f, 0.30f, 0.30f, 1.0f };    // high speed / right
        bool  velGraph = false;                                // scrolling speed graph

        // Sound ESP (footstep/noise indicator from movement — not a sound hook)
        bool  soundEsp = false;
        float soundColor[4] = { 1.0f, 0.85f, 0.30f, 1.0f };

        // Skins (per-weapon econ fallback paint kit on owned weapons)
        bool      skinsEnable = false;   // master switch
        // SkinEntry skins[40];             // indexed to kSkinWeapons[] - deprecated
        int       skinPaintKit = 0;      // fallback paint kit ID
        float     skinWear = 0.0001f;    // fallback wear
        int       skinSeed = 0;          // fallback seed

        // Combat FX (health-decrease / shot based, no hook)
        bool  hitMarker = false;
        float hitMarkerColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        bool  damageNumbers = false;
        float damageColor[4] = { 1.0f, 0.85f, 0.30f, 1.0f };
        bool  bulletTracer = false;
        float tracerColor[4] = { 0.30f, 0.90f, 1.0f, 1.0f };

        // Grenade / projectile ESP (in-air nades)
        bool  nadeEsp = false;
        float nadeColor[4] = { 0.60f, 1.0f, 0.60f, 1.0f };
        bool  nadeDistance = false;
        bool  nadeTrajectory = false;                         // predicted flight arc
        bool  infernoFill = false;                            // molotov/inferno fire area
        float infernoColor[4] = { 1.0f, 0.45f, 0.12f, 0.45f };
        bool  nadeThrow = false;                              // pre-throw lineup predictor (held grenade)
        float nadeThrowColor[4] = { 0.30f, 1.0f, 0.60f, 1.0f };
        int   nadeThrowSpeed = 750;                           // throw velocity tuning

        // Spectator list (who is watching you)
        bool  specList = false;
        float specColor[4] = { 0.90f, 0.90f, 0.95f, 1.0f };

        // Ragdoll (corpse) ESP
        bool  ragdollEsp = false;
        float ragdollColor[4] = { 0.70f, 0.70f, 0.75f, 1.0f };
        bool  ragdollGlow = false;
        float ragdollGlowColor[4] = { 0.50f, 0.50f, 0.55f, 1.0f };
        bool  ragdollChams = false;
        float ragdollChamsColor[4] = { 0.60f, 0.60f, 0.65f, 1.0f };
    };

    inline Config g_config;

    // ---- System configs (UI-bound; logic wired incrementally) ----
    struct AimbotCfg
    {
        bool  enable = false;
        int   aimKey = 0;          // index into a keybind list
        int   aimType = 0;         // 0 Hold, 1 Toggle, 2 Always
        bool  silent = false;
        int   selection = 0;       // 0 FOV, 1 Distance, 2 Health
        float fov = 3.5f;
        bool  drawFov = false;
        float fovColor[4] = { 1.0f, 1.0f, 1.0f, 0.5f };
        float smooth = 0.35f;
        int   minDamage = 25;
        int   hitChance = 75;
        bool  multipoint = true;
        bool  rcs = true;
        int   rcsX = 100;
        int   rcsY = 100;
        bool  standaloneRcs = false;
        float spreadLimit = 2.0f;
        bool  autoStop = true;
        int   hitboxPriority = 0;  // legacy
        int   hitbox = 0;          // BodyRegion: 0 Head 1 Neck 2 Chest 3 Stomach 4 Pelvis 5 LArm 6 RArm 7 LLeg 8 RLeg
        float hitboxScale = 0.85f;
        bool  safePoints = true;
        bool  preferBody = false;
    };
    struct AntiAimCfg
    {
        bool enable = false;
        int  pitch = 0;            // 0 Down ...
        int  yaw = 0;              // 0 Jitter ...
        int  yawAdd = 15;
        bool fakeYaw = false;
        int  bodyYaw = 0;          // 0 Opposite ...
        bool freestanding = false;
    };
    struct MovementCfg
    {
        bool bhop = false;
        bool autoStrafe = false;
        bool fakeDuck = false;
        bool slowWalk = false;
        int  bhopKey = 0x20;      // SPACE
        int  fakeDuckKey = 0x12;  // ALT
        int  slowWalkKey = 0x10;  // SHIFT
    };
    struct TriggerCfg
    {
        bool enable = false;
        int  key = 0;
        int  delayMs = 0;
        bool teamCheck = true;
    };
    // Per-weapon-group rage config (6 groups)
    // 0=Pistol 1=SMG 2=Rifle 3=Shotgun 4=Sniper 5=LMG
    struct RageGroupCfg
    {
        bool  enable = true;
        bool  silent = false;
        bool  noSpread = false;
        bool  doubletap = false;
        bool  forceBAim = false;
        bool  forceShotAir = false;
        bool  forceShotGround = false;
        float maxFov = 180.f;
        int   hitChance = 0;
        int   minDamage = 0;
        float pointScale = 0.80f;
        int   minDmgOverride = 0;
        int   hitChanceOverride = 0;
        bool  dynamicPointScale = false;
        bool  debugMultipoints = false;
        // Bind keys (VK code, 0 = no bind / always active when toggled)
        int   forceShotAirKey = 0;
        int   forceShotGroundKey = 0;
        int   minDmgOverrideKey = 0;
        int   hitChanceOverrideKey = 0;
        // Hitbox multicombo flags
        bool  hbHead = true;
        bool  hbChest = true;
        bool  hbStomach = true;
        bool  hbArms = false;
        bool  hbLegs = false;
        bool  hbFeet = false;
    };

    struct RageCfg
    {
        bool  masterEnable = false;
        int   aimKey = 0;
        int   aimType = 2;            // 0 Hold, 1 Toggle, 2 Always
        int   selection = 0;          // 0 FOV 1 Distance 2 Health
        RageGroupCfg groups[6]{};     // pistol/smg/rifle/shotgun/sniper/lmg
    };

    inline RageCfg     g_rage;
    inline AimbotCfg   g_aimbot;
    inline AntiAimCfg  g_antiaim;
    inline MovementCfg g_movement;
    inline TriggerCfg  g_trigger;

    struct Stats
    {
        bool entitySystemReady = false;
        bool viewMatrixReady = false;
        int  entitiesScanned = 0;   // non-null entities from the list
        int  playersFound = 0;      // alive players (team 2/3, health > 0)
        bool localFound = false;
        int  localTeam = 0;         // 2 = T, 3 = CT
        int  enemiesDrawn = 0;
    };

    bool Initialize();
    void Draw();
    void UpdateAim();
    void UpdateTrigger();
    void UpdateMisc();
    void DrawOverlay();
    void UpdateSkins();
    const Stats& GetStats();

    // Resolve an entity pointer from its list index (for the chams owner lookup).
    uintptr_t LookupEntity(int index);
}
