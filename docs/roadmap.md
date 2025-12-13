# Development Roadmap

Each step is atomic and verifiable. Do not proceed until verification passes.

---

## Stage 1: Project Bootstrap

### 1.1 Build System ✅
| Step | Task | Verification |
|------|------|--------------|
| 1.1.1 | Create CMakeLists.txt with C++20, project name | `cmake -B build` succeeds |
| 1.1.2 | Add debug/release/profile presets | `cmake --preset debug` works |
| 1.1.3 | Create src/main.cpp with empty main | `cmake --build build` produces executable |
| 1.1.4 | Add compile_commands.json export | File exists in build/, IDE picks it up |
| 1.1.5 | Add .gitignore for build/, .cache/, etc | `git status` clean after build |

### 1.2 Window Creation ✅
| Step | Task | Verification |
|------|------|--------------|
| 1.2.1 | Add GLFW via FetchContent or submodule | CMake finds GLFW |
| 1.2.2 | Initialize GLFW in main | No crash, returns success |
| 1.2.3 | Create 1600x900 window with title | Window appears on screen |
| 1.2.4 | Add main loop with glfwPollEvents | Window stays open, responds to close |
| 1.2.5 | Handle window resize callback | Print new size on resize |
| 1.2.6 | Add frame timing (delta time) | Print FPS to console each second |

### 1.3 Vulkan Instance ✅
| Step | Task | Verification |
|------|------|--------------|
| 1.3.1 | Add Vulkan SDK dependency | CMake finds Vulkan |
| 1.3.2 | Create VkInstance with app info | vkCreateInstance returns VK_SUCCESS |
| 1.3.3 | Enable validation layers (debug) | Layers load without error |
| 1.3.4 | Setup debug messenger callback | Validation messages print to console |
| 1.3.5 | Query and print available extensions | List prints, includes required ones |
| 1.3.6 | Clean destruction on exit | No validation errors on shutdown |

### 1.4 Vulkan Surface & Device ✅
| Step | Task | Verification |
|------|------|--------------|
| 1.4.1 | Create VkSurfaceKHR via GLFW | Surface created successfully |
| 1.4.2 | Enumerate physical devices | Print device names and types |
| 1.4.3 | Select suitable GPU (discrete preferred) | Correct GPU selected |
| 1.4.4 | Query queue families | Graphics + present queues found |
| 1.4.5 | Create VkDevice with required features | Device created successfully |
| 1.4.6 | Get queue handles | Graphics and present queues retrieved |
| 1.4.7 | Enable required extensions (swapchain) | No missing extension errors |

### 1.5 Swapchain ✅
| Step | Task | Verification |
|------|------|--------------|
| 1.5.1 | Query surface capabilities | Print supported formats, present modes |
| 1.5.2 | Choose format (BGRA8 SRGB preferred) | Format selected |
| 1.5.3 | Choose present mode (mailbox > fifo) | Mode selected |
| 1.5.4 | Create swapchain | Swapchain created successfully |
| 1.5.5 | Get swapchain images | Image handles retrieved |
| 1.5.6 | Create image views | Views created for all images |
| 1.5.7 | Handle swapchain recreation on resize | Resize works without crash |

### 1.6 Command Infrastructure ✅
| Step | Task | Verification |
|------|------|--------------|
| 1.6.1 | Create command pool | Pool created |
| 1.6.2 | Allocate command buffer per frame | Buffers allocated |
| 1.6.3 | Create semaphores (image available, render done) | Per-frame semaphores created |
| 1.6.4 | Create fences for CPU-GPU sync | Fences created |
| 1.6.5 | Implement frame-in-flight logic (2 frames) | Can record while previous executes |

### 1.7 Clear Screen ✅
| Step | Task | Verification |
|------|------|--------------|
| 1.7.1 | Acquire swapchain image | Acquisition succeeds |
| 1.7.2 | Begin command buffer | Recording starts |
| 1.7.3 | Transition image to transfer dst | Barrier recorded |
| 1.7.4 | vkCmdClearColorImage with color | Clear command recorded |
| 1.7.5 | Transition image to present | Barrier recorded |
| 1.7.6 | End and submit command buffer | Submit succeeds |
| 1.7.7 | Present image | **Screen shows solid color** |
| 1.7.8 | Animate clear color over time | Color smoothly changes |

---

## Stage 2: Rendering Foundation

### 2.1 Shader Pipeline ✅
| Step | Task | Verification |
|------|------|--------------|
| 2.1.1 | Create shaders/ directory structure | Directory exists |
| 2.1.2 | Write minimal vertex shader (passthrough) | GLSL compiles to SPIR-V |
| 2.1.3 | Write minimal fragment shader (solid color) | GLSL compiles to SPIR-V |
| 2.1.4 | Add shader compilation to CMake | Shaders compile on build |
| 2.1.5 | Load SPIR-V at runtime | Bytes loaded correctly |
| 2.1.6 | Create VkShaderModule | Module created |
| 2.1.7 | Implement shader hot-reload detection | File change detected |
| 2.1.8 | Implement shader hot-reload swap | Shader updates without restart |

### 2.2 Render Pass ✅
| Step | Task | Verification |
|------|------|--------------|
| 2.2.1 | Create depth format query | Supported depth format found |
| 2.2.2 | Create depth image and view | Depth buffer created |
| 2.2.3 | Define color attachment description | Attachment configured |
| 2.2.4 | Define depth attachment description | Attachment configured |
| 2.2.5 | Create subpass with attachments | Subpass defined |
| 2.2.6 | Create VkRenderPass | Render pass created |
| 2.2.7 | Create framebuffers per swapchain image | Framebuffers created |
| 2.2.8 | Recreate on resize | Resize works |

### 2.3 Graphics Pipeline ✅
| Step | Task | Verification |
|------|------|--------------|
| 2.3.1 | Define vertex input state (empty for now) | State struct filled |
| 2.3.2 | Define input assembly (triangle list) | State struct filled |
| 2.3.3 | Define viewport and scissor (dynamic) | Dynamic state configured |
| 2.3.4 | Define rasterizer state | State struct filled |
| 2.3.5 | Define multisampling (disabled) | State struct filled |
| 2.3.6 | Define depth stencil state | State struct filled |
| 2.3.7 | Define color blend state | State struct filled |
| 2.3.8 | Create empty pipeline layout | Layout created |
| 2.3.9 | Create VkPipeline | Pipeline created |
| 2.3.10 | Enable pipeline cache | Cache created and used |

### 2.4 Draw Triangle ✅
| Step | Task | Verification |
|------|------|--------------|
| 2.4.1 | Hardcode triangle vertices in shader | Vertices in shader |
| 2.4.2 | Begin render pass in command buffer | Render pass begins |
| 2.4.3 | Bind pipeline | Pipeline bound |
| 2.4.4 | Set viewport and scissor | Dynamic state set |
| 2.4.5 | vkCmdDraw(3, 1, 0, 0) | Draw recorded |
| 2.4.6 | End render pass | Render pass ends |
| 2.4.7 | Submit and present | **Triangle visible on screen** |

### 2.5 Vertex Buffers ✅
| Step | Task | Verification |
|------|------|--------------|
| 2.5.1 | Add VMA (Vulkan Memory Allocator) | VMA links and initializes |
| 2.5.2 | Define vertex struct (pos, color) | Struct defined |
| 2.5.3 | Create staging buffer with vertex data | Buffer created, data copied |
| 2.5.4 | Create device-local vertex buffer | Buffer created |
| 2.5.5 | Copy staging → device via command | Transfer completes |
| 2.5.6 | Update vertex input state | Bindings and attributes set |
| 2.5.7 | Bind vertex buffer in draw | Buffer bound |
| 2.5.8 | Draw using buffer data | **Triangle from buffer visible** |

### 2.6 Index Buffers ✅
| Step | Task | Verification |
|------|------|--------------|
| 2.6.1 | Create index buffer (staging + device) | Buffer created |
| 2.6.2 | Upload index data | Transfer completes |
| 2.6.3 | Bind index buffer | Buffer bound |
| 2.6.4 | Use vkCmdDrawIndexed | **Indexed quad visible** |

### 2.7 Uniform Buffers ✅
| Step | Task | Verification |
|------|------|--------------|
| 2.7.1 | Create uniform buffer (host-visible) | Buffer created |
| 2.7.2 | Define MVP matrix struct | Struct matches shader |
| 2.7.3 | Create descriptor set layout | Layout created |
| 2.7.4 | Create descriptor pool | Pool created |
| 2.7.5 | Allocate descriptor set | Set allocated |
| 2.7.6 | Update descriptor with buffer | Descriptor points to buffer |
| 2.7.7 | Update pipeline layout | Layout includes descriptor |
| 2.7.8 | Bind descriptor set in draw | Set bound |
| 2.7.9 | Update MVP each frame | **Rotating triangle** |

### 2.8 Camera ✅
| Step | Task | Verification |
|------|------|--------------|
| 2.8.1 | Implement perspective projection | Matrix correct (test against known) |
| 2.8.2 | Implement view matrix from pos/target/up | Matrix correct |
| 2.8.3 | Enable reverse-Z depth | Near=1, far=0 works |
| 2.8.4 | Add camera position/rotation state | State updates |
| 2.8.5 | Keyboard input (WASD) | Camera moves |
| 2.8.6 | Mouse input (look) | Camera rotates |
| 2.8.7 | Mouse capture toggle (ESC) | Cursor locks/unlocks |
| 2.8.8 | Smooth movement interpolation | Movement not jerky |

---

## Stage 3: Debug Infrastructure

### 3.1 ImGui Integration ✅
| Step | Task | Verification |
|------|------|--------------|
| 3.1.1 | Add Dear ImGui dependency | CMake finds it |
| 3.1.2 | Initialize ImGui context | Context created |
| 3.1.3 | Setup ImGui Vulkan backend | Backend initialized |
| 3.1.4 | Setup ImGui GLFW backend | Input works |
| 3.1.5 | Create ImGui render pass | Separate pass for UI |
| 3.1.6 | Render ImGui demo window | **Demo window visible** |
| 3.1.7 | Style ImGui (dark theme) | Looks good |

### 3.2 Debug Overlay ✅
| Step | Task | Verification |
|------|------|--------------|
| 3.2.1 | Create overlay window (corner) | Window in corner |
| 3.2.2 | Display FPS | FPS shows correctly |
| 3.2.3 | Display frame time (ms) | Time shows correctly |
| 3.2.4 | Display camera position | Position updates |
| 3.2.5 | Display memory usage (VMA stats) | Memory shows |
| 3.2.6 | Toggle overlay with F3 | Toggle works |

### 3.3 Console System ✅
| Step | Task | Verification |
|------|------|--------------|
| 3.3.1 | Create console window (toggleable) | Window opens/closes |
| 3.3.2 | Input text field | Can type |
| 3.3.3 | Command history (up/down) | History works |
| 3.3.4 | Command registration system | Can register commands |
| 3.3.5 | Implement "help" command | Lists commands |
| 3.3.6 | Implement "tp x y z" command | Teleports camera |

### 3.4 Logging ✅
| Step | Task | Verification |
|------|------|--------------|
| 3.4.1 | Create log macros (INFO, WARN, ERROR) | Macros compile |
| 3.4.2 | Add timestamp to log | Time shows |
| 3.4.3 | Add source location | File:line shows |
| 3.4.4 | Add category tags | [Render], [Physics], etc |
| 3.4.5 | Console output | Logs print to stdout |
| 3.4.6 | File output | Logs write to file |
| 3.4.7 | ImGui log viewer | Logs show in UI |
| 3.4.8 | Log level filtering | Can hide INFO |

### 3.5 Tracy Integration ✅
| Step | Task | Verification |
|------|------|--------------|
| 3.5.1 | Add Tracy dependency | Links correctly |
| 3.5.2 | Add frame mark | Tracy shows frames |
| 3.5.3 | Add zone macros | Zones visible |
| 3.5.4 | Instrument main loop | See frame breakdown |
| 3.5.5 | Instrument render | See render time |
| 3.5.6 | Add GPU zones | GPU work visible |
| 3.5.7 | Memory tracking | Allocations tracked |

---

## Stage 4: Math & Coordinates

### 4.1 Math Library ✅
| Step | Task | Verification |
|------|------|--------------|
| 4.1.1 | Use glm dependency | Links correctly |
| 4.1.2 | Create math types header | Types accessible |
| 4.1.3 | Verify mat4 multiplication | Test against known result |
| 4.1.4 | Verify perspective projection | Test against known result |
| 4.1.5 | Verify lookAt | Test against known result |
| 4.1.6 | AABB struct and intersection | Tests pass |
| 4.1.7 | Frustum struct and extraction | Tests pass |
| 4.1.8 | Ray struct and ray-AABB test | Tests pass |

### 4.2 Universal Coordinates ✅
| Step | Task | Verification |
|------|------|--------------|
| 4.2.1 | Define UniversalCoord struct | Compiles |
| 4.2.2 | Implement addition | (1km-1m) + (2m) = (1km+1m) |
| 4.2.3 | Implement subtraction | Result always small if nearby |
| 4.2.4 | Implement sector overflow | 999m + 100m = 1km + 99m |
| 4.2.5 | Implement sector underflow | 1m - 100m = -1km + 901m |
| 4.2.6 | Implement distance (double) | Correct for nearby objects |
| 4.2.7 | Implement to_relative(camera) | Returns vec3 for GPU |
| 4.2.8 | Unit test at 1km | Precision verified |
| 4.2.9 | Unit test at 100km | Precision verified |
| 4.2.10 | Unit test at 10,000km | Precision verified |
| 4.2.11 | Unit test at 1,000,000km | Precision verified |

### 4.3 Floating Origin ✅
| Step | Task | Verification |
|------|------|--------------|
| 4.3.1 | Store render origin (UniversalCoord) | Origin tracked |
| 4.3.2 | Compute shift threshold (500m) | Threshold configurable |
| 4.3.3 | Detect when shift needed | Detection works |
| 4.3.4 | Perform origin shift | Origin updates |
| 4.3.5 | Update all object positions on shift | Positions adjusted |
| 4.3.6 | Verify no visual pop on shift | **No jitter visible** |
| 4.3.7 | Display origin in debug overlay | Shows current origin |
| 4.3.8 | Display distance from origin | Shows meters |

### 4.4 Camera-Relative Rendering ✅
| Step | Task | Verification |
|------|------|--------------|
| 4.4.1 | Camera always at (0,0,0) in shaders | Verified in shader |
| 4.4.2 | Objects transformed relative to camera | Objects render correctly |
| 4.4.3 | Test object at 100km from origin | No jitter |
| 4.4.4 | Test object at 10,000km | No jitter |
| 4.4.5 | Test rapid movement (1km/s) | No artifacts |

---

## Stage 5: Memory Architecture

### 5.1 Allocator Framework ✅
| Step | Task | Verification |
|------|------|--------------|
| 5.1.1 | Define allocator interface | Interface compiles |
| 5.1.2 | Implement linear allocator | Alloc/reset works |
| 5.1.3 | Implement pool allocator | Alloc/free works |
| 5.1.4 | Implement stack allocator | Push/pop works |
| 5.1.5 | Add allocation tracking | Track bytes allocated |
| 5.1.6 | Add leak detection (debug) | Leaks reported on shutdown |

### 5.2 Frame Allocator ✅
| Step | Task | Verification |
|------|------|--------------|
| 5.2.1 | Create per-frame linear allocator | One per frame-in-flight |
| 5.2.2 | Reset at frame start | Memory reclaimed |
| 5.2.3 | Use for temporary command data | Commands use frame memory |
| 5.2.4 | Size: 64MB per frame | Sufficient for typical frame |
| 5.2.5 | Overflow warning | Log if exceeded |

### 5.3 GPU Memory Management ✅
| Step | Task | Verification |
|------|------|--------------|
| 5.3.1 | Query VMA statistics | Stats retrieved |
| 5.3.2 | Display VRAM usage in overlay | Shows MB used |
| 5.3.3 | Create staging buffer ring | Ring buffer works |
| 5.3.4 | Implement upload queue | Uploads queued and processed |
| 5.3.5 | Budget system (VRAM limit) | Warning on threshold |

### 5.4 Budget Enforcement ✅
| Step | Task | Verification |
|------|------|--------------|
| 5.4.1 | Define budget categories | Enum/constants defined |
| 5.4.2 | Track per-category usage | Usage updated on alloc/free |
| 5.4.3 | Warning threshold (80%) | Warning logged |
| 5.4.4 | Error threshold (95%) | Error logged, allocation may fail |
| 5.4.5 | Display budgets in debug UI | Shows per-category bars |

---

## Stage 6: Job System

### 6.1 Queue Structure ✅
| Step | Task | Verification |
|------|------|--------------|
| 6.1.1 | Define Job struct (function ptr, data, counter) | Struct compiles |
| 6.1.2 | Implement lock-free queue (MPMC) | Queue operations work |
| 6.1.3 | Test queue single-threaded | Enqueue/dequeue correct |
| 6.1.4 | Test queue multi-threaded (stress) | No data races |

### 6.2 Worker Threads ✅
| Step | Task | Verification |
|------|------|--------------|
| 6.2.1 | Create worker thread pool | Threads created |
| 6.2.2 | Workers wait on queue | Threads sleep when empty |
| 6.2.3 | Workers execute jobs | Jobs run on workers |
| 6.2.4 | Workers decrement job counter | Counter reaches 0 |
| 6.2.5 | Thread count: cores - 2 | Correct count |

### 6.3 Job Submission ✅
| Step | Task | Verification |
|------|------|--------------|
| 6.3.1 | Submit single job | Job executes |
| 6.3.2 | Submit batch of jobs | All jobs execute |
| 6.3.3 | Wait on job counter | Wait returns when done |
| 6.3.4 | Job priorities (high, normal, low) | High runs first |

### 6.4 Job Dependencies ✅
| Step | Task | Verification |
|------|------|--------------|
| 6.4.1 | Job can depend on counter | Job waits for dependency |
| 6.4.2 | Chain of dependent jobs | Executes in order |
| 6.4.3 | Fan-out / fan-in pattern | Works correctly |
| 6.4.4 | Test deadlock detection | Deadlocks reported (or prevented) |

### 6.5 Debug & Profiling ✅
| Step | Task | Verification |
|------|------|--------------|
| 6.5.1 | Tracy zones for job execution | Jobs visible in Tracy |
| 6.5.2 | Job queue length in debug UI | Shows pending count |
| 6.5.3 | Worker utilization display | Shows % busy |
| 6.5.4 | Stall detection (job taking >100ms) | Warning logged |

---

## Stage 7: Voxel Data

### 7.1 Block System ✅
| Step | Task | Verification |
|------|------|--------------|
| 7.1.1 | Define BlockID type (uint16_t) | Type defined |
| 7.1.2 | Define BlockProperties struct | Struct defined |
| 7.1.3 | Create BlockRegistry class | Class compiles |
| 7.1.4 | Register "air" block (ID 0) | Block registered |
| 7.1.5 | Register "stone" block | Block registered |
| 7.1.6 | Register "dirt", "grass" blocks | Blocks registered |
| 7.1.7 | Query block properties by ID | Properties returned |
| 7.1.8 | Debug UI: block browser | Lists all blocks |

### 7.2 Chunk Structure ✅
| Step | Task | Verification |
|------|------|--------------|
| 7.2.1 | Define CHUNK_SIZE constant (32) | Constant defined |
| 7.2.2 | Define ChunkCoord (int64 x3) | Struct defined |
| 7.2.3 | Define Chunk class | Class compiles |
| 7.2.4 | Block storage array (32³) | Array allocated |
| 7.2.5 | get_block(x, y, z) | Returns correct block |
| 7.2.6 | set_block(x, y, z, id) | Sets and marks dirty |
| 7.2.7 | Dirty flag for mesh regen | Flag works |

### 7.3 Chunk Compression ✅
| Step | Task | Verification |
|------|------|--------------|
| 7.3.1 | Implement palette compression | Palette works |
| 7.3.2 | Bits-per-block based on palette size | Correct bits used |
| 7.3.3 | Homogeneous chunk optimization | Single block = tiny size |
| 7.3.4 | Measure memory: empty chunk | <100 bytes |
| 7.3.5 | Measure memory: terrain chunk | <4KB typical |
| 7.3.6 | Measure memory: noisy chunk | <32KB worst |

### 7.4 Chunk Storage ✅
| Step | Task | Verification |
|------|------|--------------|
| 7.4.1 | Create ChunkManager class | Class compiles |
| 7.4.2 | Spatial hash map for chunks | Map works |
| 7.4.3 | get_chunk(coord) | Returns chunk or null |
| 7.4.4 | create_chunk(coord) | Creates and stores |
| 7.4.5 | LRU tracking for chunks | Access updates LRU |
| 7.4.6 | Eviction when over limit | Oldest evicted |
| 7.4.7 | Memory limit configurable | Limit respected |
| 7.4.8 | Debug UI: chunk count, memory | Stats displayed |

---

## Stage 8: Mesh Generation

### 8.1 Vertex Format
| Step | Task | Verification |
|------|------|--------------|
| 8.1.1 | Define ChunkVertex struct | Struct defined |
| 8.1.2 | Position: 3x uint8 (relative to chunk) | 3 bytes |
| 8.1.3 | Normal: uint8 (6 directions = 3 bits) | 1 byte |
| 8.1.4 | AO: uint8 (4 corners, 2 bits each) | 1 byte |
| 8.1.5 | TexCoord: 2x uint16 | 4 bytes |
| 8.1.6 | BlockID: uint16 | 2 bytes |
| 8.1.7 | Total: 12 bytes per vertex | Size verified |
| 8.1.8 | Vulkan vertex input binding | Binding works |

### 8.2 Face Generation
| Step | Task | Verification |
|------|------|--------------|
| 8.2.1 | Check block visibility (solid neighbor) | Only exposed faces |
| 8.2.2 | Generate 6 faces for single block | Faces correct |
| 8.2.3 | Chunk boundary checks (neighbor chunk) | Edge faces correct |
| 8.2.4 | Build vertex buffer from faces | Buffer filled |
| 8.2.5 | Build index buffer | Indices correct |
| 8.2.6 | Render single solid chunk | **Chunk visible** |

### 8.3 Greedy Meshing
| Step | Task | Verification |
|------|------|--------------|
| 8.3.1 | Implement 2D greedy merge for one face | Rectangles merged |
| 8.3.2 | Apply to all 6 face directions | All faces merged |
| 8.3.3 | Compare vertex count: naive vs greedy | 80%+ reduction |
| 8.3.4 | Benchmark: full chunk mesh time | <2ms |
| 8.3.5 | Render greedy-meshed chunk | **Same visual result** |

### 8.4 Ambient Occlusion
| Step | Task | Verification |
|------|------|--------------|
| 8.4.1 | Sample 4 corner neighbors per vertex | Neighbors sampled |
| 8.4.2 | Calculate AO value (0-3) | Values correct |
| 8.4.3 | Pack AO into vertex | AO in vertex |
| 8.4.4 | Apply AO in fragment shader | **Corners darker** |
| 8.4.5 | Fix AO flip for anisotropy | Diagonal consistent |

### 8.5 Async Mesh Generation
| Step | Task | Verification |
|------|------|--------------|
| 8.5.1 | Mesh generation as job | Job executes off main thread |
| 8.5.2 | Chunk state: Dirty → Meshing → Ready | States transition correctly |
| 8.5.3 | Cancel mesh job on chunk unload | No crash, no leak |
| 8.5.4 | Upload mesh to GPU on main thread | Upload works |
| 8.5.5 | Profile: mesh gen time | Tracy shows job time |

---

## Stage 9: Chunk Rendering

### 9.1 Chunk Mesh Management
| Step | Task | Verification |
|------|------|--------------|
| 9.1.1 | Create ChunkMesh struct (VkBuffer handles) | Struct defined |
| 9.1.2 | Pool allocation for chunk meshes | Pool works |
| 9.1.3 | Assign mesh to chunk | Mesh stored in chunk |
| 9.1.4 | Free mesh on chunk unload | Memory freed |

### 9.2 Multi-Chunk Rendering
| Step | Task | Verification |
|------|------|--------------|
| 9.2.1 | Generate 3x3x3 test chunks | 27 chunks created |
| 9.2.2 | Render all chunks | **27 chunks visible** |
| 9.2.3 | Move camera through chunks | All chunks render correctly |
| 9.2.4 | Check for gaps at boundaries | No gaps |

### 9.3 Frustum Culling
| Step | Task | Verification |
|------|------|--------------|
| 9.3.1 | Extract frustum planes from MVP | Planes correct |
| 9.3.2 | Test chunk AABB against frustum | Test works |
| 9.3.3 | Skip draw for culled chunks | Draw calls reduced |
| 9.3.4 | Debug: visualize frustum | Frustum wireframe visible |
| 9.3.5 | Debug: color culled chunks | Red = culled |
| 9.3.6 | Measure: % culled typical | >50% when looking at horizon |

### 9.4 Draw Call Batching
| Step | Task | Verification |
|------|------|--------------|
| 9.4.1 | Collect visible chunks into list | List populated |
| 9.4.2 | Sort by distance (front to back) | List sorted |
| 9.4.3 | Single VkCmdBindVertexBuffers where possible | Reduced binds |
| 9.4.4 | Measure draw calls | Display in debug UI |

---

## Stage 10: Indirect Rendering

### 10.1 Indirect Draw Setup
| Step | Task | Verification |
|------|------|--------------|
| 10.1.1 | Create indirect draw buffer | Buffer created |
| 10.1.2 | Define VkDrawIndexedIndirectCommand | Struct correct |
| 10.1.3 | Populate commands for all chunks | Commands filled |
| 10.1.4 | Use vkCmdDrawIndexedIndirect | Draw works |
| 10.1.5 | Multi-draw indirect | Single call for all chunks |

### 10.2 GPU Culling
| Step | Task | Verification |
|------|------|--------------|
| 10.2.1 | Create culling compute shader | Shader compiles |
| 10.2.2 | Upload chunk AABBs to GPU | Buffer uploaded |
| 10.2.3 | Upload frustum to GPU | Uniform uploaded |
| 10.2.4 | Cull in compute: zero instanceCount if culled | Culling works |
| 10.2.5 | Remove CPU frustum culling | GPU-only culling |
| 10.2.6 | Measure: GPU cull time | <1ms |
| 10.2.7 | Debug: culling stats readback | % culled displayed |

### 10.3 Chunk Data Buffer
| Step | Task | Verification |
|------|------|--------------|
| 10.3.1 | Per-chunk data struct (transform, etc) | Struct defined |
| 10.3.2 | SSBO with all chunk data | Buffer created |
| 10.3.3 | Index into SSBO via gl_DrawID | Correct data accessed |
| 10.3.4 | Update chunk transforms on origin shift | Transforms correct |

---

## Stage 11: Texturing

### 11.1 Texture Loading
| Step | Task | Verification |
|------|------|--------------|
| 11.1.1 | Add stb_image dependency | Links correctly |
| 11.1.2 | Load single texture file | Pixels loaded |
| 11.1.3 | Create VkImage + VkImageView | Image created |
| 11.1.4 | Upload texture data | Data uploaded |
| 11.1.5 | Create VkSampler | Sampler created |
| 11.1.6 | Sample texture in shader | **Textured triangle** |

### 11.2 Texture Array
| Step | Task | Verification |
|------|------|--------------|
| 11.2.1 | Create texture atlas / array | Array image created |
| 11.2.2 | Load multiple block textures | Textures loaded |
| 11.2.3 | Map BlockID → texture layer | Mapping works |
| 11.2.4 | Sample array in shader using BlockID | **Textured blocks** |
| 11.2.5 | Nearest-neighbor sampling | Crisp pixels |
| 11.2.6 | Mipmapping with aniso filtering | No shimmer at distance |

### 11.3 Block Textures
| Step | Task | Verification |
|------|------|--------------|
| 11.3.1 | Create/source basic textures (16x16) | Files exist |
| 11.3.2 | Stone, dirt, grass_top, grass_side | 4+ textures |
| 11.3.3 | Per-face texture support | Grass: top/side different |
| 11.3.4 | Debug: texture browser | Shows all textures |

---

## Stage 12: Procedural Generation

### 12.1 Noise Integration
| Step | Task | Verification |
|------|------|--------------|
| 12.1.1 | Add FastNoise2 dependency | Links correctly |
| 12.1.2 | Create noise generator | Generator created |
| 12.1.3 | Generate 2D noise slice | Values in [-1, 1] |
| 12.1.4 | Verify SIMD enabled | Check compile flags |
| 12.1.5 | Benchmark: 32² samples | <0.1ms |
| 12.1.6 | Debug: noise visualizer | 2D noise texture displayed |

### 12.2 Terrain Heightmap
| Step | Task | Verification |
|------|------|--------------|
| 12.2.1 | Configure noise for terrain | Octaves, frequency set |
| 12.2.2 | Generate height at (x, z) | Height value returned |
| 12.2.3 | Fill chunk based on height | Blocks set correctly |
| 12.2.4 | Stone below, dirt middle, grass top | Layers correct |
| 12.2.5 | Render generated chunk | **Terrain visible** |
| 12.2.6 | Generate 5x5 chunk area | **Rolling terrain** |

### 12.3 World Seed
| Step | Task | Verification |
|------|------|--------------|
| 12.3.1 | Define world seed (uint64) | Seed stored |
| 12.3.2 | Derive chunk seed from world seed + coord | Deterministic |
| 12.3.3 | Seed noise generator per chunk | Same seed = same terrain |
| 12.3.4 | Verify determinism (generate 100x) | All identical |
| 12.3.5 | Console command: set_seed | Regenerates world |

### 12.4 Async Generation
| Step | Task | Verification |
|------|------|--------------|
| 12.4.1 | Chunk generation as job | Job executes on worker |
| 12.4.2 | State: Empty → Generating → Generated | States correct |
| 12.4.3 | Priority: closest chunks first | Order correct |
| 12.4.4 | Cancel on chunk unload | No wasted work |
| 12.4.5 | Budget: max N generating per frame | Limit respected |
| 12.4.6 | Measure generation time | Tracy shows distribution |

---

## Stage 13: Chunk Loading

### 13.1 Load Distance
| Step | Task | Verification |
|------|------|--------------|
| 13.1.1 | Define view distance (chunks) | Configurable |
| 13.1.2 | Calculate chunks in sphere around player | List generated |
| 13.1.3 | Sort by distance to player | Sorted correctly |
| 13.1.4 | Request chunks not yet loaded | Requests queued |
| 13.1.5 | Unload chunks beyond distance | Chunks freed |

### 13.2 Loading Pipeline
| Step | Task | Verification |
|------|------|--------------|
| 13.2.1 | Request → Generate → Mesh → Ready | Pipeline works |
| 13.2.2 | Multiple chunks in-flight | Parallelism |
| 13.2.3 | Frame budget for generation | Budget respected |
| 13.2.4 | Frame budget for meshing | Budget respected |
| 13.2.5 | Frame budget for upload | Budget respected |

### 13.3 Dynamic Loading
| Step | Task | Verification |
|------|------|--------------|
| 13.3.1 | Move camera, new chunks load | Loading works |
| 13.3.2 | Old chunks unload | Memory freed |
| 13.3.3 | No stutter during movement | Smooth FPS |
| 13.3.4 | Walk 1000 blocks in direction | Stable memory, no leaks |

### 13.4 Debug Visualization
| Step | Task | Verification |
|------|------|--------------|
| 13.4.1 | Color chunks by state | Different colors per state |
| 13.4.2 | Show loading queue length | Count displayed |
| 13.4.3 | Show chunks loaded / unloaded per sec | Rate displayed |
| 13.4.4 | Memory graph over time | Graph updates |

---

## Stage 14: Block Interaction

### 14.1 Block Selection
| Step | Task | Verification |
|------|------|--------------|
| 14.1.1 | Ray from camera center | Ray computed |
| 14.1.2 | Ray-voxel traversal (DDA algorithm) | Traversal works |
| 14.1.3 | Find first solid block hit | Block found |
| 14.1.4 | Track which face was hit | Face direction known |
| 14.1.5 | Highlight selected block | **Wireframe on block** |
| 14.1.6 | Reach limit (5 blocks) | Can't select far blocks |

### 14.2 Block Removal
| Step | Task | Verification |
|------|------|--------------|
| 14.2.1 | Left-click sets block to air | Block removed |
| 14.2.2 | Mark chunk dirty | Flag set |
| 14.2.3 | Trigger mesh regeneration | Mesh updates |
| 14.2.4 | Handle chunk boundary | Neighbor chunk also updates |
| 14.2.5 | Remove 100 blocks rapidly | No crash, no lag |

### 14.3 Block Placement
| Step | Task | Verification |
|------|------|--------------|
| 14.3.1 | Right-click places block on face | Block placed |
| 14.3.2 | Cannot place inside player | Collision check |
| 14.3.3 | Place correct block type | Selected type used |
| 14.3.4 | Mesh updates | Visual updates |
| 14.3.5 | Place 100 blocks rapidly | Stable |

### 14.4 Block Selection Hotbar
| Step | Task | Verification |
|------|------|--------------|
| 14.4.1 | Define hotbar (9 slots) | Slots exist |
| 14.4.2 | Number keys 1-9 select slot | Selection works |
| 14.4.3 | Scroll wheel cycles slots | Cycling works |
| 14.4.4 | Display hotbar UI | **Hotbar visible** |
| 14.4.5 | Highlight selected slot | Current slot highlighted |

---

## Stage 15: Player Physics

### 15.1 Collision Detection
| Step | Task | Verification |
|------|------|--------------|
| 15.1.1 | Player AABB (0.6 x 1.8 x 0.6) | Size defined |
| 15.1.2 | Query blocks intersecting AABB | Blocks found |
| 15.1.3 | Resolve collision (push out) | Player pushed out |
| 15.1.4 | X/Y/Z resolution order | No stuck in corners |
| 15.1.5 | Walk into wall | **Blocked** |

### 15.2 Gravity & Jumping
| Step | Task | Verification |
|------|------|--------------|
| 15.2.1 | Apply gravity acceleration | Player falls |
| 15.2.2 | Ground detection | On ground flag set |
| 15.2.3 | Terminal velocity | Max fall speed |
| 15.2.4 | Jump when grounded | Player jumps |
| 15.2.5 | Coyote time (100ms) | Can jump just after edge |
| 15.2.6 | Jump buffering | Jump queued if pressed early |

### 15.3 Movement
| Step | Task | Verification |
|------|------|--------------|
| 15.3.1 | Walk speed (4.3 m/s) | Speed correct |
| 15.3.2 | Sprint speed (5.6 m/s) | Faster when sprinting |
| 15.3.3 | Air control (reduced) | Less control in air |
| 15.3.4 | Smooth acceleration | Not instant |
| 15.3.5 | Step up small blocks (0.5) | Auto-steps |

### 15.4 Fixed Timestep
| Step | Task | Verification |
|------|------|--------------|
| 15.4.1 | Physics at 60 Hz fixed | Tick rate stable |
| 15.4.2 | Accumulator for variable framerate | Works at any FPS |
| 15.4.3 | Interpolation for rendering | Smooth at high FPS |
| 15.4.4 | Debug: physics tick counter | Ticks per second shown |

---

## Stage 16: Level of Detail

### 16.1 LOD Levels
| Step | Task | Verification |
|------|------|--------------|
| 16.1.1 | Define LOD 0 = full detail | Existing mesh |
| 16.1.2 | Define LOD 1 = 2x voxels | Half resolution |
| 16.1.3 | Generate LOD 1 mesh (average blocks) | Mesh generated |
| 16.1.4 | Render LOD 1 at distance | **Coarser mesh visible** |
| 16.1.5 | LOD 2, 3, 4 (4x, 8x, 16x) | All levels work |

### 16.2 LOD Selection
| Step | Task | Verification |
|------|------|--------------|
| 16.2.1 | Calculate LOD level from distance | Level computed |
| 16.2.2 | Hysteresis (different up/down thresholds) | No thrashing |
| 16.2.3 | Per-chunk LOD tracking | Each chunk has level |
| 16.2.4 | Debug: color by LOD | Colors visible |

### 16.3 LOD Transitions
| Step | Task | Verification |
|------|------|--------------|
| 16.3.1 | Transvoxel boundary cells | Cells generated |
| 16.3.2 | Seamless LOD boundaries | **No gaps or seams** |
| 16.3.3 | Transition during movement | No pop-in |
| 16.3.4 | Fly fast, observe transitions | Acceptable quality |

### 16.4 Extended View Distance
| Step | Task | Verification |
|------|------|--------------|
| 16.4.1 | Set view distance to 32 chunks | Distance set |
| 16.4.2 | Generate/mesh at multiple LODs | All chunks loaded |
| 16.4.3 | Measure FPS | >60 FPS |
| 16.4.4 | Measure memory | Within budget |
| 16.4.5 | View distance 64, 128 chunks | Test limits |

---

## Stage 17: Serialization

### 17.1 Chunk Save/Load
| Step | Task | Verification |
|------|------|--------------|
| 17.1.1 | Serialize chunk to bytes | Bytes produced |
| 17.1.2 | LZ4 compression | Smaller size |
| 17.1.3 | Write to file | File created |
| 17.1.4 | Load from file | Chunk restored |
| 17.1.5 | Verify identical | Bit-perfect match |

### 17.2 World Save
| Step | Task | Verification |
|------|------|--------------|
| 17.2.1 | Define save directory structure | Structure defined |
| 17.2.2 | Save modified chunks on unload | Files written |
| 17.2.3 | Load saved chunks before generating | Saved data used |
| 17.2.4 | Player position save | Position persists |
| 17.2.5 | World seed save | Seed persists |

### 17.3 Background Saving
| Step | Task | Verification |
|------|------|--------------|
| 17.3.1 | Save as background job | No main thread stall |
| 17.3.2 | Periodic autosave (5 min) | Saves automatically |
| 17.3.3 | Save on exit | Clean shutdown saves |
| 17.3.4 | Dirty tracking (only save modified) | Minimal writes |

---

## Milestone Checkpoints

### M1: Tech Demo (Stages 1-9)
- [ ] Window with Vulkan rendering
- [ ] Camera movement
- [ ] Debug overlay with FPS/memory
- [ ] Job system functional
- [ ] Floating origin working
- [ ] Basic chunk rendering
- [ ] Frustum culling

### M2: Playable Prototype (Stages 10-15)
- [ ] Indirect rendering
- [ ] Textured blocks
- [ ] Procedural terrain
- [ ] Dynamic chunk loading
- [ ] Block placement/removal
- [ ] Player collision/physics

### M3: Scalable World (Stages 16-17)
- [ ] LOD system working
- [ ] 10km+ view distance
- [ ] World save/load
- [ ] Stable performance
- [ ] Memory within budget

---

## Time Estimates

| Stage | Estimated Duration |
|-------|-------------------|
| 1. Project Bootstrap | 2-3 days |
| 2. Rendering Foundation | 5-7 days |
| 3. Debug Infrastructure | 2-3 days |
| 4. Math & Coordinates | 2-3 days |
| 5. Memory Architecture | 2-3 days |
| 6. Job System | 3-4 days |
| 7. Voxel Data | 3-4 days |
| 8. Mesh Generation | 4-5 days |
| 9. Chunk Rendering | 2-3 days |
| 10. Indirect Rendering | 3-4 days |
| 11. Texturing | 2-3 days |
| 12. Procedural Generation | 3-4 days |
| 13. Chunk Loading | 3-4 days |
| 14. Block Interaction | 2-3 days |
| 15. Player Physics | 3-4 days |
| 16. Level of Detail | 5-7 days |
| 17. Serialization | 2-3 days |

These estimates assume focused work; actual time varies by experience.

