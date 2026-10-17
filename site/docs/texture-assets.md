# Original world texture archives

`src/texture_assets.c` recovers the real uncompressed WOD0 resources and the
original loader/name-join behavior. It copies original indexed pixels into
native owned memory. The static conversion helper exposes row-major images;
it does not draw a game scene or replace missing renderer behavior.

## Archives and format

The resource pointer table at **0844AEF4** supplies these six archives:

| Family | Resolution | Entry | Address | Images |
| --- | --- | --- | --- | --- |
| 0 | Base | 16 | 08526F48 | 101 |
| 0 | Half | 17 | 085FEDE0 | 101 |
| 0 | Quarter | 18 | 08635278 | 101 |
| 1 | Base | 19 | 08643090 | 121 |
| 1 | Half | 20 | 08749868 | 121 |
| 1 | Quarter | 21 | 0878B840 | 121 |

Together these contain **666 images and 2,567,040 pixel bytes**. Level setup
1F44 selects family0 for levels0..5,12,13 and family1 for levels6..11,14,15.
The corresponding background palettes are documented in `sprite-assets.md`.
All three resolutions have the same names/order in the actual archives, but
the native lookup retains the original name-based mapping rather than assuming
equal indices. Actual base dimensions range from32x64 to128x128; the lower
resolutions halve both dimensions successively. Some base textures are32x128
or128x64. They are not all square.

| Archive offset | Meaning |
| --- | --- |
| 00 | ASCII `WOD0` |
| 04 | Unknown header word; not used as a byte length |
| 08 | Unsigned16 texture count |
| 20 | Unsigned32 descriptor-table offset relative to archive base |
| 48 | Beginning of actual pixel storage |

The original header is **72 bytes**, not a generic WAD header. Each descriptor
is **16 bytes**: `name[8], u16 width, u16 height, u32 payload_offset`.
Payload offsets are relative to the **archive base**. Every actual payload
precedes its archive's descriptor table. Other header bytes are not interpreted
by this decoder. Names may occupy all eight bytes, commonly include trailing
spaces, and are not assumed to be terminated strings.

Original **AFF4** allocates and copies the72byte header, allocates/copies its
descriptor table, and replaces every payload offset with an absolute ROM
pointer. Pixel bytes themselves are neither copied nor decompressed there.
Single-player uses DMA; multiplayer uses CpuSet. A zero count returnsNULL after
the original header allocation. Native loading rejects it without retaining
an allocation. The native decoder additionally checks magic and bounds, which
the original trusted asset loader does not.

## Name joins and pixel order

Original **AF18** allocates a12byte pointer triple for every base texture at
WORLD+10 (`03004940`). It initializes all three pointers to the base payload,
then searches each lower-resolution archive independently. The **first**
case-insensitive eight-byte match replaces the respective pointer. Missing
matches retain the base pointer. Its missing-half-resolution name copy to a
temporary string does not change this fallback.

The actual comparison is **080C57F4**. Its classification table at08449385
marks only ASCII A..Z as uppercase, adding32 for comparison. Other bytes,
including bytes128..255, remain unchanged. A NUL terminates comparison; spaces
do not. The native API returns a nonnegative `int` index; B0FC's separate
signed16 conversion belongs to `map_texture_bindings.c`.

The renderer establishes **column-major pixels**, with address
`payload + x * height + y`. In the bounded original **BE60 block C9DA..CA4A**,
R8 is the selected column and R9 the base height. It selects the mip pointer
from scratch+418+4*mip, computes `height = base_height >> mip`, and copies
`height/2` halfwords beginning at `mip_pointer + column*height` into scratch+298.
Both hardware paths do this. Every real image has an even height. The following
ARM column sampler uses those contiguous bytes; its recovery is documented in
`world-raster.md` separately. This block proves storage order, not complete
BE60 clipping, UV generation, lighting, mip choice, sky or frame composition.

`ssa_texture_image_indices` transposes the original column storage into a
caller-owned row-major indexed raster. Index0 is preserved as data; the texture
decoder does not invent sprite transparency or palette effects. Local PNG
previews use the original background RGB555 palette with entry0 cleared.

## API and ownership

Zero-initialize `SsaTextureAssets`, call `ssa_texture_assets_load`, and free it
with `ssa_texture_assets_free` before reuse. Successful loading owns all pixel,
descriptor and mapping storage; the ROM buffer may then be released. Loading
is atomic: errors free partial allocations and leave the destination unchanged.
Already-owned output is rejected. Storage and metadata must remain unchanged
while borrowed by map/renderer adapters. Freeing borrowed images is invalid.

`families[2].archives[3]` contains descriptor-order images. Each image retains
its exact eight name bytes, dimensions and original payload ROM offset.
`mip_indices[base_index][mip]` records the AF18 selection; FFFF means fallback
to the base image. `ssa_texture_assets_image` resolves that fallback and returns
a borrowed image with its actual dimensions. Original raster code may continue
using shifted base dimensions even on fallback; the getter does not rewrite
or fabricate lower-resolution data to conceal that behavior.

`ssa_texture_archive_find` supports caller-owned arrays through the full u16
count range and returns the first match or-1. Loader allocations are separately
bounded to1,024 descriptors per archive,1,024 pixels per dimension and16MiB
total copied pixel storage. These limits exceed all actual assets. It validates
every table/header/payload span and requires pixels between the72byte header
and descriptor table. Malformed metadata, unsupported magic, zero dimensions,
truncation and excessive aggregate allocations fail explicitly.

The raster helper validates its complete output span before writing, including
overflow-sized strides. It writes only width bytes per row, preserving padding.
The output must not alias source pixels. Its pointer/size metadata, like other
native borrowed structures, must describe valid live storage.

## Evidence and limitations

`tests/test_texture_assets.py` compiles the native decoder with undefined
behavior checks and runs a separate ASan+UBSan fixture. It verifies:

- Exact metadata and every pixel byte for all666 actual images.
- **84 complete original AFF4 calls and28 complete AF18 calls**, including
  all six archives, both copy paths, reordered names, mixed case, duplicate
  first matches and absent mip names. Only allocation and hardware copying
  are controlled. The real name comparator and missing-name strncpy execute.
- **65,539 original name comparisons**, including all65,536 one-byte pairs,
  embedded NULs and full eight-byte names.
- **55,440 bounded original BE60 column-copy blocks**, covering every column
  of every actual image through both DMA/CpuSet paths. This is explicitly a
  block comparison, not whole-renderer coverage.
- **2,717 malformed inputs**, **2,671 guards**, and **4,677 ASan+UBSan
  assertions**, including storage ownership after freeing the input ROM.

The JSON report records the ROM/source hashes and all666 payload hashes. Test
previews under ignored `build/native-tests/texture-family*-000.png` contain
original artwork; no extracted image is added to source directories.

The requested decomp.wtf engine was used to claim and compile AFF4. Two manual
attempts reached **99.2% similarity (score75)**, not an exact match. Remaining
differences are a count-load register, instruction scheduling and one add's
operand order. The claim was released with notes so another helper could use
the shared matcher; `recovered/texture_archive.c` retains the candidate. This
native behavior recovery adds no exact-match count. World projection and a
playable native frontend remain separate work.
