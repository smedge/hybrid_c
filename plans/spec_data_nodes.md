# Spec: Data Nodes & Data Logs

## Overview

Data nodes are collectible narrative objects scattered throughout cyberspace. They contain corporate memos, research logs, personal journals, AI transcripts, and corrupted data — the breadcrumb trail that reveals the player's true nature. Early nodes are mundane (infrastructure reports, maintenance logs). As the player pushes deeper, the picture darkens. By the endgame, the fragments assemble into the full revelation.

Each node is a white spinning disk symbol in the world. The player walks into it, gets pulled to center (like portals and savepoints), receives a "Data Transfer Complete" notification, and the message is added to their **Data Logs** window (L key) for reading at their leisure.

## Data Node Placement

### Regular zones (generic cyberspace)
- Up to **10 data nodes** per zone
- Placed at landmarks during procgen, same as portals and savepoints
- Each node is wired to a message via identifier in the zone file

### Themed zones (6 biome worlds)
- **8 discoverable nodes** placed in the zone
- **1 automated transfer** triggered on entering the boss arena (no physical node — the boss's presence forces a data dump)
- **1 automated transfer** triggered on boss defeat (the dying superintelligence releases its final data)
- Total: **10 messages per themed zone**

### The Hexarchy
- Count TBD — depends on final zone count for the endgame
- Same node mechanics, hex grid placement

## Zone File Format

Data nodes wire to landmarks the same way portals and savepoints do:

```
landmark data_cache_01 resources/chunks/data_node_001.chunk 1 moderate 80 0.6 2.0
landmark_datanode data_cache_01 node_origin_001
```

Format: `landmark_datanode <landmark_type> <node_id>`

The `node_id` is a unique string that maps to a message in the narrative data file (see Message Storage below). This keeps message content out of zone files — zones only reference node IDs.

### Automated transfers (boss zones)

Boss arena entry and defeat triggers don't need landmarks. They're zone-level events:

```
datanode_on_boss_enter node_crucible_boss_enter
datanode_on_boss_defeat node_crucible_boss_defeat
```

These fire automatically at the appropriate moment, showing the same "Data Transfer Complete" notification but without physical node interaction.

## Message Storage

### Narrative data file

All data node messages live in a single file: `resources/data/messages.dat`

Line-based format:

```
# Regular zone messages
node node_origin_001
zone The Origin
title Maintenance Log 2847-A
body Infrastructure sweep complete. All sectors nominal. Routing tables updated. Note: background noise levels in sector 7 remain elevated. Filed report #4471 for review.
end

node node_origin_002
zone The Origin
title Personnel Transfer Notice
body INTERNAL MEMO — Dr. Elise Varga reassigned from Neural Interface Division to Special Projects. Effective immediately. All access credentials revoked pending new clearance level.
end

# Boss zone automated transfers
node node_crucible_boss_enter
zone The Crucible
title [FORCED EXTRACTION]
body >>>ALERT: UNAUTHORIZED SCAN DETECTED<<<\nThe superintelligence probes your neural pathways. Data floods in unbidden. Fragments of its architecture map onto your consciousness. You see what it sees: a brain. Suspended. Wired. Alive.
end
```

Fields:
- `node <id>` — matches the `node_id` in zone files or boss triggers
- `zone <name>` — display name for the Data Logs tab (must match zone name exactly)
- `title <text>` — always-visible accordion header in the Data Logs window
- `body <text>` — full message content, supports `\n` for line breaks
- `end` — terminates the entry

### Why a separate file (not embedded in zone files)

1. Writers can edit all narrative content in one place without touching zone configs
2. Message text can be long — zone files should stay clean infrastructure
3. Multiple zones can theoretically reference the same message (shared corporate memos)
4. Automated boss transfers reference messages but have no landmark

## Data Node Entity

### New files: `data_node.c/h`

### Visual: Spinning disk

The node renders as a **white disk symbol spinning on its vertical axis**. The disk is an ellipse whose horizontal radius oscillates via `cos(angle)` — creating the illusion of a 3D disk spinning in place.

```
     ___
    /   \      ← full face (front-facing)
    \___/

      |
      |        ← edge-on (side-facing)
      |

     ___
    /   \      ← full face again
    \___/
```

Implementation:
- Base shape: filled ellipse (triangle fan, ~12 segments)
- Horizontal radius: `BASE_RADIUS * abs(cos(spin_angle))` where `spin_angle` increments over time
- Vertical radius: constant `BASE_RADIUS`
- Color: white with slight cyan tint `{0.9f, 0.95f, 1.0f, 1.0f}`
- **Inner hole**: smaller black disk (same ellipse technique, ~40% of BASE_RADIUS) rendered on top to create the disk/record look
- Minimum horizontal radius clamped to ~15% so the disk never fully disappears edge-on
- Spin speed: **180 deg/sec** (one full rotation every 2 seconds)

### Bloom
- Renders into foreground bloom FBO for a soft white glow
- Bloom source: single point or small circle at node center

### States

```c
typedef enum {
    DATANODE_IDLE,       // spinning, waiting for player
    DATANODE_CHARGING,   // player inside, pulling to center
    DATANODE_COLLECTED,  // absorbed, play collection effect
    DATANODE_READING,    // message overlay shown, gameplay continues
    DATANODE_DONE        // invisible, already collected
} DataNodePhase;
```

**IDLE**: Disk spins. White glow pulses subtly (alpha oscillates 0.7–1.0).

**CHARGING**: Player entered trigger zone. Pull-to-center with quadratic ease-in (same pattern as portal/savepoint). Disk spin accelerates. Charge timer counts up.

**COLLECTED**: After dwell threshold (~800ms), node absorbed. Brief white flash/burst effect (expanding ring or particle scatter). Collection sound plays. Node ID marked as collected. Short delay (~300ms) for the flash to land, then transition to READING.

**READING**: Gameplay continues — enemies keep attacking, projectiles keep flying. The message overlay appears on screen showing the title and body text (see Message Overlay below). Player dismisses with any key press or mouse click. On dismiss, "Data Transfer Complete" notification fires, transition to DONE. If you want to read lore in peace, clear the area first or die trying.

**DONE**: Node no longer renders. If the player has already collected this node (loaded from save), it starts in DONE.

### Trigger zone

- `DATANODE_HALF_SIZE = 50` (same as portals/savepoints — 100x100 cell trigger area)
- Ship overlap check identical to portal pattern

### Pull-to-center

Same quadratic ease-in pattern used by portals and savepoints:

```c
double charge = (double)chargeTimer / CHARGE_THRESHOLD;
double ease = charge * charge;
ship_position.x += (node_center.x - ship_position.x) * ease * 0.1;
ship_position.y += (node_center.y - ship_position.y) * ease * 0.1;
```

### Message overlay (on-collection reading)

When the node is collected, the message is shown immediately — the player reads it in the moment, not later in a menu. This is critical for narrative pacing. The drip-feed revelation only works if players actually read the messages as they discover them.

**Layout**: Semi-transparent dark overlay covering the center of the screen. Smaller than the catalog/settings windows — sized to the content, not a fixed frame. Max width ~500px, vertically centered.

```
+------------------------------------------------+
|                                                |
|   MAINTENANCE LOG 2847-A                       |
|   ─────────────────────────                    |
|                                                |
|   Infrastructure sweep complete. All sectors   |
|   nominal. Routing tables updated.             |
|                                                |
|   Note: background noise levels in sector 7    |
|   remain elevated. Filed report #4471 for      |
|   review.                                      |
|                                                |
|                        [any key to continue]   |
|                                                |
+------------------------------------------------+
```

- **Title**: white, slightly larger or bolder (uppercase). Separator line below.
- **Body**: slightly dimmer white (0.8 alpha), word-wrapped to overlay width.
- **Dismiss prompt**: small, dim text at bottom-right. Fades in after ~500ms delay so the player doesn't accidentally skip past the message by mashing buttons during the collection flash.
- **Background**: dark semi-transparent quad (black at ~0.75 alpha) with thin white border.
- **Scrollbar**: If the body text exceeds the overlay's max height, a scrollbar appears on the right edge (same style as Data Logs — thin track, proportional thumb, mouse wheel support). The overlay has a max height (~60% of screen height) so longer messages scroll rather than filling the entire screen.
- **Gameplay is NOT paused** during READING state — enemies keep running, the world keeps ticking. Clear the area before collecting if you want to read in peace.
- **Boss automated transfers** use the same overlay. The `[FORCED EXTRACTION]` style titles with amber/gold color make these feel involuntary and urgent.

### Post-dismiss notification

After the player dismisses the message overlay:
```
>> Data Transfer Complete <<
```
White text, same style/position/duration as savepoint notification (center screen, 30% down, 3000ms with 1000ms fade). This confirms the message has been logged and can be re-read later.

### Collection sound

A new sound effect — something like a digital chirp/download tone. Short (~500ms). Plays at the moment of collection (COLLECTED state), before the overlay appears. Loaded/played in `data_node.c` following the existing audio ownership pattern.

### Static limits

```c
#define DATANODE_COUNT 16  // max per zone (10 placed + headroom)
```

Static arrays: `DataNodeState nodes[DATANODE_COUNT]`, `PlaceableComponent placeables[DATANODE_COUNT]`, etc.

### Key APIs

```c
void DataNode_initialize(Position position, const char *node_id);
void DataNode_cleanup(void);
void DataNode_update_all(unsigned int ticks);  // called from mode_gameplay
void DataNode_render_all(void);                // world pass — spinning disks
void DataNode_render_bloom_source(void);
void DataNode_render_notification(Screen screen);  // "Data Transfer Complete" notification
void DataNode_render_overlay(Screen screen);   // message reading overlay (UI pass)
bool DataNode_is_reading(void);                // true during READING state (gameplay continues)
bool DataNode_dismiss_reading(void);           // any key/click dismisses — returns true if was reading

// Boss-triggered automated transfers (no physical node)
void DataNode_trigger_transfer(const char *node_id);
```

## Collection Persistence

### In-memory state

```c
#define MAX_COLLECTED_NODES 256

static char collectedIds[MAX_COLLECTED_NODES][32];
static int collectedCount = 0;
```

`DataNode_is_collected(const char *node_id)` checks if a node ID has been collected. Called during `DataNode_initialize` to start already-collected nodes in DONE state.

### Save file integration

Collected node IDs are written to the checkpoint save file (same `./save/checkpoint.sav` used by savepoints) **when the player saves at a savepoint**. Data log progress is treated like everything else — unlocks, fragments, skillbar layout. If you collect 3 nodes then die before reaching a savepoint, those 3 nodes are lost and must be re-collected.

Add a section to the checkpoint file:

```
datanode node_origin_001
datanode node_origin_002
datanode node_crucible_boss_enter
```

One line per collected node. Loaded on checkpoint restore.

## Data Logs Window (Review Journal)

Messages are read on collection via the overlay. The Data Logs window is the **review journal** — a place to re-read previously collected messages, piece together the narrative, and catch anything the player might have skimmed past in the heat of gameplay.

### New files: `data_logs.c/h`

### Key binding

- **L key** opens/closes Data Logs window
- **FPS counter moves to \\ key** (backslash)

### Window layout

Same dimensions and centering as catalog: **600px wide x 400px tall**, centered on screen. Dark background with border, consistent with catalog and settings windows.

```
+----------------------------------------------------------+
| DATA LOGS                                            [X] |
+----------+-----------------------------------------------+
|          |                                               |
| The      |  > Maintenance Log 2847-A                    |
| Origin   |                                               |
|          |  v Personnel Transfer Notice                  |
| The      |    INTERNAL MEMO — Dr. Elise Varga            |
| Crucible |    reassigned from Neural Interface           |
|          |    Division to Special Projects...            |
| The      |                                               |
| Archive  |  > Anomalous Signal Report                    |
|          |                                               |
| (more)   |  > Sector 7 Audit Summary                     |
|          |                                               |
+----------+-----------------------------------------------+
```

### Tab bar (left side)

- Vertical tab stack, same style as catalog (140px wide, 35px tall, 5px gap)
- One tab per zone that has at least one collected message
- Tabs appear in narrative order (Origin first, themed zones in discovery order, Hexarchy last)
- Tabs only appear once the player has collected at least one node from that zone
- Selected tab highlighted with white/cyan border (matching data node color theme)
- No notification dots needed (unlike catalog — there's no "unseen" state that matters for lore)

### Content area (right side): Accordion

The content area shows all collected messages for the selected zone tab as an **accordion list**.

Each message entry:

```
+-------------------------------------------+
| > Maintenance Log 2847-A                  |  ← collapsed (default)
+-------------------------------------------+
```

```
+-------------------------------------------+
| v Personnel Transfer Notice               |  ← expanded
|                                           |
|   INTERNAL MEMO — Dr. Elise Varga         |
|   reassigned from Neural Interface        |
|   Division to Special Projects.           |
|   Effective immediately. All access       |
|   credentials revoked pending new         |
|   clearance level.                        |
|                                           |
+-------------------------------------------+
```

**Collapsed state**: Shows `>` indicator + title text. Single row height (~30px). Click anywhere on the row to expand.

**Expanded state**: Shows `v` indicator + title text + body content below. Body text word-wrapped to content width. Variable height based on message length. Click title row to collapse.

**Only one message expanded at a time**: Expanding a message auto-collapses the previously expanded one. This prevents overwhelming scroll depth and keeps focus on one piece of lore at a time.

**Scroll**: If the list of titles (plus one expanded body) exceeds the content area height, the list scrolls vertically. Mouse wheel scrolling within the content area.

**Scrollbar**: A vertical scrollbar renders on the right edge of the content area when content overflows. Thin track (~8px wide), with a draggable thumb whose height is proportional to visible/total content ratio. Thumb is white at ~0.4 alpha, brightens on hover/drag. Click-drag to scroll, or click the track to jump. The scrollbar provides visual feedback for how much content exists and where the player is in the list — important when a zone has 8-10 messages.

### Message ordering

Messages within a zone tab appear in the order they were collected (chronological discovery order). This preserves the player's personal narrative journey — early nodes they found first appear at the top.

### Color scheme

- Window background: dark grey/blue (consistent with catalog/settings)
- Tab text: white
- Selected tab: cyan/white highlight border
- Title text (collapsed): white, `>` indicator in cyan
- Title text (expanded): white, `v` indicator in cyan
- Body text: slightly dimmer white (0.7 alpha) for comfortable reading
- Automated transfer titles: distinct formatting — `[FORCED EXTRACTION]` style brackets rendered in a warning color (amber/gold) to distinguish involuntary data dumps from discovered nodes

### Empty state

If the player opens Data Logs before collecting any nodes:

```
+----------------------------------------------------------+
| DATA LOGS                                            [X] |
+----------------------------------------------------------+
|                                                          |
|           No data transfers recorded.                    |
|                                                          |
+----------------------------------------------------------+
```

Centered text, dimmed white.

### Two-pass rendering

Same pattern as catalog: batch geometry first (background quads, borders, tab highlights, accordion row backgrounds), flush, then render text.

### Key APIs

```c
void DataLogs_initialize(void);
void DataLogs_cleanup(void);
void DataLogs_toggle(void);       // L key
bool DataLogs_is_open(void);
void DataLogs_update(Input *input, unsigned int ticks);  // click handling, scroll
void DataLogs_render(Screen screen);

// Called when a node is collected — adds to the log
void DataLogs_add_entry(const char *node_id);

// Persistence
void DataLogs_save(FILE *f);      // write collected state to checkpoint
void DataLogs_load(FILE *f);      // restore from checkpoint
```

## Zone System Integration

### zone.h additions

Add to `LandmarkDef`:

```c
typedef struct {
    char node_id[32];
} LandmarkDataNodeWiring;

// In LandmarkDef:
LandmarkDataNodeWiring datanodes[LANDMARK_MAX_DATANODES];  // LANDMARK_MAX_DATANODES = 2
int datanode_count;
```

Add zone-level boss triggers:

```c
// In Zone struct:
char boss_enter_node_id[32];   // "" if none
char boss_defeat_node_id[32];  // "" if none
```

### zone.c parsing

New line types:
- `landmark_datanode <type> <node_id>` — wires a data node to a landmark
- `datanode_on_boss_enter <node_id>` — zone-level boss arena trigger
- `datanode_on_boss_defeat <node_id>` — zone-level boss defeat trigger

### Procgen integration

Data node landmarks use the existing landmark/hotspot placement system. They need their own chunk file (`resources/chunks/data_node_001.chunk`) that defines the small geometry footprint around the node — maybe a subtle raised platform or ring pattern on the ground to draw the eye.

The landmark influence should be `moderate` with a small radius — data nodes shouldn't dominate terrain generation. They sit in clearings, not on mountains.

## Message File Loading

### New files: `narrative.c/h`

Responsible for loading and querying `resources/data/messages.dat`.

```c
typedef struct {
    char node_id[32];
    char zone_name[64];
    char title[128];
    char body[2048];    // generous for lore text
} NarrativeEntry;

#define MAX_NARRATIVE_ENTRIES 256

void Narrative_load(const char *path);
void Narrative_cleanup(void);
const NarrativeEntry *Narrative_get(const char *node_id);
int Narrative_count(void);
```

Loaded once at game startup. Entries are static array, keyed by `node_id`. When a data node is collected, `Narrative_get(node_id)` retrieves the message content for the Data Logs window.

## Input Changes

### mode_gameplay.c

| Key | Current | New |
|-----|---------|-----|
| L | FPS counter toggle | Data Logs toggle |
| \\ | (unbound) | FPS counter toggle |

### Key handling additions

```c
if (input->keyL)
    DataLogs_toggle();

if (input->keyBackslash)
    fpsVisible = !fpsVisible;
```

Data Logs window does **not** pause gameplay. No windows do. The game takes no prisoners — clear the area before you start reading lore.

## Render Pipeline Integration

Data nodes render during the world pass alongside other entities. Bloom sources render into the foreground bloom FBO. The notification renders during the UI pass.

```
World pass:
  ... existing entity renders ...
  DataNode_render_all()

Bloom pass:
  ... existing bloom sources ...
  DataNode_render_bloom_source()

UI pass:
  ... existing HUD ...
  DataNode_render_notification(screen)
  if (DataNode_is_reading())
      DataNode_render_overlay(screen)    // on-collection message display
  if (DataLogs_is_open())
      DataLogs_render(screen)            // review journal
```

### No gameplay pause

No windows pause gameplay — not Data Logs, not the collection overlay, not catalog, not settings. The world keeps running. Any key/click calls `DataNode_dismiss_reading()` to close the overlay. The Data Logs window exists so the player can re-read anything they had to dismiss under fire.

## Minimap

Data nodes appear on the minimap as colored dots:

- **Uncollected**: yellow dot — draws the eye, signals "something here worth finding"
- **Collected**: grey dot — still visible for spatial reference, but clearly already obtained

```c
void DataNode_render_minimap(float center_x, float center_y,
                              float screen_x, float screen_y, float size, float range);
```

Called during minimap rendering alongside other minimap elements (portals, savepoints). Iterates all nodes in the current zone, converts world position to minimap position, renders a point with the appropriate color based on collected state.

## Files to Create/Modify

| File | Change |
|------|--------|
| `src/data_node.c/h` | **New** — data node entity (spinning disk, pull-to-center, collection) |
| `src/data_logs.c/h` | **New** — Data Logs window (tabbed accordion UI) |
| `src/narrative.c/h` | **New** — message file loader and query |
| `resources/data/messages.dat` | **New** — all narrative message content |
| `resources/chunks/data_node_001.chunk` | **New** — landmark chunk geometry for data nodes |
| `src/zone.h` | Add `LandmarkDataNodeWiring`, boss trigger node IDs |
| `src/zone.c` | Parse `landmark_datanode`, `datanode_on_boss_enter/defeat` |
| `src/savepoint.c` | Add datanode collection to checkpoint save/load |
| `src/mode_gameplay.c` | L key → DataLogs, \\ key → FPS, update/render calls |
| `src/input.h` | Add `keyBackslash` if not already present |
| `src/graphics.c` | DataNode bloom source call in bloom capture pass |

## Open Questions

1. **Node chunk geometry**: What should the ground marker look like around a data node? Subtle ring? Raised platform? Circuit trace pattern converging on the node position? Needs visual prototyping.

2. **Collection effect**: The brief flash on collection — expanding white ring? Particle scatter? Both? Should it be more dramatic for boss-triggered automated transfers?

3. **Sound design**: The collection chirp/download tone. Should boss-triggered transfers use a different, more ominous sound?

4. **Hexarchy message count**: Depends on final zone structure. The system supports up to 256 total messages which is plenty of headroom.

5. **Read/unread tracking**: Should the accordion show which messages are new (unread) vs already read? A subtle dot or highlight on unread titles could nudge the player to check new lore without being intrusive. Could also drive a notification dot on the L-key HUD indicator.

6. **Message body formatting**: Should the body support any markup beyond `\n` line breaks? Bold text for emphasis? Color changes for different "voices" (corporate vs AI vs personal)? Or keep it plain text for simplicity?

7. **Data Logs availability outside gameplay**: Should the Data Logs window be accessible from the title screen / pause menu? Players who want to re-read lore between sessions would benefit from this.

8. ~~**Minimap indicator**~~: Decided — yellow dot uncollected, grey dot collected (see Minimap section).
