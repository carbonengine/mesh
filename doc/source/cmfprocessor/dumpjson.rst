dumpjson
========

Description
-----------

Outputs the contents of a CMF (Carbon Mesh Format) file as JSON data. This command is useful for inspecting and debugging CMF files by converting their binary contents into a human-readable JSON format.

Syntax
------

.. code-block:: bash

   cmfprocessor dumpjson [OPTIONS] file

Arguments:

* ``file`` - Path to CMF file (required). Must be an existing file.

Options

* ``--header`` - Include file header in the output. The header contains version information and file format details.
* ``--data`` - Include data section in the output. This contains meshes, skeletons, animations, and other primary data.
* ``--metadata`` - Include metadata section in the output. Metadata contains key-value pairs with information about the file's 
  origin and processing history.
* ``--curves`` - Convert animation curve data to numbers instead of dumping as bytes. Only applicable if ``--data`` is specified. 
  Makes animation data more readable.
* ``--buffers`` - Include buffer view contents unpacked from binary buffers. Only applicable if ``--data`` is specified. This 
  expands vertex and index buffer data into readable arrays.
* ``--indent <n>`` - Number of spaces to indent for pretty printing. If not specified, output will be minified (single line). 
  Use positive integers for readable output (e.g., 2 or 4).

Examples
--------

Dump all sections with pretty printing:

.. code-block:: bash

   cmfprocessor dumpjson --header --data --metadata --indent 2 model.cmf

Dump only metadata:

.. code-block:: bash

   cmfprocessor dumpjson --metadata --indent 2 model.cmf

Dump data with expanded animation curves and buffers:

.. code-block:: bash

   cmfprocessor dumpjson --data --curves --buffers --indent 4 model.cmf

Dump minified output:

.. code-block:: bash

   cmfprocessor dumpjson --data model.cmf

Save output to file:

.. code-block:: bash

   cmfprocessor dumpjson --header --data --metadata --indent 2 model.cmf > model.json

Output Format
-------------

The output JSON structure varies based on the options specified:

**With --header:**

.. code-block:: json

   {
     "header": {
       "signature": "...",
       "version": "...",
       // ...
     }
   }

**With --data:**

.. code-block:: json

   {
     "data": {
       "meshes": [
         // ...
       ],
       "skeletons": [
         // ...
       ],
       "animations": [
         // ...
       ]
     }
   }

**With --metadata:**

.. code-block:: json

   {
     "metadata": {
       "entries": [
         {"key": "source", "value": "..."},
         {"key": "generator", "value": "..."}
       ]
     }
   }

Notes
-----

- The ``--buffers`` option can produce very large output for meshes with many vertices.
- Use ``--indent`` for human-readable output, or omit it for machine-readable compact output.
- Combine flags as needed to control which sections are included in the output.
- The output is written to stdout, so you can redirect it to a file using ``>`` operator.
