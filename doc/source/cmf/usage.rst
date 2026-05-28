Basic Usage Guide
=================

This guide covers the basic usage of the Carbon Mesh library (CMF), including how to load files, validate them, and access the data they contain.

Loading a CMF File
------------------

To load a CMF file, you need to read the file into memory and then cast the data to the appropriate structures. Here's a basic example:

.. code-block:: cpp

    #include <cmf/utils.h>
    #include <cmf/cmf.h>
    #include <vector>
    #include <cstdio>

    std::vector<uint8_t> LoadFileToMemory(const char* filename) {
        FILE* file = fopen(filename, "rb");
        if (!file) {
            return {};
        }

        fseek(file, 0, SEEK_END);
        size_t fileSize = ftell(file);
        fseek(file, 0, SEEK_SET);

        std::vector<uint8_t> fileData(fileSize);
        size_t bytesRead = fread(fileData.data(), 1, fileSize, file);
        fclose(file);

        if (bytesRead != fileSize) {
            return {};
        }

        return fileData;
    }

Validating the File
-------------------

**File validation is critical** before accessing any data in a CMF file. Validation ensures that:

1. The file has a valid CMF signature and version
2. The CRC32 checksum matches, confirming data integrity
3. The header structure is well-formed
4. All section offsets and sizes are valid
5. The main data structures are consistent and safe to access

Without validation, attempting to access a corrupted or malicious file could lead to crashes, security vulnerabilities, or undefined behavior.

The library provides the ``cmf::ValidateFile()`` function for this purpose:

.. code-block:: cpp

    #include <cmf/utils.h>

    auto fileData = LoadFileToMemory("model.cmf");
    if (fileData.empty()) {
        // Handle file read error
        return;
    }

    // Validate the file with all checks enabled
    cmf::ValidationOptions options;
    options.validateCrc = true;         // Verify CRC32 checksum
    options.validateHeader = true;      // Check header structure
    options.validateMainData = true;    // Validate main data section

    auto result = cmf::ValidateFile(fileData.data(), fileData.size(), options);
    if (!result) {
        printf("Validation failed: %s\n", result.error.c_str());
        return;
    }

**Important:** Always validate files from untrusted sources or disk storage. For files in trusted memory (e.g., already validated and stored in a content pipeline), you may skip some validation checks for performance.

Converting Offsets to Pointers
-------------------------------

CMF files store data structures with **offsets** instead of pointers. This allows the file format to be 
position-independent and relocatable. However, before you can use the data structures in memory, you should 
convert these offsets to actual memory pointers.

This is done by calling ``cmf::OffsetsToPointers()`` on each structured section after loading it into memory.

**Why is this important?**

- CMF uses ``Span`` structures (similar to ``std::span``) to represent arrays within the data
- On disk, these Spans store relative offsets to their data
- Spans allow accessing elements even if they use offsets instead of pointers
- Copying the span object that stores offsets will not work correctly, as the offsets will point to the wrong location in memory
- It is safer to convert offsets to pointers once after loading the file, so you can use the data as normal C++ structures without worrying about offsets

Here's how to properly load and prepare a CMF file for use:

.. code-block:: cpp

    #include <cmf/cmf.h>

    // After validation, cast the data to the Header structure
    auto* header = reinterpret_cast<cmf::Header*>(fileData.data());

    // Convert offsets to pointers in the header
    cmf::OffsetsToPointers(*header);

    // Access the main data section (always at index 0)
    auto* data = reinterpret_cast<cmf::Data*>(
        fileData.data() + header->sections[0].offset
    );

    // Convert offsets to pointers in the main data
    cmf::OffsetsToPointers(*data);

After calling ``OffsetsToPointers()``, all ``Span`` members in the structures become usable as normal 
array-like containers.

Accessing Data in the File
---------------------------

Once the file is loaded, validated, and offsets are converted to pointers, you can access the data through the ``cmf::Data`` structure.

The Data Structure
^^^^^^^^^^^^^^^^^^

The ``cmf::Data`` structure contains three main collections:

- ``meshes``: Array of 3D mesh definitions
- ``skeletons``: Array of skeletal armatures
- ``animations``: Array of animation sequences

Each of these is a ``Span`` that can be iterated like a standard container.

Accessing Meshes
^^^^^^^^^^^^^^^^

.. code-block:: cpp

    // Iterate through all meshes in the file
    for (const auto& mesh : data->meshes) {
        printf("Mesh: %s\n", mesh.name.data());
        printf("  Vertex count: %u\n", mesh.lods[0].vb.size / mesh.lods[0].vb.stride);
        printf("  Bounds: min(%.2f, %.2f, %.2f) max(%.2f, %.2f, %.2f)\n",
            mesh.bounds.min.x, mesh.bounds.min.y, mesh.bounds.min.z,
            mesh.bounds.max.x, mesh.bounds.max.y, mesh.bounds.max.z);

        // Access vertex declaration
        printf("  Vertex attributes:\n");
        for (const auto& attr : mesh.decl) {
            printf("    Usage: %d, Type: %d, Elements: %d\n",
                static_cast<int>(attr.usage),
                static_cast<int>(attr.type),
                attr.elementCount);
        }

        // Access LODs
        printf("  LOD count: %zu\n", mesh.lods.size());
        for (size_t i = 0; i < mesh.lods.size(); ++i) {
            const auto& lod = mesh.lods[i];
            printf("    LOD %zu: threshold=%u\n", i, lod.threshold);
        }
    }

Accessing Vertex and Index Data
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Vertex and index data are stored in separate sections (raw data sections) and are referenced through ``BufferView`` structures. These sections are typically compressed to reduce file size and improve loading performance.

Understanding Section Compression
""""""""""""""""""""""""""""""""""

CMF supports several compression methods for raw data sections:

- **None**: No compression - data can be accessed directly from the file buffer. Note that the first ``Data`` section (section 0) is always uncompressed.
- **MeshOptimizerVertexBuffer**: Vertex buffer compressed using MeshOptimizer's vertex compression
- **MeshOptimizerIndexBuffer**: Index buffer compressed using MeshOptimizer's index compression

The compression method is specified in the ``Section`` structure's ``compression`` field. When a section is compressed:

- ``compressedSize``: The size of the compressed data in the file (what you read from disk)
- ``uncompressedSize``: The size of the data after decompression (memory you need to allocate)
- ``gpuAlignment``: For compressed buffers, this stores the stride of the elements (vertex stride or index stride)

**Why use compression?**

1. **Reduced file size**: Depending on the actual contents of the buffers, the savings can be significant
2. **Faster loading**: Smaller files load faster from disk, even accounting for decompression time
3. **Lower memory bandwidth**: Particularly beneficial when streaming assets
4. **GPU-friendly**: MeshOptimizer compression is designed to maintain cache efficiency after decompression

Decompressing Section Data
"""""""""""""""""""""""""""

To access compressed data, you must first decompress it. The library provides the ``cmf::Decompress()`` function:

.. code-block:: cpp

    #include <cmf/compression.h>

    // Check if a section is compressed
    const auto& section = header->sections[sectionIndex];
    if (section.compression != cmf::SectionCompression::None) {
        // Allocate buffer for decompressed data
        std::vector<uint8_t> decompressedData(section.uncompressedSize);

        // Get pointer to compressed data in file
        const void* compressedData = fileData.data() + section.offset;

        // Decompress
        cmf::Decompress(decompressedData.data(), section, compressedData);

        // Now use decompressedData instead of the file buffer
    } else {
        // Uncompressed - can access directly
        const uint8_t* sectionData = fileData.data() + section.offset;
    }

Example: Accessing a Vertex Buffer
"""""""""""""""""""""""""""""""""""

Here's a practical example showing how to access a mesh's vertex buffer with decompression:

.. code-block:: cpp

    #include <cmf/cmf.h>
    #include <cmf/compression.h>
    #include <vector>
    #include <memory>

    const auto& mesh = data->meshes[0];
    const auto& lod = mesh.lods[0];
    const auto& vb = lod.vb;

    // Get the section containing the vertex buffer
    const auto& section = header->sections[vb.index];

    std::unique_ptr<uint8_t[]> decompressedBuffer;
    const uint8_t* sectionData = nullptr;

    // Check if decompression is needed
    if (section.compression != cmf::SectionCompression::None) {
        // Allocate buffer for decompressed data
        decompressedBuffer = std::make_unique<uint8_t[]>(section.uncompressedSize);

        // Decompress the section
        const void* compressedData = fileData.data() + section.offset;
        cmf::Decompress(decompressedBuffer.get(), section, compressedData);

        sectionData = decompressedBuffer.get();
    } else {
        // No decompression needed - access directly
        sectionData = fileData.data() + section.offset;
    }

    // Access the vertex buffer (add BufferView offset to section data)
    const uint8_t* vertexData = sectionData + vb.offset;
    uint32_t vertexCount = vb.size / vb.stride;

    // Read vertex positions (assuming first attribute is Position)
    for (uint32_t i = 0; i < vertexCount; ++i) {
        const uint8_t* vertex = vertexData + (i * vb.stride);

        // Get position attribute offset from vertex declaration
        const auto& posAttr = mesh.decl[0]; // Assuming position is first
        const float* position = reinterpret_cast<const float*>(vertex + posAttr.offset);

        printf("Vertex %u: (%.2f, %.2f, %.2f)\n", i, position[0], position[1], position[2]);
    }

Important Notes on Section Access
""""""""""""""""""""""""""""""""""

1. **Section 0 is special**: The first section always contains the main ``Data`` structure and is never compressed. You can always access it directly from the file buffer.

2. **Cache decompressed data**: Decompression is expensive. If you need to access a section multiple times, decompress it once and cache the result.

3. **BufferView offsets**: The ``BufferView`` offset is relative to the start of the decompressed section, not the file. Always add the BufferView offset to the section data pointer.

4. **Memory management**: Decompressed data needs to remain in memory while you're using it. Use smart pointers or containers to manage its lifetime.

5. **Section boundaries**: A ``BufferView`` always references data within a single section. It never spans multiple sections.

Accessing Skeletons
^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    for (const auto& skeleton : data->skeletons) {
        printf("Skeleton: %s\n", skeleton.name.data());
        printf("  Bone count: %zu\n", skeleton.bones.size());

        // Iterate through bones
        for (size_t i = 0; i < skeleton.bones.size(); ++i) {
            printf("  Bone %zu: %s (parent: %u)\n",
                i,
                skeleton.bones[i].data(),
                skeleton.parents[i]);

            // Access rest transform
            const auto& transform = skeleton.restTransforms[i];
            printf("    Position: (%.2f, %.2f, %.2f)\n",
                transform.position.x,
                transform.position.y,
                transform.position.z);
        }
    }

Accessing Animations
^^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    for (const auto& animation : data->animations) {
        printf("Animation: %s\n", animation.name.data());
        printf("  Duration: %.2f seconds\n", animation.duration);
        printf("  Channel count: %zu\n", animation.channels.size());

        // Iterate through animation channels
        for (const auto& channel : animation.channels) {
            printf("  Channel: target=%s, type=%d, curve=%u\n",
                channel.target.data(),
                static_cast<int>(channel.targetType),
                channel.curveIndex);
        }
    }

Complete Example
----------------

Here's a complete example that ties everything together:

.. code-block:: cpp

    #include <cmf/cmf.h>
    #include <cmf/utils.h>
    #include <vector>
    #include <cstdio>

    int main() {
        // Load the file
        const char* filename = "model.cmf";
        FILE* file = fopen(filename, "rb");
        if (!file) {
            printf("Failed to open file\n");
            return 1;
        }

        fseek(file, 0, SEEK_END);
        size_t fileSize = ftell(file);
        fseek(file, 0, SEEK_SET);

        std::vector<uint8_t> fileData(fileSize);
        fread(fileData.data(), 1, fileSize, file);
        fclose(file);

        // Validate the file
        cmf::ValidationOptions options{true, true, true};
        auto validationResult = cmf::ValidateFile(
            fileData.data(),
            fileData.size(),
            options
        );

        if (!validationResult) {
            printf("Validation failed: %s\n", validationResult.error.c_str());
            return 1;
        }

        // Cast to header and convert offsets to pointers
        auto* header = reinterpret_cast<cmf::Header*>(fileData.data());
        cmf::OffsetsToPointers(*header);

        // Access the main data section
        auto* data = reinterpret_cast<cmf::Data*>(
            fileData.data() + header->sections[0].offset
        );
        cmf::OffsetsToPointers(*data);

        // Now you can safely access the data
        printf("File contains:\n");
        printf("  %zu meshes\n", data->meshes.size());
        printf("  %zu skeletons\n", data->skeletons.size());
        printf("  %zu animations\n", data->animations.size());

        // Access mesh data
        for (const auto& mesh : data->meshes) {
            printf("\nMesh: %s\n", mesh.name.data());
            printf("  Topology: %s\n",
                mesh.topology == cmf::MeshTopology::TriangleList
                    ? "Triangle List" : "Point List");
            printf("  LODs: %zu\n", mesh.lods.size());
            printf("  Areas: %zu\n", mesh.areas.size());

            if (!mesh.boneBindings.empty()) {
                printf("  Bone bindings: %zu\n", mesh.boneBindings.size());
            }
        }

        return 0;
    }

Best Practices
--------------

1. **Always validate files** before use, especially from untrusted sources
2. **Keep file data alive** - The ``Header`` and ``Data`` pointers point into the file data buffer, so the buffer must remain valid for the lifetime of these pointers
3. **Cache decompressed sections** - Decompress each section only once and cache the result for subsequent access
4. **Check compression type** - Always check ``section.compression`` before accessing section data to determine if decompression is needed
5. **Don't decompress section 0** - The main data section (section 0) is never compressed and should be accessed directly

Thread Safety
-------------

The CMF library structures are not thread-safe for modification. However, once a file is loaded and offsets are converted to pointers, the data can be safely read from multiple threads simultaneously, provided the file data buffer is not modified or destroyed.

Memory Management
-----------------

The CMF library does not manage memory for you. You are responsible for:

- Allocating and maintaining the file data buffer
- Ensuring the buffer remains valid while ``Header`` and ``Data`` pointers are in use
- Freeing the buffer when done

Consider using ``std::shared_ptr`` or similar smart pointers to manage the lifetime of the file data buffer, especially when sharing data between multiple objects or threads.
