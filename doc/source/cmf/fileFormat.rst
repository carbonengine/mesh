CMF File Format
===============

The CMF File format is a binary format used for storing 3D mesh data along with skeletons and animations. It is designed to be efficient for both storage and runtime access, making it suitable for real-time applications such as games and simulations.

File Structure
--------------

A CMF file consists of several sections, each serving a specific purpose.
The file contains a header and several sections. Each section is designed to be relocatable in memory, allowing selective loading of data as needed.

The main sections are:
1. **Header**: Single section conaining data that identifies the file and any data needed to parse the rest of the file.
2. **Main Data**: Mandatory section containing  the main data structures such as meshes, skeletons, and animations.
3. **Raw Data**: Zero or more sections containing raw binary data such as vertex buffers and index buffers.
4. **Metadata**: An optional section at the end containing additional information, not needed for rendering purposes, but rather for other tools.

Structured Data
---------------
Header, Main Data, and Metadata sections contain structured data, which is organized a tree-like data structures defined in `../../include/cmf/v1.h`. These structures are mirrored
directly to disk. The only data container used in these structures is a `Span`: an array-like structure that does not own its memory, very similar to `std::span` in C++20.
When the structures are serialized to disk, all `Span` members pointers are converted to offsets relative to the start of the data section. Hence, loading a structured section
from disk is a matter of reading the entire section into memory and then fixing up all the pointers using a call to `OffsetsToPointers`.

When writing a structured section to disk, the library collects all the memory chunks referenced by `Span` members into a continuous memory block and
converts the pointers to offsets using `PointersToOffsets`. After that, the block is ready to be written to disk. 

Offsets in the structured data may only point to data contained in the same section.

For pointers between sections, the library uses `BufferView` structures, which contain the number of a section, an offset and a size. The offset is relative to the start of the target section.
The entire data, referenced by a `BufferView` must be contained in a single section.

Header Section
--------------
The header section contains the following fields:
- **Magic Number**: A unique identifier for the CMF file format.
- **Version**: The version of the CMF format.
- **CRC32**: A checksum for verifying the integrity of the file. The checksum is calculated over the entire file, after the CRC32 field itself.
- **Section Directory**: List of sections following the header, including their offsets, sizes, compression methods, and other relevant information.

The header must contain a valid magic number, the version number that matches the library version, and a valid CRC32 checksum. It also must contain at least one section: the main data section.
Other sections are optional. When more than one section is present, the first one must always be the main data section, followed by zero or more raw data sections, and finally the optional metadata section.
Sections must not overlap and must be sorted in ascending order of offset. A section may not extend beyond the end of the file.

Structured Data Section
-----------------------
The structured data section contains the main data structures of the CMF file. This includes:
- **Meshes**: Definitions of 3D meshes, including vertex attributes, indices, and material references.
- **Skeletons**: Definitions of skeletons, including bones and their hierarchical relationships.
- **Animations**: Definitions of animations, including keyframes and interpolation methods.

Meshes
^^^^^^
A mesh contains data to render a 3D mesh or a point cloud. CMF supports mesh LODing, sub-materials, skeletal meshes, morph targets.

The file may contain zero or more meshes. Each mesh must contain at least one mesh LOD - the "main", authored geometry. The screen size for the main LOD is always 0xffffffff. If the mesh contains more than
one LOD, the additional LODs must be sorted in descending order of screen size. 

If the mesh represents a point cloud (i.e. its `primitiveType` is `PointList`), mesh LODs may not contain index buffers. If the mesh represents a triangle mesh, each mesh LOD must contain an index buffer. An index buffer
may contain either 16-bit or 32-bit indices, identified by `stride` attribute of their `BufferView`.

The vertex declaration in the `decl` member of the `Mesh` structure defines the layout of each vertex in the vertex buffer. The declaration is a list of `VertexAttribute` structures, each defining an attribute such as position, normal, UV coordinates, etc.
The actual vertex data is stored in the raw data section, referenced by a `BufferView` in the `vb` member of the `MeshLod` structure. The vertex buffer contains an array of vertices, each laid out according to the vertex declaration.
There are certain restrictions on the vertex declaration:
- A vertex declaration must contain at least one attribute.
- A vertex declaration must contain a Position attribute with `usageIndex` 0.
- A vertex declaration may not contain duplicates of (`usage` and `usageIndex`) pairs.
- A vertex declaration element type element count must be between 1 and 4.
- If the vertex declaration contains a `PackedTangent` element, it may not contain a `Normal`, `Tangent` or `Binormal` element with the same `usageIndex`.

The `areas` member of the `Mesh` structure defines the sub-materials of the mesh. Each area defines a range of indices in the index buffer that use a specific material. The number of areas in each mesh LOD must be the same as the number of areas in the mesh.

The mesh `boneBindings` member defines the mapping between bones in the skeleton and the bone indices used in the vertex data. The number of bone bindings must be less than 256 (i.e. there is a limit of 255 bones per mesh).
The skeleton definition may or may not exist in the same file as the mesh. If it does not exist, the application must provide the skeleton at runtime. If the skeleton exists in the file, the
mesh references it as a skeleton through the `skeleton` member. 


Raw Data Sections
-----------------
Raw data sections contain binary data such as vertex buffers and index buffers. Each raw data section is referenced by the structured data section through `BufferView` structures.
Normally, a 3D application would load these sections into GPU memory after possibly decompressing them.

Metadata Section
----------------
The metadata section is optional and can contain any additional information that is not essential for rendering. The data stored in this section is a collection of key-value pairs, 
where keys and values are strings.

