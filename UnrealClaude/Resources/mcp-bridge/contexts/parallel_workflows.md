# Parallel Tool Execution & Subagent Workflow Patterns

Guidance for decomposing complex Unreal tasks into parallel MCP tool calls and Claude Code subagents.

---

## Concurrency & Timeout Reference

| Layer | Limit | Default | Env Override |
|-------|-------|---------|-------------|
| Game thread dispatch | Sync timeout | 30s | — |
| MCPTaskQueue | Max concurrent tasks | 4 | — |
| MCPTaskQueue | Per-task timeout | 2 min | — |
| Bridge async | Overall async timeout | 5 min | `MCP_ASYNC_TIMEOUT_MS` |
| Bridge async | Poll interval | 2s | `MCP_POLL_INTERVAL_MS` |
| execute_script | Script timeout | 10 min | — |

**Key rules:**
- **Max 3 subagents** (4 task slots minus 1 for lead agent)
- **Max 4 simultaneous MCP tool calls** across all agents combined — extras queue, adding latency
- Each subagent should make **≤ 8 sequential tool calls** to stay well within the 2-min task timeout
- Read-only tools (~50-200ms) are safe to batch; modifying tools (1-5s each) need more budget
- If a workflow needs >4 parallel operations, **batch in waves of 3-4**, wait for completion, then next wave

---

## Tool Parallelization Classes

| Class | Tools | Rule |
|-------|-------|------|
| **Parallel-safe** (read-only) | asset_search, get_level_actors, blueprint_query, asset_dependencies, asset_referencers, capture_viewport, get_output_log | Call freely in parallel. No conflicts. |
| **Per-object safe** (modifying) | spawn_actor, move_actor, set_property, blueprint_modify, material, character, character_data, asset, enhanced_input, anim_blueprint_modify | Parallelize on DIFFERENT actors/assets. Never modify same object from two calls. |
| **Sequential only** | open_level, delete_actors, execute_script, cleanup_scripts, run_console_command | Must run alone. open_level invalidates all refs. |

---

## Workflow Pattern 1: Level Setup

**Trigger phrases:** "set up a level", "build a level", "create a scene", "populate the level"

### Phase 1: Survey (lead agent, ~1s)
```
Parallel read-only calls (up to 4):
  - get_level_actors → see what exists
  - asset_search (meshes) → find available static meshes
  - asset_search (materials) → find available materials
  - asset_search (blueprints) → find available blueprint actors
```

### Phase 2: Execute (≤3 subagents, ~15-30s each)
```
Subagent A — Lighting:
  - spawn_actor (DirectionalLight, "Sun_Main", rotation for key light)
  - spawn_actor (SkyLight, "SkyLight_Fill")
  - spawn_actor (ExponentialHeightFog, "Fog_Atmosphere")
  - set_property on each for intensity/color (~6 tool calls total)

Subagent B — Environment Meshes:
  - spawn_actor (StaticMeshActor, "Floor_Main", mesh + transform)
  - spawn_actor (StaticMeshActor, "Wall_North", ...)
  - spawn_actor (StaticMeshActor, "Wall_East", ...)
  - set_property for materials (~6-8 tool calls total)

Subagent C — Gameplay Actors:
  - spawn_actor (PlayerStart, "PlayerStart_0")
  - spawn_actor (Blueprint actor, "PickupHealth_01")
  - spawn_actor (Blueprint actor, "PickupAmmo_01")
  - set_property for gameplay values (~4-6 tool calls total)
```

### Phase 3: Verify (lead agent, ~1s)
```
Parallel read-only calls:
  - get_level_actors → confirm all actors present
  - capture_viewport → visual check
```

**Timeout budget:** Phase 1 ~1s + Phase 2 ~30s + Phase 3 ~1s ≈ 32s total

---

## Workflow Pattern 2: Character Pipeline

**Trigger phrases:** "set up a character", "create character pipeline", "character with movement and input"

### Phase 1: Survey (lead agent, ~1s)
```
Parallel read-only calls:
  - asset_search (character blueprints)
  - asset_search (animation blueprints)
  - asset_search (input actions)
```

### Phase 2: Execute (≤3 subagents, ~15-30s each)
```
Subagent A — Character Config:
  - character (create_data_asset, "DA_PlayerCharacter")
  - character_data (set values: health, speed, jump)
  - spawn_actor (character blueprint into level)
  - set_property (assign data asset) (~4-6 tool calls)

Subagent B — Animation Blueprint:
  - anim_blueprint_modify (create state machine "Locomotion")
  - anim_blueprint_modify (add states: Idle, Walk, Run, Jump)
  - anim_blueprint_modify (create transitions with rules)
  - anim_blueprint_modify (set animation sequences) (~6-8 tool calls)

Subagent C — Input Bindings:
  - enhanced_input (create actions: IA_Move, IA_Look, IA_Jump)
  - enhanced_input (create mapping context: IMC_Default)
  - enhanced_input (bind keys to actions)
  - enhanced_input (assign mapping context to character) (~5-7 tool calls)
```

### Phase 3: Verify (lead agent, ~1s)
```
Parallel read-only calls:
  - blueprint_query (character BP) → confirm components
  - asset_search (verify all assets created)
```

**Timeout budget:** Phase 1 ~1s + Phase 2 ~30s + Phase 3 ~1s ≈ 32s total

---

## Workflow Pattern 3: Blueprint Construction

**Trigger phrases:** "create multiple blueprints", "set up blueprint classes", "build BP hierarchy"

### Key Rule
Sequential within one Blueprint, parallel across different Blueprints (up to 3 simultaneously).

### Phase 1: Survey (lead agent, ~1s)
```
  - asset_search (existing blueprints to avoid name collisions)
```

### Phase 2: Execute
```
If creating 3 BPs (e.g., BP_Weapon, BP_Projectile, BP_Pickup):

Subagent A — BP_Weapon:
  - blueprint_modify (create BP, add components)
  - blueprint_modify (add variables, functions)
  - blueprint_modify (compile) (~4-6 sequential calls)

Subagent B — BP_Projectile:
  - blueprint_modify (create BP, add components)
  - blueprint_modify (add variables, functions)
  - blueprint_modify (compile) (~4-6 sequential calls)

Subagent C — BP_Pickup:
  - blueprint_modify (create BP, add components)
  - blueprint_modify (add variables, functions)
  - blueprint_modify (compile) (~4-6 sequential calls)
```

### Phase 3: Verify (lead agent)
```
Parallel read-only calls:
  - blueprint_query (BP_Weapon)
  - blueprint_query (BP_Projectile)
  - blueprint_query (BP_Pickup)
```

**Timeout budget:** Phase 1 ~0.5s + Phase 2 ~20s + Phase 3 ~1s ≈ 22s total

**Anti-pattern:** Do NOT have two subagents modify the same Blueprint. Blueprint compilation is not thread-safe — one agent per BP.

---

## Workflow Pattern 4: Scene Audit

**Trigger phrases:** "audit the scene", "what's in the level", "analyze the level", "scene report"

### Execution (lead agent only, no subagents needed)
```
Parallel read-only calls (wave 1, max 4):
  - get_level_actors → full actor list
  - asset_search (type: "Blueprint") → available BPs
  - get_output_log → recent warnings/errors
  - capture_viewport → visual snapshot

Parallel read-only calls (wave 2, if needed):
  - blueprint_query (per interesting BP found)
  - asset_dependencies (per asset of interest)
```

**Timeout budget:** Wave 1 ~1s + Wave 2 ~1s ≈ 2s total

**Note:** Pure read-only workflows don't need subagents — just batch tool calls in waves of 4.

---

## Workflow Pattern 5: Material Pipeline

**Trigger phrases:** "create materials", "set up materials", "material pipeline", "assign materials"

### Phase 1: Survey (lead agent, ~1s)
```
Parallel read-only calls:
  - asset_search (existing materials)
  - asset_search (textures)
  - get_level_actors → actors that need materials
```

### Phase 2: Create Materials (≤3 subagents if >3 materials, else direct)
```
If creating 6 materials, batch in 2 waves of 3:

Wave 1 — Subagents A, B, C create MI_Wood, MI_Metal, MI_Concrete:
  Each subagent: material (create instance) + material (set parameters) (~2-3 calls each)

Wave 2 — Subagents A, B, C create MI_Glass, MI_Fabric, MI_Stone:
  Each subagent: material (create instance) + material (set parameters) (~2-3 calls each)
```

### Phase 3: Assign (lead agent, sequential per actor)
```
For each actor:
  - material (assign to actor mesh slot)
  (Sequential — each actor is a different call, safe to batch 4 at a time)
```

**Timeout budget:** Phase 1 ~1s + Phase 2 ~10s per wave + Phase 3 ~5s ≈ 26s total

---

## Workflow Pattern 6: Asset Discovery

**Trigger phrases:** "find all assets", "asset audit", "dependency analysis", "what assets are used"

### Execution (lead agent only, no subagents needed)
```
Parallel read-only calls (wave 1):
  - asset_search (type: "StaticMesh")
  - asset_search (type: "Material")
  - asset_search (type: "Blueprint")
  - asset_search (type: "Texture")

Parallel read-only calls (wave 2, based on results):
  - asset_dependencies (per key asset found)
  - asset_referencers (per key asset found)
  (Batch in groups of 4)
```

**Timeout budget:** Wave 1 ~1s + Wave 2 ~1s per batch ≈ 2-5s total

**Note:** Like scene audit, purely read-only — no subagents needed, just parallel tool calls.

---

## Anti-Patterns

### 1. Too Many Subagents
**Wrong:** Spawning 5 subagents for a level setup.
**Why:** Only 4 task queue slots. The 5th agent's calls queue behind the others, causing cascading delays and potential timeouts.
**Fix:** Max 3 subagents. Combine related work into fewer agents.

### 2. Too Many Sequential Calls in One Subagent
**Wrong:** A single subagent making 20 sequential MCP tool calls.
**Why:** At 1-5s per modifying call, 20 calls = 20-100s, risking the 2-min task timeout.
**Fix:** Keep subagents to ≤8 sequential tool calls. Split larger workloads across waves.

### 3. Same-Object Race Condition
**Wrong:** Two subagents both calling `blueprint_modify` on BP_Character.
**Why:** Blueprint graph operations aren't thread-safe. Concurrent modifications cause data corruption or compile failures.
**Fix:** One agent per Blueprint/asset. Parallelize across different objects only.

### 4. No Survey Before Execute
**Wrong:** Spawning subagents immediately without checking existing state.
**Why:** Leads to naming collisions (duplicate actors), missing dependencies, and wasted work.
**Fix:** Always do a read-only survey phase first.

### 5. Parallelizing Sequential-Only Tools
**Wrong:** Calling `open_level` in one subagent while another runs `spawn_actor`.
**Why:** `open_level` invalidates all actor references. The spawn will fail or target the wrong level.
**Fix:** Sequential-only tools must run alone. Wait for completion before continuing.

---

## Subagent Instructions Template

When spawning a subagent for Unreal MCP work, include these constraints in the prompt:

```
You are a subagent working on [SPECIFIC TASK].

Constraints:
- Make at most 8 sequential MCP tool calls (budget: ~30s total)
- Only modify objects assigned to you: [LIST SPECIFIC NAMES]
- Do NOT modify objects owned by other subagents
- Do NOT call open_level, delete_actors, or execute_script
- If a tool call fails, report the error — do not retry more than once
- If you finish early, report what was created/modified

Your specific tasks:
1. [Tool call 1 with exact parameters]
2. [Tool call 2 with exact parameters]
...
```
