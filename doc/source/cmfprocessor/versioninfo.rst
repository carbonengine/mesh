versioninfo
===========

Description
-----------

Prints current version information for the CMF Processor tool and CMF file format version.

The version information is printed to stdout as a JSON object.

Syntax
------

.. code-block:: bash

   cmfprocessor versioninfo


Output Format
-------------

The output is a string representation of a JSON object:

.. code-block:: text

   {
       "cmfprocessor": cmprocessor_version,
       "format": cmf_format_version
   }

Note that `cmprocessor_version` is a string, but `cmf_format_version` is a number.

Examples
--------

Compute hash of an FBX file:

.. code-block:: bash

   cmfprocessor versioninfo


Compute hash in a script (PowerShell):

.. code-block:: powershell

   $versioninfo = cmfprocessor versioninfo
   Write-Host "Version Info: $versioninfo"

Compute hash in a script (Bash):

.. code-block:: bash

   versioninfo=$(cmfprocessor versioninfo)
   echo "Version Info: $versioninfo"

