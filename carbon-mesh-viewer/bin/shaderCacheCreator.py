import sys
import os

def enumerate_files( directory, output):
    shaderGroups = {}

    for filename in os.listdir(directory):
        if not filename.endswith('.spv.h'):
            continue

        parts = filename.split('.')
        name = parts[0]
        shaderType = parts[1]
        shaders = shaderGroups.get(name, [None, None])
        if shaderType == 'vert':
            shaders[0] = filename
        elif shaderType == 'frag':
            shaders[1] = filename

        shaderGroups[name] = shaders

    code = ""

    for name, shaders in shaderGroups.items():
   
        if not shaders[0] or not shaders[1]:
            print("Warning: Shader group '%s' is missing a shader." % name)
            continue

        vertShader = "std::nullopt"
        fragShader = "std::nullopt"

        if shaders[0]:
            vertShader = """Shader( {
                #include \"%s\"
             })""" % shaders[0]
        if shaders[1]:
            fragShader = """Shader( {
                #include \"%s\"
             })""" % shaders[1]

        code +=  "{\"%s\", { \n            %s,\n            %s\n         } }" % (name, vertShader, fragShader)

    with open(output, 'w') as f:
        f.write(code)


if __name__ == '__main__':
    if len(sys.argv) != 3:
        print("Usage: shaderCacheCreator.py <input_directory> <output file>")
        sys.exit(1)
    enumerate_files(sys.argv[1], sys.argv[2])
