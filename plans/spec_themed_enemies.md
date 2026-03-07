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
- **Effect cells**: Ember cells — orange glow, DOT while standing
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
| sub_mend | **sub_cauterize** | Cauterize | Heals one target (smaller than mend) + **cauterizes** — the healed target AND all allies within 100 units of it get burn cleansed + brief burn immunity (~3s). Also creates a burn aura around the target that damages nearby enemies for ~3s. When enemies use it: defender cauterizes a wounded ally, that ally + nearby allies get burn cleansed and fire immunity, player nearby gets burned by the aura. When player uses it: self-heal + burn cleanse (100-unit radius catches nearby friendlies if co-op ever exists) + brief burn immunity + burn aura hurts nearby enemies. *(Narrative: "Cauterization seals the wound — fire can't take hold.")* |
| sub_aegis | **sub_immolate** | Immolation Shield | Shorter duration than aegis but the shield **denies area around the shielded target** — anything in melee range takes burn DOT. Attackers at range take reflected fire. The shield IS area denial — you can't stand near an immolated target. |

#### Stealth/Utility Subs

| Base Sub | Fire Sub | Name | Description |
|----------|----------|------|-------------|
| sub_stealth | **sub_smolder** | Smolder Cloak | Heat shimmer camouflage — semi-transparent (not fully invisible). Shorter cloak duration but ambush attack **ignites the ground around the target** (burn DOT + area denial burst). The ambush creates denied space, not just damage. |

#### Debuff/Control Subs

| Base Sub | Fire Sub | Name | Description |
|----------|----------|------|-------------|
| sub_emp | **sub_heatwave** | Heatwave Pulse | Expanding heat ring that **denies the area it passes through** — anything caught suffers 2x feedback accumulation for ~5s. Doesn't disable like EMP — overheats. The ring leaves a brief heat shimmer on the ground that applies the same debuff. Standing still = overheating. |
| sub_resist | **sub_temper** | Temper Aura | Fire resistance aura for allies + **allies' attacks create burn zones** while in range. The corruptor turns the whole squad into area denial machines. Defensive buff that multiplies the zone's core mechanic. |

### Fire Enemy Variant Summary

| Enemy | Fire Variant | Loadout | Narrative Behavior | AI Tweaks |
|-------|-------------|---------|-------------------|-----------|
| Mine | Fire Mine | sub_cinder | "Blast creates expanding fire ring" | Cinder detonates into persistent burning ground patch |
| Hunter | Fire Hunter | sub_ember, sub_flak | "Projectiles detonate into 3-second burn zones" | Faster attack cadence, shorter burst delay |
| Seeker | Fire Seeker | sub_blaze | "Dash trails ignite into flame corridors lasting 4 seconds" | Slightly shorter orbit phase |
| Defender | Fire Defender | sub_cauterize, sub_immolate | "Cauterizes — healed allies gain brief fire immunity" | Same flee AI, cauterize + immolate on allies |
| Stalker | Fire Stalker | sub_smolder, sub_blaze | *(not in narrative — extends thesis)* | Heat shimmer cloak, ambush creates burn zone |
| Corruptor | Fire Corruptor | sub_scorch, sub_heatwave, sub_temper | *(not in narrative — extends thesis)* | Fire trail while sprinting, temper turns squad into area denial |

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
- **Effect cells**: Frost cells — blue glow, reduced traction while standing
- **Boss**: CRYONIS

### Ice Subroutine Set

Every ice sub answers: **"How does this punish commitment?"** — slow the player's movement, freeze them in place, reduce their ability to reposition, and shatter them when they're locked down.

#### Projectile Subs

| Base Sub | Ice Sub | Name | Description |
|----------|---------|------|-------------|
| sub_mgun | **sub_frost** | Frost Bolt | Projectiles apply **2-second movement slow** on hit. Sustained fire stacks to ice-lock — 3+ stacks freezes the target briefly. Slower projectiles than mgun but each hit compounds. *(Narrative: "Projectiles apply 2-second movement debuff on hit. Sustained fire creates ice-locked targets.")* |
| sub_tgun | **sub_shatter** | Shatter Shot | Heavy projectile that deals bonus damage to slowed/frozen targets. The combo finisher — frost bolt locks them down, shatter shot breaks them apart. Low fire rate, high payoff on frozen targets. |

#### Movement Subs

| Base Sub | Ice Sub | Name | Description |
|----------|---------|------|-------------|
| sub_egress | **sub_glaciate** | Glacial Dash | Dash that leaves a **frozen trail lasting ~5s**. Anything on the trail has reduced traction — sliding momentum, can't stop or turn quickly. Punishes anyone who steps on the path. *(Narrative: "Dash leaves a frozen trail lasting 5 seconds. Players caught on ice have reduced traction.")* |
| sub_sprint | **sub_permafrost** | Permafrost Sprint | Speed boost that freezes the ground permanently (until the sprint ends). Longer trail duration than glaciate but narrower. Corruptors lay down ice highways. |

#### Deployable Subs

| Base Sub | Ice Sub | Name | Description |
|----------|---------|------|-------------|
| sub_mine | **sub_cryomine** | Cryo Mine | Detonation creates a **cryo blast** — slows everything in radius for ~3s instead of dealing burst damage. Less lethal than a mine, but sets up kills for everything else in the zone. *(Narrative: "Detonation creates a cryo blast that slows everything in radius for 3 seconds.")* |

#### Support Subs

| Base Sub | Ice Sub | Name | Description |
|----------|---------|------|-------------|
| sub_mend | **sub_stasis** | Stasis Field | Projects a stasis field on the target — **briefly freezes a damaged ally**, halting all damage AND all function. Not a heal — a pause button. The ally is invulnerable but inert. Buys time for other defenders to respond. *(Narrative: "Project stasis fields instead of standard heals — briefly freeze damaged allies, halting damage but also halting function. A different approach to preservation.")* |
| sub_aegis | **sub_cryoshield** | Cryo Shield | Ice armor that absorbs damage and **shatters when it breaks** — the shatter deals AoE burst to anything nearby + applies slow. Attackers are punished for breaking through. The shield rewards patience (let it expire) and punishes aggression. |

#### Stealth/Utility Subs

| Base Sub | Ice Sub | Name | Description |
|----------|---------|------|-------------|
| sub_stealth | **sub_flash_freeze** | Flash Freeze | Camouflage via frozen stillness — the stalker becomes a static ice formation, nearly invisible when motionless. Ambush attack **freezes the target solid** for a brief duration. The ultimate commitment punish — you walked past something frozen and now you are too. |

#### Debuff/Control Subs

| Base Sub | Ice Sub | Name | Description |
|----------|---------|------|-------------|
| sub_emp | **sub_absolute_zero** | Absolute Zero | Expanding cold ring that **suppresses all cooldown recovery** in its radius for ~5s. Skills don't recharge. The ice doesn't disable your systems — it slows them to nothing. You can still act, but your options won't come back. |
| sub_resist | **sub_arctic_aura** | Arctic Aura | Aura that **slows all enemies that enter range** + allies in range gain slow-on-hit for their attacks. The corruptor turns the whole area into a friction trap. Everything within range is fighting through ice. |

### Ice Enemy Variant Summary

| Enemy | Ice Variant | Loadout | Narrative Behavior | AI Tweaks |
|-------|------------|---------|-------------------|-----------|
| Mine | Ice Mine | sub_cryomine | "Cryo blast that slows everything in radius for 3 seconds" | Slow instead of damage — sets up kills |
| Hunter | Ice Hunter | sub_frost, sub_shatter | "Projectiles apply 2-second movement debuff. Sustained fire creates ice-locked targets" | Longer engagement range, slower movement |
| Seeker | Ice Seeker | sub_glaciate | "Dash leaves frozen trail lasting 5 seconds. Reduced traction on ice" | Longer orbit phase, trail is the weapon |
| Defender | Ice Defender | sub_stasis, sub_cryoshield | "Stasis fields — briefly freeze damaged allies, halting damage and function" | Same flee AI, stasis instead of heal |
| Stalker | Ice Stalker | sub_flash_freeze, sub_glaciate | *(extends thesis)* | Frozen stillness cloak, ambush freezes target |
| Corruptor | Ice Corruptor | sub_permafrost, sub_absolute_zero, sub_arctic_aura | *(extends thesis)* | Lays ice highways, suppresses cooldowns |

---

## The Miasma (Poison) — Complete Design

> **Combat thesis: Attrition. You're always rotting. No safe harbor, no clean regen.**

> *"TOXIS modifies its programs continuously. No two encounters are identical. The mutation is the point."*

### Poison Theme Identity

- **Colors**: Sickly green, yellow-green, dark purple accents, bile highlights
- **Bloom**: Toxic green glow
- **Audio**: Acid drip/sizzle, bubbling ooze, hissing gas, wet squelch on DOT tick
- **Ambient FX**: Dripping toxic particles, green mist aura
- **Effect cells**: Toxic cells — green glow, stacking corruption DOT while standing
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
- **Effect cells**: Pulse cells — red glow, slow integrity drain while standing
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
- **Effect cells**: Charged cells — electric blue glow, chain damage relay (damage to player chains to nearby charged cells)
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
- **Effect cells**: Void cells — dark purple, suppresses sub cooldown recovery while standing
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
- **`slow.c/h`** (Ice) — manages movement slow/freeze on entities. Slow state: speed multiplier, duration, stack count, freeze threshold. Applied by frost, glaciate, cryomine, etc. Visual: blue tint on slowed entity, ice crystal overlay on frozen.
- **`dot.c/h`** (Poison/Blood) — generic DOT system for corruption and bleed. DOT state: damage/sec, duration, type (poison/bleed), lifesteal fraction. Applied by toxin, leech, hemorrhage, etc. Could potentially unify with burn.c or remain separate for distinct visuals/audio.

### Fragment/Progression Scaling

With ~74 total subs, the fragment system needs:
- `FRAG_TYPE_COUNT` grows significantly (14 → ~74)
- Save file format must handle dynamic fragment type count gracefully
- Catalog UI must handle many more tabs/entries (possibly sub-tabs per zone theme)
- Skillbar's 10-slot limit becomes the real build constraint — choosing from 74 subs

### Theme Effect Cell Integration

Themed enemies should synergize with their zone's effect cells:
- Fire enemies standing on ember cells could get a damage boost or heal
- Player standing on ember cells while fighting fire enemies takes DOT from ground AND from enemy abilities
- This creates positional gameplay — lure enemies off their terrain advantage

### Zone Transition Behavior

When a themed enemy is killed in its home zone and the player later returns:
- Respawned enemies are still themed (theme is zone-wide, permanent)
- Fragment progress persists in save data
- Themed subs unlocked in fire zone work everywhere (not zone-locked)
