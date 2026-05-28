hash
====

Description
-----------

Computes and prints the MD5 hash of an arbitrary file. This command is useful for comparing hashes of files
to the hashes stored in CMF metadata.

The hash is printed to stdout as a 32-character hexadecimal string.

Syntax
------

.. code-block:: bash

   cmfprocessor hash file

Arguments:

* ``file`` - Path to a file to hash (required). Must be an existing file. Can be any file type, not just CMF files.

Output Format
-------------

The output is a single line containing the 32-character MD5 hash:

.. code-block:: text

   a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6

Examples
--------

Compute hash of an FBX file:

.. code-block:: bash

   cmfprocessor hash model.fbx


Compute hash in a script (PowerShell):

.. code-block:: powershell

   $hash = cmfprocessor hash model.fbx
   Write-Host "Hash: $hash"

Compute hash in a script (Bash):

.. code-block:: bash

   hash=$(cmfprocessor hash model.fbx)
   echo "Hash: $hash"

Integration with Other Commands
--------------------------------

The hash command is automatically used by the ``fbximport`` command to store a ``sourceHash`` metadata entry in generated CMF files. This allows you to:

1. Verify that a CMF file was generated from a specific FBX file
2. Detect when source files have changed and regeneration is needed
3. Track the provenance of generated assets

Example workflow:

.. code-block:: bash

   # Generate CMF and capture source hash in metadata
   cmfprocessor fbximport source.fbx output.cmf

   # Later, verify if source has changed
   cmfprocessor dumpjson --metadata output.cmf > metadata.json
   # Compare sourceHash in metadata.json with current hash
   cmfprocessor hash source.fbx
