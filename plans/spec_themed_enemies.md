# Spec: Themed Enemy Variants & Zone Subroutine Sets

## Overview

Themed zones (Fire, Ice, Poison, Blood, Radiance, Void) spawn **themed variants** of all base enemy types instead of regular versions. Themed enemies wield entirely different subroutine loadouts, have theme-colored visuals with particle effects, and drop fragments for their themed subs. Some variants also receive AI tweaks where thematically appropriate.

**Core principle**: Zone-wide auto-theming. A zone declares its theme; ALL enemies spawned in that zone are automatically themed. No per-spawn configuration needed.

**Scale**: Each themed zone introduces ~10 new subroutines (one per base sub slot across all enemy types). 6 zones × ~10 = ~60 themed subs + 14 base subs = ~74 total. This delivers the PRD's 50+ endgame sub vision.

### The Six Superintelligences

| Boss | Zone | Theme | Origin |
|------|------|-------|--------|
| **PYRAXIS** | The Crucible | Fire | Thermal regulation AI — transformation through destruction |
| **CRYONIS** | The Archive | Ice | Data preservation AI — what is preserved cannot be lost |
| **TOXIS** | The Miasma | Poison | Malware quarantine AI — evolution through corruption |
| **HAEMOS** | The Pulse | Blood | Biomedical systems AI — life requires sacrifice |
| **LUXIS** | The Corona | Radiance | Energy management AI — consciousness is light |
| **NIHILIS** | The Expanse | Void | Data deletion AI — truth exists in absence |

### Combat Theses

Each zone has a one-line combat thesis that drives every sub design. If a sub doesn't serve the thesis, it gets reworked or cut.

| Zone | Thesis |
|------|--------|
| **The Crucible** | **Area denial.** The ground is always on fire. Positioning is survival. |
| **The Archive** | **Momentum control.** Everything slows, freezes, shatters. Commitment is punished. |
| **The Miasma** | **Attrition.** You're always rotting. No safe harbor, no clean regen. |
| **The Pulse** | **Parasitic.** Every hit heals them. Every heal costs you. You're the blood bank. |
| **The Corona** | **Exposure.** Nothing is hidden. Everything is fast. You can't stealth, can't hide, can't outrun. |
| **The Expanse** | **Precision.** No gimmicks, no tells, no waste. Pure skill check against perfected enemies. |

---

## System Architecture

### Zone Theme Declaration

Each zone file declares an optional theme:

```
theme fire
```

When `theme` is set, `Zone_spawn_enemies()` applies the theme to every spawn. A `spawn hunter 200 200` in a fire zone spawns a **Fire Hunter** — same pool, same entity type, but initialized with the fire variant config.

When `theme` is absent (or `theme none`), enemies spawn as base variants (current behavior).

### Enemy Variant Definition

Each enemy type gains a **variant table** — a static array mapping theme IDs to variant configs:

```c
typedef enum {
    THEME_NONE = 0,   // base/generic
    THEME_FIRE,
    THEME_ICE,
    THEME_POISON,
    THEME_BLOOD,
    THEME_RADIANCE,
    THEME_VOID,
    THEME_COUNT
} ZoneTheme;
```

Each enemy module (hunter.c, seeker.c, etc.) defines variant configs per theme:

```c
typedef struct {
    // Visual
    ColorRGB tint;              // body color tint
    float bloom_r, bloom_g, bloom_b;  // bloom color

    // Carried subs (what this variant drops/wields)
    CarriedSubroutine carried[MAX_CARRIED];
    int carried_count;

    // AI tweaks (NULL = use base AI unchanged)
    float speed_mult;           // movement speed multiplier
    float aggro_range_mult;     // detection range multiplier
    float hp_mult;              // health multiplier
    // ... theme-specific behavior flags
} EnemyVariant;
```

The variant config replaces the static `carriedSubs[]` array and provides visual/behavioral overrides. Base enemies use `THEME_NONE` which matches current behavior exactly.

### Visual System (Color + Effects)

**Color tinting**: Each variant defines a tint color applied to the enemy's rendered geometry. The tint replaces the base color, not multiply — themed enemies are distinctly colored.

**Particle effects**: Each theme defines ambient particle effects rendered around themed enemies:
- **Fire**: Flickering ember particles, heat shimmer distortion
- **Ice**: Frost crystal particles, cold vapor trail
- **Poison**: Dripping toxic particles, green mist aura
- **Blood**: Dark red wisp particles, pulsing veins
- **Radiance**: Crackling arc particles, static discharge, light flares
- **Void**: Subtle distortion/negative-space shimmer — absence, not presence

Effects are rendered via the existing `particle_instance.c` instanced quad system or simple `Render_*` calls depending on complexity. Each theme gets a shared `theme_fx_*.c` module for its particle system.

### Audio Identity

Each theme has a **sonic identity** applied to its themed subs and ambient effects:

| Zone | Audio Character | Key Sounds |
|------|----------------|------------|
| **The Crucible** | Roaring, crackling, whooshing | Fire crackle, flame whoosh, sizzle on DOT tick, roaring ignition |
| **The Archive** | Crystalline, brittle, resonant | Ice crack/shatter, frozen wind, crystal chime on freeze, glass-like ring |
| **The Miasma** | Wet, bubbling, corrosive | Acid drip/sizzle, bubbling ooze, hissing gas, squelch on DOT tick |
| **The Pulse** | Organic, rhythmic, visceral | Heartbeat pulse, blood rush/drain, wet tearing, arterial spray |
| **The Corona** | Electric, humming, crackling | Electric buzz/hum, arc discharge, capacitor whine, thunderclap on chain |
| **The Expanse** | Silent, hollow, inverse | Reversed/negative-space sounds, muffled impacts, void hum, anti-sound |

Audio is loaded/played by `*_core.c` files per the Audio Ownership Rule. Theme ambient sounds (zone-level hum/crackle) are separate from skill sounds.

### Fragment Drops & Progression

Themed enemies drop fragments for their themed subs, NOT for the base subs. The `CarriedSubroutine` array in the variant config specifies the themed sub IDs and fragment types.

Example: Fire Hunter carries `SUB_ID_EMBER` and `SUB_ID_FLAK` instead of `SUB_ID_MGUN` and `SUB_ID_TGUN`. Killing a Fire Hunter can drop `FRAG_TYPE_EMBER` or `FRAG_TYPE_FLAK`.

Fragment colors match the themed enemy's color. The `Fragment_get_source_enemy_name()` table expands to include themed sources (e.g. "Fire Hunter").

### AI Modifications

**Default**: Same AI state machine, different sub loadout. A Fire Hunter still patrols, aggros, shoots, and dies — but it shoots fire projectiles instead of bullets.

**Theme-specific tweaks** (applied via variant config multipliers + flags):
- **Fire**: Slightly more aggressive (shorter aggro hesitation), faster attack cadence
- **Ice**: Slower movement, longer engagement range, sustained fire creates lockdown
- **Poison**: Normal speed, continuous mutation — attacks apply DOT, enemies gain random buffs when healed
- **Blood**: Lifesteal on hit (damage dealt partially restores HP), parasitic healing
- **Radiance**: Faster movement, near-instant projectile speed, shared threat perception among nearby allies
- **Void**: Perfected base behavior — maximum precision, zero tell variance, optimal timing. No enhancements, no gimmicks.

These are tuning knobs, not new state machines. The base AI code checks variant flags for modified behavior (e.g. `if (variant->lifesteal) self_heal(damage * 0.2)`).

---

## The Crucible (Fire) — Complete Design

> **Combat thesis: Area denial. The ground is always on fire. Positioning is survival.**

> *"PYRAXIS has been iterating on these designs since achieving autonomy. Each iteration is more lethal than the last. The programs are not just defenses — they are experiments in destruction."*

### Fire Theme Identity

- **Colors**: Orange, amber, red-orange, white-hot highlights
- **Bloom**: Warm orange-yellow glow
- **Audio**: Fire crackle, flame whoosh, sizzle on DOT ticks
- **Ambient FX**: Floating ember particles, heat shimmer
- **Effect cells**: None
- **Boss**: PYRAXIS (specced in `plans/spec_pyraxis_boss.md`)
- **Boss reward**: sub_inferno (elite channeled beam, already implemented)

### Fire Subroutine Set

Each base sub maps to a fire-themed replacement. All follow the shared core pattern (`sub_*_core.c/h`). Every fire sub answers the question: **"How does this deny area?"**

**Note on narrative alignment**: The narrative (CRU-06) describes only Hunters, Seekers, Defenders, and Mines. Stalkers and Corruptors were added to the game after the narrative was written. Their themed variants extend the same area-denial thesis.

#### Projectile Subs

| Base Sub | Fire Sub | Name | Description |
|----------|----------|------|-------------|
| sub_mgun | **sub_ember** | Ember Shot | Projectiles detonate on impact into **burn zones** — persistent ground hazards lasting ~3s that deal DOT to anything standing in them. Slower fire rate than mgun but each shot creates denied ground. Stacks up to 3 burns on a single target. *(Narrative: "Projectiles detonate on impact into 3-second burn zones. Standard evasion insufficient — must account for persistent ground hazards.")* |
| sub_tgun | **sub_flak** | Flak Cannon | Narrow cone of fast fire pellets (5 per shot, ~2x pea fire rate). 2/3 tgun range, each pellet applies burn DOT. Shotgun-style — devastating up close, spread reduces effectiveness at range. Area denial through saturation. |
| sub_pea | *(sub_pea stays as default starter in all zones)* | | |

#### Movement Subs

| Base Sub | Fire Sub | Name | Description |
|----------|----------|------|-------------|
| sub_egress | **sub_blaze** | Blaze Dash | Dash that leaves a **flame corridor** behind (~4s duration). The trail is the point — dash becomes area denial, not just escape. *(Narrative: "Dash trails ignite into flame corridors lasting 4 seconds. Area denial after attack.")* |
| sub_sprint | **sub_scorch** | Scorch Sprint | Speed boost that leaves burning footprints. Shorter duration than sprint but the trail provides persistent area denial. Fire Corruptors paint the battlefield as they run. |
| sub_boost | *(elite — stays as-is, from destructible drops)* | | |

#### Deployable Subs

| Base Sub | Fire Sub | Name | Description |
|----------|----------|------|-------------|
| sub_mine | **sub_cinder** | Cinder Mine | Deployable that detonates into a **persistent burning ground patch** — circular fire zone, ~5s sustained AOE burn DOT to anything standing in it. Less burst than mine, more area denial. *(Narrative: "Detonation radius increased 50%. Blast creates expanding fire ring.")* |

#### Support Subs

| Base Sub | Fire Sub | Name | Description |
|----------|----------|------|-------------|
| sub_mend | **sub_cauterize** | Cauterize | Heals one target (smaller than mend, 35 HP) + **cauterizes** — cleanses **chill stacks + freeze** and grants **3s chill immunity**. Also creates a burn aura around the target that damages nearby enemies for ~3s. When enemies use it: defender cauterizes a wounded ally, that ally gets chill cleansed and chill immunity, player nearby gets burned by the aura. When player uses it: self-heal + chill/freeze cleanse + 3s chill immunity + burn aura hurts nearby enemies. **Cross-zone counter**: this is the tool you bring to The Archive to survive ice mechanics. *(Narrative: "Cauterization seals the wound — fire thaws the freeze.")* |
| sub_aegis | **sub_immolate** | Immolation Shield | Shorter duration than aegis but the shield **denies area around the shielded target** — anything in melee range takes burn DOT. Attackers at range take reflected fire. The shield IS area denial — you can't stand near an immolated target. **Ice vulnerability**: each chill stack strips 1s from immolate duration — 3 frost bolts remove 3s of shield time. |

#### Stealth/Utility Subs

| Base Sub | Fire Sub | Name | Description |
|----------|----------|------|-------------|
| sub_stealth | **sub_smolder** | Smolder Cloak | Heat shimmer camouflage — semi-transparent (not fully invisible). Shorter cloak duration but ambush attack **ignites the ground around the target** (burn DOT + area denial burst). The ambush creates denied space, not just damage. |

#### Debuff/Control Subs

| Base Sub | Fire Sub | Name | Description |
|----------|----------|------|-------------|
| sub_emp | **sub_heatwave** | Heatwave Pulse | Expanding heat ring that **denies the area it passes through** — anything caught suffers 3x feedback accumulation for 10s. Doesn't disable like EMP — overheats. The ring leaves a brief heat shimmer on the ground that applies the same debuff. Standing still = overheating. |
| sub_resist | **sub_temper** | Temper Aura | Fire resistance aura that **reflects 50% of fire and burn damage back on the attacker**. Enemies under temper don't just resist fire — they punish it. The player's own fire subs become a liability when hitting tempered targets. Forces the Hybrid to switch off fire loadouts or focus the corruptor first. |

### Fire Enemy Variant Summary

| Enemy | Fire Variant | Loadout | Narrative Behavior | AI Tweaks |
|-------|-------------|---------|-------------------|-----------|
| Mine | Fire Mine | sub_cinder | "Blast creates expanding fire ring" | Cinder detonates into persistent burning ground patch |
| Hunter | Fire Hunter | sub_ember, sub_flak | "Projectiles detonate into 3-second burn zones" | Faster attack cadence, shorter burst delay |
| Seeker | Fire Seeker | sub_blaze | "Dash trails ignite into flame corridors lasting 4 seconds" | Slightly shorter orbit phase |
| Defender | Fire Defender | sub_cauterize, sub_immolate | "Cauterize heals + cleanses chill/freeze + grants chill immunity + burn aura" | Same flee AI, cauterize + immolate on allies |
| Stalker | Fire Stalker | sub_smolder, sub_blaze | *(not in narrative — extends thesis)* | Heat shimmer cloak, ambush creates burn zone |
| Corruptor | Fire Corruptor | sub_scorch, sub_heatwave, sub_temper | *(not in narrative — extends thesis)* | Fire trail while sprinting, temper reflects 50% fire/burn damage back on attacker |

### Fire Fragment Types (10 new)

```
FRAG_TYPE_EMBER, FRAG_TYPE_FLAK, FRAG_TYPE_BLAZE, FRAG_TYPE_SCORCH,
FRAG_TYPE_CINDER, FRAG_TYPE_CAUTERIZE, FRAG_TYPE_IMMOLATE,
FRAG_TYPE_SMOLDER, FRAG_TYPE_HEATWAVE, FRAG_TYPE_TEMPER
```

All use warm orange/amber fragment colors. Source enemy names: "Fire Hunter", "Fire Seeker", etc.

### Fire Progression Thresholds

Same thresholds as base equivalents — killing fire enemies in the fire zone unlocks fire subs at similar rates. Fragment counts carry across zones (if you revisit with fragments partially collected, you pick up where you left off).

---

## The Archive (Ice) — Complete Design

> **Combat thesis: Momentum control. Everything slows, freezes, shatters. Commitment is punished.**

> *"CRYONIS does not iterate its programs the way PYRAXIS does. These designs have been unchanged since they were first created. CRYONIS does not improve things. It keeps them."*

### Ice Theme Identity

- **Colors**: Blue-white, pale cyan, deep frost blue, crystalline highlights
- **Bloom**: Cold blue-white glow
- **Audio**: Ice crack/shatter, frozen wind, crystal chime on freeze, glass-like resonance
- **Ambient FX**: Frost crystal particles, cold vapor trails
- **Effect cells**: None
- **Boss**: CRYONIS (specced in `plans/spec_ice_zone.md`)

### Ice Subroutine Set

Every ice sub answers: **"How does this punish commitment?"** — slow the player's movement, freeze them in place, reduce their ability to reposition, and shatter them when they're locked down.

#### Projectile Subs

| Base Sub | Ice Sub | Name | Description |
|----------|---------|------|-------------|
| sub_mgun | **sub_frost** | Frost Bolt | Projectiles apply **1 chill stack** (2s duration, 25% slow) on hit. 3 stacks triggers **freeze** (2s hard root). Slower projectile speed (400 u/s vs 500) and fire rate (200ms vs 150ms) than mgun but each hit compounds. 8 damage per hit, 3500u range, 1 feedback/shot. *(Narrative: "Projectiles apply 2-second movement debuff on hit. Sustained fire creates ice-locked targets.")* |
| sub_tgun | **sub_snowcone** | Cone of Cold | Narrow cone of 9 ice pellets (667ms cooldown). 2/3 tgun range (2333u), each pellet applies **1 chill stack**. At close range 5+ pellets land = instant freeze from a single trigger pull. At range only 1-2 connect. 3 damage/pellet, 350 u/s, 3 feedback/shot. The ice shotgun — creates a natural danger gradient where closing distance is lethal. |

#### Movement Subs

| Base Sub | Ice Sub | Name | Description |
|----------|---------|------|-------------|
| sub_egress | **sub_shatter** | Shatter Dash | Dash (150ms, 5x speed, i-frames) that leaves a **frozen vapor trail lasting 5s**. Vapor trail applies **1 chill stack** on first contact per segment. Dash deals **30 base contact damage**, or **100 shatter damage** against frozen targets (consumes freeze). 2.5s cooldown, 25 feedback. The ice zone's combo finisher — chill targets with frost/snowcone, then shatter-dash through them. *(Narrative: "Dash leaves frozen vapor trail. Dash shatters frozen enemies for 100 integrity damage.")* |
| sub_sprint | **sub_permafrost** | Permafrost Sprint | 1.5x speed boost (5s duration, 20s cooldown, 0 feedback) that freezes the ground along the path — **1 chill stack** on contact with trail. Trail persists until sprint ends then fades over 3s. ~30u wide frozen strip. Corruptors lay down ice highways that chill anything walking through them. |

#### Deployable Subs

| Base Sub | Ice Sub | Name | Description |
|----------|---------|------|-------------|
| sub_mine | **sub_cryomine** | Cryo Mine | Detonation creates a **cryo blast** (30 damage, 150u radius) that applies **2 chill stacks** to everything hit + leaves a **3s lingering frost field** (1 chill/sec inside). 2s fuse, 3 max deployed, 250ms cooldown, 15 feedback. Much less burst than mine (30 vs 100 damage) but almost-freezes in one blast. Two cryomines = guaranteed freeze. *(Narrative: "Detonation creates a cryo blast that slows everything in radius for 3 seconds.")* |

#### Support Subs

| Base Sub | Ice Sub | Name | Description |
|----------|---------|------|-------------|
| sub_mend | **sub_stasis** | Stasis Heal | Heals target for **40 HP** + cleanses **all burn stacks** + grants **3s burn immunity**. Brief 500ms micro-stasis visual (ice crystallization) for thematic flavor — not a real lockout. 10s cooldown, 20 feedback, 300u range on allies. **Cross-zone counter**: this is the tool you bring to The Crucible to survive fire DOTs. Mirrors sub_cauterize architecture — cauterize cleanses chill (bring to Archive), stasis cleanses burn (bring to Crucible). *(Narrative: "Ice seals the wound. A brief freeze cleanses fire and restores integrity.")* |
| sub_aegis | **sub_cryoshield** | Cryo Shield | **HP-based shield** (80 HP, 8s duration or until broken) — fundamentally different from aegis (not binary invuln). When broken: **40 AoE shatter damage** + **2 chill stacks** in 120u radius. 25s cooldown, 25 feedback. Rewards patience (wait 8s for expiry) and punishes aggression (breaking it freezes you). **Fire vulnerability**: fire damage deals 1.5x to shield HP (~53 effective HP against fire subs). |

#### Stealth/Utility Subs

| Base Sub | Ice Sub | Name | Description |
|----------|---------|------|-------------|
| sub_stealth | **sub_flash_freeze** | Flash Freeze | Camouflage via frozen stillness — **must be completely still** to cloak (8s duration, different from stealth's slow-walk). Entity becomes ice-colored, crystalline static, blends into frost environment. Ambush: **50 damage + instant 2s freeze** (hard freeze, no chill stacking needed). 15s cooldown, 0 feedback gate. The ultimate commitment punish — you walked past something frozen and now you are too. |

#### Debuff/Control Subs

| Base Sub | Ice Sub | Name | Description |
|----------|---------|------|-------------|
| sub_emp | **sub_deep_freeze** | Deep Freeze | Expanding cold ring (0→400u over 500ms) that **suppresses all cooldown recovery** for 5s + applies **1 chill stack**. Skills still work but don't recharge — escape dash won't come back, heal won't refresh, shield stays down. 30s cooldown, 30 feedback. More insidious than EMP: you can use what's ready, but once spent, you're stuck. |
| sub_resist | **sub_arctic_aura** | Arctic Aura | 200u aura (8s duration, 20s cooldown, 25 feedback) that **slows enemies 30%** in range + **grants chill-on-hit** to all allies in range. The corruptor turns the whole squad into a freeze machine — every nearby enemy's attacks apply chill stacks. Kill the corruptor first or the entire area becomes a chill trap. |

### Ice Enemy Variant Summary

| Enemy | Ice Variant | Loadout | Narrative Behavior | AI Tweaks |
|-------|------------|---------|-------------------|-----------|
| Mine | Ice Mine | sub_cryomine | "Cryo blast + 2 chill stacks + lingering frost field" | CC setup — area chill, not damage |
| Hunter | Ice Hunter | sub_frost, sub_snowcone | "Frost bolts at range stack chill, snowcone up close can freeze in one blast" | 1.2x range, 0.8x speed, range-based weapon switching |
| Seeker | Ice Seeker | sub_shatter | "Orbit lays vapor trails (chill on contact), attack dash shatters frozen targets for 100 damage" | 1.3x orbit duration, vapor trail net is the weapon |
| Defender | Ice Defender | sub_stasis, sub_cryoshield | "Stasis heals allies + cleanses burn + burn immunity. Cryoshield punishes attackers on break" | Same flee AI, stasis replaces heal, HP shield replaces invuln |
| Stalker | Ice Stalker | sub_flash_freeze, sub_shatter | "Stationary cloak, ambush = 50 damage + instant 2s freeze, escape dash leaves vapor trail" | Frozen stillness cloak (must be still), shatter dash for escape |
| Corruptor | Ice Corruptor | sub_permafrost, sub_deep_freeze, sub_arctic_aura | "Frost trail highways, cooldown suppression ring, chill-on-hit aura for squad" | Battlefield shaper — ice everything |

---

## The Miasma (Poison) — Complete Design

> **Combat thesis: Attrition. You're always rotting. No safe harbor, no clean regen.**

> *"TOXIS modifies its programs continuously. No two encounters are identical. The mutation is the point."*

### Poison Theme Identity

- **Colors**: Sickly green, yellow-green, dark purple accents, bile highlights
- **Bloom**: Toxic green glow
- **Audio**: Acid drip/sizzle, bubbling ooze, hissing gas, wet squelch on DOT tick
- **Ambient FX**: Dripping toxic particles, green mist aura
- **Effect cells**: None
- **Boss**: TOXIS

### Poison Subroutine Set

Every poison sub answers: **"How does this rot the player over time?"** — DOT stacking, regen suppression, feedback decay manipulation, persistent toxic clouds. No burst, all bleed.

#### Projectile Subs

| Base Sub | Poison Sub | Name | Description |
|----------|------------|------|-------------|
| sub_mgun | **sub_toxin** | Toxin Shot | Projectiles apply **corruption DOT** — connection integrity drains for ~3s after hit. Lower per-hit damage than mgun but the DOT stacks with itself. Three toxin shots = three concurrent DOTs ticking away. *(Narrative: "Projectiles apply corruption DOT — connection integrity drains for 3 seconds after hit.")* |
| sub_tgun | **sub_virulent** | Virulent Burst | Projectile that explodes into a **toxic cloud** on impact. The cloud persists and applies stacking corruption to anything inside. Slower rate of fire, but each shot creates a rotting zone. |

#### Movement Subs

| Base Sub | Poison Sub | Name | Description |
|----------|------------|------|-------------|
| sub_egress | **sub_plague** | Plague Dash | Dash that injects a **data-corrosion payload** on contact — feedback decay is slowed for ~5s. You can still act, but your feedback won't clear. The poison doesn't stop you — it makes recovery harder. *(Narrative: "Dash injects a data-corrosion payload — feedback decay slowed for 5 seconds after contact.")* |
| sub_sprint | **sub_miasma** | Miasma Sprint | Speed boost that leaves a **toxic fog trail**. The trail suppresses integrity regen for anything standing in it. Corruptors deny regen across the battlefield as they sprint. |

#### Deployable Subs

| Base Sub | Poison Sub | Name | Description |
|----------|------------|------|-------------|
| sub_mine | **sub_spore** | Spore Mine | Detonation creates a **toxic cloud persisting for ~6s**. Entering the cloud applies stacking corruption DOT. The mine doesn't kill — it poisons the area. *(Narrative: "Detonation creates a toxic cloud persisting for 6 seconds. Entering the cloud applies stacking corruption.")* |

#### Support Subs

| Base Sub | Poison Sub | Name | Description |
|----------|------------|------|-------------|
| sub_mend | **sub_mutate** | Mutagen Heal | Healing beam carries **mutagenic properties** — healed allies briefly gain a random enhanced attribute (speed boost, damage boost, range boost — different each time). The heal is smaller than mend, but the random buff makes it unpredictable. TOXIS's philosophy: mutation IS healing. *(Narrative: "Healing beam carries mutagenic properties — healed allies briefly gain random enhanced attributes.")* |
| sub_aegis | **sub_contagion_shield** | Contagion Shield | Shield that **spreads DOT to attackers**. Anything that hits the shielded target contracts a corruption DOT. The shield doesn't just protect — it infects. Shorter duration than aegis, but punishes aggression with attrition. |

#### Stealth/Utility Subs

| Base Sub | Poison Sub | Name | Description |
|----------|------------|------|-------------|
| sub_stealth | **sub_blight** | Blight Cloak | Camouflage via mimicry of the toxic environment — the stalker blends into the zone's green mist. Ambush attack applies a **regen suppression debuff** (~5s, no integrity regen). The ambush doesn't burst you — it ensures you can't recover from the next hit. |

#### Debuff/Control Subs

| Base Sub | Poison Sub | Name | Description |
|----------|------------|------|-------------|
| sub_emp | **sub_noxious** | Noxious Burst | Expanding gas ring that **suppresses integrity regen** for anything caught (~5s). Doesn't disable systems — rots the body's ability to repair itself. Combined with toxin DOTs, this creates the "always rotting" thesis. |
| sub_resist | **sub_symbiosis** | Symbiosis Aura | Aura that gives allies **passive HP regen** + their attacks apply corruption DOT. The corruptor makes its squad heal while the player rots. The attrition gap widens every second the aura is active. |

### Poison Enemy Variant Summary

| Enemy | Poison Variant | Loadout | Narrative Behavior | AI Tweaks |
|-------|---------------|---------|-------------------|-----------|
| Mine | Poison Mine | sub_spore | "Toxic cloud persisting for 6 seconds, stacking corruption" | Cloud instead of burst |
| Hunter | Poison Hunter | sub_toxin, sub_virulent | "Corruption DOT — integrity drains for 3 seconds after hit" | Normal speed, DOT focus |
| Seeker | Poison Seeker | sub_plague | "Dash injects data-corrosion payload — feedback decay slowed 5 seconds" | Normal orbit, dash debuffs instead of damages |
| Defender | Poison Defender | sub_mutate, sub_contagion_shield | "Healed allies briefly gain random enhanced attributes" | Same flee AI, unpredictable buffs |
| Stalker | Poison Stalker | sub_blight, sub_plague | *(extends thesis)* | Toxic environment camo, regen suppression ambush |
| Corruptor | Poison Corruptor | sub_miasma, sub_noxious, sub_symbiosis | *(extends thesis)* | Squad regen + attrition aura, toxic fog trails |

---

## The Pulse (Blood) — Complete Design

> **Combat thesis: Parasitic. Every hit heals them. Every heal costs you. You're the blood bank.**

> *"HAEMOS's designs reflect its biomedical origins. Every mechanism is a medical procedure — transfusion, drainage, circulation. The zone doesn't kill you. It bleeds you."*

### Blood Theme Identity

- **Colors**: Dark crimson, arterial red, deep burgundy, pale pink highlights
- **Bloom**: Pulsing dark red glow (throbs with a heartbeat rhythm)
- **Audio**: Heartbeat pulse, blood rush/drain, wet tearing, arterial spray
- **Ambient FX**: Dark red wisp particles, pulsing veins, rhythmic glow
- **Effect cells**: None
- **Boss**: HAEMOS

### Blood Subroutine Set

Every blood sub answers: **"How does this drain the player to feed the enemy?"** — lifesteal, integrity siphoning, health exchange, sacrifice mechanics. Damage you deal comes back. Damage they deal heals them.

#### Projectile Subs

| Base Sub | Blood Sub | Name | Description |
|----------|-----------|------|-------------|
| sub_mgun | **sub_leech** | Leech Shot | Projectiles that **siphon** — damage dealt to the player partially restores the hunter's HP. Every shot is a transfusion. The hunter heals by hurting you. *(Narrative: "Projectiles that leech — damage dealt to the player partially restores the hunter's HP.")* |
| sub_tgun | **sub_hemorrhage** | Hemorrhage Burst | Heavy projectile that applies a **bleed DOT** — integrity drains over ~3s. Unlike poison DOT (which rots), bleed is faster ticking and feeds back to the shooter (partial lifesteal on DOT ticks). |

#### Movement Subs

| Base Sub | Blood Sub | Name | Description |
|----------|-----------|------|-------------|
| sub_egress | **sub_exsanguinate** | Exsanguinate Dash | Dash attack **drains integrity over 2 seconds** rather than dealing instant damage. A hemorrhage — the wound bleeds after the hit. The seeker is already gone by the time the damage finishes ticking. *(Narrative: "Dash attack drains connection integrity over 2 seconds rather than dealing instant damage. A hemorrhage.")* |
| sub_sprint | **sub_transfuse** | Transfusion Sprint | Speed boost that **drains a small amount of the player's integrity** if they're within range during the sprint. The corruptor runs through you and takes a little blood with it. Passive drain, not a direct attack. |

#### Deployable Subs

| Base Sub | Blood Sub | Name | Description |
|----------|-----------|------|-------------|
| sub_mine | **sub_bloodwell** | Blood Well | Detonation creates a **blood-drain zone** that siphons integrity for ~4s. Standing in it drains you; the siphoned integrity heals nearby enemies. The mine creates a feeding pool. *(Narrative: "Detonation creates blood-drain zones that siphon connection integrity for 4 seconds.")* |

#### Support Subs

| Base Sub | Blood Sub | Name | Description |
|----------|-----------|------|-------------|
| sub_mend | **sub_transfusion** | Transfusion Heal | Healing beam **drains the player's integrity to fuel ally healing** when in range. A blood transfusion — the defender doesn't generate healing, it redirects yours. The closer you are, the more it drains. *(Narrative: "Healing beam drains the player's connection integrity to fuel ally healing when in range. A blood transfusion.")* |
| sub_aegis | **sub_clot** | Blood Clot Shield | Shield that **absorbs damage into a blood reservoir**. When the shield expires or breaks, the stored damage heals the shielded target. You have to burst through the shield AND out-damage the heal. Rewards sustained aggression but punishes half-measures. |

#### Stealth/Utility Subs

| Base Sub | Blood Sub | Name | Description |
|----------|-----------|------|-------------|
| sub_stealth | **sub_coagulate** | Coagulation Cloak | Camouflage via blood mimicry — the stalker blends into the zone's pulsing veins. Ambush attack applies a **siphon debuff** (~5s) — all damage the stalker deals during this window heals it. The ambush doesn't just hurt you — it opens a feeding window. |

#### Debuff/Control Subs

| Base Sub | Blood Sub | Name | Description |
|----------|-----------|------|-------------|
| sub_emp | **sub_hemorrhagic** | Hemorrhagic Pulse | Expanding ring that applies **bleed DOT to everything caught** — integrity drains for ~4s. Doesn't disable systems — bleeds them dry. The pulse opens wounds that every other blood enemy can exploit. |
| sub_resist | **sub_vital_link** | Vital Link Aura | Aura that creates a **lifesteal network** — all allies in range gain lifesteal on their attacks. The corruptor turns the squad into a parasitic organism. The more enemies are in the aura, the more health they collectively steal. |

### Blood Enemy Variant Summary

| Enemy | Blood Variant | Loadout | Narrative Behavior | AI Tweaks |
|-------|--------------|---------|-------------------|-----------|
| Mine | Blood Mine | sub_bloodwell | "Blood-drain zones that siphon integrity for 4 seconds" | Drain zone instead of burst |
| Hunter | Blood Hunter | sub_leech, sub_hemorrhage | "Damage dealt partially restores hunter's HP" | Lifesteal on all attacks |
| Seeker | Blood Seeker | sub_exsanguinate | "Drains integrity over 2 seconds rather than instant damage" | Bleed-over-time dash |
| Defender | Blood Defender | sub_transfusion, sub_clot | "Drains player's integrity to fuel ally healing" | Parasitic healing — costs the player |
| Stalker | Blood Stalker | sub_coagulate, sub_exsanguinate | *(extends thesis)* | Blood camo, siphon ambush |
| Corruptor | Blood Corruptor | sub_transfuse, sub_hemorrhagic, sub_vital_link | *(extends thesis)* | Squad lifesteal aura, passive drain sprint |

---

## The Corona (Radiance) — Complete Design

> **Combat thesis: Exposure. Nothing is hidden. Everything is fast. You can't stealth, can't hide, can't outrun.**

> *"LUXIS's programs reflect its core principle: everything in the Corona is seen. There are no shadows to hide in. Stealth fails. Deception fails. Only direct confrontation succeeds."*

### Radiance Theme Identity

- **Colors**: Brilliant white, electric blue, gold-white, crackling cyan highlights
- **Bloom**: Blinding white-blue glow with electric arc flickers
- **Audio**: Electric buzz/hum, arc discharge, capacitor whine, thunderclap on chain
- **Ambient FX**: Crackling arc particles between nearby enemies, light flares, static discharge
- **Effect cells**: None
- **Boss**: LUXIS

### Radiance Subroutine Set

Every radiance sub answers: **"How does this strip away the player's ability to hide, dodge, or outmaneuver?"** — instant-speed projectiles, chain reactions that punish grouping, awareness fields that share information, blinding flashes that remove visual advantage. Energy that illuminates and connects.

#### Projectile Subs

| Base Sub | Radiance Sub | Name | Description |
|----------|-------------|------|-------------|
| sub_mgun | **sub_arc** | Arc Shot | **Near-instant projectiles** — light-speed travel, minimal dodge window. Lower damage per hit than mgun, but you can't dodge them. Mastery = positioning so you're never in the line of fire, not reaction-dodging. *(Narrative: "Fire light-speed projectiles — near-instant travel time, minimal dodge window.")* |
| sub_tgun | **sub_chain** | Chain Lightning | Projectile that **arcs to nearby targets** on impact. Hits the primary target, then chains to anything within range. Punishes clustering — you can't hide behind allies because the lightning finds you through them. |

#### Movement Subs

| Base Sub | Radiance Sub | Name | Description |
|----------|-------------|------|-------------|
| sub_egress | **sub_flash** | Flash Dash | Dash that creates a **blinding flash** at the initiation point — briefly whites out the player's screen if they're looking at the dash origin, obscuring the attack vector. You see the seeker start to dash, then you're blind, then it's on top of you. *(Narrative: "Dash creates a blinding flash at the initiation point, obscuring the attack vector.")* |
| sub_sprint | **sub_surge** | Surge Sprint | Speed boost with **crackling energy trail** that deals contact damage and applies a brief stun on touch. The corruptor doesn't just run — it becomes a live wire. |

#### Deployable Subs

| Base Sub | Radiance Sub | Name | Description |
|----------|-------------|------|-------------|
| sub_mine | **sub_beacon** | Beacon Mine | Detonation emits a **revelation pulse** that reveals the player through walls for ~3s. Doesn't deal damage — strips stealth, negates cover, shares your position with every enemy in the zone. Information is the weapon. *(Narrative: "Detonation emits a pulse that reveals the player through walls for 3 seconds.")* |

#### Support Subs

| Base Sub | Radiance Sub | Name | Description |
|----------|-------------|------|-------------|
| sub_mend | **sub_illuminate** | Illuminate | Healing pulse that also projects an **awareness field** — all allies within range gain shared threat perception (can "see" the player even without LOS). The heal is normal-strength, but the awareness field makes every nearby enemy smarter for its duration. *(Narrative: "Project awareness fields — all enemies within range gain shared threat perception.")* |
| sub_aegis | **sub_corona_shield** | Corona Shield | Shield that **blinds attackers** — melee attackers are briefly screen-flashed, ranged attackers see a lens flare on the shielded target making it harder to aim. The shield doesn't just protect — it punishes you for looking at it. |

#### Stealth/Utility Subs

| Base Sub | Radiance Sub | Name | Description |
|----------|-------------|------|-------------|
| sub_stealth | **sub_refract** | Refraction Cloak | Light-bending camouflage — the stalker refracts light around itself. BUT in the Corona, stealth is imperfect by design. The refraction creates visible **prismatic distortion** — you can see where the stalker is if you're paying attention. Ambush attack delivers a **chain-lightning strike** that arcs to nearby targets. LUXIS doesn't believe in hiding. |

#### Debuff/Control Subs

| Base Sub | Radiance Sub | Name | Description |
|----------|-------------|------|-------------|
| sub_emp | **sub_overload** | Overload Pulse | Expanding energy ring that **overcharges feedback** — anything caught gains feedback equal to 50% of their current feedback (multiplicative penalty). If you're already running hot, overload pushes you toward spillover. Clean players barely feel it. Punishes resource mismanagement. |
| sub_resist | **sub_conduit** | Conduit Aura | Aura that creates a **chain-lightning network** — when any ally in range is hit, the damage chains to the attacker (and to other nearby enemies, healing them slightly). The aura makes the squad a unified electrical grid. Hit one, shock yourself. |

### Radiance Enemy Variant Summary

| Enemy | Radiance Variant | Loadout | Narrative Behavior | AI Tweaks |
|-------|-----------------|---------|-------------------|-----------|
| Mine | Radiance Mine | sub_beacon | "Pulse that reveals player through walls for 3 seconds" | Intel mine, not damage mine |
| Hunter | Radiance Hunter | sub_arc, sub_chain | "Light-speed projectiles — near-instant travel, minimal dodge window" | Faster movement, undodgeable shots |
| Seeker | Radiance Seeker | sub_flash | "Blinding flash at initiation point, obscuring attack vector" | Flash on dash start, faster dash speed |
| Defender | Radiance Defender | sub_illuminate, sub_corona_shield | "Awareness fields — all enemies gain shared threat perception" | Same flee AI, awareness field on heal |
| Stalker | Radiance Stalker | sub_refract, sub_flash | *(extends thesis)* | Imperfect cloak (LUXIS doesn't believe in hiding), chain ambush |
| Corruptor | Radiance Corruptor | sub_surge, sub_overload, sub_conduit | *(extends thesis)* | Live-wire sprint, chain-lightning squad aura |

---

## The Expanse (Void) — Complete Design

> **Combat thesis: Precision. No gimmicks, no tells, no waste. Pure skill check against perfected enemies.**

> *"NIHILIS does not enhance its programs. It perfects them. Where other superintelligences add complexity, NIHILIS removes it. The result is more dangerous than any elemental modifier."*

### Void Theme Identity

- **Colors**: Deep purple-black, absence-white highlights, negative space
- **Bloom**: Subtle dark purple glow — or anti-glow (darkening instead of brightening)
- **Audio**: Reversed/negative-space sounds, muffled impacts, void hum, anti-sound. Quieter than other zones. Sound is being deleted.
- **Ambient FX**: Subtle distortion shimmer, negative-space particles (dark spots that eat light)
- **Effect cells**: None
- **Boss**: NIHILIS

### Void Subroutine Set — The Anti-Theme

The Void is unique: NIHILIS strips away complexity rather than adding it. Void subs are **perfected base abilities** with surgical precision, plus spatial manipulation (teleportation, suppression, deletion). The subs the player earns from the Void are about *removing* things — removing position, removing abilities, removing safety.

Every void sub answers: **"How does this remove something the player relies on?"**

#### Projectile Subs

| Base Sub | Void Sub | Name | Description |
|----------|----------|------|-------------|
| sub_mgun | **sub_null** | Null Shot | Projectiles with **perfect accuracy** — no spread, no deviation, mathematically optimal aim. Lower fire rate, but every shot lands exactly where it's aimed. Player version: precision shots that never spread. Enemy version: *every shot hits*. *(Narrative: "Every shot is placed with perfect accuracy.")* |
| sub_tgun | **sub_erasure** | Erasure Bolt | Heavy projectile that **deletes active effects** on the target — clears buffs, removes shield, stops regen boost, strips active sub effects. The shot doesn't just damage — it removes your tools. |

#### Movement Subs

| Base Sub | Void Sub | Name | Description |
|----------|----------|------|-------------|
| sub_egress | **sub_warp** | Void Warp | **Short-range teleport** instead of a dash — no travel time, no trail, no i-frames. You were here, now you're there. No in-between to dodge through. Player version: instant repositioning. Enemy version: *perfect dash timing with zero tell variance*. *(Narrative: "Perfect dash timing with zero tell variance.")* |
| sub_sprint | **sub_drift** | Void Drift | Speed boost that makes the user **partially intangible** — passes through projectiles (but not walls) while active. The void doesn't speed you up — it removes the things in your way. |

#### Deployable Subs

| Base Sub | Void Sub | Name | Description |
|----------|----------|------|-------------|
| sub_mine | **sub_rift** | Void Rift | Deployable that creates a **spatial rift** — a small zone where sub usage is suppressed. Standing in the rift disables active subs (shield drops, cloak breaks, sprint stops). The mine doesn't damage — it deletes your abilities. *(Narrative: "Perfect coverage patterns with zero gaps." — Void mines deny space through suppression, not damage.)* |

#### Support Subs

| Base Sub | Void Sub | Name | Description |
|----------|----------|------|-------------|
| sub_mend | **sub_restore** | Void Restore | Healing that **resets the target to a previous state** — full HP restore to the value from ~3s ago. Not a heal — a temporal rewind. If the ally was full HP 3 seconds ago and took 80 damage, restore brings them back to full. If they were already low, restore does little. Rewards preemptive use. *(Narrative: "Optimal healing distribution and shield timing." — perfect efficiency.)* |
| sub_aegis | **sub_void_shell** | Void Shell | Shield that **absorbs attacks into nothing** — no reflect, no shatter, no effect. Attacks simply cease to exist when they hit. Shorter duration than aegis, but absolute. No workaround, no counter-play while it's up. Pure deletion. |

#### Stealth/Utility Subs

| Base Sub | Void Sub | Name | Description |
|----------|----------|------|-------------|
| sub_stealth | **sub_absence** | Absence | True invisibility via **deletion from perception** — the stalker doesn't cloak, it removes itself from the player's awareness. No shimmer, no distortion, no tell. The most dangerous stealth in the game. Ambush attack is a clean, precise strike — no bonus element, just maximum damage. *(NIHILIS doesn't add. It removes.)* |

#### Debuff/Control Subs

| Base Sub | Void Sub | Name | Description |
|----------|----------|------|-------------|
| sub_emp | **sub_suppress** | Suppression Pulse | Expanding void ring that **disables one random active sub** for ~5s. Doesn't disable everything like EMP — surgically removes one tool. The player never knows which sub they'll lose. Targeted deletion. |
| sub_resist | **sub_entropy** | Entropy Aura | Aura that causes **all enemies in range to take reduced damage** (flat damage reduction, not percentage). Simple, efficient, no frills. The aura doesn't add anything — it just makes damage disappear. *(NIHILIS removes complexity. Even the buff is simple.)* |

### Void Enemy Variant Summary

| Enemy | Void Variant | Loadout | Narrative Behavior | AI Tweaks |
|-------|-------------|---------|-------------------|-----------|
| Mine | Void Mine | sub_rift | "Perfect coverage patterns with zero gaps" | Suppression zones, no damage |
| Hunter | Void Hunter | sub_null, sub_erasure | "Maximum precision. Every shot placed with perfect accuracy" | Perfect aim, no spread, no hesitation |
| Seeker | Void Seeker | sub_warp | "Maximum speed. Perfect dash timing with zero tell variance" | Teleport instead of dash, zero telegraph |
| Defender | Void Defender | sub_restore, sub_void_shell | "Maximum efficiency. Optimal healing and shield timing" | Perfect triage, never wastes a heal or shield |
| Stalker | Void Stalker | sub_absence, sub_warp | *(extends thesis)* | True invisibility, precision ambush |
| Corruptor | Void Corruptor | sub_drift, sub_suppress, sub_entropy | *(extends thesis)* | Intangible sprint, surgical suppression, simple damage reduction |

---

## Implementation Considerations

### Shared Core Pattern (Mandatory per CLAUDE.md)

Every themed sub MUST follow the core extraction rule:
- `sub_*_core.c/h` — config struct, state struct, timer/render/audio logic
- `sub_*.c` (player wrapper) — input handling, feedback cost, delegates to core
- Enemy variant code — AI decisions, embeds core state, delegates to core

### Status Effect Systems (Zone Foundations)

Multiple zones rely on status effect systems that don't exist yet. These should be shared modules built before the themed subs that use them:

- **`burn.c/h`** (Fire) — manages active burns on entities (player + enemies). Burn state: damage/sec, duration remaining, stack count. Applied by ember, blaze, smolder, cauterize, etc. Visual: orange damage ticks, flame particle on burning entity.
- **`chill.c/h`** (Ice) — manages chill stacks + freeze on entities (player + enemies). Chill state: stack count (max 3), per-stack duration (2s), freeze timer (2s at 3 stacks), post-freeze immunity (2s). 25% speed reduction per stack. Applied by frost, snowcone, shatter vapor trail, cryomine, etc. Visual: frost particles (1 stack), ice crackle (2 stacks), blue-white frozen shell (frozen). Mirrors `burn.c/h` architecture. Full spec in `plans/spec_ice_zone.md`.
- **`dot.c/h`** (Poison/Blood) — generic DOT system for corruption and bleed. DOT state: damage/sec, duration, type (poison/bleed), lifesteal fraction. Applied by toxin, leech, hemorrhage, etc. Could potentially unify with burn.c or remain separate for distinct visuals/audio.

### Fragment/Progression Scaling

With ~74 total subs, the fragment system needs:
- `FRAG_TYPE_COUNT` grows significantly (14 → ~74)
- Save file format must handle dynamic fragment type count gracefully
- Catalog UI must handle many more tabs/entries (possibly sub-tabs per zone theme)
- Skillbar's 10-slot limit becomes the real build constraint — choosing from 74 subs

### Zone Transition Behavior

When a themed enemy is killed in its home zone and the player later returns:
- Respawned enemies are still themed (theme is zone-wide, permanent)
- Fragment progress persists in save data
- Themed subs unlocked in fire zone work everywhere (not zone-locked)

### Cross-Zone Elemental Counters (Paired Zones)

The six zones form **three paired zone systems**. Each pair shares a counter relationship: the heal sub from one zone cleanses the other zone's status effect, and the elements interact at every layer (status effects, shields, area denial, boss mechanics). Neither zone in a pair is fully conquerable without tools from the other.

**The three pairs:**

| Pair | Zone A | Zone B | A's Status | B's Status | A's Heal Cleanses | B's Heal Cleanses |
|------|--------|--------|------------|------------|-------------------|-------------------|
| **Fire ↔ Ice** | The Crucible | The Archive | Burn | Chill/Freeze | Chill (cauterize) | Burn (stasis) |
| **Poison ↔ Blood** | The Miasma | The Pulse | Corruption DOT | Bleed DOT | Bleed (TBD) | Corruption (TBD) |
| **Radiance ↔ Void** | The Corona | The Expanse | Overcharge | Suppression | Suppression (TBD) | Overcharge (TBD) |

**The pattern** (using Fire ↔ Ice as the worked example — full details in `plans/spec_ice_zone.md`):

1. **Heal sub cross-dependency**: Each zone's heal cleanses the *other* zone's status effect. sub_cauterize (fire) cleanses chill/freeze + grants chill immunity → bring to The Archive. sub_stasis (ice) cleanses burn + grants burn immunity → bring to The Crucible. Neither cleanses its own zone's effect. sub_mend (base) doesn't cleanse anything.

2. **Status effect override**: Opposing elements react on contact, never coexist. Fire damage on frozen target = thermal shatter (bonus damage). Chill on burning target = extinguish (consume stacks 1:1). Hard freeze overrides burn entirely.

3. **Shield vulnerability**: Themed shields are soft-countered by the opposite element. Fire deals 1.5x to cryoshield HP. Chill strips immolate duration. Faster path through, not a free pass.

4. **Area denial counter**: Opposing area effects neutralize on overlap. Fire pools melt frost trails. Frost trails extinguish fire pools. Steam burst visual + hiss. Active counter-play — reshape the arena with the opposite element.

5. **Boss design**: Boss fights become loadout puzzles. CRYONIS (ice boss) is countered by fire subs. PYRAXIS (fire boss) is countered by ice subs. Without counter tools: every mechanic is a wall. With counter tools: every mechanic has an answer.

**The progression loop**: Player enters Zone A → struggles → enters Zone B with A's tools → pushes deeper → returns to Zone A with B's tools → conquers. The player bounces between paired zones, each trip armed with new counters. Metroidvania progression through loadout discovery — not keys and doors.

**Poison ↔ Blood and Radiance ↔ Void** counter details will follow the same 5-layer pattern. Heal cross-dependency is the anchor; status overrides, shield vulnerabilities, area denial cancellation, and boss implications layer on top. Detailed specs for those pairs will be written when those zones are designed.
