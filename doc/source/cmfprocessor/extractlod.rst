extractlod
==========

Description
-----------

Creates a new CMF file containing only the last (lowest detail) LOD of each mesh. This command is useful for creating 
simplified impostor versions of models to be used while the original high-detail models are downloaded.

If no mesh in the source file has more than one LOD, the output file is not created and the command finishes successfully.

The command also removes audio occlusion mesh data from the output file. All skeletons and animations are retained without modification.

Syntax
------

.. code-block:: bash

   cmfprocessor extractlod src dst

Arguments:

* ``src`` - Path to the source CMF file (required). Must be an existing file.
* ``dst`` - Path to the output CMF file (required). "Low-detail" version of the file will be created at this location.

Behavior
--------

For each mesh in the source file:

1. If the mesh has multiple LODs, only the last (lowest detail) LOD is retained.
2. The threshold for the last LOD is set to maximum, ensuring it's visible at all distances.
3. If the mesh has only one LOD, it remains unchanged.
4. Audio occlusion mesh data is removed.

If no meshes have multiple LODs, the command completes successfully without writing an output file.

Examples
--------

Extract the lowest LOD from a model:

.. code-block:: bash

   cmfprocessor extractlod model.cmf model_lowdetail.cmf
