# Connected native actor rendering

`SsaActorScene` composes actor collection/projection, wall clip binding, mixed
depth ordering and actual indexed sprite sampling after each fresh
`SsaWorldScene` geometry draw. It owns queue/projection scratch and borrows the
geometry, world, actor assets and math tables at stable addresses. Its camera
pointer is borrowed only during a draw and cleared on success or failure.

The collector visits visible sectors in ascending order and follows the actual
world sector lists. Each visited stable body needs one source snapshot carrying
animation, displayed frame, facing, owner and flags. Missing/duplicate sources
are errors. The pass mutates geometry's mixed links, so run it once after fresh
geometry, rather than twice on the same wall list. On late failure, earlier
queue/pixel effects remain; the update guards and camera borrow are cleared.

Wall clips come from actual BE60 results. Closed clips use the original121-byte
windows, including the adjacent byte that the original inclusive endpoint can
read. Dynamic clips borrow the remaining stable opening allocation rather than
fabricating a byte beyond the copied span. Original scratch overlap behavior
is preserved within explicit bounded native storage. The framebuffer retains
the120-byte predecessor row required by world columns; actor rasterization uses
the actual14400 pixel area without an extra hidden row.

The viewer exposes Sam by clearing80 only in his source snapshot when the H
chase pose is usable. His canonical flags remain unchanged. D338's second pass
draws the original player overlay, and recovered65/66 period4 animations follow
the idle/walking predicates. This is an explicit PC rendering extension to the
original first-person game. It does not alter gameplay aim or collision.

`test-actor-scene-rom` executes whole originalB6B0 without actor hooks, including
original BSP/wall/plane drawing, D2D0/D338/D490/1238/01828/09238/ARM sampling.
Only BIOS divide/CpuSet and DMA hardware operations are modeled. It compares256
views across all16 maps,3686400 pixels plus the predecessor row, all projected
records, actor/clip/order data, wall/plane/opening state,317 actor commands and
2584 walls. Half the frames advance native and original00908 animation. The
proof uses385 genuine map objects in their original preactivation state.
The original's720-byte opening space can overflow for other camera poses;
unsafe original states are outside the equality oracle, while native storage
is explicitly24000 bytes. See `native-actor-scene-validation.json`.

`test-actor-scene` is a separate ASan/UBSan executable using actual map owners,
player movement, animation, jump/H/input events and both renderer stages. It
passes160522 assertions and688 guards over3840 movement frames on all16 maps,
including1278 airborne and1600 visible-Sam frames. It verifies that canonical
player flags stay unchanged and that initialization/free/draw refuse active
dependency updates. Its116 missing-source failures include100 cases where an
emitted queue prefix survives while raster pixels remain unchanged; camera
borrows clear on every exit. Another64 owner checks verify that freeing scenes
does not destroy borrowed assets/worlds and freeing map actors preserves the
separate player. See `native-actor-scene-integration-validation.json`.

These actor/world pixel tests establish rendering behavior. They do not
establish complete game startup, enemy AI, combat, pickups, HUD, audio or menus.
The executable remains a diagnostic native viewer until those stages are
implemented and integrated.
