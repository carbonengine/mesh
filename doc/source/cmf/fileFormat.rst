CMF File Format
===============

The CMF File format is a binary format used for storing 3D mesh data along with skeletons and animations. It is designed to be efficient for both storage and runtime access, making it suitable for real-time applications such as games and simulations.

File Structure
--------------

A CMF file consists of several sections, each serving a specific purpose.
The file contains a header and several sections. Each section is designed to be relocatable in memory, allowing selective loading of data as needed.

The main sections are:

1. **Header**: Single section conaining data that identifies the file and any data needed to parse the rest of the file.
2. **Main Data**: Mandatory section containing the main data structures such as meshes, skeletons, and animations.
3. **Raw Data**: Zero or more sections containing raw binary data such as vertex buffers and index buffers.
4. **Metadata**: An optional section at the end containing additional information, not needed for rendering purposes, but rather for other tools.

Structured Data
---------------
Header, Main Data, and Metadata sections contain structured data, which is organized a tree-like data structures defined in `../../include/cmf/v1.h`. These structures are mirrored
directly to disk. The only data container used in these structures is a ``cmf::Span``: an array-like structure that does not own its memory, very similar to ``std::span`` in C++20.
When the structures are serialized to disk, all ``cmf::Span`` members pointers are converted to offsets relative to the start of the data section. Hence, loading a structured section
from disk is a matter of reading the entire section into memory and then fixing up all the pointers using a call to ``cmf::OffsetsToPointers``.

When writing a structured section to disk, the library collects all the memory chunks referenced by ``cmf::Span`` members into a continuous memory block and
converts the pointers to offsets using ``cmf::PointersToOffsets``. After that, the block is ready to be written to disk. 

Offsets in the structured data may only point to data contained in the same section.

For pointers between sections, the library uses ``cmf::BufferView`` structures, which contain the number of a section, an offset and a size. The offset is relative to the start of the target section.
The entire data, referenced by a ``cmf::BufferView`` must be contained in a single section.

Header Section
--------------

The header section is always the first part of a CMF file and contains essential information needed to parse the rest of the file. The header structure contains the following fields:

- **Magic Number (signature)**: A 4-byte unique identifier (``FILE_SIGNATURE``) for the CMF file format. This allows applications to quickly verify that a file is indeed a CMF file.
- **Version**: A 4-byte version number indicating which version of the CMF specification the file conforms to. The current version is 1 (``FILE_VERSION = 1``).
- **Header Size**: A 4-byte value indicating the total size of the header in bytes, including the section directory.
- **CRC32**: A 4-byte checksum for verifying the integrity of the file. The checksum is calculated over the entire file excluding the signature, version, headerSize, and CRC32 fields themselves.
- **Section Directory**: A list of ``cmf::Section`` structures describing all sections that follow the header.

Each ``cmf::Section`` entry in the directory contains:

- **offset**: Byte offset from the start of the file to the beginning of the section data.
- **compressedSize**: Size of the section data as stored in the file (in bytes).
- **uncompressedSize**: Size of the section data after decompression (in bytes). Equal to ``compressedSize`` if the section is not compressed.
- **gpuAlignment**: Required memory alignment for GPU buffer sections (0 if no specific alignment is needed).
- **type**: Type of the section (Data, GpuBuffer, or Metadata).
- **compression**: Compression method used for the section (None, MeshOptimizerVertexBuffer, or MeshOptimizerIndexBuffer).

File Validation
^^^^^^^^^^^^^^^

A valid CMF file must satisfy the following requirements:

- The header must contain a valid magic number matching ``FILE_SIGNATURE``.
- The version number must match the library version (currently 1).
- The CRC32 checksum must match the calculated checksum of the file contents.
- At least one section must be present: the main Data section.
- The first section must always be the main Data section (type = ``SectionType::Data``).
- If a Metadata section is present, it must be the last section.
- All sections between the Data section and optional Metadata section must be GpuBuffer sections.
- Sections must not overlap in the file.
- Sections must be sorted in ascending order by their offset.
- No section may extend beyond the end of the file.

Structured Data Section
-----------------------

The structured data section (also called the "main Data section") is the first section after the header and contains the primary data structures of the CMF file. This section is mandatory 
and contains the top-level ``cmf::Data`` structure, which holds:

- **meshes**: An array of ``cmf::Mesh`` structures defining 3D geometry.
- **skeletons**: An array of ``cmf::Skeleton`` structures defining bone hierarchies for skeletal animation.
- **animations**: An array of ``cmf::Animation`` structures defining keyframe animations.

All arrays may be empty (zero-length), but at least one of them typically contains data for the file to be useful. The structured data uses ``cmf::Span`` arrays with relative offsets 
that must be converted to pointers after loading via ``cmf::OffsetsToPointers``.

References between structures within this section use indices (e.g., a mesh references a skeleton by index). References to data in other sections use ``cmf::BufferView`` structures 
with section indices and offsets.

Meshes
^^^^^^

A mesh contains data to render a 3D mesh or a point cloud. CMF supports mesh LODing, sub-materials (areas), skeletal animation, and morph targets. Each mesh in the file is defined 
by a ``cmf::Mesh`` structure containing:

- **name**: A unique identifier for the mesh.
- **decl**: Vertex declaration defining the layout of vertex data.
- **lods**: Array of LOD levels with increasing visual simplification.
- **areas**: Array of material regions within the mesh.
- **boneBindings**: Mapping of bones for skeletal animation.
- **morphTargets**: Definitions of morph target (blend shape) data.
- **uvDensities**: Per-area UV density metrics for texture LOD.
- **bounds**: Axis-aligned bounding box enclosing all geometry.
- **audioOcclusionMesh**: Optional simplified mesh for audio calculations.
- **topology**: Primitive type (TriangleList or PointList).
- **skeleton**: Index of the skeleton used by this mesh (0xff if no skeleton).

Mesh Topology
+++++++++++++

The ``topology`` field specifies how vertices are connected, using the ``MeshTopology`` enum:

- **TriangleList**: Each set of 3 indices defines an independent triangle. This is the most common topology for rendering solid objects.
- **PointList**: Each vertex is rendered as an independent point. This topology is used for point clouds and particle effects.

When ``topology`` is ``TriangleList``, all mesh LODs must include an index buffer. When ``topology`` is ``PointList``, mesh LODs must not contain index buffers, and rendering uses the 
vertices directly in the order they appear in the vertex buffer.

Level of Detail (LOD)
+++++++++++++++++++++

The file may contain zero or more meshes. Each mesh must contain at least one mesh LOD - the "main", authored geometry. The screen size for the main LOD is always ``0xffffffff`` (``MeshLod::MAX_THRESHOLD``). 
If the mesh contains more than one LOD, the additional LODs must be sorted in descending order of screen size (threshold value).

Each ``cmf::MeshLod`` contains:

- **vb**: BufferView referencing the vertex buffer in a GpuBuffer section.
- **ib**: BufferView referencing the index buffer in a GpuBuffer section (empty for point clouds).
- **areas**: Array of ``cmf::LodMeshArea`` structures defining index ranges for each material area in this LOD.
- **morphTargets**: Array of vertex buffers for morph target deltas in this LOD.
- **threshold**: Maximum screen diameter in pixels at which this LOD should be visible. LODs with higher thresholds are used when the object appears larger on screen.

The LOD system allows applications to automatically select appropriate mesh detail based on distance from camera, screen coverage, or performance budgets.

Vertex Declaration and Buffers
+++++++++++++++++++++++++++++++

If the mesh represents a point cloud (i.e. its ``topology`` is ``PointList``), mesh LODs may not contain index buffers. If the mesh represents a triangle mesh, each mesh LOD must contain an index buffer. An index buffer
may contain either 16-bit or 32-bit indices, identified by ``stride`` attribute of their ``cmf::BufferView`` (2 bytes for 16-bit, 4 bytes for 32-bit).

The vertex declaration in the ``decl`` member of the ``cmf::Mesh`` structure defines the layout of each vertex in the vertex buffer. The declaration is a list of ``cmf::VertexElement`` structures, each defining an attribute such as position, normal, UV coordinates, etc.
The actual vertex data is stored in the raw data section, referenced by a ``cmf::BufferView`` in the ``vb`` member of the ``cmf::MeshLod`` structure. The vertex buffer contains an array of vertices, each laid out according to the vertex declaration.

Each ``cmf::VertexElement`` specifies:

- **usage**: Semantic meaning of the attribute (Position, Normal, TexCoord, etc.).
- **usageIndex**: Index for attributes that may appear multiple times (e.g., TexCoord0, TexCoord1).
- **type**: Data type of each component (Float32, Float16, UInt8Norm, etc.).
- **elementCount**: Number of components (1-4).
- **offset**: Byte offset of this attribute within a single vertex.

There are certain restrictions on the vertex declaration:

- A vertex declaration must contain at least one attribute.
- A vertex declaration may not contain duplicates of (``usage`` and ``usageIndex``) pairs.
- A vertex declaration element count must be between 1 and 4.
- If the vertex declaration contains a ``PackedTangent`` element, it may not contain a ``Normal``, ``Tangent`` or ``Binormal`` element with the same ``usageIndex``.

The total vertex size (stride) is determined by the maximum of (offset + elementSize) across all vertex elements, where elementSize = elementCount * sizeOf(type).

Mesh Areas
++++++++++

The ``areas`` member of the ``cmf::Mesh`` structure defines the sub-materials of the mesh. Each ``cmf::MeshArea`` contains:

- **name**: Identifier for the material area (often corresponds to a material name).
- **bounds**: Axis-aligned bounding box for geometry in this area.
- **bones**: Array of bone indices that influence vertices in this area (used for frustum culling optimization).
- **affectedByBones**: Boolean flag indicating whether this area uses skeletal animation.
- **affectedByMorphTargets**: Boolean flag indicating whether this area is affected by morph targets.

For each LOD, the ``areas`` member contains corresponding ``cmf::LodMeshArea`` structures that specify the index ranges for a subset of the mesh index buffer that belongs to each area. Each ``cmf::LodMeshArea`` contains:

- **firstElement**: Starting primitive in the index buffer for this area.
- **elementCount**: Number of primitives used by this area.

For triangle meshes, the index buffer is interpreted as a list of triangles, so each primitive corresponds to 3 indices. For point clouds, each primitive corresponds to a single vertex. 

The number of areas in each mesh LOD must be the same as the number of areas in the mesh. This allows applications to render each material area separately while sharing vertex data. 

Bone Bindings
+++++++++++++

The mesh ``boneBindings`` member defines bones of the skeleton that affect this mesh. 
When the mesh contains BoneIndices vertex element, the values of indices in that element refer to the bone bindings 
defined in this array. Each ``cmf::BoneBinding`` contains:

- **name**: Name of the bone (must match a bone name in the referenced skeleton).
- **bounds**: Bounding box of vertices influenced by this bone.

The skeleton definition may or may not exist in the same file as the mesh. If it does not exist, the application must provide the skeleton at runtime. If the skeleton exists in the file, the
mesh references it through the ``skeleton`` member (an index into the skeletons array).

Morph Targets
^^^^^^^^^^^^^

Morph targets (also known as blend shapes) allow for deformation of mesh geometry, commonly used for facial animations or other detailed deformations. The CMF format supports morph targets
as part of the mesh definition.

The ``morphTargets`` member of the ``cmf::Mesh`` structure contains:

- **decl**: A vertex declaration defining which vertex attributes are affected by morph targets and their layout in morph target vertex buffers (typically Position, Normal, Tangent).
  The same vertex declaration is used for all morph targets of this mesh and must be compatible with the vertex declaration of the main 
  mesh (e.g., if the morph target has a Normal attribute with usage index 0, the mesh declaration must also have a Normal attribute with usage index 0).
- **targets**: An array of ``cmf::MorphTarget`` structures, each with a name and ``maxDisplacement`` value indicating the maximum distance any vertex moves when this target is fully applied.

For each mesh LOD, the ``morphTargets`` member contains an array of ``cmf::LodMorphTarget`` structures, each referencing a vertex buffer containing the values 
for that morph target vertex elements. The vertex data layout must match the morph target vertex declaration in the main mesh structure, and
the number of vertices in each morph target vertex buffer must match the number of vertices in the main mesh vertex buffer for that LOD.

Audio Occlusion Mesh
^^^^^^^^^^^^^^^^^^^^

The ``audioOcclusionMesh`` member of the ``cmf::Mesh`` structure contains an optional simplified triangle mesh used for audio occlusion calculations. This mesh typically has much lower
polygon count than the visual mesh to improve performance of audio ray-casting operations. Since the mesh is normally very small and is
used on CPU, the mesh data is contained directly in the structure. The audio occlusion mesh contains:

- **vertices**: Array of 3D vertex positions.
- **indices**: Array of triangle indices (using 16-bit unsigned integers).
- **bounds**: Axis-aligned bounding box enclosing the mesh.

UV Densities
^^^^^^^^^^^^

The ``uvDensities`` member of the ``cmf::Mesh`` structure contains per-area UV density values, which can be used for texture LOD selection and streaming optimization. Each value represents
the ratio of texture space to world space for the corresponding mesh area.

Skeletons
^^^^^^^^^

A skeleton defines a hierarchical bone structure used for skeletal animation. The file may contain zero or more skeletons. Each skeleton contains:

- **name**: A unique identifier for the skeleton.
- **bones**: An array of bone names in the skeleton.
- **parents**: An array of parent bone indices, defining the hierarchical structure. Root bones must use ``0xffffffff`` as the "no parent" sentinel value.
- **restTransforms**: An array of ``cmf::Transform`` structures defining the default pose of each bone in its parent's coordinate space. Each transform contains position (Vector3), rotation (Quaternion), and scale (Vector3).
- **invBindTransforms**: An array of inverse bind pose matrices. These matrices transform vertices from model space to bone space for skinning calculations.
- **boneMasks**: An optional array of ``cmf::BoneMask`` structures defining named subsets of bones with associated weights. Bone masks can be used to blend animations on different parts of the skeleton independently.

The bone hierarchy must form a valid tree structure with no cycles. Each non-root bone must have exactly one parent whose index is less than the bone's own index; roots use ``0xffffffff``. 
The arrays ``bones``, ``parents``, ``restTransforms``, and ``invBindTransforms`` must all have the same length.

The ``boneMasks`` member allows defining named subsets of bones with associated weights. This can be useful for blending animations on different parts of the skeleton independently (e.g., upper body vs lower body).
A ``cmf::BoneMask`` contains:

- **name**: Identifier for the mask.
- **weights**: Array of ``cmf::BoneWeight`` structures, each specifying a bone index and a weight value (0.0 to 1.0) indicating how much this bone is affected by the mask.

Animations
^^^^^^^^^^

Animations define how skeleton bones or morph targets change over time. The file may contain zero or more animations. Each animation contains:

- **name**: A unique identifier for the animation.
- **channels**: An array of ``cmf::AnimationChannel`` structures defining what is being animated.
- **curves**: An array of ``cmf::AnimationCurve`` structures containing the actual keyframe data.
- **duration**: The total length of the animation in seconds.

Each ``cmf::AnimationChannel`` specifies:

- **target**: The name of the bone or morph target being animated.
- **targetType**: The type of animation target (``BonePosition``, ``BoneRotation``, ``BoneScale``, ``MorphTarget``, or ``Other``).
- **curveIndex**: Index into the ``curves`` array indicating which curve provides the animation data for this channel.

Note that the referenced curve dimension should match the target type: BonePosition and BoneScale channels  use 3D vector curves, 
BoneRotation channels  use quaternion curves (4D), and MorphTarget channels  use scalar curves (1D).

Each ``cmf::AnimationCurve`` contains:

- **valueDimension**: Number of components in each value (1 for scalars, 3 for vectors, 4 for quaternions).
- **interpolation**: Interpolation method between keyframes (``Step`` for instant transitions, ``Linear`` for linear interpolation).
- **knotType**: Data type used for time values.
- **valueType**: Data type used for animated values.
- **knotCount**: Number of keyframes in the curve.
- **knots**: Raw binary data containing time values for each keyframe.
- **values**: Raw binary data containing the animated values at each keyframe.

The ``knots`` array must contain ``knotCount`` values of type ``knotType``, representing the time in seconds for each keyframe. The ``values`` array must contain ``knotCount * valueDimension``
values of type ``valueType``. Knot times must be sorted in ascending order and should fall within the range [0, duration] of the animation.

Raw Data Sections
-----------------

Raw data sections contain binary data such as vertex buffers, index buffers. Each raw data section is referenced by the structured data section through ``cmf::BufferView`` structures.
A raw data section may optionally be compressed to reduce file size. Normally, a 3D application would load these sections into GPU memory after possibly decompressing them.

Section Types
^^^^^^^^^^^^^

The CMF format supports three types of sections, identified by the ``SectionType`` enum:

1. **Data**: Contains the main structured data. The first section after the header must always be of this type.
2. **GpuBuffer**: Contains raw binary data intended to be loaded directly into GPU memory, such as vertex buffers and index buffers. These sections may have GPU-specific alignment requirements specified by the ``gpuAlignment`` field.
3. **Metadata**: Contains additional structured data not essential for rendering. If present, this must be the last section in the file.

Compression
^^^^^^^^^^^

Raw data sections may be compressed using one of the following methods, specified by the ``SectionCompression`` enum:

- **None**: No compression applied. The ``compressedSize`` and ``uncompressedSize`` fields in the section descriptor will be equal.
- **MeshOptimizerVertexBuffer**: Compressed using the meshoptimizer library's vertex buffer compression algorithm, which is optimized for vertex cache locality and compression ratio.
- **MeshOptimizerIndexBuffer**: Compressed using the meshoptimizer library's index buffer compression algorithm, which exploits the specific properties of index data.

When a section is compressed, the ``compressedSize`` field indicates the actual size of the data in the file, while ``uncompressedSize`` indicates the size after decompression. 
Applications must decompress the data before use. The main structured Data section and Metadata section are typically not compressed.

GPU Alignment
^^^^^^^^^^^^^

The ``gpuAlignment`` field in a section descriptor specifies the alignment requirement for GPU buffers. 
When loading a GpuBuffer section, the application should ensure the decompressed data is allocated at an address that satisfies the 
alignment requirement before uploading to GPU memory. Note that for compressed sections, the ``gpuAlignment`` is used by the decompression algorithm 
and must match the underlying buffer stride.

BufferView References
^^^^^^^^^^^^^^^^^^^^^

Raw data sections are accessed through ``cmf::BufferView`` structures, which contain:

- **index**: The section index in the header's section directory (0-based, where 0 is the main Data section).
- **offset**: Byte offset from the start of the section to the start of the referenced data.
- **size**: Total size of the data in bytes.
- **stride**: Size of each element in bytes. For example, a vertex buffer might have stride = 32 (bytes per vertex), and an index buffer using 16-bit indices would have stride = 2.

The referenced data ``[offset, offset + size)`` must be entirely contained within the target section. For index buffers, the stride field (2 for 16-bit indices, 4 for 32-bit indices) 
determines the index type. For vertex and morph target buffers, the stride should match the size calculated from the vertex declaration.

Metadata Section
----------------

The metadata section is optional and can contain any additional information that is not essential for rendering. If present, this section must be the last section in the file.

The metadata is stored as a collection of key-value pairs through the ``cmf::Metadata`` structure, which contains:

- **entries**: An array of ``cmf::MetadataEntry`` structures.

Each ``cmf::MetadataEntry`` contains:

- **key**: A string identifier for the metadata field.
- **value**: A string value associated with the key.

Common Uses
^^^^^^^^^^^

Metadata can be used to store various types of information, including but not limited to:

- **Source Information**: Original file paths, authoring tool names and versions, export timestamps.
- **Asset Properties**: Artist notes, copyright information, usage guidelines.
- **Build Information**: Build system identifiers, version control commit hashes, build timestamps.
- **Custom Tool Data**: Application-specific data that doesn't fit into the standard CMF structures.

The metadata format is intentionally flexible, allowing applications to define their own conventions for key naming and value formatting. Since metadata is optional and not used for rendering,
it can be stripped from files in production builds to reduce file size if desired.

Example Metadata Entries
^^^^^^^^^^^^^^^^^^^^^^^^

Typical metadata entries might look like:

- ``key="sourceFile", value="character.fbx"``
- ``key="exportTool", value="CMF Exporter 1.0"``
- ``key="exportDate", value="2024-01-15T10:30:00Z"``
- ``key="author", value="Artist Name"``
- ``key="copyright", value="Copyright 2024 Company Name"``

Data Types and Limitations
---------------------------

Element Types
^^^^^^^^^^^^^

The CMF format supports various element types for vertex data, animation curves, and other numeric data, defined by the ``ElementType`` enum:

- **Float32**: 32-bit IEEE floating point (most common, high precision).
- **Float16**: 16-bit half-precision floating point (smaller storage, lower precision).
- **UInt16Norm**: 16-bit unsigned integer normalized to [0, 1] range.
- **UInt16**: 16-bit unsigned integer (used for bone indices and similar data).
- **Int16Norm**: 16-bit signed integer normalized to [-1, 1] range.
- **Int16**: 16-bit signed integer.
- **UInt8Norm**: 8-bit unsigned integer normalized to [0, 1] range (often used for colors).
- **UInt8**: 8-bit unsigned integer (often used for bone indices).
- **Int8Norm**: 8-bit signed integer normalized to [-1, 1] range.
- **Int8**: 8-bit signed integer.

Normalized integer types are useful for reducing memory footprint while maintaining acceptable precision for data like normals, colors, and texture coordinates. The application is responsible 
for converting between normalized integer representations and floating-point values during rendering.

Vertex Attributes
^^^^^^^^^^^^^^^^^

The ``Usage`` enum defines the semantic meaning of vertex attributes:

- **Position**: 3D position in model space (required, must have usageIndex 0).
- **Normal**: Surface normal vector.
- **Tangent**: Tangent vector for normal mapping.
- **Binormal**: Binormal (bitangent) vector for normal mapping.
- **TexCoord**: Texture coordinates (UV), may have multiple sets with different usageIndex values.
- **Color**: Vertex colors, may have multiple sets.
- **BoneIndices**: Indices of bones influencing this vertex (for skeletal animation).
- **BoneWeights**: Weights of bone influences (must sum to 1.0 for each vertex).
- **PackedTangent**: Compressed tangent space (combines normal, tangent, and binormal).
- **PackedTangentLegacy**: Legacy format (angle-based) for packed tangent space (deprecated).

File Format Constraints
^^^^^^^^^^^^^^^^^^^^^^^

The CMF format has several important constraints:

- Maximum of 255 skeleton references per file (8-bit skeleton index in Mesh, with 0xff meaning no skeleton).
- Vertex element count must be between 1 and 4 components.
- Packed tangent formats are mutually exclusive with separate Normal/Tangent/Binormal attributes for the same usageIndex.
- Index buffers may use either 16-bit or 32-bit indices, determined by the stride field (2 or 4 bytes).
- Animation curve knot times must be sorted in ascending order.
- Bone hierarchy must form a valid tree with no cycles.

Memory Layout and Performance Considerations
---------------------------------------------

Relocatable Sections
^^^^^^^^^^^^^^^^^^^^

All CMF sections are designed to be relocatable in memory. This means:

- The main Data section and Metadata section use relative offsets (via ``cmf::Span``) instead of absolute pointers, allowing them to be loaded anywhere in memory.
- After loading a structured section, applications should call ``cmf::OffsetsToPointers`` to convert relative offsets to usable pointers based on the section's actual memory address.
- GpuBuffer sections can be loaded independently and don't contain pointer data.

Selective Loading
^^^^^^^^^^^^^^^^^

The section-based structure enables selective loading strategies:

- Load only the header and main Data section initially, deferring GpuBuffer sections until needed.
- Load different mesh LODs on demand based on distance or visibility.
- Stream GpuBuffer sections asynchronously while rendering with lower LODs.
- Skip loading the Metadata section entirely in production builds.
