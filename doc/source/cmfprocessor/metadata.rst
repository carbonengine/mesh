metadata
========

Description
-----------

Modifies metadata in a CMF file. This command allows you to add, change, or remove metadata entries in the file
in-place. Metadata entries are key-value pairs that store information about the file's origin, processing 
history, and other attributes.

Syntax
------

.. code-block:: bash

   cmfprocessor metadata [OPTIONS] file

Arguments:

* ``file`` - Path to CMF file (required). Must be an existing file. The file will be modified in-place.

Options:

* ``--set <key> <value>`` - Set a metadata key-value pair. If the key already exists, its value is updated. 
  If the key doesn't exist, a new entry is created. May be specified multiple times to set multiple keys in a 
  single command.
* ``--unset <key>`` - Remove a metadata key. If the key exists, it is removed from the metadata. If the key 
  doesn't exist, no action is taken. May be specified multiple times to remove multiple keys in a single command.

Behavior
--------

The command modifies metadata entries in the file:

1. For each ``--set`` operation, the key-value pair is added or updated
2. For each ``--unset`` operation, the key is removed if it exists
3. The file is saved after all operations are complete

Both ``--set`` and ``--unset`` can be used together in the same command.

Examples
--------

Set a single metadata entry:

.. code-block:: bash

   cmfprocessor metadata --set author "John Doe" model.cmf

Set multiple metadata entries:

.. code-block:: bash

   cmfprocessor metadata --set author "John Doe" --set version "1.2" --set description "Character model" model.cmf

Remove a metadata entry:

.. code-block:: bash

   cmfprocessor metadata --unset oldKey model.cmf

Remove multiple metadata entries:

.. code-block:: bash

   cmfprocessor metadata --unset oldKey --unset deprecatedInfo model.cmf

Combine set and unset operations:

.. code-block:: bash

   cmfprocessor metadata --unset oldVersion --set version "2.0" --set updated "2024-01-15" model.cmf

Update an existing metadata value:

.. code-block:: bash

   cmfprocessor metadata --set status "final" model.cmf

Common Metadata Keys
--------------------

While you can use any key names, some common metadata keys used by cmfprocessor and related tools include:

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Key
     - Description
   * - ``source``
     - Original source file path
   * - ``sourceHash``
     - MD5 hash of the source file
   * - ``generator``
     - Tool used to generate the CMF file
   * - ``generatorVersion``
     - Version of the generator tool
   * - ``options``
     - JSON string of options used during generation

Use Cases
---------

- Adding custom metadata for asset management systems
- Updating version information when models are revised
- Removing outdated or incorrect metadata entries
- Documenting the origin and processing history of models
- Tagging models with searchable attributes

Notes
-----

- The file is modified in-place; consider backing up before use
- Metadata keys and values are stored as strings
- There is no restriction on key names; any valid string can be used
- Setting a key that already exists will overwrite its previous value
- Unsetting a key that doesn't exist is a no-op (no error)
- Multiple set/unset operations can be combined in a single command for efficiency
- Metadata does not affect the rendering or processing of the mesh data itself
