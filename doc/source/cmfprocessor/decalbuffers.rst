decalbuffers
============

Description
-----------

Generates decal index buffers for meshes in a CMF file. This command identifies triangles from a mesh that intersect with 
oriented bounding boxes (decals) and outputs the index buffer data for rendering those triangles.

Decal index buffers allow efficient rendering of decals on mesh surfaces by pre-computing which triangles are affected by 
each decal's bounding volume. The output is a JSON array containing index buffer data for each decal and LOD level.

Decal transforms are specified as position, rotation (quaternion), and scale values that define the oriented bounding box for each decal.
The bounds of the box are from -1 to +1 in each axis before transformation, so the scale values effectively control the size of the decal volume.

Syntax
------

.. code-block:: bash

   cmfprocessor decalbuffers [OPTIONS] file

Arguments:

* ``file`` - Path to CMF file (required). Must be an existing file.

Options:

* ``--decal <transform>`` - Decal transform: position, rotation (quaternion), scale values, separated with commas (required). Can be specified multiple times for multiple decals.

  Format: ``posX,posY,posZ,rotX,rotY,rotZ,rotW,scaleX,scaleY,scaleZ``

  Where:
    - ``posX,posY,posZ`` - Position vector (3 floats)
    - ``rotX,rotY,rotZ,rotW`` - Rotation quaternion (4 floats)
    - ``scaleX,scaleY,scaleZ`` - Scale vector (3 floats)

* ``--mesh <index>`` - Index of the mesh in the CMF file to generate decal buffers for (default: 0)

Output Format
-------------

The output is printed to stdout as a JSON array with the following structure:

.. code-block:: 

   [
     [
       [index1, index2, index3, ...],
       [index1, index2, index3, ...],
       ...
     ],
     ...
   ]

The structure is organized as:

* **Outer array** - One element per decal (in the order specified by ``--decal`` options)
* **Middle array** - One element per LOD level in the mesh
* **Inner array** - Index buffer containing indices of triangles that intersect the decal's bounding box

Examples
--------

Generate decal buffers for a single decal:

.. code-block:: bash

   cmfprocessor decalbuffers --decal 0,0,0,0,0,0,1,1,1,1 model.cmf

Generate for multiple decals:

.. code-block:: bash

   cmfprocessor decalbuffers --decal 0,0,0,0,0,0,1,1,1,1 --decal 5,0,0,0,0,0,1,2,2,2 model.cmf

Generate for a specific mesh:

.. code-block:: bash

   cmfprocessor decalbuffers --mesh 2 --decal 0,0,0,0,0,0,1,1,1,1 model.cmf

Save output to file:

.. code-block:: bash

   cmfprocessor decalbuffers --decal 0,0,0,0,0,0,1,1,1,1 model.cmf > decals.json

Example with PowerShell:

.. code-block:: powershell

   $output = cmfprocessor decalbuffers --decal "1.5,2.0,0.0,0,0,0,1,0.5,0.5,0.5" model.cmf
   $output | Out-File decals.json

Requirements and Constraints
-----------------------------

- The target mesh must use ``TriangleList`` topology
- The mesh must have a Position element in its vertex declaration
- The mesh index must be within range of meshes in the CMF file
- At least one decal transform must be specified

Notes
-----

- The command uses a triangle-oriented bounding box intersection test to determine which triangles are affected by each decal
- The decal transform represents an oriented bounding box with extents from -1 to +1 in each axis before transformation
- Index buffer data is generated for all LOD levels in the specified mesh
- The output can be used to create optimized draw calls for decal rendering
- Empty arrays indicate no triangles intersect the decal volume for that LOD level

See Also
--------

* :doc:`dumpjson <dumpjson>` - For inspecting mesh structure and topology
* :doc:`validate <validate>` - For verifying CMF file integrity
