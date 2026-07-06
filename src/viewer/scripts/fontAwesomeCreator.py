# Copyright © 2026 CCP ehf.

"""Generate ImGui-readable Font Awesome headers.

This script has two purposes:
1) Generate a font byte-data header consumable by Dear ImGui.
2) Generate a Font Awesome icon define header for use in C++ UI code.

The output style intentionally mirrors the spirit of
https://github.com/ocornut/imgui/blob/master/misc/fonts/binary_to_compressed_c.cpp
by emitting a `unsigned int` data array and explicit byte size.
"""

import json
import re
import sys
from pathlib import Path
from typing import Dict, Iterable, List, Tuple


def _find_repo_root():
	"""Resolve repository root from this script location."""
	return Path(__file__).resolve().parents[3]


def _chunked(values, chunk_size):
	"""
	Yield successive `chunk_size`-sized chunks from `values`.
	@param values: An iterable of values to chunk.
	@param chunk_size: The size of each chunk to yield.
	@return: An iterator of lists, each containing up to `chunk_size` elements from `values`.
	"""
	chunk = []
	for value in values:
		chunk.append(value)
		if len(chunk) == chunk_size:
			yield chunk
			chunk = []
	if chunk:
		yield chunk


def _bytes_to_u32_words(data):
	"""
	Convert a bytes object into a list of 32-bit unsigned integers (words).
	@param data: A bytes object to convert.
	@return: A list of 32-bit unsigned integers representing the input bytes.
	"""
	words = []
	for i in range(0, len(data), 4):
		block = data[i : i + 4]
		value = 0
		for byte_index, byte_value in enumerate(block):
			value |= byte_value << (byte_index * 8)
		words.append(value)
	return words


def _format_word_array(words, per_line = 12):
	"""
	Format a list of 32-bit unsigned integers as a C-style array.
	@param words: A list of 32-bit unsigned integers to format.
	@param per_line: The number of elements per line in the output array.
	@return: A string representing the formatted C-style array.
	"""
	formatted_lines = []
	for chunk in _chunked(words, per_line):
		joined = ", ".join(f"0x{value:08x}" for value in chunk)
		formatted_lines.append(f"    {joined},")
	return "\n".join(formatted_lines)


def _sanitize_macro_suffix(icon_name):
	"""
	Sanitize an icon name to be used as a C++ macro suffix.
	@param icon_name: The original icon name.
	@return: A sanitized string suitable for use as a macro suffix.
	"""
	upper = icon_name.upper().replace("-", "_")
	upper = re.sub(r"[^A-Z0-9_]", "_", upper)
	upper = re.sub(r"_+", "_", upper).strip("_")
	return upper if upper else "UNKNOWN"


def _codepoint_to_utf8_hex_escapes(codepoint):
	"""
	Convert a Unicode codepoint to a string of UTF-8 hex escapes.
	@param codepoint: The Unicode codepoint to convert.
	@return: A string of UTF-8 hex escapes representing the codepoint.
	"""
	encoded = chr(codepoint).encode("utf-8")
	return "".join(f"\\x{byte:02x}" for byte in encoded)


def _format_ratio(width, height):
	"""
	Format the ratio of width to height as a C-style float literal.
	@param width: The width value.
	@param height: The height value.
	@return: A string representing the ratio as a C-style float literal.
	"""
	if height == 0:
		return "1.0f"

	ratio = width / height
	formatted = f"{ratio:.6g}"
	if "." not in formatted:
		formatted += ".0"
	return f"{formatted}f"


def _read_icon_size(payload):
	"""
	Read the size of an icon from its payload.
	@param payload: The icon payload dictionary.
	@return: A tuple containing the width and height of the icon.
	"""
	svg_payload = payload.get("svg")
	if not isinstance(svg_payload, dict):
		return 0.0, 0.0

	solid_payload = svg_payload.get("solid")
	if not isinstance(solid_payload, dict):
		return 0.0, 0.0

	width = solid_payload.get("width")
	height = solid_payload.get("height")
	if isinstance(width, (int, float)) and isinstance(height, (int, float)):
		return float(width), float(height)

	view_box = solid_payload.get("viewBox")
	if isinstance(view_box, list) and len(view_box) >= 4:
		view_box_width = view_box[2]
		view_box_height = view_box[3]

		return float(view_box_width), float(view_box_height)

	return 0.0, 0.0


def _load_icons(icons_json_path):
	"""
	Load and filter icons from a Font Awesome icons.json file.
	@param icons_json_path: The path to the icons.json file.
	@return: A list of tuples containing icon name, codepoint, width, and height.
	"""
	with icons_json_path.open("r", encoding="utf-8") as input_file:
		icon_data: Dict[str, Dict[str, object]] = json.load(input_file)

	icons: List[Tuple[str, int, float, float]] = []
	for icon_name, payload in icon_data.items():
		styles = payload.get("styles", [])
		unicode_hex = payload.get("unicode")
		if "solid" not in styles or not isinstance(unicode_hex, str):
			continue

		codepoint = int(unicode_hex, 16)
		width, height = _read_icon_size(payload)
		icons.append((icon_name, codepoint, width, height))

	icons.sort(key=lambda item: item[0])
	return icons


def _build_font_header(ttf_bytes, ttf_path):
	"""
	Build a C++ header string containing the TTF font data as a byte array.
	@param ttf_bytes: The bytes of the TTF font file.
	@param ttf_path: The path to the TTF font file.
	@return: A string representing the C++ header with the font data.
	"""
	words = _bytes_to_u32_words(ttf_bytes)
	array_body = _format_word_array(words)

	return (
		"// Auto-generated by fontAwesomeCreator.py.\n"
		"// Source TTF: "
		f"{ttf_path.as_posix()}\n"
		"// Dear ImGui usage example:\n"
		"// io.Fonts->AddFontFromMemoryTTF(\n"
		"//     const_cast<unsigned int*>(fa_solid_900_ttf_data),\n"
		"//     static_cast<int>(fa_solid_900_ttf_size),\n"
		"//     16.0f, &config, iconRanges);\n"
		"#pragma once\n\n"
		"static const unsigned int fa_solid_900_ttf_size = "
		f"{len(ttf_bytes)}u;\n"
		f"static const unsigned int fa_solid_900_ttf_data[{len(words)}] =\n"
		"{\n"
		f"{array_body}\n"
		"};\n"
	)


def _build_icon_define_header(icons, icons_path):
	"""
	Build a C++ header string defining Font Awesome icons with their UTF-8 representations and aspect ratios.
	@param icons: A list of tuples containing icon name, codepoint, width, and height.
	@param icons_path: The path to the icons.json file.
	@return: A string representing the C++ header with icon definitions.
	"""
	if not icons:
		raise ValueError("No solid icons were found in the input icons metadata.")

	min_cp = min(codepoint for _, codepoint, _, _ in icons)
	max_cp = max(codepoint for _, codepoint, _, _ in icons)

	lines: List[str] = [
		"// Auto-generated by fontAwesomeCreator.py.",
		f"// Source icons.json: {icons_path.as_posix()}",
		"#pragma once",
		"",
		f"#define ICON_MIN_FA 0x{min_cp:04x}",
		f"#define ICON_MAX_FA 0x{max_cp:04x}",
		"static const unsigned short ICON_RANGE_FA[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };",
		"",
		"struct FaIcon",
		"{",
		"\tconst char* text;",
		"\tfloat xyRatio;",
		"};",
		"",
	]

	for icon_name, codepoint, width, height in icons:
		macro_suffix = _sanitize_macro_suffix(icon_name)
		utf8_escaped = _codepoint_to_utf8_hex_escapes(codepoint)
		ratio = _format_ratio(width, height)
		lines.append(f"static const FaIcon ICON_FA_{macro_suffix} = {{ \"{utf8_escaped}\", {ratio} }};")

	lines.append("")
	return "\n".join(lines)


def _parse_args(argv):
	"""
	Supported forms:
	- fontAwesomeCreator.py <input_ttf> <icons_json> <out_font_header> <out_define_header>
	"""
	if len(argv) == 5:
		input_ttf = Path(argv[1]).resolve()
		icons_json = Path(argv[2]).resolve()
		output_font_header = Path(argv[3]).resolve()
		output_define_header = Path(argv[4]).resolve()
		return input_ttf, icons_json, output_font_header, output_define_header

	usage = (
		"Usage:\n"
		"  python fontAwesomeCreator.py <input_ttf> <icons_json> <out_font_header> <out_define_header>"
	)
	raise ValueError(usage)


def main(argv):
	try:
		input_ttf, icons_json, out_font_header, out_define_header = _parse_args(argv)

		if not input_ttf.is_file():
			raise FileNotFoundError(f"Input TTF not found: {input_ttf}")
		if not icons_json.is_file():
			raise FileNotFoundError(f"Input icons JSON not found: {icons_json}")

		out_font_header.parent.mkdir(parents=True, exist_ok=True)
		out_define_header.parent.mkdir(parents=True, exist_ok=True)

		ttf_bytes = input_ttf.read_bytes()
		icons = _load_icons(icons_json)

		out_font_header.write_text(_build_font_header(ttf_bytes, input_ttf), encoding="utf-8")
		out_define_header.write_text(_build_icon_define_header(icons, icons_json), encoding="utf-8")
	except Exception as exc:  # broad catch to keep the generator easy to diagnose from CMake logs
		print(str(exc))
		return 1

	return 0


if __name__ == "__main__":
	sys.exit(main(sys.argv))
