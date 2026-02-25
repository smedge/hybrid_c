# Spec: Hexagonal Grid System (The Hexarchy)

## Overview

After defeating all six superintelligences, a final portal opens to **The Hexarchy** — an alien realm where the fundamental grid geometry shifts from square to hexagonal. The player has spent the entire game on a square grid. Entering the Hexarchy and seeing the world's geometry change communicates "alien" at the deepest possible level — not through color palette or enemy design, but through the mathematical structure of space itself.

This is a **parallel system**, not a replacement. The square grid (`map.c/h`) stays for the entire game. The hex grid (`hex_map.c/h`) is Hexarchy-only. Both use the same `MapCell` data structure, the same bloom pipeline, the same rendering backend, the same entity/ECS layer. The hex grid is different geometry fed into the same pipeline.

## What Changes

| System | Change |
|--------|--------|
| Map storage + lookup | New `hex_map.c/h` module — hex grid storage, coordinate math, cell access |
| Cell rendering | Hexagonal fill + outlines instead of square quads |
| Collision | Hex-edge testing (6 edges) instead of square (4 edges) |
| Stencil mask | Hex polygons instead of quads for cloud reflection + weapon lighting |
| Zone file format | `gridtype hex` header flag triggers hex map loading |
| Godmode editor | Hex-aware pixel-to-hex coordinate mapping |
| Minimap / world map | Hex cell rendering on radar and M-key overlay |
| Procgen | Hex noise sampling, 6-neighbor connectivity |

## What Stays the Same

This is the big list. The hex grid is a map-layer change. Everything above the map is untouched:

- **Entity/ECS layer** — entities move in world-space coordinates regardless of grid shape. `PlaceableComponent`, `CollidableComponent`, `RenderableComponent` unchanged.
- **All subroutines** — sub_pea, sub_mine, sub_egress, sub_boost, sub_mend, sub_aegis, sub_disintegrate, sub_inferno, sub_gravwell, sub_stealth. They fire projectiles, deal damage, move the player — all in world-space.
- **All enemy AI** — hunters, seekers, defenders, stalkers, mines. `Enemy_move_toward`, `Enemy_move_away_from` work in world-space. Pathfinding uses world-space LOS checks. AI doesn't know or care about grid shape.
- **Batch renderer** — `Render_*` API accepts triangles, lines, points in world-space. Hex cells are just different vertex positions fed into the same batches.
- **Shader programs** — color shader and text shader are geometry-agnostic. No changes.
- **Three render passes** — world, bloom, UI. Same pipeline.
- **All four bloom instances** — foreground, background, disintegrate, weapon lighting. Bloom operates on FBOs — shape of the source geometry is irrelevant.
- **Cloud reflection** (`map_reflect.c/h`) — stencil-tested fullscreen quad sampling `bg_bloom->pong_tex`. The stencil mask changes shape (hex instead of square), but the composite shader and reflection math are identical.
- **Weapon lighting** (`map_lighting.c/h`) — same situation. Stencil mask shape changes, composite is identical.
- **Skillbar, catalog, progression** — UI systems. No map dependency.
- **Player stats** — integrity, feedback, shield, i-frames. No map dependency.
- **Audio** — sound loading, playback, spatial audio. No map dependency.
- **Settings** — graphics toggles, persistence. No map dependency.
- **Particle instance renderer** — GPU instanced quads. No map dependency.

## Hex Coordinate System

### Choice: Axial coordinates (with cube for algorithms)

Use **axial coordinates** (q, r) for storage and everyday use, with conversion to **cube coordinates** (q, r, s where q + r + s = 0) for algorithms that need it (distance, rotation, ring iteration). This is the standard recommendation from Red Blob Games' hex grid reference and the choice used by virtually every hex game in production.

**Why not offset coordinates:** Offset coords (odd-q / even-q) create asymmetry — odd and even columns have different neighbor offsets. Every algorithm needs `if (col % 2)` branches. Axial coordinates are symmetric. All six neighbors are computed with the same offset table regardless of position.

```c
typedef struct {
    int q;  // column axis
    int r;  // row axis (diagonal)
} HexCoord;

typedef struct {
    int q;
    int r;
    int s;  // s = -q - r (redundant but useful for algorithms)
} CubeCoord;
```

### Cube conversion

```c
static inline CubeCoord hex_to_cube(HexCoord h) {
    return (CubeCoord){ h.q, h.r, -h.q - h.r };
}

static inline HexCoord cube_to_hex(CubeCoord c) {
    return (HexCoord){ c.q, c.r };
}
```

### Neighbor offsets (axial)

Six neighbors, constant offsets regardless of position:

```c
static const HexCoord hex_directions[6] = {
    { +1,  0 }, { +1, -1 }, {  0, -1 },  // E, NE, NW
    { -1,  0 }, { -1, +1 }, {  0, +1 }   // W, SW, SE
};

static inline HexCoord hex_neighbor(HexCoord h, int direction) {
    return (HexCoord){ h.q + hex_directions[direction].q,
                       h.r + hex_directions[direction].r };
}
```

### Hex distance

Manhattan distance in cube coordinates, divided by 2:

```c
static inline int hex_distance(HexCoord a, HexCoord b) {
    CubeCoord ac = hex_to_cube(a);
    CubeCoord bc = hex_to_cube(b);
    return (abs(ac.q - bc.q) + abs(ac.r - bc.r) + abs(ac.s - bc.s)) / 2;
}
```

## Hex Cell Geometry and Sizing

### Orientation: Flat-top

Flat-top hexagons (flat edge on top and bottom, pointy sides left and right). This gives horizontal rows that feel natural for side-scrolling-adjacent 2D navigation and aligns with the existing camera orientation.

```
   ____
  /    \
 /      \
 \      /
  \____/
```

### Size

The hex `size` parameter is the distance from center to vertex (the circumradius). To maintain comparable gameplay density to the current 100.0-unit square cells:

- Square cell: 100.0 x 100.0 = 10,000 sq units
- Hex area: `(3 * sqrt(3) / 2) * size^2`
- For equivalent area: `size = sqrt(10000 / (3 * sqrt(3) / 2))` ~ **62.0**

Round to **HEX_SIZE = 60.0** for clean math. This gives:
- Hex width (flat-top): `2 * size = 120.0`
- Hex height: `sqrt(3) * size ~ 103.9`
- Hex area: ~9,353 sq units (close to the 10,000 of a square cell)

```c
#define HEX_SIZE 60.0
#define HEX_WIDTH (2.0 * HEX_SIZE)                    // 120.0
#define HEX_HEIGHT (1.7320508075688772 * HEX_SIZE)     // ~103.9 (sqrt(3) * size)
```

### Grid dimensions

For a hex map comparable to the 1024x1024 square map:

- Flat-top hex column spacing: `3/2 * size = 90.0`
- Flat-top hex row spacing: `sqrt(3) * size ~ 103.9`
- To cover ~102,400 world units (1024 * 100): `102400 / 90 ~ 1138` columns, `102400 / 103.9 ~ 985` rows

Use **HEX_MAP_COLS = 1024** and **HEX_MAP_ROWS = 1024** for consistency with the square map's sizing conventions. The actual world-space footprint will be slightly different (~92,160 x ~106,394) but comparable.

```c
#define HEX_MAP_COLS 1024
#define HEX_MAP_ROWS 1024
#define HEX_HALF_COLS (HEX_MAP_COLS / 2)
#define HEX_HALF_ROWS (HEX_MAP_ROWS / 2)
```

## Coordinate Conversions

### Axial hex to world-space (center of hex)

Flat-top layout:

```c
static inline double hex_to_world_x(int q, int r) {
    return (q - HEX_HALF_COLS) * (1.5 * HEX_SIZE);
}

static inline double hex_to_world_y(int q, int r) {
    return (r - HEX_HALF_ROWS) * (SQRT3 * HEX_SIZE)
         + (q - HEX_HALF_COLS) * (SQRT3_OVER_2 * HEX_SIZE);
}
```

The `q` offset in the Y formula is the axial coordinate's diagonal: even and odd columns are staggered.

### World-space to axial hex (pixel to hex)

The inverse — given a world-space point, find which hex it's in. This is the critical function for godmode editing, collision queries, and any world-to-grid lookup.

**Algorithm**: Convert world-space to fractional axial coordinates, then round to the nearest hex using cube rounding:

```c
HexCoord hex_from_world(double wx, double wy) {
    // Invert the hex_to_world transform to get fractional axial coords
    double fq = (wx * 2.0/3.0) / HEX_SIZE + HEX_HALF_COLS;
    double fr = ((-wx / 3.0) + (wy * SQRT3 / 3.0)) / HEX_SIZE + HEX_HALF_ROWS;

    // Cube round to nearest hex
    return hex_round(fq, fr);
}

HexCoord hex_round(double fq, double fr) {
    double fs = -fq - fr;
    int q = (int)round(fq);
    int r = (int)round(fr);
    int s = (int)round(fs);

    double q_diff = fabs(q - fq);
    double r_diff = fabs(r - fr);
    double s_diff = fabs(s - fs);

    // Enforce q + r + s = 0 by resetting the component with largest rounding error
    if (q_diff > r_diff && q_diff > s_diff)
        q = -r - s;
    else if (r_diff > s_diff)
        r = -q - s;
    // else s = -q - r (implicit, we don't store s)

    return (HexCoord){ q, r };
}
```

### Hex vertex positions (flat-top)

Six vertices at 60-degree intervals, starting from the right:

```c
void hex_vertices(double cx, double cy, double vx[6], double vy[6]) {
    for (int i = 0; i < 6; i++) {
        double angle_deg = 60.0 * i;
        double angle_rad = angle_deg * M_PI / 180.0;
        vx[i] = cx + HEX_SIZE * cos(angle_rad);
        vy[i] = cy + HEX_SIZE * sin(angle_rad);
    }
}
```

Vertex order (flat-top, counter-clockwise from right):
- 0: right (0 degrees)
- 1: upper-right (60 degrees)
- 2: upper-left (120 degrees)
- 3: left (180 degrees)
- 4: lower-left (240 degrees)
- 5: lower-right (300 degrees)

## Module Design: `hex_map.c/h`

### Storage

Parallel to `map.c`'s `MapCell *map[MAP_SIZE][MAP_SIZE]`:

```c
static MapCell *hex_grid[HEX_MAP_COLS][HEX_MAP_ROWS];
static MapCell hexEmptyCell = { true, false, {0,0,0,0}, {0,0,0,0} };
static MapCell hexBoundaryCell = { true, false, {0,0,0,0}, {0,0,0,0} };

static MapCell hexCellPool[HEX_MAP_COLS * HEX_MAP_ROWS];
static int hexCellPoolCount = 0;
```

Same `MapCell` struct — colors, circuit pattern, empty flag. No changes to the data model.

### API (mirrors map.h)

```c
// hex_map.h
void HexMap_initialize(void);
void HexMap_clear(void);
void HexMap_set_cell(int q, int r, const MapCell *cell);
void HexMap_clear_cell(int q, int r);
const MapCell *HexMap_get_cell(int q, int r);
void HexMap_set_boundary_cell(const MapCell *cell);
void HexMap_clear_boundary_cell(void);

// Coordinate conversion
HexCoord HexMap_world_to_hex(double wx, double wy);
void HexMap_hex_to_world(int q, int r, double *wx, double *wy);

// Collision
Collision HexMap_collide(void *state, const PlaceableComponent *placeable,
                         const Rectangle boundingBox);
bool HexMap_line_test_hit(double x0, double y0, double x1, double y1,
                          double *hit_x, double *hit_y);

// Rendering
void HexMap_render(const void *state, const PlaceableComponent *placeable);
void HexMap_render_bloom_source(void);
void HexMap_render_stencil_mask_all(const Mat4 *proj, const Mat4 *view_mat);
void HexMap_render_minimap(float center_x, float center_y,
                           float screen_x, float screen_y, float size, float range);

// Query
bool HexMap_cell_is_solid(int q, int r);
void HexMap_hex_neighbors(int q, int r, HexCoord out[6]);

// Circuit patterns
void HexMap_set_circuit_traces(bool enabled);
bool HexMap_get_circuit_traces(void);
```

### Active map pointer

A global flag or function pointer determines which map system is active for the current zone. This avoids `if (hex_mode)` scattered through every caller:

```c
// map_dispatch.h — thin dispatch layer
typedef enum { GRID_SQUARE, GRID_HEX } GridType;

GridType Map_get_grid_type(void);
void Map_set_grid_type(GridType type);
```

Callers that need to know grid type (collision system, zone loader, godmode editor) check this flag. Most callers don't need it — entities work in world-space and never touch the map directly.

The ECS entity system already calls `Map_collide` through a function pointer on `CollidableComponent`. For hex zones, the zone loader sets this function pointer to `HexMap_collide` instead of `Map_collide`. The entity system doesn't know or care.

Similarly, `Map_render` is called as a `RenderableComponent` function pointer. For hex zones, it points to `HexMap_render`.

## Rendering

### Hex cell fill

Each non-empty hex cell renders as a filled hexagon — 4 triangles (fan from center) using `Render_triangle`:

```c
void hex_render_fill(double cx, double cy, const MapCell *cell) {
    ColorFloat c = Color_rgb_to_float(&cell->primaryColor);
    double vx[6], vy[6];
    hex_vertices(cx, cy, vx, vy);

    for (int i = 0; i < 6; i++) {
        int next = (i + 1) % 6;
        Render_triangle(cx, cy, vx[i], vy[i], vx[next], vy[next],
                        c.red, c.green, c.blue, c.alpha);
    }
}
```

6 triangles per hex cell (triangle fan). At comparable cell counts to the square grid, this is 6 triangles vs 2 triangles per cell — 3x more triangles for fill. However, view frustum culling keeps the on-screen count manageable. At worst case (max zoom out), visible cells number ~2,000-3,000, so fill is ~18,000 triangles. Well within the 65,536 vertex batch limit.

### Hex outlines / borders

Outlines render on edges where a non-empty cell borders an empty cell (or a visually different cell), same logic as the square grid's `cells_match_visual` check but with 6 edges instead of 4.

Each edge is a thick line segment between two adjacent vertices:

```c
void hex_render_outline_edge(double vx0, double vy0, double vx1, double vy1,
                             const MapCell *cell, float thickness) {
    ColorFloat c = Color_rgb_to_float(&cell->outlineColor);
    Render_thick_line(vx0, vy0, vx1, vy1, thickness,
                      c.red, c.green, c.blue, c.alpha);
}
```

For each hex cell, check all 6 neighbors. If the neighbor is empty or visually different, render that edge's outline. This produces clean cell boundaries where hex terrain meets open space.

### Circuit patterns adapted to hex

The current square circuit patterns are axis-aligned trace lines within the cell. For hex cells, circuit patterns follow the hex geometry:

- **Three axes** instead of two: traces run along the three natural hex axes (0/120/240 degrees for flat-top)
- **Center hub**: traces radiate from cell center toward edge midpoints
- **Adjacency-aware**: traces extend toward edges that connect to other circuit cells, creating continuous circuit networks across hex terrain

The `circuitPattern` bool on `MapCell` stays — it just triggers hex-shaped trace rendering instead of square. The circuit atlas pre-rendering (`Map_render_circuit_pattern_for_atlas`) gets a hex variant with 6 adjacency flags instead of 4.

### Bloom source rendering

`HexMap_render_bloom_source()` re-renders hex cell fills into the bloom FBO, identical to `Map_render_bloom_source()` but with hex geometry. The bloom pipeline doesn't care about shape — it blurs whatever pixels are in the FBO.

## Collision

### Hex cell solidity test

Given a world-space point, determine if it's inside a solid hex cell:

```c
bool HexMap_point_is_solid(double wx, double wy) {
    HexCoord h = HexMap_world_to_hex(wx, wy);
    return HexMap_cell_is_solid(h.q, h.r);
}
```

This is the hex equivalent of checking `map[x][y]->empty`.

### Entity AABB vs hex cells

`HexMap_collide` receives an entity's AABB (axis-aligned bounding box) and tests it against nearby hex cells. The approach:

1. Convert AABB corners to hex coordinates to find the range of hex cells to test
2. For each candidate hex cell that's non-empty, test AABB vs hex polygon overlap

For the overlap test, there are two options:

**Option A — AABB vs hex polygon (SAT)**: Separating Axis Theorem with the AABB's 2 axes + the hex's 3 unique edge normals = 5 axes to test. Precise but more math per cell.

**Option B — AABB vs hex bounding circle + point-in-hex refinement**: Quick reject with bounding circle, then test AABB corners against hex polygon. Faster for most cases (circle reject is cheap), slightly imprecise at hex corners.

**Recommendation**: Start with Option A (SAT). Entity bounding boxes are small relative to hex cells, so the candidate cell count per query is low (typically 1-4 cells). The per-cell cost of SAT is negligible at that count. If profiling shows collision as a bottleneck, switch to Option B.

```c
Collision HexMap_collide(void *state, const PlaceableComponent *placeable,
                         const Rectangle bbox) {
    // Find candidate hex cells that could overlap the AABB
    // Sample the AABB corners + center to get candidate hex coordinates
    HexCoord candidates[16];
    int count = hex_cells_overlapping_aabb(bbox, candidates, 16);

    Collision collision = { false, true };
    for (int i = 0; i < count; i++) {
        if (!HexMap_cell_is_solid(candidates[i].q, candidates[i].r))
            continue;

        double cx, cy;
        HexMap_hex_to_world(candidates[i].q, candidates[i].r, &cx, &cy);
        if (hex_aabb_overlap(cx, cy, bbox)) {
            collision.collisionDetected = true;
            return collision;
        }
    }
    return collision;
}
```

### Line-of-sight / projectile line test

`HexMap_line_test_hit` is the hex equivalent of `Map_line_test_hit`. Given a line segment, find the first solid hex cell it intersects.

**Algorithm**: Hex line drawing. Walk the line through hex cells using the standard hex line-draw algorithm (lerp between start and end in cube coordinates, round each sample to the nearest hex):

```c
bool HexMap_line_test_hit(double x0, double y0, double x1, double y1,
                          double *hit_x, double *hit_y) {
    HexCoord h0 = HexMap_world_to_hex(x0, y0);
    HexCoord h1 = HexMap_world_to_hex(x1, y1);
    int dist = hex_distance(h0, h1);

    if (dist == 0) {
        if (HexMap_cell_is_solid(h0.q, h0.r)) {
            *hit_x = x0;
            *hit_y = y0;
            return true;
        }
        return false;
    }

    // Walk the line in fractional cube coordinates
    for (int i = 0; i <= dist; i++) {
        double t = (double)i / dist;
        // Lerp in cube space, round to nearest hex
        HexCoord h = hex_lerp_round(h0, h1, t);

        if (HexMap_cell_is_solid(h.q, h.r)) {
            // Compute exact intersection point with hex edges
            double cx, cy;
            HexMap_hex_to_world(h.q, h.r, &cx, &cy);
            double vx[6], vy[6];
            hex_vertices(cx, cy, vx, vy);

            // Test line against all 6 hex edges, find earliest hit
            double best_t = 2.0;
            for (int e = 0; e < 6; e++) {
                int next = (e + 1) % 6;
                double t_hit;
                if (line_segment_intersect(x0, y0, x1, y1,
                                           vx[e], vy[e], vx[next], vy[next],
                                           &t_hit)) {
                    if (t_hit < best_t) best_t = t_hit;
                }
            }
            if (best_t <= 1.0) {
                *hit_x = x0 + (x1 - x0) * best_t;
                *hit_y = y0 + (y1 - y0) * best_t;
                return true;
            }
        }
    }
    return false;
}
```

This is slightly more expensive than the square grid's AABB line test (6 edge tests per hex vs 4 for a square, plus the hex walk overhead), but line tests are already cheap per-call. The hex walk visits fewer cells than the square grid's rectangular sweep for diagonal lines, partially offsetting the per-cell cost.

## Stencil Mask Rendering

`HexMap_render_stencil_mask_all` writes stencil values for cloud reflection and weapon lighting. Same two-value scheme as the square grid:

- Circuit hex cells write stencil value 1
- Solid hex cells write stencil value 2
- Reflection tests `== 2` (solid only)
- Lighting tests `>= 1` (all cells)

The only change is rendering hexagons instead of quads into the stencil buffer:

```c
void HexMap_render_stencil_mask_all(const Mat4 *proj, const Mat4 *view_mat) {
    // Same stencil setup as Map_render_stencil_mask_all
    glEnable(GL_STENCIL_TEST);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

    // Circuit cells: stencil = 1
    glStencilFunc(GL_ALWAYS, 1, 0xFF);
    for each visible circuit hex cell:
        render hex polygon to stencil

    // Solid cells: stencil = 2
    glStencilFunc(GL_ALWAYS, 2, 0xFF);
    for each visible solid hex cell:
        render hex polygon to stencil

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glStencilMask(0xFF);  // MUST restore or glClear can't clear stencil
}
```

The composite shaders for cloud reflection and weapon lighting don't change at all — they sample FBO textures and test stencil values. The stencil mask's shape is hexagonal, but the shaders only see "stencil passes" or "stencil fails."

## Zone System Integration

### `gridtype` field

Zone files gain an optional `gridtype` line in the header:

```
name The Hexarchy
size 1024
gridtype hex
seed 99999
```

If `gridtype` is absent or `square`, the zone uses the existing square `map.c`. If `gridtype hex`, the zone loader initializes `hex_map.c` and routes all map operations through it.

### Zone struct addition

```c
// In Zone struct (zone.h)
typedef enum { ZONE_GRID_SQUARE, ZONE_GRID_HEX } ZoneGridType;

// Add to Zone struct:
ZoneGridType grid_type;  // default ZONE_GRID_SQUARE
```

### Zone loader changes

In `Zone_load()`:

1. Parse `gridtype` line, set `zone->grid_type`
2. If hex: call `HexMap_clear()` instead of `Map_clear()`
3. Set `Map_set_grid_type(GRID_HEX)` so dispatch layer routes correctly
4. Parse `cell` lines as hex coordinates: `cell <q> <r> <type_id>` (same format, different semantic — q,r instead of x,y)
5. Call `HexMap_set_cell()` instead of `Map_set_cell()`
6. Set entity collidable function pointer to `HexMap_collide`
7. Set entity renderable function pointer to `HexMap_render`

### Zone saver changes

In `Zone_save()`, if grid type is hex, write `gridtype hex` and save cell coordinates as q,r.

### Cell lines in zone files

Square zone:
```
cell 512 512 circuit
```

Hex zone:
```
cell 512 512 circuit
```

Same format — the coordinates are just interpreted as (q, r) axial instead of (grid_x, grid_y). The zone's `gridtype` header determines interpretation.

## Godmode Editor

### Hex-aware pixel-to-hex mapping

The current editor converts mouse click position to grid coordinates: `grid_x = world_x / MAP_CELL_SIZE + HALF_MAP_SIZE`. For hex zones, this becomes `HexMap_world_to_hex(world_x, world_y)`.

The editor already has a grid-type dispatch point (click handler). Adding a hex branch:

```c
if (Map_get_grid_type() == GRID_HEX) {
    HexCoord h = HexMap_world_to_hex(world_x, world_y);
    // Place/remove cell at (h.q, h.r)
} else {
    // Existing square grid logic
}
```

### Hex cursor highlight

In godmode, the cursor highlights the cell under the mouse. For hex zones, render a hex outline instead of a square outline at the cursor position. This uses the same `hex_vertices` function to compute the highlight polygon.

### Neighbor detection for borders

The editor's border rendering logic currently checks 4 neighbors (N/S/E/W). For hex zones, check 6 neighbors using `hex_directions[]`. The `cells_match_visual` comparison function is unchanged — it's comparing `MapCell` data regardless of grid shape.

### Undo

Undo system stores `(q, r, old_cell, new_cell)` tuples. Same structure as the square grid undo — just different coordinate semantics.

## Procgen Considerations

### Hex noise sampling

The procgen noise system samples at grid coordinates. For hex grids, sample at world-space positions of hex cell centers instead of square cell centers:

```c
for each hex cell (q, r):
    double wx, wy;
    HexMap_hex_to_world(q, r, &wx, &wy);
    double noise_val = noise_at(wx * freq, wy * freq, ...);
    // Apply thresholds as before
```

Same noise function, same thresholds, same influence field. The sampling points are just hex-centered instead of square-centered. The output naturally produces hex-shaped terrain.

### Neighbor uniformity

Hex grids have a key advantage for procgen: all 6 neighbors are equidistant (center-to-center distance is constant). In a square grid, diagonal neighbors are sqrt(2) farther than cardinal neighbors, creating directional bias in generated terrain. Hex grids produce more isotropic terrain — corridors and wall formations don't favor horizontal/vertical over diagonal.

### Connectivity validation

Flood fill for connectivity checking uses 6 neighbors instead of 4 (or 8). Hex flood fill is actually cleaner than square — no ambiguity about whether diagonals count as connected.

### Corridor carving

A* pathfinding on hex grids uses 6 neighbors. Carved corridors naturally follow hex geometry — no staircase artifacts from axis-aligned pathing.

### Center anchor rotation

Hex grids support 6-fold rotation (60-degree increments) instead of 4-fold (90-degree). Combined with 2 mirrors, that's 12 possible transforms instead of 8. More variety per seed.

```c
typedef enum {
    HEX_TRANSFORM_IDENTITY,
    HEX_TRANSFORM_ROT60,
    HEX_TRANSFORM_ROT120,
    HEX_TRANSFORM_ROT180,
    HEX_TRANSFORM_ROT240,
    HEX_TRANSFORM_ROT300,
    HEX_TRANSFORM_MIRROR,
    HEX_TRANSFORM_MIRROR_ROT60,
    HEX_TRANSFORM_MIRROR_ROT120,
    HEX_TRANSFORM_MIRROR_ROT180,
    HEX_TRANSFORM_MIRROR_ROT240,
    HEX_TRANSFORM_MIRROR_ROT300,
    HEX_TRANSFORM_COUNT  // 12
} HexChunkTransform;
```

Rotation in cube coordinates is elegant — 60-degree rotation is just a cyclic permutation of (q, r, s) with sign flips.

## Spatial Grid

The spatial grid (`spatial_grid.c/h`) operates in world-space: it converts entity world positions to bucket coordinates. **Hex zones can reuse the existing spatial grid without changes.** The bucket grid is a performance partitioning layer — it doesn't need to align with the map grid.

The only consideration is bucket sizing. Current buckets are 64x64 square cells = 6,400 world units per side. For hex zones with HEX_SIZE=60:

- Column spacing: 90 world units
- Row spacing: ~104 world units
- 64 hex columns span: 64 * 90 = 5,760 world units
- 64 hex rows span: 64 * 104 = 6,656 world units

Close enough to the square grid's 6,400 that the same bucket sizing works. The active 3x3 neighborhood still covers more than enough area around the player. **No changes to spatial_grid.c/h.**

## Minimap and World Map

### Minimap (radar)

The minimap renders filled cells as colored pixels/dots. For hex zones, render each cell as a small hexagon (or a colored dot at the hex center — at minimap scale, individual hex shape is barely visible). The hex center positions are computed from `HexMap_hex_to_world`.

### World map (M key overlay)

The world map texture bake iterates all cells and writes colored pixels. For hex zones, the bake computes pixel positions from hex-to-world conversion. At world map scale (1024x1024 grid compressed to screen), individual hex shapes aren't discernible — colored dots at hex centers produce the same visual result as the square grid.

### Fog of war

Fog of war state is stored per cell. For hex zones: `static bool hex_revealed[HEX_MAP_COLS][HEX_MAP_ROWS]`. The reveal radius calculation and per-frame update use hex distance instead of rectangular range. Same concept, hex coordinates.

## Estimated Scope and Phasing

### Phase 1: Core hex map module
**Files**: `hex_map.c/h`
**Scope**: Storage, coordinate math, set/get/clear cell, world-to-hex and hex-to-world conversions, boundary cell. No rendering, no collision.
**Estimate**: Small. Mostly math.

### Phase 2: Hex rendering
**Files**: `hex_map.c` (render functions)
**Scope**: Hex cell fill, outlines with neighbor-aware border detection, circuit pattern adaptation, bloom source rendering, view frustum culling for hex cells.
**Estimate**: Medium. The rendering itself is straightforward (triangles into the batch renderer), but circuit pattern adaptation needs design iteration.

### Phase 3: Hex collision
**Files**: `hex_map.c` (collision functions)
**Scope**: `HexMap_collide` (entity AABB vs hex), `HexMap_line_test_hit` (LOS/projectile), SAT hex-AABB overlap test.
**Estimate**: Medium. SAT implementation is well-documented. Line-vs-hex walk needs care.

### Phase 4: Stencil mask + FBO integration
**Files**: `hex_map.c` (stencil functions), minor touches to `map_reflect.c` and `map_lighting.c` for dispatch
**Scope**: Hex stencil mask rendering. Cloud reflection and weapon lighting composite shaders are unchanged — only the stencil geometry changes shape.
**Estimate**: Small. The hard work was done in the square grid implementation. This is just different polygons.

### Phase 5: Zone system integration
**Files**: `zone.c/h`, `map_dispatch.c/h` (new, thin)
**Scope**: `gridtype` parsing, hex cell loading/saving, grid type dispatch, function pointer routing for collision and rendering.
**Estimate**: Small-Medium. Zone loader already has the structure — adding a branch for hex coordinates.

### Phase 6: Godmode editor
**Files**: Godmode editor files
**Scope**: Hex cursor highlight, hex-aware placement, 6-neighbor border detection, undo with hex coords.
**Estimate**: Small. The editor is already structured for grid-coordinate operations — swapping in hex coordinate math.

### Phase 7: Procgen adaptation
**Files**: `procgen.c`
**Scope**: Hex noise sampling, 6-neighbor flood fill, hex corridor carving, 12-fold center anchor transforms.
**Estimate**: Medium. The procgen algorithms are grid-shape-aware. Each needs a hex code path.

### Phase 8: Minimap / world map / fog of war
**Files**: `hud.c`, `map_window.c`, `fog_of_war.c`
**Scope**: Hex-aware minimap rendering, world map texture bake, fog reveal with hex distance.
**Estimate**: Small. At minimap/world-map scale, hex vs square is barely visible. The math changes but the visual result is similar.

### Total estimate

The hex grid system is primarily a **map-layer** change. Everything above the map (entities, AI, subroutines, bloom, UI) is untouched. The core work is in Phases 1-3 (hex map module, rendering, collision). Phases 4-8 are integration work that leverages existing infrastructure.

The system can be built incrementally. Phase 1 alone (storage + coordinate math) enables unit testing of hex math. Phase 2 adds visual feedback. Phase 3 makes it playable. Phases 4-8 bring it to feature parity with the square grid.

## Open Questions

1. **Circuit pattern design**: The square grid's circuit traces are axis-aligned horizontal/vertical lines. Hex cells have 3 natural axes (0/120/240). Should hex circuits use all three axes? Two? A different visual language entirely (e.g., honeycomb-internal patterns)? Needs visual prototyping.

2. **Background grid lines**: The square grid renders faint grid lines in empty space. Should hex zones render faint hex grid lines? This would be visually striking — the hex grid visible in empty space immediately communicates "alien geometry." Recommend yes.

3. **Hex cell size tuning**: HEX_SIZE = 60.0 is derived from area-equivalence with 100.0 square cells. Gameplay may want slightly larger or smaller hexes for feel. Tunable constant, easy to adjust.

4. **Movement feel**: Entities move in world-space (continuous), so movement feel doesn't change. But wall collision shapes are different — hex walls produce different corner-catching and wall-sliding behavior than square walls. May need movement tuning for the Hexarchy zones.

5. **Obstacle blocks for hex grids**: The square grid's obstacle blocks are small rectangular patterns. Hex obstacle blocks would be hex-shaped clusters. Need new obstacle authoring for hex geometry, or adapt the obstacle system to work with hex cells.

6. **Border rendering performance**: 6 edges per hex (vs 4 per square) means 50% more border geometry. Combined with 6 triangles per fill (vs 2), hex rendering is ~3x the triangle count of square. View frustum culling keeps this manageable, but worth profiling on large hex zones at minimum zoom.

7. **Map_line_test consumers**: Several systems call `Map_line_test_hit` directly (enemy LOS, projectile collision, etc.). These need to dispatch to `HexMap_line_test_hit` in hex zones. A dispatch function or function pointer in the map dispatch layer handles this. Audit all callers.

8. **Save file compatibility**: The save system stores player position in world coordinates (grid-agnostic). Zone transitions between square and hex zones should work seamlessly — the player's world position is just a point, regardless of what grid it's in. Verify savepoint serialization doesn't assume square grid coordinates.
