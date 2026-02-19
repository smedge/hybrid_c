# Subroutine Unification Spec

## Vision

In the fiction of Hybrid, enemies are security programs **running subroutines**. When you kill them, you capture those subroutines. The code should reflect this truth: enemies and players use the **same** underlying ability systems with different activation contexts.

Currently, every enemy has bespoke implementations of mechanics that are near-identical to the player subs: hunter projectiles are copy-pasted SubProjectile logic, seeker dash is copy-pasted sub_egress physics, defender aegis is copy-pasted sub_aegis timers. This doubles maintenance burden and will compound with every new enemy type.

**Goal**: Extract shared mechanical cores from each subroutine. Player modules and enemy modules both instantiate these cores. AI decides *when* to activate; the core handles *what happens*.

**Non-goal**: We are NOT creating a generic "Ability" base class or inheritance hierarchy. Each ability type gets its own concrete core. C99 doesn't have polymorphism and we don't need it — we need shared state machines with different activation sources.

---

## Principles

1. **Fiction-first**: If an enemy uses an ability, it IS a subroutine. Same code path.
2. **Instance-based cores**: Each core is a struct + functions. Player creates one static instance. Enemy creates N instances (one per entity or one shared pool).
3. **Activation decoupled**: Cores expose `try_activate()` — player calls it on input, AI calls it on decision. Core handles cooldowns, state transitions, timers.
4. **Rendering unified**: Bloom/light source rendering lives in the core. Color/size are config parameters so enemies and players can have distinct visuals from the same renderer.
5. **Damage context-aware**: Cores don't apply damage directly. They report "hit at position X for Y damage" and the caller (enemy system or player system) routes it appropriately.
6. **Drop system organic**: Enemies carry SubroutineIds. On death, they drop fragments for what they carry.

---

## Architecture Overview

### Current Architecture
```
Player Input → sub_pea.c (static singleton) → SubProjectile pool
                                                  ↓
Hunter AI → hunter.c (bespoke HunterProjectile) → separate pool, separate logic
```

### Unified Architecture
```
Player Input → sub_pea.c (thin wrapper) → SubProjectilePool (shared core)
                                              ↓ same code
Hunter AI → hunter.c (AI decisions) ────────→ SubProjectilePool (shared core)
```

### Core Pattern

Each ability type follows this pattern:

```c
// Core state struct (instance-based)
typedef struct {
    // Phase/state machine
    // Timers
    // Entity-specific state (positions, directions, etc.)
} SubXxxCore;

// Configuration (const, shared across instances of same variant)
typedef struct {
    // Damage values, cooldowns, speeds, durations
    // Visual parameters (colors, sizes)
    // Audio sample pointers
} SubXxxConfig;

// Core API
void SubXxx_core_init(SubXxxCore *core);
bool SubXxx_core_try_activate(SubXxxCore *core, const SubXxxConfig *cfg, /* activation context */);
void SubXxx_core_update(SubXxxCore *core, const SubXxxConfig *cfg, unsigned int ticks);
void SubXxx_core_render(const SubXxxCore *core, const SubXxxConfig *cfg, Position ownerPos);
void SubXxx_core_render_bloom(const SubXxxCore *core, const SubXxxConfig *cfg, Position ownerPos);
void SubXxx_core_render_light(const SubXxxCore *core, const SubXxxConfig *cfg, Position ownerPos);
float SubXxx_core_get_cooldown_fraction(const SubXxxCore *core, const SubXxxConfig *cfg);
```

Player wrappers (`sub_pea.c`, `sub_egress.c`, etc.) become thin layers:
- Static instance of the core
- Static config with player-specific values
- `_update()` reads player input, calls `core_try_activate()` when triggered
- `_render()` delegates to `core_render()` with ship position

Enemy code creates core instances per entity (or per pool for projectiles):
- Core instance stored in enemy state struct
- AI state machine decides when to call `core_try_activate()`
- Enemy render calls `core_render()` with enemy position

---

## Per-Ability Unification Plans

### 1. PROJECTILES (sub_projectile → hunter, future enemies)

**Current state**: `SubProjectilePool` + `SubProjectileConfig` already exist and are well-abstracted. Sub_pea and sub_mgun are thin wrappers. Hunter has a separate `HunterProjectile` system that's structurally identical but with different constants and player-damage logic baked in.

**What changes**:

#### A. Generalize SubProjectileConfig

Current SubProjectileConfig:
```c
typedef struct {
    int fire_cooldown_ms;
    double feedback_cost;       // Player-only concept
    float light_proj_alpha;
    float light_spark_alpha;
    SubroutineId sub_id;        // Player-only concept
} SubProjectileConfig;
```

New SubProjectileConfig:
```c
typedef struct {
    // Firing
    int fire_cooldown_ms;       // Cooldown between individual shots
    int burst_count;            // Shots per burst (1 for pea/mgun, 3 for hunter)
    int burst_interval_ms;      // Time between burst shots (0 if burst_count=1)
    int burst_cooldown_ms;      // Cooldown after full burst (0 if no burst, uses fire_cooldown_ms)

    // Physics
    double velocity;            // Projectile speed (3500 player, 2000 hunter)
    int ttl_ms;                 // Time to live (1000 player, 800 hunter)

    // Damage
    double damage;              // Damage per projectile (50 pea, 20 mgun, 15 hunter)

    // Pool
    int pool_size;              // Max simultaneous projectiles (8 player, 64 hunter)

    // Visuals
    float color_r, color_g, color_b;  // Projectile/spark color (white for player, orange for hunter)
    float light_proj_alpha;
    float light_spark_alpha;
    float light_proj_radius;    // Light circle radius (60 player, 180 hunter)
    float light_spark_radius;   // Spark light radius (90 player, 300 hunter)
    float spark_size;           // Spark quad size (15 player, 12 hunter)

    // Audio (pointers, loaded by owner)
    Mix_Chunk **sample_fire;
    Mix_Chunk **sample_hit;
} SubProjectileConfig;
```

#### B. Remove player-specific coupling from SubProjectilePool

Current `SubProjectile_update()` calls:
- `Skillbar_is_active(cfg->sub_id)` — player-only
- `PlayerStats_add_feedback(cfg->feedback_cost)` — player-only
- `input->mouseLeft` — player-only

New approach: split into `SubProjectile_update_physics()` (shared) and firing logic (caller-controlled).

```c
// Called by owner to fire a projectile (replaces input check inside update)
// Returns true if projectile was fired (cooldown was ready, slot available)
bool SubProjectile_try_fire(SubProjectilePool *pool, const SubProjectileConfig *cfg,
                            Position origin, Position target);

// Update all in-flight projectiles: movement, TTL, wall collision, spark decay
// Does NOT handle input or firing — that's the caller's job
void SubProjectile_update_physics(SubProjectilePool *pool, const SubProjectileConfig *cfg,
                                  unsigned int ticks);

// Check if any projectile hits a target rectangle. Returns damage if hit.
// Caller decides what to do with the damage (apply to enemy HP, apply to player HP, etc.)
double SubProjectile_check_hit(SubProjectilePool *pool, const SubProjectileConfig *cfg,
                               Rectangle target);
```

#### C. Player wrapper (sub_pea.c) becomes:

```c
void Sub_Pea_update(const Input *input, unsigned int ticks, PlaceableComponent *pl) {
    SubProjectile_update_physics(&pool, &cfg, ticks);

    if (input->mouseLeft && Skillbar_is_active(SUB_ID_PEA)) {
        Position target = {input->mouseX, input->mouseY};
        if (SubProjectile_try_fire(&pool, &cfg, pl->position, target)) {
            PlayerStats_add_feedback(FEEDBACK_COST);
        }
    }
}
```

#### D. Hunter becomes:

```c
// In HunterState struct:
// SubProjectilePool projPool;  // replaces HunterProjectile projectiles[]

// In HUNTER_SHOOTING state:
if (burstTimer <= 0 && burstShotsFired < BURST_COUNT) {
    if (SubProjectile_try_fire(&h->projPool, &hunterProjCfg, pl->position, shipPos)) {
        burstShotsFired++;
        burstTimer = BURST_INTERVAL;
    }
}

// In Hunter_update (physics only runs once, not per-hunter):
// This is the existing "index 0" guard pattern.
// With per-hunter pools this becomes per-entity.
// OR: keep a single shared pool (64 projectiles across all hunters)
// and run physics once per frame.
SubProjectile_update_physics(&sharedHunterPool, &hunterProjCfg, ticks);

// Damage check against player (in projectile update callback or separate):
// SubProjectile_check_hit returns damage; caller applies to PlayerStats
```

**Decision: per-hunter pools vs shared pool?**

Hunters currently share 64 projectiles across all 128 hunters. This is efficient — most hunters aren't shooting simultaneously. Per-hunter pools of 8 would waste memory (128 * 8 = 1024 projectile structs vs 64).

**Recommendation**: Keep shared pools for enemies of the same type. The pool is per-enemy-type, not per-entity. The config determines pool size. Player subs each get their own pool (they already do).

```c
// graphics.c or enemy manager:
static SubProjectilePool hunterProjectilePool;  // Shared across all hunters
static const SubProjectileConfig hunterProjCfg = {
    .fire_cooldown_ms = 0,        // Burst logic handles timing
    .burst_count = 3,
    .burst_interval_ms = 100,
    .burst_cooldown_ms = 1500,
    .velocity = 2000.0,
    .ttl_ms = 800,
    .damage = 15.0,
    .pool_size = 64,
    .color_r = 1.0f, .color_g = 0.5f, .color_b = 0.1f,  // Orange
    // ...
};
```

#### E. Damage routing

The key asymmetry: player projectiles damage enemies, enemy projectiles damage the player. The core doesn't care — it just tracks projectile positions and reports hits.

```c
// In enemy update (checking player projectile hits):
double dmg = SubProjectile_check_hit(&peaPool, &peaCfg, enemyHitBox);
if (dmg > 0) enemy->hp -= dmg * stealth_multiplier;

// In player update (checking hunter projectile hits):
double dmg = SubProjectile_check_hit(&hunterPool, &hunterCfg, shipHitBox);
if (dmg > 0) PlayerStats_damage(dmg);
```

This replaces the current divergent paths where hunter.c has `PlayerStats_damage(PROJ_DAMAGE)` inline and enemy_util.c has `Sub_Pea_check_hit(hitBox)` with hardcoded damage.

**Important**: `Enemy_check_player_damage()` in enemy_util.c currently centralizes all player weapon hit detection with hardcoded damage values per weapon. This function would be updated to call `SubProjectile_check_hit()` with each player projectile pool's config, getting damage from the config instead of hardcoded constants.

---

### 2. DASH (sub_egress → seeker)

**Current state**: Sub_egress is a simple 3-state machine (READY→DASHING→COOLDOWN). Seeker has an 8-state AI machine with dash as one phase embedded in an orbit→windup→dash→recover cycle.

**Key insight**: The *dash physics* are nearly identical (150ms burst, ~4000-5000 velocity, direction-locked). The *AI orchestration* (orbit, windup, recovery) is seeker-specific and should stay in seeker.c. We extract the dash core; seeker's AI calls it.

#### A. DashCore struct

```c
typedef struct {
    // State
    bool active;                // Currently dashing
    int dashTimeLeft;           // Remaining dash duration (ms)
    int cooldownMs;             // Remaining cooldown (ms)
    double dirX, dirY;          // Normalized dash direction
    bool hitThisDash;           // One-hit-per-dash flag

    // Tracking
    Position startPos;          // Where dash started (for distance calc)
} SubDashCore;

typedef struct {
    int duration_ms;            // Dash duration (150ms both)
    double speed;               // Dash velocity (4000 player, 5000 seeker)
    int cooldown_ms;            // Post-dash cooldown (2000ms both)
    double damage;              // Contact damage (50 player, 80 seeker)
    double hit_half_size;       // Collision corridor half-width (50 player)
    bool wall_stops_dash;       // Seeker: true (stops on wall). Player: false
    double wall_recovery_factor; // Momentum carry after wall hit (0.1 for seeker)
} SubDashConfig;
```

#### B. Core API

```c
// Start a dash in the given direction. Returns false if on cooldown.
bool SubDash_try_activate(SubDashCore *core, const SubDashConfig *cfg,
                          double dirX, double dirY, Position startPos);

// Tick the dash. Returns velocity (vx, vy) to apply to owner's position.
// Caller handles actual position update (because wall collision is context-specific).
void SubDash_update(SubDashCore *core, const SubDashConfig *cfg, unsigned int ticks,
                    double *out_vx, double *out_vy);

// Check contact damage against a target. Returns damage if hit this frame, 0 if not.
// Respects one-hit-per-dash flag.
double SubDash_check_hit(SubDashCore *core, const SubDashConfig *cfg,
                         Position ownerPos, Rectangle target);

// Query state
bool SubDash_is_active(const SubDashCore *core);
float SubDash_get_cooldown_fraction(const SubDashCore *core, const SubDashConfig *cfg);
```

#### C. Player wrapper (sub_egress.c):

```c
static SubDashCore core;
static const SubDashConfig cfg = { 150, 4000.0, 2000, 50.0, 50.0, false, 0.0 };

void Sub_Egress_update(const Input *input, unsigned int ticks) {
    SubDash_update(&core, &cfg, ticks, &out_vx, &out_vy);
    // Ship applies velocity

    if (shift_edge_detected && Skillbar_is_active(SUB_ID_EGRESS)) {
        double dx, dy;
        // ... determine direction from WASD/facing ...
        if (SubDash_try_activate(&core, &cfg, dx, dy, shipPos)) {
            PlayerStats_add_feedback(25.0);
            PlayerStats_set_iframes(cfg.duration_ms);
        }
    }
}
```

#### D. Seeker integration:

```c
// In SeekerState:
SubDashCore dashCore;

// In SEEKER_WINDING_UP → transition to DASHING:
SubDash_try_activate(&s->dashCore, &seekerDashCfg, dashDirX, dashDirY, pos);

// In SEEKER_DASHING state:
double vx, vy;
SubDash_update(&s->dashCore, &seekerDashCfg, ticks, &vx, &vy);
// Apply movement with wall collision check
// If wall hit: transition to RECOVERING with momentum = vx*0.1, vy*0.1
// Contact damage:
double dmg = SubDash_check_hit(&s->dashCore, &seekerDashCfg, pos, shipHitBox);
if (dmg > 0) PlayerStats_damage(dmg);

// Cooldown managed by seeker's own recovery state, not by core cooldown
// (Seeker's "cooldown" is the full orbit→windup cycle, not a simple timer)
```

**Note**: Seeker's recovery phase (momentum decay) is AI-specific behavior that stays in seeker.c. The dash core just handles the active dash burst. Seeker ignores the core's cooldown timer and uses its own state machine for pacing.

---

### 3. SHIELD (sub_aegis → defender)

**Current state**: Sub_aegis has a 3-state machine (READY→ACTIVE→COOLDOWN) with hexagon ring visual. Defender has nearly identical timers (10s active, 30s cooldown) and hexagon ring visual, but with additional AI-reactive behavior (auto-pop on threat, mine break grace period, protection radius for allies).

#### A. ShieldCore struct

```c
typedef struct {
    // State
    bool active;                // Shield currently up
    int activeMs;               // Remaining active duration
    int cooldownMs;             // Remaining cooldown
    float pulseTimer;           // Visual pulse animation

    // Break tracking
    int graceMs;                // Post-break grace period (defender: 200ms, player: 0)
} SubShieldCore;

typedef struct {
    int duration_ms;            // Shield duration (10000 both)
    int cooldown_ms;            // Post-shield cooldown (30000 both)
    int break_grace_ms;         // Grace period after forced break (200 defender, 0 player)
    float ring_radius;          // Visual hex ring radius (35 player, 18 defender [BODY_SIZE+8])
    float ring_thickness;       // Line width (2.0 player, 1.5 defender)
    float color_r, color_g, color_b;  // Ring color (cyan both)
    float pulse_speed;          // Animation speed (6.0 both)

    // Audio
    Mix_Chunk **sample_activate;
    Mix_Chunk **sample_deactivate;
} SubShieldConfig;
```

#### B. Core API

```c
// Activate shield. Returns false if already active or on cooldown.
bool SubShield_try_activate(SubShieldCore *core, const SubShieldConfig *cfg);

// Force-break the shield (e.g., mine explosion). Starts grace period + cooldown.
void SubShield_break(SubShieldCore *core, const SubShieldConfig *cfg);

// Tick timers. Returns true if shield just expired this frame.
bool SubShield_update(SubShieldCore *core, const SubShieldConfig *cfg, unsigned int ticks);

// Query
bool SubShield_is_active(const SubShieldCore *core);
bool SubShield_in_grace(const SubShieldCore *core);
float SubShield_get_cooldown_fraction(const SubShieldCore *core, const SubShieldConfig *cfg);

// Render hex ring at position (for both player ship and defender body)
void SubShield_render(const SubShieldCore *core, const SubShieldConfig *cfg, Position pos);
void SubShield_render_bloom(const SubShieldCore *core, const SubShieldConfig *cfg, Position pos);
void SubShield_render_light(const SubShieldCore *core, const SubShieldConfig *cfg, Position pos);
```

#### C. Player wrapper (sub_aegis.c):

```c
static SubShieldCore core;
static const SubShieldConfig cfg = { 10000, 30000, 0, 35.0f, 2.0f, 0.6f,0.9f,1.0f, 6.0f, ... };

void Sub_Aegis_update(const Input *input, unsigned int ticks) {
    bool expired = SubShield_update(&core, &cfg, ticks);
    if (expired) PlayerStats_set_shielded(false);

    // Check for external break (mine popped shield via PlayerStats)
    if (core.active && !PlayerStats_is_shielded()) {
        SubShield_break(&core, &cfg);
    }

    if (input->keyF && Skillbar_is_active(SUB_ID_AEGIS)) {
        if (SubShield_try_activate(&core, &cfg)) {
            PlayerStats_set_shielded(true);
            PlayerStats_add_feedback(30.0);
        }
    }
}
```

#### D. Defender integration:

```c
// In DefenderState:
SubShieldCore shieldCore;

// AI decision to shield:
if (should_protect_ally && !SubShield_is_active(&d->shieldCore)) {
    SubShield_try_activate(&d->shieldCore, &defenderShieldCfg);
}

// Mine break:
if (dmg.mine_hit && SubShield_is_active(&d->shieldCore)) {
    SubShield_break(&d->shieldCore, &defenderShieldCfg);
}

// Damage check:
bool shielded = SubShield_is_active(&d->shieldCore) || SubShield_in_grace(&d->shieldCore);

// Render:
SubShield_render(&d->shieldCore, &defenderShieldCfg, pl->position);
```

---

### 4. HEAL (sub_mend → defender, future coop)

**Jason's directive**: The heal IS a subroutine. The defender uses it on allies. The player uses it on self. Future coop players could use it on each other. Same sub, different targeting context.

**Current state**: Sub_mend is self-only (instant 50HP + 3x regen boost, 10s cooldown). Defender's heal targets allies via EnemyRegistry scan (50HP, 4s cooldown, beam visual).

#### A. HealCore struct

```c
typedef struct {
    // State
    bool ready;                 // Can heal (cooldown expired)
    int cooldownMs;             // Remaining cooldown

    // Beam visual (for targeted heals)
    bool beamActive;
    int beamTimerMs;
    Position beamOrigin;
    Position beamTarget;
} SubHealCore;

typedef struct {
    double heal_amount;         // HP restored (50 both)
    int cooldown_ms;            // Cooldown (10000 player, 4000 defender)

    // Regen boost (player-specific, zeroed for enemies)
    int regen_boost_duration_ms;    // 5000 for player, 0 for enemies
    double regen_boost_multiplier;  // 3.0 for player, 0.0 for enemies

    // Beam visual
    int beam_duration_ms;       // 200ms for defender, 0 for player (no beam on self-heal)
    float beam_thickness;       // 2.5 for defender
    float color_r, color_g, color_b;  // Beam/effect color

    // Audio
    Mix_Chunk **sample_heal;
} SubHealConfig;
```

#### B. Core API

```c
// Attempt a heal. origin/target are positions for beam visual.
// Returns true if heal fired (cooldown was ready).
// Caller is responsible for applying HP change to the target.
bool SubHeal_try_activate(SubHealCore *core, const SubHealConfig *cfg,
                          Position origin, Position target);

// Tick cooldown and beam timer.
void SubHeal_update(SubHealCore *core, const SubHealConfig *cfg, unsigned int ticks);

// Query
bool SubHeal_is_ready(const SubHealCore *core);
float SubHeal_get_cooldown_fraction(const SubHealCore *core, const SubHealConfig *cfg);

// Render beam (if beamActive). No-op if beam_duration_ms == 0.
void SubHeal_render_beam(const SubHealCore *core, const SubHealConfig *cfg);
void SubHeal_render_beam_bloom(const SubHealCore *core, const SubHealConfig *cfg);
```

#### C. Player wrapper (sub_mend.c):

```c
static SubHealCore core;
static const SubHealConfig cfg = {
    .heal_amount = 50.0,
    .cooldown_ms = 10000,
    .regen_boost_duration_ms = 5000,
    .regen_boost_multiplier = 3.0,
    .beam_duration_ms = 0,      // No beam on self-heal
    .sample_heal = &sampleHeal,
};

void Sub_Mend_update(const Input *input, unsigned int ticks) {
    SubHeal_update(&core, &cfg, ticks);

    if (input->keyG && Skillbar_is_active(SUB_ID_MEND)) {
        Position shipPos = Ship_get_position();
        if (SubHeal_try_activate(&core, &cfg, shipPos, shipPos)) {
            PlayerStats_heal(cfg.heal_amount);
            PlayerStats_boost_regen(cfg.regen_boost_duration_ms, cfg.regen_boost_multiplier);
            PlayerStats_add_feedback(20.0);
        }
    }
}

// Future coop: same code but target = ally position, beam visual renders
```

#### D. Defender integration:

```c
// In DefenderState:
SubHealCore healCore;

// In try_heal_ally():
// ... EnemyRegistry scan finds target (existing logic) ...
if (found_wounded) {
    if (SubHeal_try_activate(&d->healCore, &defenderHealCfg, myPos, targetPos)) {
        EnemyRegistry_heal(target_type, target_idx, defenderHealCfg.heal_amount);
    }
}

// Render beam:
SubHeal_render_beam(&d->healCore, &defenderHealCfg);
```

**Why this works for coop**: When coop arrives, the player's mend could target nearby allies. The SubHealCore already supports beam visuals and origin/target positions. The only new code would be ally detection logic (similar to EnemyRegistry but for players).

---

### 5. MINE (sub_mine → enemy mine)

**Current state**: Two completely separate implementations with 90% identical logic (same fuse timers, same explosion visuals, same audio files, same colors). Enemy mines are ECS entities; player mines are a simple static array.

**Key difference**: Enemy mines have a **detection radius** (passive trigger on proximity). Player mines are **actively placed** and always armed. This is actually a behavioral difference, not a mechanical one — the fuse/explosion/damage core is identical.

#### A. MineCore struct

```c
typedef struct {
    bool active;
    int phase;                  // MINE_DORMANT, MINE_ARMED, MINE_EXPLODING, MINE_DEAD
    Position position;
    unsigned int phaseTicks;
    unsigned int blinkTimer;    // Blink cycle for red dot
} SubMineCore;

typedef struct {
    int armed_duration_ms;      // Fuse time (2000 player, 500 enemy)
    int exploding_duration_ms;  // Explosion visual (100 both)
    int dead_duration_ms;       // Cleanup delay (0 player [immediate], 10000 enemy)
    double explosion_size;      // Blast radius (250 player, 250 enemy)
    double mine_body_size;      // Visual body (10 both)
    double damage;              // Explosion damage (100 player self-dmg, varies for enemies)

    // Visuals
    float body_r, body_g, body_b;       // Mine body color (grey both)
    float dot_r, dot_g, dot_b;          // Armed dot color (red both)
    float explosion_r, explosion_g, explosion_b; // Explosion color (white both)
    float light_armed_radius;           // Light during armed (180 player, 150 enemy)
    float light_exploding_radius;       // Light during explosion (750 both)

    // Audio
    Mix_Chunk **sample_arm;
    Mix_Chunk **sample_explode;
    Mix_Chunk **sample_reset;   // Enemy only (door.wav on respawn), NULL for player
} SubMineConfig;
```

#### B. Core API

```c
// Arm a mine at position. Returns false if already active.
bool SubMine_arm(SubMineCore *core, const SubMineConfig *cfg, Position pos);

// Force-detonate (e.g., hit by projectile). Skips remaining fuse.
void SubMine_detonate(SubMineCore *core, const SubMineConfig *cfg);

// Tick the mine lifecycle. Returns true during EXPLODING phase (for damage checks).
bool SubMine_update(SubMineCore *core, const SubMineConfig *cfg, unsigned int ticks);

// Check if an AABB overlaps the explosion zone (only valid during EXPLODING).
bool SubMine_check_explosion_hit(const SubMineCore *core, const SubMineConfig *cfg,
                                 Rectangle target);

// Query
bool SubMine_is_exploding(const SubMineCore *core);
bool SubMine_is_active(const SubMineCore *core);

// Render
void SubMine_render(const SubMineCore *core, const SubMineConfig *cfg);
void SubMine_render_bloom(const SubMineCore *core, const SubMineConfig *cfg);
void SubMine_render_light(const SubMineCore *core, const SubMineConfig *cfg);
```

#### C. Player wrapper (sub_mine.c):

```c
static SubMineCore mines[MAX_PLAYER_MINES];  // 3 mines
static const SubMineConfig cfg = { 2000, 100, 0, 250.0, 10.0, 100.0, ... };

void Sub_Mine_update(const Input *input, unsigned int ticks) {
    for (int i = 0; i < MAX_PLAYER_MINES; i++) {
        bool exploding = SubMine_update(&mines[i], &cfg, ticks);
        if (exploding) {
            // Check self-damage
            Rectangle shipBox = Ship_get_hitbox();
            if (SubMine_check_explosion_hit(&mines[i], &cfg, shipBox)) {
                PlayerStats_damage(cfg.damage);
            }
        }
    }

    if (input->keySpace && cooldownTimer <= 0 && Skillbar_is_active(SUB_ID_MINE)) {
        // Find free slot, arm mine at ship position
        for (int i = 0; i < MAX_PLAYER_MINES; i++) {
            if (!SubMine_is_active(&mines[i])) {
                SubMine_arm(&mines[i], &cfg, Ship_get_position());
                PlayerStats_add_feedback(15.0);
                cooldownTimer = 250;
                break;
            }
        }
    }
}
```

#### D. Enemy mine integration:

```c
// In MineState:
SubMineCore core;

// Mine_resolve (player enters detection radius):
if (!SubMine_is_active(&m->core)) {
    SubMine_arm(&m->core, &enemyMineCfg, placeable->position);
}

// Mine_update:
bool exploding = SubMine_update(&m->core, &enemyMineCfg, ticks);
// After DEAD phase ends, core returns to dormant; enemy system handles respawn

// Direct body hit:
SubMine_detonate(&m->core, &enemyMineCfg);

// Fragment drop on explosion:
if (exploding && m->killedByPlayer) Fragment_spawn(...);
```

---

### 6. BOOST (sub_boost → defender fleeing)

**Current state**: Sub_boost is an elite movement modifier (hold shift = unlimited speed boost). Defender has a `boosting` flag that doubles speed during chase/flee.

**Assessment**: These are mechanically simple enough that a shared core adds minimal value. However, for fiction consistency, defender's boosted movement should be recognized as the same subroutine.

#### Approach: Lightweight shared constant

```c
// sub_boost_core.h
typedef struct {
    double speed_multiplier;    // 2.0 for both
    bool active;
} SubBoostCore;

// Player: active when shift held + Skillbar_is_active(SUB_ID_BOOST)
// Defender: active during chase/flee states
// Same multiplier applied to movement speed
```

This is the simplest unification — just shared constants and a flag. The "boost" subroutine is inherently just "go faster," so there's not much state machine to extract.

---

## Drop System Redesign

### Current System

Drops are hardcoded per enemy type:
- Hunter dies → FRAG_TYPE_HUNTER (if sub_mgun locked)
- Seeker dies → FRAG_TYPE_SEEKER (if sub_egress locked)
- Defender dies → FRAG_TYPE_MEND or FRAG_TYPE_AEGIS (smart RNG if locked)
- Mine dies → FRAG_TYPE_MINE (if sub_mine locked)

100% drop rate if subroutine is locked, 0% if unlocked. No drops post-unlock.

### New System: Carried Subroutines

Each enemy type declares what subroutines it carries:

```c
typedef struct {
    SubroutineId sub_id;        // Which sub this is
    FragmentType frag_type;     // What fragment it drops
    float drop_chance;          // 0.0 - 1.0 probability on death
} CarriedSubroutine;

// Hunter carries:
static const CarriedSubroutine hunterSubs[] = {
    { SUB_ID_MGUN, FRAG_TYPE_HUNTER, 1.0f },  // Always drops when locked
};

// Seeker carries:
static const CarriedSubroutine seekerSubs[] = {
    { SUB_ID_EGRESS, FRAG_TYPE_SEEKER, 1.0f },
};

// Defender carries:
static const CarriedSubroutine defenderSubs[] = {
    { SUB_ID_MEND, FRAG_TYPE_MEND, 0.5f },    // 50% chance each
    { SUB_ID_AEGIS, FRAG_TYPE_AEGIS, 0.5f },
};

// Mine carries:
static const CarriedSubroutine mineSubs[] = {
    { SUB_ID_MINE, FRAG_TYPE_MINE, 1.0f },
};
```

### Drop Logic (shared helper in enemy_util.c):

```c
void Enemy_drop_fragments(Position deathPos, const CarriedSubroutine *subs, int subCount) {
    for (int i = 0; i < subCount; i++) {
        if (Progression_is_unlocked(subs[i].sub_id)) continue;  // Already unlocked
        float roll = (float)(rand() % 1000) / 1000.0f;
        if (roll < subs[i].drop_chance) {
            Fragment_spawn(deathPos, subs[i].frag_type);
            return;  // One fragment per death (keep existing behavior)
        }
    }
}
```

### Post-Unlock Drops (Future Enhancement)

Once all subs are unlocked, enemies could still drop fragments at lower rates for:
- Skill upgrades (future system)
- Currency
- Score multipliers

This is out of scope for this spec but the `CarriedSubroutine` struct supports it by keeping `drop_chance` as a float rather than a bool.

---

## Enemy Loadout Table

After unification, each enemy's behavior is defined by which subs it carries:

| Enemy | Subroutines | Behavior |
|-------|------------|----------|
| **Mine** | sub_mine | Dormant → armed → explode lifecycle |
| **Hunter** | sub_mgun (projectile) | Patrol → chase → burst fire |
| **Seeker** | sub_egress (dash) | Stalk → orbit → windup → dash |
| **Defender** | sub_aegis (shield), sub_mend (heal), sub_boost (movement) | Support allies, flee player |

Future enemies would compose from existing subs:
| Future Enemy | Subroutines | Concept |
|-------------|------------|---------|
| **Sentinel** | sub_pea, sub_aegis | Shielded turret |
| **Phantom** | sub_stealth, sub_egress | Cloaked assassin |
| **Warden** | sub_mgun, sub_mend | Self-healing ranged |
| **Hive** | sub_mine, sub_mine | Deploys mines offensively |

This is the real payoff — new enemies become **loadout configurations** rather than ground-up implementations.

---

## Implementation Phases

### Phase 1: Extract Cores (No Behavior Change)

Create the shared core files without changing any existing behavior. Player wrappers and enemy code continue working exactly as before.

**Files created:**
- `sub_projectile_core.c/h` (or extend existing `sub_projectile.c/h`)
- `sub_dash_core.c/h`
- `sub_shield_core.c/h`
- `sub_heal_core.c/h`
- `sub_mine_core.c/h`

**Approach**: Write the core structs and functions. Write unit-test-style verification that core output matches current behavior. Do NOT wire anything yet.

**Estimated scope**: 5 files, ~200-300 lines each. Pure new code, no existing changes.

### Phase 2: Wire Player Subs to Cores

Replace the internal logic of each player sub module with calls to the corresponding core.

**Files modified:**
- `sub_pea.c` → uses `SubProjectile` core (already mostly there)
- `sub_mgun.c` → uses `SubProjectile` core (already mostly there)
- `sub_egress.c` → uses `SubDashCore`
- `sub_aegis.c` → uses `SubShieldCore`
- `sub_mend.c` → uses `SubHealCore`
- `sub_mine.c` → uses `SubMineCore`

**Validation**: Game plays identically. No visual or behavioral differences. This is pure refactor.

**Estimated scope**: 6 files modified, each becomes thinner. ~50-80 lines per file removed, replaced with core calls.

### Phase 3: Wire Enemies to Cores

Replace bespoke enemy ability code with calls to the shared cores.

**Files modified:**
- `hunter.c` → `HunterProjectile` array replaced with `SubProjectilePool`
- `seeker.c` → dash physics replaced with `SubDashCore`
- `defender.c` → aegis timers replaced with `SubShieldCore`, heal replaced with `SubHealCore`
- `mine.c` → mine lifecycle replaced with `SubMineCore`
- `enemy_util.c` → `Enemy_check_player_damage()` updated to use config-driven damage values
- `enemy_util.c` → `Enemy_drop_fragments()` added with CarriedSubroutine support

**Validation**: All enemies behave identically to before. Fragment drops work the same. This is pure refactor.

**Estimated scope**: 5 files modified, significant restructuring. This is the biggest phase.

### Phase 4: Unify Damage Routing

Centralize damage values in configs instead of hardcoded constants scattered across enemy_util.c and individual enemy files.

**Files modified:**
- `enemy_util.c` → `Enemy_check_player_damage()` reads from `SubProjectileConfig.damage` instead of hardcoded 50/20/100/10/20 values
- Remove hardcoded damage constants from hunter.c, seeker.c, etc.

### Phase 5: Drop System Migration

Replace hardcoded fragment drops with `CarriedSubroutine` declarations.

**Files modified:**
- `hunter.c`, `seeker.c`, `defender.c`, `mine.c` → replace inline drop logic with `Enemy_drop_fragments()` calls
- `enemy_util.c/h` → add `CarriedSubroutine` struct and `Enemy_drop_fragments()` helper

---

## Migration Safety

Each phase is independently shippable and testable:

- **Phase 1**: Zero risk (new files only, nothing calls them yet)
- **Phase 2**: Low risk (player subs are well-tested, easy to verify)
- **Phase 3**: Medium risk (enemy behavior is complex, needs careful testing)
- **Phase 4**: Low risk (damage values don't change, just source of truth moves)
- **Phase 5**: Low risk (drop behavior doesn't change, just code structure)

**Rollback strategy**: Each phase is a separate branch/commit. If phase 3 breaks something, revert to phase 2 — player subs still use cores, enemies still use bespoke code, everything works.

---

## What Stays Separate

Not everything should be unified. These remain enemy-specific or player-specific:

1. **AI State Machines**: Seeker's orbit→windup→dash cycle, hunter's patrol→chase→shoot cycle, defender's support→flee logic. These are **not** subroutines — they're the AI that decides when to use subroutines.

2. **PlayerStats Integration**: Feedback costs, i-frames, regen boosts. These are player-progression mechanics, not subroutine mechanics. Player wrappers handle this layer.

3. **EnemyRegistry**: The defender's ally-targeting scan is defender AI, not heal mechanics. The heal core just heals a target at a position — who and how to find the target is AI.

4. **Stealth Detection**: `Enemy_check_stealth_proximity()` is enemy AI behavior, not a subroutine mechanic.

5. **Collision Layers**: Enemy projectiles hit the player; player projectiles hit enemies. The core reports hits; the caller routes damage to the right system.

6. **Entity System Integration**: Enemy mines are ECS entities; player mines are static arrays. The core doesn't care about entity management — it just tracks mine state. The owner handles lifecycle.

---

## Design Decisions (Resolved)

1. **Sub_pea as starting weapon**: Pure progression concern — threshold=0 means you're born with it. Unification treats it the same as any other projectile sub mechanically. No special-casing needed.

2. **Projectile pool sizing**: One pool per enemy type (hunterPool, sentinelPool, etc.). Each type has different configs (speed, TTL, damage, color) so separate pools are the clean approach.

3. **Elite sub drops from enemies**: Yes — randomly generated minibosses carrying elite subs (sub_inferno, sub_disintegrate) out in the open world. The CarriedSubroutine struct supports this. **Deferred** until the unified skill system is solid and procgen miniboss spawning exists.

4. **Defender carrying 3 subs**: Randomize across carried subs, prioritizing locked ones. Boost fragments also drop from a hidden world location (existing destructible). The CarriedSubroutine array with per-sub drop chances handles this naturally.

5. **Visual differentiation**: Enemy projectiles use their enemy's color palette (e.g., hunter = orange, future sentinel = cyan). Player projectiles stay white. Same core, different config colors. This reinforces the visual language — you can tell at a glance who fired what.

