# Copyright © 2025 CCP ehf.

import os
import re
import sys


VERTEX_DECLARATION_PATTERN = re.compile(
    r"^\s*layout\s*\(\s*location\s*=\s*(\d+)\s*\)\s*in\s+\w+\s+(\w+)\s*;"
)

MULTI_USAGE_VERTEX_NAME = re.compile(
	r"^\s*#pragma\s+multi_usage\s+(\w+)\s*$"
)

SHADER_USAGE_LOOKUP = {
    "inposition": "cmf::Usage::Position",
    "innormal": "cmf::Usage::Normal",
    "inbinormal": "cmf::Usage::Binormal",
    "intangent": "cmf::Usage::Tangent",
    "inpackedtangents": "cmf::Usage::PackedTangent",
    "inpackedtangentslegacy": "cmf::Usage::PackedTangentLegacy",
    "incolor": "cmf::Usage::Color",
    "intexcoord": "cmf::Usage::TexCoord",
    "inboneindices": "cmf::Usage::BoneIndices",
    "inboneweights": "cmf::Usage::BoneWeights",
}


def shader_wrapper(shader_name):
    return 'Shader( {\n                #include "%s"\n            } )' % shader_name


def parse_vertex_declarations(shader_path):
    declarations = []
    multi_usage_names = []

    with open(shader_path, "r") as vertex_shader:
        for line in vertex_shader:
            multi_usage_names_match = MULTI_USAGE_VERTEX_NAME.match(line)
            if multi_usage_names_match:
                multi_usage_names.append(multi_usage_names_match.group(1).lower())
                continue

            match = VERTEX_DECLARATION_PATTERN.match(line)
            if not match:
                continue


            variable_name = match.group(2).lower()
            if variable_name not in SHADER_USAGE_LOOKUP:
                raise KeyError(
                    "could not find '%s' in applicable vertex variable names %s"
                    % (variable_name, SHADER_USAGE_LOOKUP.keys())
                )

            declarations.append((int(match.group(1)), SHADER_USAGE_LOOKUP[variable_name], variable_name in multi_usage_names))

    if len(multi_usage_names) > 1:
        print("ERROR: Shader '%s' has multi usage index elements: %s" % (os.path.basename(shader_path), multi_usage_names))
        print("Only zero or one elements can be multi index usage")
        sys.exit(1)

    return declarations


def collect_vertex_declarations(source_dir):
    vertex_declarations = {}

    for filename in sorted(os.listdir(source_dir)):
        if filename.endswith(".vert"):
            shader_name = filename.rsplit(".", 1)[0]
            vertex_declarations[shader_name] = parse_vertex_declarations(os.path.join(source_dir, filename))

    return vertex_declarations


def collect_shader_groups(binary_dir):
    shader_groups = {}

    for filename in sorted(os.listdir(binary_dir)):
        if not filename.endswith(".spv.h"):
            continue

        parts = filename.split(".")
        if len(parts) < 3:
            continue

        shader_name = parts[0]
        shader_type = parts[1]
        group = shader_groups.setdefault(shader_name, {"vert": None, "frag": None, "comp": None})

        if shader_type in group:
            group[shader_type] = filename

    return shader_groups


def display_name(shader_name):
    if not shader_name.startswith("model_"):
        return shader_name

    parts = shader_name[len("model_"):].split("_")
    return " ".join(part[:1].upper() + part[1:] for part in parts if part)


def format_input_layout(shader_name, vertex_declarations):
    entries = vertex_declarations.get(shader_name, [])
    return ",\n".join("{%d, %s, %d}" % (location, usage, int(multi_usage)) for location, usage, multi_usage in entries)


def format_shader_entry(shader_name, shader_group, vertex_declarations):
    vert_shader = "std::nullopt"
    frag_shader = "std::nullopt"
    comp_shader = "std::nullopt"

    if shader_group["vert"]:
        vert_shader = shader_wrapper(shader_group["vert"])
    if shader_group["frag"]:
        frag_shader = shader_wrapper(shader_group["frag"])
    if shader_group["comp"]:
        comp_shader = shader_wrapper(shader_group["comp"])

    return (
        '    {"%s", { \n'
        '        %s,\n'
        '        %s,\n'
        '        %s,\n'
        '        %s,\n'
        '        { %s }\n'
        '        } \n'
        '    },'
        % (
            display_name(shader_name),
            vert_shader,
            frag_shader,
            comp_shader,
            str(shader_name.startswith("model_")).lower(),
            format_input_layout(shader_name, vertex_declarations),
        )
    )


def enumerate_files(source_dir, binary_dir, output_file):
    vertex_declarations = collect_vertex_declarations(source_dir)
    shader_groups = collect_shader_groups(binary_dir)

    entries = []
    for shader_name in sorted(shader_groups.keys()):
        shader_group = shader_groups[shader_name]

        if (not shader_group["vert"] or not shader_group["frag"]) and not shader_group["comp"]:
            print("Warning: Shader group '%s' is missing a shader." % shader_name)
            continue

        entries.append(format_shader_entry(shader_name, shader_group, vertex_declarations))

    code = "\n".join(entries)

    with open(output_file, "w") as f:
        f.write(code)

if __name__ == '__main__':
    if len(sys.argv) != 4:
        print("Usage: shaderCacheCreator.py <source_shader_directory> <built_shader_directory> <output file>")
        sys.exit(1)
    enumerate_files(sys.argv[1], sys.argv[2], sys.argv[3])