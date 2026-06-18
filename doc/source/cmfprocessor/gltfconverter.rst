gltfconverter
============

Description
-----------

Converts CMF files to glTF. glTF is an open file format developed by the Khronos Group.

The command does not modify the CMF input files.

glTF stores raw binary data in a `.bin` file, and json data in a `.gltf` file. The gltfconverter can combine these, embedding the binary data as base64 in the `.gltf` file.

Multiple CMF files can be merged into one glTF file. The typical use-case is a model file with separate animation files, allowing the model to be loaded with all its animations in a conventional glTF viewer, i.e. the glTF Sample Viewer Web App by Khronos.

Note that CMF does not contain material or texture information. Hence, the glTF files likewise don't contain information about materials or textures.

Other limitations:
- The gltfconverter does not handle arbitrary data stored in cmf files, such as particle effect data, that may be mapped to some vertex attributes.
- The gltfconverter does not handle pose libraries stored in cmf files, that may consist of curves containing multiple keyframes at the same timestamp.

Syntax
------

.. code-block:: bash

   cmfprocessor gltfconverter [OPTIONS] cmf_file gltf_file

Arguments:

* ``cmf_file`` - Path to input CMF file (required). Must be an existing file.
* ``gltf_file`` - Path to output glTF file (required). If it already exists, it will be overwritten.

Options:

* ``--src2 <path>`` - Path to a secondary CMF file that will be merged into the glTF file. Must be an existing file. This flag can be added repeatedly to merge multiple files.
* ``--combinedfile`` - Embed the binary data in the `.gltf` file as base64. Without this flag, separate `.bin` and `.gltf` are created. When used, the `.bin` file is not created, as the data resides in the `.gltf` file.

Examples
--------

Convert one CMF file to glTF:

.. code-block:: bash

   cmfprocessor gltfconverter model.cmf model.gltf

Embed binary data into the `.gltf` file:

.. code-block:: bash

   cmfprocessor gltfconverter --combinedfile model.cmf model.gltf

Merge a secondary CMF file containing animations into the glTF:

.. code-block:: bash

   cmfprocessor gltfconverter --src2 animation.cmf model.cmf model.gltf

Merge multiple secondary CMF files containing animations into the glTF:

.. code-block:: bash

   cmfprocessor gltfconverter --combinedfile --src2 animation1.cmf --src2 animation2.cmf model.cmf model.gltf

Embed binary data and merge secondary CMF files:

.. code-block:: bash

   cmfprocessor gltfconverter --combinedfile --src2 animation.cmf model.cmf model.gltf
