# Spec: Combat Air Patrols (CAP)

## Overview

Enemies currently spawn at a point and loiter — they wander randomly near their spawn until the player enters aggro range. Combat Air Patrols replace that idle loitering with **deliberate patrol routes** between waypoints. Hunters sweep corridors in formation, seekers prowl perimeters, and the space between landmarks becomes actively hostile territory instead of a gallery of stationary targets.

CAP transforms the feel of the game: instead of walking into encounters, the player gets *found* by encounters. Patrols moving through the world create dynamic tension — you hear gunfire before you see the source, you spot a formation crossing your path and have to decide: engage, evade, or ambush.

## The Problem

Right now, obstacle-spawned enemies (and landmark-spawned enemies) idle near their spawn point with random wander. This creates predictable, static encounters — the player always initiates combat. There's no sense of the security programs actively *patrolling* their domain. The world feels reactive, not alive.

## Core Concept

Each patrol-capable enemy gets assigned a **patrol route** — a pair of waypoints (A and B). The enemy flies between them at patrol speed, then reverses. When a patrol group shares the same route, they maintain **formation spacing** along the path, creating the visual of a coordinated security sweep.

When the player enters detection range, patrol enemies break formation and switch to their normal combat AI (chase, shoot, dash, etc.). After the player leaves detection range or dies, surviving patrol enemies **return to their route** and resume patrolling.

## Waypoint Generation

### Obstacle-spawned patrols
When the scatter algorithm places an obstacle with enemy spawns, it generates a second waypoint for the patrol route:
- **Waypoint A**: The spawn position (already determined by obstacle placement + transform)
- **Waypoint B**: A random point 64–128 units away from A
- B must be in open space (not inside a wall)
- B should have line-of-sight to A (no walls blocking the path) — or at minimum, the patrol path shouldn't pass through solid geometry
- Direction of B from A is random (seeded from zone RNG for determinism)

### Landmark-spawned patrols
Landmark spawns could also get patrol routes. The waypoint B would be generated the same way — random direction, 64–128 units, LOS check.

### Hand-placed patrols (future)
Godmode could eventually support placing patrol routes manually — click waypoint A, click waypoint B, assign enemy type. But that's a later enhancement.

## Patrol Behavior

### State machine addition
Each patrol-capable enemy gains a new AI state: `PATROL`. This sits alongside existing states (IDLE, CHASE, ATTACK, etc.).

```
PATROL → (player detected) → existing combat AI (CHASE/ATTACK/etc.)
combat AI → (player lost / disengaged) → RETURN_TO_PATROL → PATROL
```

### Patrol movement
- Enemy flies from A to B at **patrol speed** (slower than combat chase speed — maybe 60-70% of chase speed)
- On reaching B (within threshold distance), reverses direction toward A
- Continuous back-and-forth loop
- Smooth turnaround (decelerate → reverse → accelerate) rather than instant snap

### Detection & engagement
- Patrol enemies use the same detection range as their current AI (LOS check, aggro radius)
- On detecting the player, they **break patrol** and switch to normal combat behavior
- No special delay — if they see you, they engage immediately

### Returning to patrol
- After combat ends (player dead, player out of range for N seconds), enemy enters `RETURN_TO_PATROL`
- Flies back to nearest point on their A↔B route
- Resumes patrol from that point
- If wounded, still returns to patrol (they don't heal — that's the defender's job)

## Formations

### Group patrols
When multiple enemies share the same obstacle spawn (e.g., patrol_encounter with 3 spawns), they share the same patrol route but offset their position along it:
- **Spacing**: Even distribution along the A↔B segment
- As they patrol, they maintain relative spacing (leader at front, others trailing)
- Formation breaks on combat engagement — each enemy fights independently

### Formation types (stretch goal)
- **Line abreast**: Side by side, perpendicular to patrol direction. Good for sweeping wide corridors.
- **Trail**: Single file along the patrol direction. Natural for narrow spaces.
- **Chevron**: V-formation. Looks badass.

For v1, trail formation (spaced along the route) is simplest and looks good.

## Per-Enemy-Type Behavior

### Hunters (primary CAP candidate)
- Natural fit — ranged enemies patrolling and shooting
- Patrol speed: ~250 (vs 400 combat speed)
- Fire while patrolling? Open question (see below)
- Formation: trail or line abreast

### Seekers
- Fast melee — patrol creates a different dynamic
- Patrol speed: ~200 (vs their insane dash speed)
- On detection: wind up and dash as normal
- Scary because they're already moving when they spot you

### Mines
- Stationary by nature — probably don't patrol
- Could do slow drift patrols? Floating mine sweeps? Cool but different spec.

### Defenders
- Support unit — patrols near allies to provide healing/shields
- Could patrol with a hunter escort group
- Flees when player gets close (existing behavior)

### Stalkers
- Stealth + ambush — patrolling while cloaked is terrifying
- Reveals and ambushes when player crosses patrol path
- Perfect thematic fit

## Open Questions

1. **Should hunters fire while patrolling?** If they spot the player at range, do they open fire immediately while still on the patrol path, or do they first break patrol and switch to chase? Firing while patrolling = more dangerous, feels like active security. Breaking first = more telegraphed, gives player reaction time.

2. **Patrol speed values**: What feels right? Too slow and patrols feel like they're sleepwalking. Too fast and the player can't react. Needs playtesting. Starting suggestion: 60% of combat speed.

3. **Return-to-patrol timeout**: How long after losing the player does an enemy wait before returning to patrol? Immediate feels robotic. Long delay means they effectively stop patrolling after first contact. Maybe 5-10 seconds?

4. **Do all obstacle spawns get patrols, or is it opt-in?** A `patrol` flag on obstacle spawn lines would let us have some stationary guards and some patrollers in the same obstacle. Or: all spawns in obstacles with a `patrol` style get routes. Or: patrol probability on the spawn line.

5. **Waypoint B validation**: How strict is the LOS check? Full raycast against all walls? Simplified grid check? What if no valid B point exists (obstacle in a tight corner) — fall back to idle wander?

6. **Formation integrity**: When one enemy in a formation dies, do the others close ranks or maintain their original spacing with a gap? Closing ranks looks more alive but requires coordination logic.

7. **Patrol route variety**: Always straight A↔B? Or could routes be triangular (A→B→C→A) for more interesting sweep patterns? v1 should be A↔B, future could add multi-waypoint routes.

8. **Audio/visual telegraph**: Should patrolling enemies have a subtle visual or audio cue? A patrol-mode engine hum, formation lights, slightly different movement animation? Helps the player spot patrols before being spotted.

9. **Obstacle chunk integration**: Should the chunk format support explicit patrol waypoints (`patrol_b 5 3`)? Or is random generation sufficient? Hand-authored waypoints would allow precise patrol lanes through obstacle geometry.

10. **Interaction with fog of war**: Does the player see patrol routes on the minimap / fog reveal? Seeing a formation blip moving on the edge of fog would be incredibly tense.

## Integration Points

- **Enemy AI modules** (hunter.c, seeker.c, stalker.c, defender.c): New PATROL and RETURN_TO_PATROL states
- **procgen.c / scatter_obstacles()**: Generate waypoint B for each obstacle spawn
- **zone.h / ZoneSpawn**: Add patrol waypoint fields (world_x_b, world_y_b, has_patrol)
- **chunk.h / ChunkSpawn**: Optional patrol data
- **enemy_util.c**: Shared patrol movement + formation spacing helpers

## Not In Scope

- Multi-waypoint routes (v1 is A↔B only)
- Hand-placed patrol routes in godmode
- Mine drift patrols
- Dynamic patrol route changes based on player behavior
- Alert states propagating between patrol groups

## Done When

- Obstacle-spawned enemies patrol between two waypoints instead of idling
- Patrol routes are deterministic (seeded from zone RNG)
- Enemies break patrol and engage on player detection
- Enemies return to patrol after disengagement
- Group spawns maintain formation spacing along route
- Patrol waypoints respect map geometry (no patrolling through walls)
- At least hunters and seekers support patrol behavior
- The space between landmarks feels actively hostile — security programs sweeping the corridors
