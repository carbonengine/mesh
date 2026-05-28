generatelods
============

Description
-----------

Generates level-of-detail (LOD) meshes for a CMF file. LODs are progressively simplified versions of meshes that can be used at 
different viewing distances to improve rendering performance while maintaining visual quality.

The command modifies the file in-place, adding LOD levels to meshes that have only one LOD.

Generating LODs would normally be performed when importing source models with the :doc:`fbximport <fbximport>` command, 
but this command allows you to add or regenerate LODs for existing CMF files that were imported without them.

Syntax
------

.. code-block:: bash

   cmfprocessor generatelods [OPTIONS] file

Arguments:

* ``file`` - Path to CMF file (required). Must be an existing file. The file will be modified in-place.

Options:

* ``--config <path>`` - Path to a JSON config file that specifies LOD generation options. Must be an existing file. See Configuration section below.
* ``--force`` - Force regeneration of LODs even if they already exist for a mesh. Without this flag, meshes with existing multiple LODs are skipped. When used, only the first (highest detail) LOD is kept and new LODs are generated from it.

Configuration
-------------

The ``--config`` option accepts a JSON file with the following structure:

.. code-block:: json

   {
     "generate": true,
     "method": "Simplygon",
     "simplygon": {
       "maxLods": 6,
       "geometryImportance": 1.0,
       "areaImportance": 1.0,
       "normalImportance": 1.0,
       "uvImportance": 1.0,
       "skinningImportance": 1.0,
       "vertexColorImportance": 1.0,
       "screenSizeFactor": 2.0,
       "lockVertexChannel": ""
     }
   }

See :doc:`fbximport <fbximport>` documentation for details on the configuration options.

Examples
--------

Generate LODs with default settings:

.. code-block:: bash

   cmfprocessor generatelods model.cmf

Generate with custom configuration:

.. code-block:: bash

   cmfprocessor generatelods --config lod_config.json model.cmf

Force regeneration of existing LODs:

.. code-block:: bash

   cmfprocessor generatelods --force model.cmf

Generate with custom configuration and force regeneration:

.. code-block:: bash

   cmfprocessor generatelods --config lod_config.json --force model.cmf

Example Configuration File (lod_config.json)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Conservative LOD generation (preserves detail):

.. code-block:: json

   {
     "generate": true,
     "method": "Simplygon",
     "simplygon": {
       "maxLods": 4,
       "uvImportance": 0.5,
       "screenSizeFactor": 1.5
     }
   }

LOD generation with locked vertices:

.. code-block:: json

   {
     "generate": true,
     "method": "Simplygon",
     "simplygon": {
       "lockVertexChannel": "LockVertices"
     }
   }
