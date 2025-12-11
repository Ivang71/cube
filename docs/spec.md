# Cosmic Voxel Engine - Technical Specification

## Project Vision
Voxel-based space exploration with planetary-scale terrain, star system traversal, atmospheric effects at extreme distances, realistic audio, and multiplayer.

---

## Architecture Principles

### Non-Negotiable Foundations
These decisions affect everything and cannot be retrofitted:

| Decision | Rationale |
|----------|-----------|
| Floating origin from day 1 | Every system must handle coordinate rebasing |
| Camera-relative rendering | GPU never sees absolute world positions |
| Job system for all heavy work | Main thread never blocks on generation/meshing |
| Indirect rendering pipeline | Draw call architecture affects entire renderer |
| Memory budgets enforced | Systems must respect limits, not assume infinite RAM |
| Debug tooling as features | Profiling/visualization built alongside systems |

### Design for Debuggability
- Every system exposes metrics via ImGui
- Visual debug modes for all spatial systems
- Frame-by-frame stepping capability
- Deterministic replay from recorded inputs
- Isolated unit tests for core algorithms

---

## Phase 1: Core Architecture

### 1.1 Build System & Tooling
- CMake with debug/release/profile presets
- Compile commands export for IDE integration
- Shader compilation pipeline for SPIR-V
- Hot-reload infrastructure for shaders
- Asset pipeline stub (textures, models, audio)

### 1.2 Debug Infrastructure (First-Class)
- Tracy profiler integration with custom zones
- ImGui debug overlay framework
- Console system with command registration
- Logging with categories and severity levels
- Frame capture for offline analysis
- Memory allocation tracking with leak detection
- Vulkan validation layers (debug builds)

### 1.3 Vulkan Foundation
- Instance/device creation with feature detection
- Swapchain management with resize handling
- Command buffer ring (frames in flight)
- Descriptor set layout abstraction
- Pipeline cache and creation helpers
- Synchronization primitives (fences, semaphores)
- Debug markers for RenderDoc integration

### 1.4 Coordinate System (Critical Path)
- Universal coordinate: `int64 sector[3] + double local[3]`
- Sector size: 1km (balances precision and shift frequency)
- Arithmetic operations with sector overflow handling
- Camera-relative transform computation
- Origin shift system with configurable threshold
- **Validation**: Unit tests at 1km, 100km, 10,000km, 1M km

### 1.5 Memory Architecture
- Arena allocator for per-frame scratch memory
- Pool allocators for fixed-size objects (chunks, entities)
- Stack allocator for temporary allocations
- VMA integration for GPU memory
- Staging buffer ring (64MB default)
- Budget system with per-category limits and warnings
- **Debug**: Memory viewer showing allocations by category

### 1.6 Job System
- Lock-free work-stealing queues
- Worker threads: `core_count - 2` (leave headroom)
- Job priorities: immediate, normal, background
- Job dependencies via counters
- Fiber support for suspendable jobs
- Main thread job execution for GPU-dependent work
- **Debug**: Job timeline visualization, stall detection

### 1.7 Math Library
- cglm with SIMD enabled
- Thin C++ wrappers for ergonomics where needed
- Frustum, AABB, sphere, ray primitives
- Intersection tests optimized with SIMD
- Transform hierarchy utilities
- **Validation**: Precision tests, SIMD correctness tests

---

## Phase 2: Renderer Foundation

### 2.1 Render Graph
- Declarative pass specification
- Automatic resource transitions
- Transient resource allocation
- Pass culling for unused outputs
- **Debug**: Graph visualization, pass timing breakdown

### 2.2 GPU Resource Management
- Bindless descriptor model (descriptor indexing)
- Texture array for block textures
- Buffer device address for vertex/index data
- Resource streaming queue with priority
- **Budget**: Track VRAM usage, warn on threshold

### 2.3 Indirect Rendering Pipeline (Critical Path)
- GPU-driven draw calls from day 1
- Draw command buffer updated by compute
- Per-chunk visibility buffer
- Single draw call for all opaque geometry
- Separate transparent pass
- **Debug**: Draw call counter, culling statistics overlay

### 2.4 Camera System
- Camera-relative view matrix (camera always at origin)
- Reverse-Z depth buffer for precision
- Jittered projection for TAA
- Previous frame matrices for motion vectors
- Smooth interpolation for input

### 2.5 Basic Shading
- Forward+ initial pass (simpler than deferred)
- PBR material inputs (albedo, roughness, metallic, emission)
- Single directional light (sun)
- Basic ambient from constant
- **Debug**: Visualize normals, depth, material channels

---

## Phase 3: Voxel Core

### 3.1 Block Registry
- Block ID: 16-bit (65K block types)
- Properties: solidity, transparency, collision, emission
- Texture indices into block texture array
- Runtime registration (mod support ready)
- **Debug**: Block browser showing all registered blocks

### 3.2 Chunk Data Structure
- Chunk size: 32³ voxels
- Storage: palette compression (4-bit to 16-bit indices)
- Homogeneous chunk optimization (single block type = 1 byte)
- Dirty flags: 6-bit mask for face regeneration
- Neighbor chunk weak references
- **Memory**: ~2KB typical, ~32KB worst case per chunk

### 3.3 Chunk Manager
- Spatial hash map: `hash(sector, local_chunk) → Chunk*`
- LRU eviction with distance weighting
- Memory budget enforcement
- Chunk state machine: `Empty → Loading → Meshing → Ready → Dirty`
- Request queue with distance-based priority
- **Debug**: Chunk state visualization, cache hit rate

### 3.4 Mesh Generation
- Greedy meshing for face merging
- Ambient occlusion per vertex (4 neighbors)
- Separate opaque and transparent meshes
- Vertex format: position (3×u8), normal (u8), AO (u8), texcoord (2×u16), blockID (u16) = 12 bytes
- Job-based: never on main thread
- **Target**: <2ms per chunk, <0.5ms typical

### 3.5 Chunk Rendering Integration
- Chunk registers draw command on Ready
- Visibility updated by GPU culling compute
- Frustum + occlusion culling
- Draw commands batched into indirect buffer
- **Debug**: Wireframe chunk bounds, LOD level coloring

---

## Phase 4: World Generation

### 4.1 Noise System
- FastNoise2 with SIMD (AVX2/SSE4)
- Noise graph for combining operations
- Domain warping for organic terrain
- Fractal types: FBM, ridged, billow
- Cached noise for heightmaps
- **Debug**: Noise preview window, parameter tweaking

### 4.2 Terrain Pipeline
- Heightmap from 2D simplex (base terrain)
- 3D density for caves and overhangs
- Biome selection from temp/humidity
- Surface block selection per biome
- Decoration pass (flora, rocks)
- Ore placement via 3D worley

### 4.3 Determinism
- 64-bit world seed
- Chunk seed: `hash(world_seed, chunk_coord)`
- Thread-safe: no shared mutable state
- Reproducible across runs and machines
- **Validation**: Generate same chunk 1000x, verify identical

### 4.4 Generation Threading
- Background job priority
- Cancelable on chunk unload
- Budget: max N chunks per frame
- Prefetch based on velocity vector
- **Debug**: Generation queue length, average time

---

## Phase 5: Player & Physics

### 5.1 Player Controller
- First-person camera
- AABB collision hull
- Ground detection, slope handling
- Jump with coyote time
- Smooth step-up for small obstacles
- **Debug**: Show collision hull, ground normal

### 5.2 Physics Foundation
- Fixed timestep: 60Hz
- Simple rigid body for player
- Voxel collision via chunk queries
- Spatial hash for entity broadphase
- Sleeping for stationary objects
- **Budget**: <4ms per physics tick

### 5.3 Block Interaction
- Ray-voxel intersection
- Block placement with collision check
- Block removal with neighbor updates
- Reach distance limit
- **Debug**: Show interaction ray, target block highlight

### 5.4 Coordinate Handling in Physics
- Physics operates in double precision
- Local simulation regions (~2km radius)
- Object handoff between regions
- Synced with floating origin shifts

---

## Phase 6: Level of Detail

### 6.1 LOD Mesh Generation
- LOD levels: 0 (full), 1 (2x), 2 (4x), 3 (8x), 4 (16x)
- Block averaging for lower LODs
- Transvoxel for seamless boundaries
- Async generation like base meshes
- **Memory**: LOD meshes share pools

### 6.2 LOD Selection
- Distance-based LOD assignment
- Hysteresis: 20% band to prevent thrashing
- Screen-space error metric option
- Per-chunk LOD tracking
- **Debug**: LOD level coloring, transition visualization

### 6.3 LOD Transitions
- Geomorphing: vertex interpolation over distance
- Or: Dithered alpha blend
- Temporal stability important
- **Validation**: No visible pops at any speed

### 6.4 Hybrid Distant Rendering
- Beyond mesh LOD: imposter billboards
- Pre-rendered chunk views
- Update imposters on chunk change
- Eventually: analytical sphere for planets
- **Target**: Visible detail at 100km+

---

## Phase 7: Planetary Scale

### 7.1 Spherical World
- Cube-sphere mapping
- 6 quadtrees (one per cube face)
- Face edge stitching for seamless terrain
- Gravity direction = toward planet center
- Horizon culling optimization

### 7.2 Planet Definition
- Radius, mass, surface gravity
- Rotation period, axial tilt
- Atmosphere parameters (if present)
- Terrain generation seed
- **Data**: Planet registry, orbital elements

### 7.3 Multi-Body System
- Origin tracking: which body is reference
- Smooth origin transfer between bodies
- Relative rendering for distant bodies
- **Validation**: No jitter at any position in system

### 7.4 Space Transition
- Seamless surface to orbit
- LOD loading during ascent
- Skybox blend: atmosphere → space
- **Target**: No loading screens

---

## Phase 8: Atmospheric Rendering

### 8.1 Precomputed Scattering
- Bruneton 2017 model
- Transmittance LUT: 256×64
- Scattering LUT: 32×128×32×8
- Irradiance LUT: 64×16
- Precompute on planet load (~50ms)

### 8.2 Runtime Rendering
- Raymarch for aerial perspective
- Single/multiple scattering lookup
- Sun disk with limb darkening
- Twilight and night sky blend
- **Performance**: <1ms at 1080p

### 8.3 Volumetric Effects
- Froxel-based volumetric fog
- God rays via volumetric shadow
- Cloud layer (2D noise initially)
- **Budget**: 1-2ms total

### 8.4 Distant Visibility
- Lights visible through atmosphere
- Atmospheric extinction model
- HDR bloom for bright sources
- Ship entry plasma effect
- **Goal**: See ships at 100km

---

## Phase 9: Advanced Rendering

### 9.1 Shadow System
- Cascaded shadow maps: 4 cascades
- Stable cascade boundaries
- Shadow cache for distant cascades
- PCF soft shadows
- **Budget**: <3ms total

### 9.2 Deferred Upgrade (Optional)
- G-buffer: albedo, normal, material, emission
- Light accumulation pass
- Many lights support
- Keep forward path for transparents

### 9.3 Post-Processing
- TAA with motion vectors
- GTAO for ambient occlusion
- Bloom with threshold
- Auto-exposure via histogram
- ACES tone mapping
- **Budget**: <2ms total

### 9.4 Debug Visualization
- Wireframe mode
- Depth visualization
- Normal visualization
- Overdraw heat map
- Chunk bounds
- LOD coloring

---

## Phase 10: Audio Engine

### 10.1 Core System
- OpenAL Soft backend
- Source pooling (64 sources typical)
- Distance attenuation curves
- HRTF spatialization
- Streaming for music/ambience

### 10.2 Environmental Audio
- Raycast for reverb parameters
- Obstruction via ray tests
- Material-based absorption
- **Update rate**: Every 100ms sufficient

### 10.3 Doppler & Propagation
- Velocity tracking for Doppler
- Speed of sound per medium
- Delay from distance
- Vacuum behavior in space

### 10.4 Debug
- Active source visualization
- Reverb zone display
- Audio profiler overlay

---

## Phase 11: Multiplayer Foundation

### 11.1 Networking Layer
- GameNetworkingSockets (Valve)
- Reliable + unreliable channels
- Connection state machine
- Encryption via library

### 11.2 Protocol
- FlatBuffers serialization
- Packet versioning
- Delta compression
- Bandwidth metering

### 11.3 Client-Server Model
- Server authoritative
- Client prediction for movement
- Server reconciliation
- Input buffer (3 frames)

### 11.4 State Sync
- Entity interpolation
- Snapshot system
- Priority-based updates
- **Bandwidth target**: <50KB/s per player

---

## Phase 12: Multiplayer World

### 12.1 Chunk Sync
- Server owns chunk state
- Client subscribes by position
- Delta updates for changes
- Checksum validation

### 12.2 Interest Management
- Spatial partitioning
- Relevance scoring
- Bandwidth budgeting
- Dynamic update rates

### 12.3 Persistence
- SQLite for chunk storage
- LevelDB for key-value (players, etc.)
- Periodic autosave
- Graceful shutdown

---

## Phase 13: Vehicles

### 13.1 Construct System
- Voxel-based vehicles
- Local coordinate space
- Multi-chunk support
- Collision mesh generation

### 13.2 Physics
- Rigid body dynamics
- Thrust forces
- Aerodynamic model (atmosphere)
- RCS for space

### 13.3 Flight Modes
- Surface: wheeled/hover
- Atmospheric: lift + thrust
- Space: Newtonian + RCS
- Seamless transitions

---

## Phase 14: Gameplay

### 14.1 Inventory & Crafting
- Item definitions
- Inventory slots
- Recipe system
- Crafting UI

### 14.2 Resources
- Mining mechanics
- Resource types
- Tool effectiveness
- Ore distribution

### 14.3 Progression
- Tech tree
- Blueprints
- Research mechanics

---

## Phase 15: Polish

### 15.1 Performance Pass
- Profile all systems
- Optimize hot paths
- Memory reduction
- Load time optimization

### 15.2 Quality Pass
- Visual polish
- Audio polish
- UX improvements
- Bug fixing

### 15.3 Platform Support
- Windows primary
- Linux secondary
- Steam integration
- Controller support

---

## Validation Gates

Each phase must pass before proceeding:

### Phase 1 Gate
- [ ] 144 FPS with empty scene
- [ ] Origin shift at 1km works
- [ ] No memory leaks in 1hr run
- [ ] Tracy shows all systems
- [ ] Jobs complete without deadlock

### Phase 2 Gate
- [ ] 10,000 draw commands via indirect
- [ ] <1ms for culling compute
- [ ] Render graph handles resize
- [ ] Reverse-Z depth works

### Phase 3 Gate
- [ ] Greedy mesh <2ms per chunk
- [ ] 1000 chunks loaded, 60 FPS
- [ ] Chunk eviction works correctly
- [ ] Block placement/removal works

### Phase 4 Gate
- [ ] Deterministic generation (verified)
- [ ] Generation <5ms per chunk
- [ ] 100 chunks/second sustainable
- [ ] Terrain looks acceptable

### Phase 5 Gate
- [ ] Player can walk, jump, collide
- [ ] No physics explosions
- [ ] Block interaction works
- [ ] 60Hz physics stable

### Phase 6 Gate
- [ ] LOD transitions seamless
- [ ] No LOD thrashing
- [ ] 10km view distance playable
- [ ] Memory within budget

### Phase 7 Gate
- [ ] Walk on spherical planet
- [ ] Gravity works correctly
- [ ] No precision issues
- [ ] Surface-to-orbit transition

### Phase 8 Gate
- [ ] Atmosphere renders correctly
- [ ] Day/night cycle works
- [ ] <2ms atmosphere cost
- [ ] Distant lights visible

---

## Performance Budgets

### Frame Budget (16.6ms for 60 FPS)
| System | Budget |
|--------|--------|
| Physics | 2ms |
| Chunk generation | 4ms |
| Mesh generation | 4ms |
| Culling | 1ms |
| Rendering | 8ms |
| Audio | 1ms |
| Game logic | 2ms |
| **Headroom** | 2ms |

### Memory Budget
| Category | RAM | VRAM |
|----------|-----|------|
| Chunk data | 2GB | - |
| Chunk meshes | 512MB | 1.5GB |
| Textures | 256MB | 1GB |
| Audio | 256MB | - |
| Entities | 256MB | - |
| Frame scratch | 64MB | 128MB |
| **Total** | ~3.5GB | ~2.5GB |

---

## Technology Stack

| Component | Choice | Rationale |
|-----------|--------|-----------|
| Language | C++20 | Performance, control |
| Graphics | Vulkan 1.2 | Modern, explicit |
| Windowing | GLFW | Simple, cross-platform |
| Math | cglm | SIMD, performant |
| Noise | FastNoise2 | SIMD, feature-rich |
| Audio | OpenAL Soft | 3D audio, HRTF |
| Networking | GameNetworkingSockets | Reliable, NAT punch |
| Serialization | FlatBuffers | Zero-copy, fast |
| Compression | LZ4 | Fast decompression |
| Profiling | Tracy | Comprehensive |
| Debug UI | Dear ImGui | Industry standard |
| Storage | SQLite + LevelDB | Chunks + KV data |

---

## Debug Tools Checklist

Built alongside systems, not afterthoughts:

- [ ] Frame profiler overlay (Tracy integration)
- [ ] Memory usage breakdown
- [ ] Chunk state visualizer (colored by state)
- [ ] LOD level visualizer
- [ ] Coordinate display (sector + local)
- [ ] Origin shift indicator
- [ ] Physics debug draw (colliders, velocities)
- [ ] Audio source visualization
- [ ] Network stats overlay
- [ ] Draw call / triangle counter
- [ ] Culling statistics
- [ ] Job queue visualizer
- [ ] Noise preview tool
- [ ] Block inspector
- [ ] Console with commands
- [ ] Determinism verification mode
