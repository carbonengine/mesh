# Copyright © 2025 CCP ehf.

import sys
import os
import re

def enumerate_files( sourceDir, binaryDir, output):
    shaderGroups = {}
    vertexDeclarations = {}
    
    # matches the pattern:
    # layout( location = 0 ) in vec3 inPosition;
    # grabs the 0 and inPosition
    pattern = r'layout\s*\(\s*location\s*=\s*(\d+)\s*\)\s*in\s+\w+\s+(\w+);'

    shaderUsageLookup = {
        "inposition": "cmf::Usage::Position",
        "innormal": "cmf::Usage::Normal",
        "inbinormal": "cmf::Usage::Binormal",
        "intangent": "cmf::Usage::Tangent",
        "inpackedtangents": "cmf::Usage::PackedTangent",
        "inpackedtangentslegacy": "cmf::Usage::PackedTangentLegacy",
        "incolor": "cmf::Usage::Color",
        "intexcoord": "cmf::Usage::TexCoord",
        "inboneindices": "cmf::Usage::BoneIndices",
        "inboneweights": "cmf::Usage::BoneWeights"
    };
        
    # find all the input vertex declarations of vertex shaders
    for filename in os.listdir(sourceDir):
        if not filename.endswith('.vert'):
            continue
        
        parts = filename.split('.')
        name = parts[0]

        with open(os.path.join(sourceDir, filename), 'r') as vertexShader:
            inputDeclarations = []
            for line in vertexShader:
                match = re.search(pattern, line)
                if match:
                    variableName = str(match.group(2)).lower()
                    try:
                        usage = shaderUsageLookup[variableName]
                    except KeyError as e:
                        print("could not find '%s' in applicable vertex variable names %s" % ( variableName, shaderUsageLookup.keys() ))
                        raise e

                    inputDeclarations.append( ( int(match.group(1)), usage ))
            vertexDeclarations[name] = inputDeclarations        
    for filename in os.listdir(binaryDir):
        if not filename.endswith('.spv.h'):
            continue

        parts = filename.split('.')
        name = parts[0]
        shaderType = parts[1]
        shaders = shaderGroups.get(name, [None, None, None])
        if shaderType == 'vert':
            shaders[0] = filename
        elif shaderType == 'frag':
            shaders[1] = filename
        elif shaderType == 'comp':
            shaders[2] = filename

        shaderGroups[name] = shaders

    code = ""

    for name, shaders in shaderGroups.items():
   
        if (not shaders[0] or not shaders[1]) and not shaders[2]:
            print("Warning: Shader group '%s' is missing a shader." % name)
            continue

        vertShader = "std::nullopt"
        fragShader = "std::nullopt"
        compShader = "std::nullopt"
        if shaders[0]:
            vertShader = """Shader( {
                #include \"%s\" 
                }
             )""" % ( shaders[0],  )
        if shaders[1]:
            fragShader = """Shader( {
                #include \"%s\"
             } )""" % shaders[1]
        if shaders[2]:
            compShader = """Shader( {
                #include \"%s\"
             } )""" % shaders[2]


        splitNames = name.replace("model_", "").split("_")
        displayName = name
        if name.startswith("model_"):
            displayName = " ".join(s[0].upper() + s[1:] for s in splitNames)
           
        code +=  """
{\"%s\", { 
    %s,
    %s,
    %s,
    %s,
    { %s }
    } 
},""" % (displayName, vertShader, fragShader, compShader, str(name.startswith("model_")).lower(), ",\n".join(["{%d, %s}" % (index, usage) for (index, usage) in vertexDeclarations.get(name, {}) ]))

    code = code[:-1]
    with open(output, 'w') as f:
        f.write(code)


if __name__ == '__main__':
    if len(sys.argv) != 4:
        print("Usage: shaderCacheCreator.py <source_shader_directory> <built_shader_directory> <output file>")
        sys.exit(1)
    enumerate_files(sys.argv[1], sys.argv[2], sys.argv[3])
