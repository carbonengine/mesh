generateaudioocclusion
======================

Description
-----------

Generates audio occlusion mesh data for meshes in a CMF file. Audio occlusion meshes are simplified versions of the original 
geometry used for audio raytracing and occlusion calculations, providing better performance than using the full-detail mesh.

The command modifies the file in-place, adding or updating audio occlusion data for triangle-based meshes.

Generating audio occlusion data would normally be performed when importing source models with the :doc:`fbximport <fbximport>` command, 
but this command allows you to add or regenerate audio occlusion data for existing CMF files that were imported without it.

Syntax
------

.. code-block:: bash

   cmfprocessor generateaudioocclusion [OPTIONS] file

Arguments:

* ``file`` - Path to CMF file (required). Must be an existing file. The file will be modified in-place.

Options:

* ``--config <path>`` - Path to a JSON config file that specifies audio occlusion generation options. Must be an existing file. See Configuration section below.
* ``--force`` - Force regeneration of audio occlusion data even if it already exists for a mesh. Without this flag, meshes with existing audio occlusion data are skipped.

Configuration
-------------

The ``--config`` option accepts a JSON file with the following structure:

.. code-block:: json

   {
     "generate": true,
     "method": "Simplygon",
     "simplygon": {
       "screenSize": 80,
       "holeFilling": "Medium"
     }
   }

See :doc:`fbximport <fbximport>` documentation for details on the configuration options.

Examples
--------

Generate audio occlusion data with default settings:

.. code-block:: bash

   cmfprocessor generateaudioocclusion model.cmf

Generate with custom configuration:

.. code-block:: bash

   cmfprocessor generateaudioocclusion --config audio_config.json model.cmf

Force regeneration of existing audio occlusion data:

.. code-block:: bash

   cmfprocessor generateaudioocclusion --force model.cmf

Generate with custom configuration and force regeneration:

.. code-block:: bash

   cmfprocessor generateaudioocclusion --config audio_config.json --force model.cmf

Example Configuration File (audio_config.json)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: json

   {
     "generate": true,
     "method": "Simplygon",
     "simplygon": {
       "screenSize": 120,
       "holeFilling": "High"
     }
   }

Notes
-----

- The file is modified in-place; consider backing up before use
- Only triangle list meshes can have audio occlusion data generated
- Use ``--force`` to regenerate with different settings
- Higher ``screenSize`` values produce more accurate but more expensive occlusion meshes
