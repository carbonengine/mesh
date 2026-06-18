cmfprocessor
============

cmfprocessor is a command-line tool for working with CMF (Carbon Mesh Format) files. It provides multiple sub-commands for importing, processing, inspecting, validating and exporting 3D mesh data.

General Usage
-------------

.. code-block:: bash

   cmfprocessor [--version] <command> [options] [arguments]

Global Options
--------------

* ``--version``, ``-v`` - Display version information and exit
* ``--help``, ``-h`` - Show help message and exit

Available Commands
------------------

Import and Conversion
^^^^^^^^^^^^^^^^^^^^^

.. toctree::
   :maxdepth: 1

   fbximport

Processing and Optimization
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. toctree::
   :maxdepth: 1

   generatelods
   generateaudioocclusion
   extractlod
   decalbuffers

Inspection and Debugging
^^^^^^^^^^^^^^^^^^^^^^^^^

.. toctree::
   :maxdepth: 1

   dumpjson
   validate
   hash

Metadata Management
^^^^^^^^^^^^^^^^^^^

.. toctree::
   :maxdepth: 1

   metadata

Export
^^^^^^

.. toctree::
   :maxdepth: 1

   gltfconverter

Command Summary
---------------

.. list-table::
   :header-rows: 1
   :widths: 25 75

   * - Command
     - Description
   * - :doc:`fbximport <fbximport>`
     - Converts an FBX or OBJ file into CMF format with support for meshes, skeletons, animations, LODs, and more
   * - :doc:`generatelods <generatelods>`
     - Generates level-of-detail (LOD) meshes for existing CMF files to optimize rendering performance
   * - :doc:`generateaudioocclusion <generateaudioocclusion>`
     - Creates simplified audio occlusion meshes for sound raytracing and occlusion calculations
   * - :doc:`extractlod <extractlod>`
     - Creates impostor low-detail CMF files containing only the lowest-detail LODs from original files
   * - :doc:`decalbuffers <decalbuffers>`
     - Generates decal index buffers for meshes, identifying triangles that intersect with oriented bounding boxes
   * - :doc:`dumpjson <dumpjson>`
     - Outputs the contents of a CMF file as human-readable JSON for inspection and debugging
   * - :doc:`validate <validate>`
     - Validates a CMF file to ensure structural integrity and format compliance
   * - :doc:`hash <hash>`
     - Computes and prints the MD5 hash of any file for integrity verification
   * - :doc:`metadata <metadata>`
     - Adds, modifies, or removes metadata key-value pairs in CMF files
   * - :doc:`gltfconverter <gltfconverter>`
     - Converts a CMF file to GLTF, including meshes, skeletons and animations

Quick Start
-----------

Convert an FBX file to CMF:

.. code-block:: bash

   cmfprocessor fbximport model.fbx model.cmf

Generate LODs for an existing CMF file:

.. code-block:: bash

   cmfprocessor generatelods model.cmf

Inspect the contents of a CMF file:

.. code-block:: bash

   cmfprocessor dumpjson --data --metadata --indent 2 model.cmf

Validate a CMF file:

.. code-block:: bash

   cmfprocessor validate model.cmf

Convert a CMF file to GLTF:
.. code-block:: bash

   cmfprocessor gltfconverter model.cmf model.gltf

Common Workflows
----------------

Complete FBX Import with LODs and Audio Occlusion
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: bash

   # Import FBX with LOD and audio occlusion generation
   cmfprocessor fbximport --config import_config.json model.fbx model.cmf

Example ``import_config.json``:

.. code-block:: json

   {
     "mesh": {
       "uvSets": 2,
       "lods": {
         "generate": true
       },
       "audioOcclusionMesh": {
         "generate": true
       }
     }
   }

Post-Process Existing CMF Files
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: bash

   # Add LODs to a file that doesn't have them
   cmfprocessor generatelods --config lod_config.json model.cmf

   # Add audio occlusion meshes
   cmfprocessor generateaudioocclusion model.cmf

   # Add metadata
   cmfprocessor metadata --set version "1.0" --set author "Team Name" model.cmf

Asset Verification Pipeline
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: bash

   # Validate file structure
   cmfprocessor validate model.cmf

   # Check content
   cmfprocessor dumpjson --data model.cmf > model.json

   # Compute hash for tracking
   cmfprocessor hash model.cmf > model.md5

Exporting CMF to GLTF with secondary CMF files containing animations
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: bash

   # Export a CMF file containing the model, with separate CMF files containing animations
   cmfprocessor gltfconverter --src2 animation1.cmf --src2 animation2.cmf model.cmf model.gltf

Error Handling
--------------

All commands return exit code 0 on success and 1 on error. Error messages are written to stderr.

Example error handling in PowerShell:

.. code-block:: powershell

   cmfprocessor validate model.cmf
   if ($LASTEXITCODE -ne 0) {
       Write-Error "Validation failed"
       exit 1
   }

Example error handling in Bash:

.. code-block:: bash

   if ! cmfprocessor validate model.cmf; then
       echo "Validation failed" >&2
       exit 1
   fi

Getting Help
------------

For version information:

.. code-block:: bash

   cmfprocessor --version

For general help:

.. code-block:: bash

   cmfprocessor --help

For help on a specific command, refer to the individual command documentation pages linked above.
