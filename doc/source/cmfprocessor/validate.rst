validate
========

Description
-----------

Validates a CMF (Carbon Mesh Format) file to ensure it is properly formatted and contains valid data. 
This command checks the file structure, data integrity, and format compliance without modifying the file.

If the file is valid, the command completes silently with a success exit code. 
If the file is invalid, an error message is displayed and the command exits with an error code.

Syntax
------

.. code-block:: bash

   cmfprocessor validate file

Arguments:

* ``file`` - Path to CMF file to validate (required). Must be an existing file.

Behavior
--------

The command loads the CMF file and performs validation by checking its structure and content. 

If any validation check fails, the program exits with a non-zero exit code and an error message is displayed indicating what went wrong.

Examples
--------

Validate and check exit code in a script (PowerShell):

.. code-block:: powershell

   cmfprocessor validate model.cmf
   if ($LASTEXITCODE -eq 0) {
       Write-Host "File is valid"
   } else {
       Write-Host "File is invalid"
   }

Validate and check exit code in a script (Bash):

.. code-block:: bash

   if cmfprocessor validate model.cmf; then
       echo "File is valid"
   else
       echo "File is invalid"
   fi

Validate multiple files in a directory (PowerShell):

.. code-block:: powershell

   Get-ChildItem *.cmf | ForEach-Object {
       Write-Host "Validating $($_.Name)..."
       cmfprocessor validate $_.FullName
       if ($LASTEXITCODE -eq 0) {
           Write-Host "  Valid" -ForegroundColor Green
       } else {
           Write-Host "  Invalid" -ForegroundColor Red
       }
   }

Validate multiple files in a directory (Bash):

.. code-block:: bash

   for file in *.cmf; do
       echo "Validating $file..."
       if cmfprocessor validate "$file"; then
           echo "  Valid"
       else
           echo "  Invalid"
       fi
   done

Use Cases
---------

- Verifying file integrity after transfer or storage
- Checking files before processing in a pipeline
- Automated testing of CMF file generation
- Debugging file format issues
- Quality assurance for asset pipelines

Notes
-----

- The validate command is read-only and never modifies the file
- Validation is performed automatically when any other command loads a file
- A successful validation means the file can be safely processed by other commands
- Validation does not check the semantic correctness of the content (e.g., whether normals are unit vectors), only structural validity
- The command is useful in automated build and deployment pipelines
