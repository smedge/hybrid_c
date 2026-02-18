# Pre-Rendered Circuit Pattern Tiles

## Problem

`render_circuit_pattern()` regenerates every visible circuit cell's traces per frame — seeded RNG, occupancy grid checks, thick_line triangles, filled_circle endpoints. With hundreds of visible circuit cells, this is the most expensive part of map rendering by a wide margin.

## Approach

Pre-render a small set of circuit pattern textures at initialization. At render time, select a pattern per cell via spatial hash, apply rotation/mirroring for variety, and draw as a textured quad. Chamfering works automatically through geometry clipping.

## Edge Connectivity

The current system doesn't have true cross-cell trace continuity — that was attempted but never produced good-looking results. What it DOES have is traces extending all the way to the cell edge when a **solid (non-circuit) neighbor** exists on that side. This creates the illusion that traces run underneath the adjacent solid block — a subtle but important visual cue that the circuit board continues behind walls.

Pre-baked patterns must preserve this behavior. This means patterns are categorized by **which edges have solid neighbors** (i.e., which edges traces are allowed to extend to).

### Edge Connectivity Classes

Each cell has 4 edges (N, E, S, W). Each edge is either "open" (traces can extend to it — solid neighbor present) or "closed" (traces stay inset — empty or circuit neighbor). This gives 2^4 = 16 connectivity classes:

| Class | Connected Edges | Count | Description |
|-------|----------------|-------|-------------|
| 0 | none | 1 | Island — all traces stay inset |
| 1 | N, E, S, W (one at a time) | 4 | One-edge — traces reach one side |
| 2 | NE, NS, NW, ES, EW, SW | 6 | Two-edge — traces reach two sides |
| 3 | NES, NEW, NSW, ESW | 4 | Three-edge — traces reach three sides |
| 4 | NESW | 1 | Surrounded — traces reach all sides |

**Rotation/mirror reduces the unique classes:**
- 0-edge: 1 class (no rotation needed)
- 1-edge: 1 class × 4 rotations = covers all 4 single-edge variants
- 2-edge adjacent (NE): 1 class × 4 rotations = covers NE, ES, SW, NW
- 2-edge opposite (NS): 1 class × 2 rotations = covers NS, EW
- 3-edge: 1 class × 4 rotations = covers all 4 three-edge variants
- 4-edge: 1 class (no rotation needed)

**= 6 unique connectivity shapes**, each needing its own set of base patterns.

### Pattern Counts Per Connectivity Class

Generate multiple base patterns per connectivity class for variety:

| Connectivity | Base Patterns | × Rotations | × Mirror | Total Variants |
|-------------|---------------|-------------|----------|----------------|
| 0-edge (island) | 4 | 4 | 2 | 32 |
| 1-edge | 4 | 4 | 2 | 32 per edge = 128 |
| 2-edge adjacent | 3 | 4 | 2 | 24 per pair = 96 |
| 2-edge opposite | 3 | 2 | 2 | 12 per pair = 24 |
| 3-edge | 3 | 4 | 2 | 24 per combo = 96 |
| 4-edge (surrounded) | 3 | 4 | 2 | 24 |
| **Total** | **20** | | | **400** |

20 base patterns, 400 effective variants. More than enough visual variety.

### Selection Logic at Render Time

```c
// Determine which edges connect to solid neighbors
int conn_n = !nPtr->empty && !nPtr->circuitPattern;
int conn_e = !ePtr->empty && !ePtr->circuitPattern;
int conn_s = !sPtr->empty && !sPtr->circuitPattern;
int conn_w = !wPtr->empty && !wPtr->circuitPattern;

// Map to connectivity class + required rotation
int conn_class, base_rotation;
classify_connectivity(conn_n, conn_e, conn_s, conn_w,
    &conn_class, &base_rotation);

// Select pattern within class, apply additional random rotation/mirror
unsigned int hash = (unsigned int)(x * 73856093u) ^ (unsigned int)(y * 19349663u);
int pattern_index = hash % patterns_per_class[conn_class];
int extra_mirror = (hash >> 16) & 1;

// Final UV transform = base_rotation (for connectivity) + extra_mirror (for variety)
```

The `base_rotation` orients the pattern so the connected edges face the correct directions. The `extra_mirror` adds variety without changing which edges connect (mirroring preserves edge connectivity for symmetric transforms).

**Note**: Extra rotation beyond `base_rotation` is only valid for connectivity classes with rotational symmetry (0-edge and 4-edge can rotate freely; opposite-edge can rotate by 90°; others cannot rotate further without breaking edge alignment).

## Implementation

### Step 1: Pattern Texture Atlas

Generate all 20 patterns into a single texture atlas at init time.

- **Texture size**: 20 tiles at 128×128 each = 2560×128 atlas (or 5×4 grid at 640×512). Could also do 256×256 per tile for higher detail — profile to decide.
- **Format**: `GL_RGBA8` — RGB for trace color, alpha for trace mask (1.0 on traces/pads, 0.0 on fill area)
- **Generation**: Reuse existing `render_circuit_pattern()` logic, rendering into an FBO instead of the batch. For each connectivity class, set the appropriate adjacency flags (e.g., 1-edge North pattern gets `adjN=1, adjE=0, adjS=0, adjW=0`) so traces extend to the correct edges. Generate multiple patterns per class for variety.
- **Alternative generation**: Render using the batch renderer into the FBO with an orthographic projection matching the tile size. Traces render as white on transparent black. Color is applied at sampling time.

### Step 2: Circuit Tile Shader

New shader (or extend existing) for textured circuit quads:

```glsl
// Vertex
layout(location = 0) in vec2 a_position;  // world-space quad corner
layout(location = 1) in vec2 a_texcoord;  // UV into atlas tile
out vec2 v_texcoord;
uniform mat4 u_projection;
uniform mat4 u_view;

void main() {
    gl_Position = u_projection * u_view * vec4(a_position, 0.0, 1.0);
    v_texcoord = a_texcoord;
}

// Fragment
in vec2 v_texcoord;
uniform sampler2D u_atlas;
uniform vec4 u_trace_color;   // outline color for this cell
uniform vec4 u_fill_color;    // primary color for this cell (for via holes)
out vec4 fragColor;

void main() {
    float mask = texture(u_atlas, v_texcoord).a;
    // Trace where mask > 0, transparent fill otherwise
    fragColor = vec4(u_trace_color.rgb, u_trace_color.a * mask);
}
```

**Or batch-friendly approach**: Encode the atlas UV offset and rotation per-vertex so the entire visible circuit grid can be drawn in one batched draw call without per-cell uniform changes. This is much better for performance.

### Step 3: UV Transform for Rotation/Mirror

For each cell, compute a 2×2 UV transform based on hash:

| Transform | UV.x | UV.y |
|-----------|------|------|
| 0° normal | u | v |
| 90° CW | v | 1-u |
| 180° | 1-u | 1-v |
| 270° CW | 1-v | u |
| Mirror + any rotation | negate one axis before rotation |

Applied per-vertex when computing texture coordinates for the quad.

### Step 4: Modify render_cell

Replace the `render_circuit_pattern()` call in `render_cell`:

```c
if (mapCell.circuitPattern) {
    View view = View_get_view();
    if (view.scale >= 0.15) {
        // Select pattern and transform from cell position
        unsigned int hash = (unsigned int)(x * 73856093u) ^ (unsigned int)(y * 19349663u);
        int pattern_index = hash % PATTERN_COUNT;            // 0-19
        int rotation = (hash >> 8) % 4;                       // 0-3
        int mirror = (hash >> 16) & 1;                        // 0-1

        // Draw textured quad (or chamfered textured triangles)
        render_circuit_textured(ax, ay, pattern_index, rotation, mirror,
            &outlineColorF, &primaryColor,
            chamfer_ne, chamfer_sw, chamf);
    }
}
```

### Step 5: Chamfering

**No special handling needed.** The chamfered fill polygon is already computed in `render_cell` as a fan of triangles with clipped corners. Instead of pushing flat-colored triangles, push textured triangles with the same vertex positions. The GPU clips the texture at the chamfer diagonal naturally.

The circuit traces near chamfered corners get cut off at the diagonal edge, which reads as an intentionally trimmed circuit board — consistent with the chamfer's visual purpose (exposed edge where two empty sides meet).

## Trade-offs

### Preserved: Traces-Under-Solid-Blocks Effect

The edge connectivity system (see above) preserves the key visual: traces extending to cell edges where solid blocks are adjacent, creating the illusion of circuitry running underneath walls. This was the only cross-cell visual cue the current system had — true trace-to-trace continuity between circuit cells was never achieved and isn't attempted here either.

### Gained: Massive Performance Win

Current cost per visible circuit cell:
- ~24 RNG iterations + occupancy grid checks
- ~10-20 thick_line calls (4 triangles each = 40-80 triangles)
- ~10-20 filled_circle calls (10 triangles each = 100-200 triangles)
- Occupancy grid memset per cell
- Total: ~150-300 triangles per cell, all computed on CPU

Pre-baked cost per visible circuit cell:
- 1 hash computation
- 2 textured triangles (one quad) or 3-4 for chamfered
- Zero CPU pattern generation

**~100x reduction in per-cell triangle count for circuit rendering.**

### Gained: Zoom-Independent Quality

Current thick_line traces have fixed pixel width that doesn't scale well at extreme zoom levels. Pre-baked textures can be filtered cleanly at any zoom via standard texture filtering (mipmaps if desired).

## Open Questions

1. **Tile resolution**: 128×128 vs 256×256? Higher res preserves fine trace detail at close zoom. Profile both.
2. **Atlas vs individual textures**: Atlas is one texture bind, better for batching. Individual textures are simpler to manage. Atlas is probably better.
3. **Trace color**: Bake as white-on-transparent and colorize in shader (flexible, one atlas serves all cell colors) vs bake with actual colors (simpler shader but need separate atlas per color palette). White-on-transparent is clearly better.
4. **Via holes**: Current vias show fill color through a ring. Could encode via holes as a second channel (R = trace, G = via hole) and composite in the shader. Or just skip vias — they're subtle detail that may not be missed.
5. **Bloom source**: Current bloom source pass re-renders circuit outlines. With pre-baked textures, bloom source would sample the same atlas. Should work identically.
6. **Batch rendering strategy**: Could we encode pattern index + rotation + mirror + atlas UV offset as vertex attributes and draw ALL visible circuit cells in a single draw call? This would be the ultimate optimization — one texture bind, one draw call for the entire circuit grid.
