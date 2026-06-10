fbximport
=========

Description
-----------

Converts an FBX or an OBJ file into CMF (Carbon Mesh Format). This command imports meshes, skeletons, and animations from FBX files and outputs 
them in the optimized CMF format.

Since CMF is not a scene storage program, it is not the aim of the tool to consume any FBX file. Rather FBX files need to 
be carefully exported from DCC tools with some sanitation. Nevertheless, the importer tries its best to convert an FBX file into a CMF file.


Syntax
------

.. code-block:: bash

   cmfprocessor fbximport [OPTIONS] fbx_path cmf_path

Arguments:

* ``fbx_path`` - path to the source FBX file (required). Must be an existing file.
* ``cmf_path`` - path to the output CMF file (required). The output file will be created at this location.

Options:

* ``--config <config_path>`` - path to an existing JSON file containing further settings for customizing the import.
* ``--mesh <name>`` - name of the mesh node to include in the CMF file. This overrides ``mesh.filter`` setting in the JSON config file. There may be multiple 
  ``--mesh`` options specified with different mesh node names to include in the output.
* ``--skeleton <name>`` - name of the skeleton root node to include in the CMF file. This overrides ``skeleton.filter`` setting in the JSON config file. There may be multiple 
  ``--skeleton`` options specified with different root node names to include in the output.
* ``--animation <name>`` - name of the animation to include in the CMF file. This overrides ``animation.filter`` setting in the JSON config file. There may be multiple 
  ``--animation`` options specified with different animation track names to include in the output.
* ``--path <source_path>`` - overrides the source FBX path included in the CMF metadata. See `Metadata`_ below.


Configuration files
-------------------

Configuration files are JSON files storing multiple settings controlling the import process. The structure of the JSON file is not rigid, i.e., the importer only tries to find the
data in it that it can recognize. Furthermore, none of the settings in the file are mandatory: all settings have default values.

The root of the JSON file is expected to be an object containing optional fields ``mesh``, ``skeleton``, ``animation`` that are objects themselves containing settings for mesh, skeleton, 
and animation importing, respectively as well as other settings described below.

General settings
^^^^^^^^^^^^^^^^

``unitsPerMeter``
  Type ``number``, default ``0``. When this setting is set to a positive value, it overrides the units in the source FBX file (and OBJ file's 1.0) and treats them as the specified number of units per meter. 
  This allows importing FBX files with incorrect or missing unit information. When this setting is set to ``0``, the importer uses the units specified in the source FBX file, or 1.0 for OBJ files.

``lowdetailSuffix``
  Type ``string``, default ``""``. If this setting is not an empty string, the importer will create a "low-detail" version of the output file in the same directory as the
  main output CMF file, with the same name but with the specified suffix before the file extension. The low-detail version of the file will only contain the lowest LOD for each mesh
  along with all the skeletons and animations. These "low-detail" files can be used for streaming in games: the game can load the low-detail version of the file first, and then load
  the main file in the background to get higher LODs. If this setting is an empty string, no low-detail version of the file is created. Also, if LOD generation is disabled or results
  in no generated LODs, the low-detail version of the file will not be created regardless of this setting, as it would be identical to the main file. This built-in low-detail output
  overlaps with the functionality of the ``extractlod`` command: use ``fbximport`` with ``lowdetailSuffix`` when generating CMF files from FBX, and use ``extractlod`` when you need to
  create a low-detail CMF from an existing CMF file after import.

Mesh settings
^^^^^^^^^^^^^

``mesh.import``
  Type ``boolean``, default ``true``. Whether to import meshes into CMF file. When this setting is set to ``false`` meshes are skipped during import.

``mesh.filter``
  Type ``array`` of ``string``, default ``[]``. Names of meshes to include in the CMF file. This setting allows only importing meshes with specific names. When the array is empty, all meshes are imported.
  Note that if ``mesh.import`` is ``false``, none of the meshes are imported regardless of this setting. If one or more ``--mesh`` command line arguments are specified, their mesh names completely override this
  setting.

``mesh.normals``
  Type ``boolean``, default ``true``. Whether to import vertex normals for meshes. Without this setting, the resulting CMF file meshes will contain no normals. If this setting is ``true`` and ``mesh.regenerateNormals`` is false,
  the importer expects to find normal data in source meshes, and it will fail to import the file if a mesh does not contain normals.

``mesh.tangents``
  Type ``number``, default ``1``. Number of tangent spaces (tangent and binormal vectors) to generate/import. The importer would only try to import tangents for UV set 0; any extra tangent spaces are always generated. If the
  source mesh does not contain tangent information, the importer will silently generate tangents for UV set 0 as well. If the value of this setting is greater than zero, mesh normals are required, so ``mesh.normals`` must be
  set to ``true``. The value of this setting may not exceed ``mesh.uvSets``.

``mesh.alwaysComputeTangents``
  Type ``boolean``, default ``true``. Ignore tangent data in the source FBX file, i.e., always recompute tangents for UV set 0. This setting only has an effect when ``mesh.tangents`` is greater than zero.

``mesh.compressTangents``
  Type ``boolean``, default ``true``. Compresses tangent space vectors in the output vertex buffer. Instead of having (Usage::Normal, Usage::Tangent#, Usage::Binormal#) vertex attributes, the mesh will have Usage::PackedTangent# or
  Usage::PackedTangentLegacy# attributes.

``mesh.legacyCompressedTangents``
  Type ``boolean``, default ``false``. Use legacy compression scheme for tangent spaces. This setting only has an effect when ``mesh.compressTangents`` is ``true``. Legacy compression should only be used to keep
  compatibility with old assets and shaders.

``mesh.colors``
  Type ``number``, default ``0``. Number of vertex color sets to import from the source mesh. The source mesh must have at least this number of vertex color channels, otherwise the importer fails.

``mesh.colorType``
  Type ``string``, default ``Float16``. Number format to use for vertex colors in the output mesh. Possible supported values are ``UInt8Norm``, ``Float16``, and ``Float32``. With ``UInt8Norm`` vertex colors are stored in 
  8 bits per channel and normalized to [0, 1] range, while with ``Float16`` they are stored as half-precision floats, which allows for HDR colors at the expense of increased memory requirements. The ``Float32`` 
  option is also available, but it is rarely needed as it takes more memory without much visual improvement over ``Float16``.

``mesh.uvSets``
  Type ``number``, default ``1``. Number of UV sets to import from the source mesh. Source meshes are expected to have this many UV sets defined; the import fails if they don't.

``mesh.flipV``
  Type ``boolean``, default ``true``. Flip the V coordinate of texture UVs. This is needed when the source UVs are in a different convention than the one used in CMF, for example, 
  when they are exported from DCC tools with the "OpenGL" UV convention.

``mesh.uvType``
  Type ``string``, default ``Float16``. Number format to use for UV coordinates in the output mesh. Possible supported values are ``Float32`` and ``Float16``. 

``mesh.skinning``
  Type ``boolean``, default ``true``. Import skinning information for meshes: bones affecting the mesh, bones affecting each vertex (Usage::BoneIndices), and optionally their weights (Usage::BoneWeights). It is not required
  for the mesh to have a skin deformer when this setting is ``true``. If the source mesh does not have a skin deformer, it is exported without skin information without any errors.

``mesh.bonesPerVertex``
  Type ``number``, default ``4``. Maximum number of bones affecting a mesh vertex. If the source vertex is affected by more than the specified number of bones, only the maximum specified number of bones with the largest weights
  are recorded with renormalized weights. Only values ``4`` and ``1`` are currently supported. When the value is ``1``, the importer will drop Usage::BoneWeights attribute from the output mesh completely, as it is assumed
  that each vertex is affected by only one bone and weights are redundant.

``mesh.boneIndexType``
  Type ``string``, default ``Uint8``. Number format for per-vertex bone indices. Allowed values are ``Uint8`` and ``Uint16``. With ``Uint8`` the mesh can only be affected by up to 256 bones, while with ``Uint16`` - up to 65536
  at the expense of increased memory requirements.

``mesh.regenerateNormals``
  Type ``boolean``, default ``false``. Recalculates vertex normals using mesh vertex positions and smoothing data, instead of using source vertex normals in the FBX file. If the source mesh is missing smoothing data, the mesh
  is assumed to be fully smooth. This setting only has an effect if ``mesh.normals`` is ``true``. Normally, it is preferable to use vertex normals stored in the FBX file, but in some cases it may be beneficial to generate them
  on import, see `Morph targets`_.

``mesh.morphTargets``
  Type ``object``, default ``{}``. Settings related to morph targets. See below.

``mesh.morphTargets.import``
  Type ``boolean``, default ``true``. Whether to import morph targets (blend shapes) for meshes. Having this setting set to ``true`` does not require all source meshes to have blend shape deformers. If the source mesh does not 
  have a blend shape deformer, it is exported without morph targets without any error.

``mesh.morphTargets.useCustomNormals``
  Type ``boolean``, default ``false``. Use per-blend shape normal data stored in custom FBX properties instead of regenerating blend shape normals on import. See `Morph targets`_. This setting only has an effect if 
  ``mesh.morphTargets.import`` and ``mesh.normals`` are set to ``true``.

``mesh.lods``
  Type ``object``, default ``{}``. Settings related to LOD generation. See below.

``mesh.lods.generate``
  Type ``boolean``, default ``true``. Whether the program needs to generate LODs for meshes. Set to ``false`` to skip LOD generation.

``mesh.lods.method``
  Type ``string``, default ``Simplygon``. Method/library used to generate LODs. Currently, the only method supported is Simplygon.

``mesh.lods.simplygon``
  Type ``object``, default ``{}``. Options for Simplygon LOD generation.

``mesh.lods.simplygon.maxLods``
  Type ``number``, default ``6``. Maximum number of LODs (including LOD0) for the mesh.

``mesh.lods.simplygon.geometryImportance``
  Type ``number``, default ``1``. Relative importance of vertex positions when generating LODs, compared to other features. May not be less than zero.

``mesh.lods.simplygon.areaImportance``
  Type ``number``, default ``1``. Relative importance of mesh area splits when generating LODs, compared to other features. May not be less than zero.

``mesh.lods.simplygon.normalImportance``
  Type ``number``, default ``1``. Relative importance of vertex normals when generating LODs, compared to other features. May not be less than zero.

``mesh.lods.simplygon.uvImportance``
  Type ``number``, default ``1``. Relative importance of texture UVs when generating LODs, compared to other features. May not be less than zero.

``mesh.lods.simplygon.skinningImportance``
  Type ``number``, default ``1``. Relative importance of bone weights when generating LODs, compared to other features. May not be less than zero.

``mesh.lods.simplygon.vertexColorImportance``
  Type ``number``, default ``1``. Relative importance of vertex colors when generating LODs, compared to other features. May not be less than zero.

``mesh.lods.simplygon.screenSizeFactor``
  Type ``number``, default ``2``. Factor to apply to the target screen size. For example, if the factor is 2, and Simplygon generates a LOD targeting
  screen size 100px, the threshold for CMF mesh lod will be set to 200px. The factor is needed because Simplygon is very conservative when targeting
  screen sizes, and the asset may get away with much more relaxed LODs.

``mesh.lods.simplygon.lockVertexChannel``
  Type ``string``, default ``""``. The name of the vertex color set used as a "locked" vertex mask. Locked vertices are preserved by Simplygon when generating LODs.
  The program will set the locked flag based on the contents of the red channel in the vertex color: the vertex is locked if the red channel is not black.

``mesh.audioOcclusionMesh.generate``
  Type ``boolean``, default ``false``. Whether to generate an audio occlusion mesh for the mesh. The audio occlusion mesh is a simplified version of the original mesh 
  that can be used for audio occlusion calculations in games. Audio occlusion mesh is an extremely low polygon mesh with just vertex positions. The mesh
  may not have any degenerate triangles or T-junctions.

``mesh.audioOcclusionMesh.method``
  Type ``string``, default ``Simplygon``. Method/library used to generate audio occlusion meshes. Currently, the only method supported is Simplygon.

``mesh.audioOcclusionMesh.simplygon.screenSize``
    Type ``number``, default ``80``. Target screen size in pixels for audio occlusion mesh when using Simplygon. The smaller the value, the more aggressive the simplification is.
    The number may not be less than 20.

``mesh.audioOcclusionMesh.simplygon.holeFilling``
  Type ``string``, default ``Medium``. Determines how aggressively Simplygon fills holes in the mesh when generating audio occlusion mesh. 
  Possible values are ``Disabled``, ``Low``, ``Medium``, and ``High``. 


Skeleton settings
^^^^^^^^^^^^^^^^^

``skeleton.import``
  Type ``boolean``, default ``true``. Whether to import skeletons into CMF file. When this setting is set to ``false`` skeletons are skipped during import.

``skeleton.filter``
  Type ``array`` of ``string``, default ``[]``. Names of skeletons (root bone nodes) to include in the CMF file. This setting allows only importing skeletons with specific names. When the array is empty, all skeletons are imported.
  Note that if ``skeleton.import`` is ``false``, none of the skeletons are imported regardless of this setting. If one or more ``--skeleton`` command line arguments are specified, their skeleton names completely override this
  setting.

``skeleton.moveToOrigin``
  Type ``boolean``, default ``true``. If this setting is ``true``, move the root bone to the origin (i.e., reset its translation and rotation, but not scaling). 

Animation settings
^^^^^^^^^^^^^^^^^^

``animation.import``
  Type ``boolean``, default ``true``. Whether to import animations into CMF file. When this setting is set to ``false`` animations are skipped during import.

``animation.filter``
  Type ``array`` of ``string``, default ``[]``. Names of animation tracks to include in the CMF file. This setting allows only importing animations with specific names. When the array is empty, all animation tracks are imported.
  Note that if ``animation.import`` is ``false`` none of the skeletons are imported regardless of this setting. If one or more ``--animation`` command line arguments are specified, their animation names completely override this
  setting.

``animation.moveToOrigin``
  Type ``boolean``, default ``true``. If this setting is ``true``, the animation for the root bone translation and orientation is relative to its pose on the first frame.

``animation.reduceKeyframes``
  Type ``boolean``, default ``true``. If this setting is ``true``, the animation keyframes will be reduced to remove redundant keys reconstructible by interpolation.

``animation.keyReductionTolerance``
  Type ``number``, default ``0.001``. Tolerance value used when reducing animation keyframes. Keys that can be reconstructable within this tolerance are considered redundant and removed.
  The smaller the value, the more keyframes will be retained.

``animation.optimizeFormat``
  Type ``boolean``, default ``true``. If this setting is ``true``, the importer tries to find a smaller representation for the animation data (normalized fixed-point values, half-precision floats, etc.), 
  optimizing the format for storage.

``animation.keyTolerance``
  Type ``number``, default ``0.001``. Tolerance value for keyframe times used when optimizing curve format. Smaller the values may prevent format optimization.

``animation.valueTolerance``
  Type ``number``, default ``0.0001``. Tolerance value for keyframe values used when optimizing curve format. Smaller the values may prevent format optimization.


Examples
^^^^^^^^

A few examples of settings files.

Settings for exporting skeleton and animation files:::

    {
        "mesh": {
            "import": false
        }
    }

Settings for hard-skinned object with no animations:::

    {
        "mesh": {
            "bonesPerVertex": 1
        },
        "animation": {
            "import": false
        }
    }

General notes
-------------

* The importer uses a legacy coordinate system transformation from FBX coordinate space to CMF space. It may not always produce expected results, but it is kept for backward compatibility. If there are issues with 
  coordinate system transform during import, especially for left-handed coordinate systems, it is better to change the coordinate system setup in DCC tool to match CMF and re-export the FBX file.
* When importing meshes, the importer treats each mesh node as a separate mesh, disregarding any instancing. So if the source file contains several mesh nodes referring to the same mesh, the imported CMF file will contain several meshes,
  one for each mesh node.
* When exporting skeletons, the root bone is considered to be the top-most bone in the hierarchy. Howerver if this bone is not a top-level node in the scene (i.e., directly under the scene node), the importer will treat
  its direct parent node as the skeleton root. This is done for compatibility with the legacy import pipeline.

Morph targets
-------------

The importer supports importing morph targets from FBX blend shapes. The importer will always store vertex positions in the morph target buffer. It will also store normals and/or tangent spaces according to ``mesh.normals`` and ``mesh.tangents``
settings. While FBX format allows storing vertex normals for blend shapes, DCC tools normally don't export them as they are considered to be derived data. The importer does not even try to import normals from FBX blend shapes. Instead, it will
always recompute blend shape normals based on deformed mesh geometry. When recomputing, it will try to use smoothness data for the mesh. If that is missing, it assumes the mesh to be fully smooth. Note that the resulting normals may differ
from the base mesh normals somewhat, even if the vertices in the blend shape don't move. This is the result of possibly different algorithms used for comping normals in the importer vs. DCC tool. Depending on the use case, it may cause
visual artifacts when morph targets are applied to the mesh. To avoid that, it may be beneficial to recompute base mesh vertex normals on import for meshes with blend shapes using ``mesh.regenerateNormals`` setting. Doing that will ensure that
base mesh normals will match blend shape normals for parts of the mesh not affected by the blend shape.

Alternatively, the importer can fetch per-blend shape face vertex normals from custom FBX attributes. This behaviour is controlled by ``mesh.morphTargets.useCustomNormals`` setting. The importer will look for a custom FBX attribute on the mesh node
for the mesh containing a blend shape. The name of the attribute should match the name of the blend shape with ``bsNormals_`` prepended to it. The attribute must be a string containing a base64-encoded array of per face vertex (not per vertex!) normal vectors 
(three 32-bit numbers per normal). There must be a custom property for each blend shape. Generating such FBX file would require some scripting on the DCC side. It also makes source FBX file sizes significantly larger. This custom normals mode
should only be used as a last resort if generating normals does not produce acceptable results.

LOD Generation
--------------

The importer supports LOD generation for meshes. Currently, the only supported method of generating LODs is with Simplygon library. Simplygon SDK is not open-source or free, so Simplygon support is optional and under a feature flag. LOD generation can take a lot of time for larger models.

LOD generation with Simplygon targets multiple screen sizes for the mesh with at least 10% polygon reduction between LODs and chooses a few resulting LODs with the biggest change in polygon count, so that the total number of LODs stays under the specified limit. There are a number of options
to control the relative importance of different mesh features (vertex positions, normals, etc.) when removing vertices.

Simplygon is very conservative when targeting specific screen sizes: it tries to create a LOD so that the LOD switch would be imperceptible at the specified screen size. For games, this may be too conservative, so there is an option 
``mesh.lods.simplygonOptions.screenSizeFactor`` to scale the target screen size when saving CMF. 

For certain models, there may be a need to preserve some vertices across LODs. For example, if a character model contains separate meshes for the head and body, those would be the vertices on the neck seam. It is possible to "lock" such vertices.
For that, the mesh needs to contain a vertex color set, with the red color channel containing a "locked" flag: the vertex is considered locked if the channel is not fully black. This behavior is controlled by 
``mesh.lods.simplygon.lockVertexChannel`` setting: it contains the name of the vertex color set. If the setting is not an empty string, the mesh must contain the specified color set. 


Metadata
--------

The importer will inject a few metadata keys into the generated CMF file that can be useful for asset pipelines. It injects the following values:

``source``
  Path to the source FBX file relative to the directory of the output CMF file. This may allow tools to find the source FBX file given a CMF file. It is possible to override the contents of this value using ``--path`` command line argument. Overriding
  may be useful if, for example, there is a known root directory for all asset files, and the path can be formed relative to that directory. Or if the path is a virtual file system path, or CMS path.

``sourceHash``
  A string containing a hash value for the source file contents. Can be used to check if the source file has changed since the last time it was exported to CMF.

``generator``
  A string ``fbximporter`` signifying that the CMF file was generated using the FBX importer program.

``generatorVersion``
  The current version of ``fbximporter``. Can be used to find outdated CMF files generated with older versions of the importer.

``options``
  The value is the JSON encoding of all the options used to generate the CMF file. It is not a verbatim copy of the configuration file: any extra data not recognized by the importer is dropped from it, but all the default values, as well as 
  command line overrides, are populated. This data can be used for re-importing the CMF file with the same options as before.

Examples
--------

Basic import:

.. code-block:: bash

   cmfprocessor fbximport model.fbx model.cmf

Import specific meshes:

.. code-block:: bash

   cmfprocessor fbximport --mesh "Body" --mesh "Head" model.fbx model.cmf

Import with configuration file:

.. code-block:: bash

   cmfprocessor fbximport --config import_config.json model.fbx model.cmf

Import with custom metadata path:

.. code-block:: bash

   cmfprocessor fbximport --path "assets/models/model.fbx" model.fbx output/model.cmf
