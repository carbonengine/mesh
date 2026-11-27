# Copyright © 2026 CCP ehf.
"""Check changed C++ code with clang-format and clang-tidy, and fail if either reports anything.

By default the script:
  1. Finds C++ files under src/ and include/ that differ from the merge-base with
     the base branch (committed, staged, unstaged and untracked changes).
  2. Checks the changed lines with clang-format (--dry-run --Werror), like CI does.
     --fix-format applies the formatting to the changed lines instead.
  3. Configures (once) a Ninja build in .cmake-build-tidy with a compile database,
     reusing an existing build's vcpkg_installed folder when it can. It always uses
     BUILD_TOOLS=ON, so cmfprocessor and viewer sources are covered.
  4. Cleans the compile database of flags clang-tidy can't handle (MSVC/Clang PCH
     usage, /MP, /GL).
  5. Runs clang-tidy on the changed .cpp files, plus the .cpp files that
     (transitively) include changed headers.
  6. Reports only diagnostics on changed lines (use --all-lines for whole files).
     Compiler errors are always reported, wherever they are.

Exit codes: 0 = clean, 1 = clang-format or clang-tidy findings, 2 = setup/usage error.

Examples:
  python scripts/run_clang_tidy.py                      # changes vs origin/main
  python scripts/run_clang_tidy.py --base HEAD          # only uncommitted changes
  python scripts/run_clang_tidy.py src/cmf/writer.cpp   # just this file (changed lines)
  python scripts/run_clang_tidy.py --all-lines src/cmf/writer.cpp
  python scripts/run_clang_tidy.py --format-only        # quick formatting check, no build
  python scripts/run_clang_tidy.py --fix-format         # reformat changed lines, then run clang-tidy
"""

import argparse
import concurrent.futures
import json
import os
import platform
import re
import shlex
import shutil
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path( __file__ ).resolve().parent.parent
SOURCE_DIRS = ( "src", "include" )
SOURCE_EXTS = { ".cpp", ".cc", ".cxx" }
HEADER_EXTS = { ".h", ".hpp", ".hxx", ".inl" }
IS_WINDOWS = platform.system() == "Windows"

DIAG_RE = re.compile( r"^(?P<file>.+?):(?P<line>\d+):(?P<col>\d+): (?P<kind>warning|error|note): (?P<msg>.*)$" )


def Fail( message ):
	print( f"error: {message}", file=sys.stderr )
	sys.exit( 2 )


def Log( message ):
	print( message, flush=True )


def NormPath( path ):
	return os.path.normcase( os.path.normpath( os.path.abspath( path ) ) )


def Run( cmd, env=None, cwd=REPO_ROOT, check=True, capture=True ):
	result = subprocess.run( cmd, cwd=cwd, env=env, text=True, encoding="utf-8", errors="replace",
							 stdout=subprocess.PIPE if capture else None, stderr=subprocess.STDOUT if capture else None )
	if check and result.returncode != 0:
		output = result.stdout or ""
		cmdText = cmd if isinstance( cmd, str ) else " ".join( map( str, cmd ) )
		Fail( f"command failed ({result.returncode}): {cmdText}\n{output[-4000:]}" )
	return result


def Git( *args ):
	return Run( [ "git", *args ] ).stdout


# --------------------------------------------------------------------------------------------------
# Toolchain discovery


def FindVisualStudio():
	vswhere = Path( os.environ.get( "ProgramFiles(x86)", r"C:\Program Files (x86)" ) ) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
	if not vswhere.exists():
		return None
	out = Run( [ str( vswhere ), "-latest", "-products", "*", "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath" ], check=False ).stdout.strip()
	return Path( out.splitlines()[0] ) if out else None


def LoadVcvarsEnvironment( vsPath ):
	vcvars = vsPath / "VC" / "Auxiliary" / "Build" / "vcvars64.bat"
	if not vcvars.exists():
		Fail( f"vcvars64.bat not found at {vcvars}" )
	# Raw command string: list2cmdline would escape the inner quotes, which cmd.exe doesn't understand.
	out = Run( f'cmd /d /s /c ""{vcvars}" >nul && set"' ).stdout
	env = {}
	for line in out.splitlines():
		key, sep, value = line.partition( "=" )
		if sep and key:
			env[key] = value
	# Windows variable names are case-insensitive, and PATH is usually spelled "Path". Normalise it to a single
	# "PATH" key: a second, differently-cased key would shadow the real one in child processes.
	pathKeys = [ k for k in env if k.upper() == "PATH" ]
	pathValue = ";".join( env.pop( k ) for k in pathKeys )
	ninjaDir = vsPath / "Common7" / "IDE" / "CommonExtensions" / "Microsoft" / "CMake" / "Ninja"
	if ninjaDir.exists():
		pathValue = f"{pathValue};{ninjaDir}"
	env["PATH"] = pathValue
	return env


def FindLlvmTool( name, explicit, vsPath, path=None ):
	"""Locate clang-tidy / clang-format: explicit path, env var, Visual Studio's bundled LLVM, PATH, Homebrew."""
	envVar = name.upper().replace( "-", "_" )
	candidates = []
	if explicit:
		candidates.append( Path( explicit ) )
	if os.environ.get( envVar ):
		candidates.append( Path( os.environ[envVar] ) )
	if vsPath:
		candidates.append( vsPath / "VC" / "Tools" / "Llvm" / "x64" / "bin" / f"{name}.exe" )
	onPath = shutil.which( name, path=path )
	if onPath:
		candidates.append( Path( onPath ) )
	if platform.system() == "Darwin":
		candidates += [ Path( f"/opt/homebrew/opt/llvm/bin/{name}" ), Path( f"/usr/local/opt/llvm/bin/{name}" ) ]
	for candidate in candidates:
		if candidate.exists():
			return candidate
	Fail( f"{name} not found. Pass --{name} <path> or set {envVar}." )


def DefaultPreset():
	if IS_WINDOWS:
		return "x64-windows-release"
	if platform.system() == "Darwin":
		return "arm64-osx-release" if platform.machine() == "arm64" else "x64-osx-release"
	Fail( "unsupported platform; pass --preset explicitly" )


def PresetTriplet( presetName ):
	presets = { p["name"]: p for p in json.loads( ( REPO_ROOT / "CMakePresets.json" ).read_text( encoding="utf-8" ) )["configurePresets"] }
	user = REPO_ROOT / "CMakeUserPresets.json"
	if user.exists():
		presets.update( { p["name"]: p for p in json.loads( user.read_text( encoding="utf-8" ) ).get( "configurePresets", [] ) } )

	def Resolve( name ):
		preset = presets.get( name )
		if preset is None:
			return None
		triplet = preset.get( "cacheVariables", {} ).get( "VCPKG_TARGET_TRIPLET" )
		if triplet:
			return triplet
		parents = preset.get( "inherits", [] )
		for parent in ( [ parents ] if isinstance( parents, str ) else parents ):
			triplet = Resolve( parent )
			if triplet:
				return triplet
		return None

	if presetName not in presets:
		Fail( f"unknown preset '{presetName}'" )
	return Resolve( presetName )


# Packages from the vcpkg "tools" feature (share/ folder names). A reused vcpkg_installed must have them, because the
# tidy build always configures with BUILD_TOOLS=ON and can't install anything into another build's folder.
TOOLS_PACKAGES = ( "glfw3", "imgui", "shaderc", "cli11" )


def FindReusableVcpkgInstall( triplet, buildDir ):
	if not triplet:
		return None
	for candidate in sorted( REPO_ROOT.glob( ".cmake-build-*" ) ):
		if NormPath( candidate ) == NormPath( buildDir ):
			continue
		installed = candidate / "vcpkg_installed"
		if not ( ( installed / triplet ).is_dir() and ( installed / "vcpkg" ).is_dir() ):
			continue
		missingTools = [ p for p in TOOLS_PACKAGES if not ( installed / triplet / "share" / p ).is_dir() ]
		if missingTools:
			Log( f"Not reusing {installed}: it lacks tools packages ({', '.join( missingTools )}); configure it with BUILD_TOOLS=ON to make it reusable." )
			continue
		return installed
	return None


# --------------------------------------------------------------------------------------------------
# Build / compile database


def Configure( args, buildDir, env ):
	triplet = PresetTriplet( args.preset )
	cmd = [ "cmake", "--preset", args.preset, "-S", str( REPO_ROOT ), "-B", str( buildDir ), "-G", "Ninja",
			"-DCMAKE_EXPORT_COMPILE_COMMANDS=ON", "-DBUILD_TOOLS=ON", "-DBUILD_DOCUMENTATION=OFF" ]

	installed = None
	if args.vcpkg_installed_dir:
		installed = Path( args.vcpkg_installed_dir )
	elif not args.no_reuse_vcpkg:
		installed = FindReusableVcpkgInstall( triplet, buildDir )
	if installed:
		Log( f"Reusing vcpkg packages from {installed}" )
		cmd += [ f"-DVCPKG_INSTALLED_DIR={installed.as_posix()}", "-DVCPKG_MANIFEST_INSTALL=OFF" ]
	else:
		Log( "No reusable vcpkg install found; vcpkg will build all dependencies (slow on first run)." )

	withSimplygon = args.with_simplygon
	if withSimplygon is None:
		withSimplygon = bool( installed and ( installed / triplet / "share" / "simplygon" ).is_dir() )
	cmd.append( f"-DWITH_SIMPLYGON={'ON' if withSimplygon else 'OFF'}" )

	Log( f"Configuring {buildDir} (preset {args.preset}, Simplygon {'ON' if withSimplygon else 'OFF'}) ..." )
	Run( cmd, env=env )


def BuildGeneratedHeaders( buildDir, env ):
	# Also re-runs CMake if any CMakeLists.txt changed, which refreshes compile_commands.json.
	Run( [ "cmake", "--build", str( buildDir ), "--target", "CompileShaders", "CompileFontAwesome" ], env=env )


PCH_PATTERNS = [
	re.compile( r'\s[/-]Y[uc]("[^"]*"|\S*)' ),  # MSVC: use/create PCH
	re.compile( r'\s[/-]Fp("[^"]*"|\S*)' ),  # MSVC: PCH file
	re.compile( r'\s[/-]MP\d*(?=\s|$)' ),  # MSVC: parallel build
	re.compile( r'\s[/-]GL(?=\s|$)' ),  # MSVC: whole program optimisation
	re.compile( r'\s-Xclang\s+-include-pch\s+-Xclang\s+("[^"]*"|\S+)' ),  # Clang: PCH file
	re.compile( r'\s-Winvalid-pch(?=\s|$)' ),
]


def CleanCommand( command ):
	for pattern in PCH_PATTERNS:
		command = pattern.sub( "", command )
	return command


# Forced includes (MSVC /FI, GCC/Clang -include, optionally wrapped in -Xclang). CMake's target_precompile_headers
# injects cmake_pch.hxx this way, so headers listed in it have no textual #include in the sources.
FORCED_INCLUDE_RE = re.compile( r'(?:^|\s)(?:[/-]FI\s*|-include\s+(?:-Xclang\s+)?)("[^"]*"|\S+)' )


def WriteCleanCompileDatabase( buildDir, tidyDbDir ):
	"""Write the cleaned compile database. Returns (set of normalised source paths in it,
	{ normalised forced-include path: set of normalised source paths that force-include it })."""
	source = buildDir / "compile_commands.json"
	if not source.exists():
		Fail( f"{source} was not generated" )
	entries = json.loads( source.read_text( encoding="utf-8" ) )
	inDatabase, forcedIncluders = set(), {}
	for entry in entries:
		if "command" in entry:
			entry["command"] = CleanCommand( entry["command"] )
		elif "arguments" in entry:
			joined = subprocess.list2cmdline( entry["arguments"] ) if IS_WINDOWS else shlex.join( entry["arguments"] )
			entry.pop( "arguments" )
			entry["command"] = CleanCommand( " " + joined ).strip()
		directory = Path( entry.get( "directory", "" ) )
		file = NormPath( directory / entry["file"] )
		inDatabase.add( file )
		for match in FORCED_INCLUDE_RE.finditer( entry["command"] ):
			forced = NormPath( directory / match.group( 1 ).strip( '"' ) )
			forcedIncluders.setdefault( forced, set() ).add( file )
	tidyDbDir.mkdir( parents=True, exist_ok=True )
	( tidyDbDir / "compile_commands.json" ).write_text( json.dumps( entries, indent=1 ), encoding="utf-8" )
	return inDatabase, forcedIncluders


# --------------------------------------------------------------------------------------------------
# Change detection


def IsProjectSource( relPath ):
	parts = Path( relPath ).parts
	return bool( parts ) and parts[0] in SOURCE_DIRS and Path( relPath ).suffix.lower() in SOURCE_EXTS | HEADER_EXTS


def ResolveBase( base ):
	if base is None:
		for candidate in ( "origin/main", "main" ):
			if Run( [ "git", "rev-parse", "--verify", "--quiet", candidate ], check=False ).returncode == 0:
				base = candidate
				break
		else:
			Fail( "could not find origin/main or main; pass --base" )
	if base == "HEAD":
		return "HEAD"
	return Git( "merge-base", "HEAD", base ).strip()


def ChangedLines( base ):
	"""Return { relPath: list of (start, end) } for changed lines, or None for 'whole file'.

	An empty list means the file changed but has no added lines (deletion-only edits) or was deleted. Such files
	are still used to select translation units, but have no lines to format or to report warnings on."""
	changes = {}
	diff = Git( "diff", "-U0", "--no-color", "--no-ext-diff", "--diff-filter=AMR", base, "--", *SOURCE_DIRS )
	current = None
	for line in diff.splitlines():
		if line.startswith( "+++ " ):
			path = line[4:]
			current = path[2:] if path.startswith( "b/" ) else None
			if current and not IsProjectSource( current ):
				current = None
			if current:
				changes.setdefault( current, [] )
		elif line.startswith( "@@" ) and current:
			match = re.search( r"\+(\d+)(?:,(\d+))?", line )
			start, count = int( match.group( 1 ) ), int( match.group( 2 ) or 1 )
			if count > 0:
				changes[current].append( ( start, start + count - 1 ) )
	# Deleted files: a deleted header can still be included by files that weren't changed.
	for path in Git( "diff", "--name-only", "--no-renames", "--diff-filter=D", base, "--", *SOURCE_DIRS ).splitlines():
		if IsProjectSource( path ) and not ( REPO_ROOT / path ).exists():
			changes.setdefault( path, [] )
	for path in Git( "ls-files", "--others", "--exclude-standard", "--", *SOURCE_DIRS ).splitlines():
		if IsProjectSource( path ):
			changes[path] = None
	return changes


def SelectChanges( args ):
	"""Files and line ranges to check, after applying the file arguments and --all-lines."""
	if args.all_lines and args.files:
		return { f: None for f in args.files }
	changes = ChangedLines( ResolveBase( args.base ) )
	if args.files:
		wanted = set( args.files )
		changes = { p: r for p, r in changes.items() if p in wanted }
	if args.all_lines:
		changes = { p: None for p in changes }
	return changes


def IncludersOf( headers, forcedIncluders ):
	"""Transitively find project .cpp files that include any of the given headers, textually or through forced
	includes (forcedIncluders comes from WriteCleanCompileDatabase)."""
	# Everything is keyed by normalised absolute path, so generated forced-include files (e.g. cmake_pch.hxx in the
	# build directory) can take part in the walk alongside project files.
	tracked = [ p for p in Git( "ls-files", "--cached", "--others", "--exclude-standard", "--", *SOURCE_DIRS ).splitlines() if IsProjectSource( p ) ]
	relByNorm = { NormPath( REPO_ROOT / p ): p for p in tracked }
	contents = {}
	for path in [ *relByNorm, *forcedIncluders ]:
		try:
			contents[path] = Path( path ).read_text( encoding="utf-8", errors="replace" )
		except OSError:
			pass

	pending = [ NormPath( REPO_ROOT / h ) for h in headers ]
	seen, sources = set( pending ), set()
	while pending:
		header = pending.pop()
		for source in forcedIncluders.get( header, () ):
			if source in relByNorm:
				sources.add( relByNorm[source] )
		name = re.escape( Path( header ).name )
		includeRe = re.compile( r'^\s*#\s*include\s*[<"](?:[^>"]*[\\/])?' + name + r'[>"]', re.MULTILINE | re.IGNORECASE )
		for path, text in contents.items():
			if path in seen or not includeRe.search( text ):
				continue
			if Path( path ).suffix.lower() in SOURCE_EXTS:
				if path in relByNorm:
					sources.add( relByNorm[path] )
			else:
				seen.add( path )
				pending.append( path )
	return sources


# --------------------------------------------------------------------------------------------------
# Running clang-tidy


def ParseDiagnostics( output ):
	"""Split clang-tidy output into (file, line, kind, text) blocks; notes are attached to their parent."""
	diagnostics, current = [], None
	for line in output.splitlines():
		match = DIAG_RE.match( line )
		if match and match.group( "kind" ) != "note":
			current = [ match.group( "file" ), int( match.group( "line" ) ), match.group( "kind" ), [ line ] ]
			diagnostics.append( current )
		elif line.startswith( "error:" ):
			current = [ None, 0, "error", [ line ] ]
			diagnostics.append( current )
		elif current is not None and not re.match( r"^\d+ (warnings?|errors?) generated", line ):
			current[3].append( line )
	return diagnostics


def RunClangTidy( clangTidy, tidyDbDir, source, headerFilter, env ):
	cmd = [ str( clangTidy ), "-p", str( tidyDbDir ), "--quiet" ]
	if headerFilter:
		cmd.append( f"--header-filter={headerFilter}" )
	cmd.append( str( REPO_ROOT / source ) )
	result = Run( cmd, env=env, check=False )
	return source, result.returncode, result.stdout or ""


def CheckFormat( clangFormat, changes, fix ):
	"""Check (or fix, with fix=True) clang-format on the changed lines of each changed file. Returns the number of
	files with violations (0 when fixing)."""
	version = Run( [ str( clangFormat ), "--version" ], check=False ).stdout.strip()
	# An empty range list (deletion-only edit or deleted file) has nothing to format. Don't let it reach the
	# no-"--lines" case, which would format the whole file.
	toFormat = sorted( ( p, r ) for p, r in changes.items() if r != [] and ( REPO_ROOT / p ).exists() )
	Log( f"{'Formatting' if fix else 'Checking formatting of'} {len( toFormat )} file(s) with {version} ..." )
	badFiles = 0
	for path, ranges in toFormat:
		cmd = [ str( clangFormat ), "--style=file" ]
		cmd += [ f"--lines={start}:{end}" for start, end in ranges or [] ]
		cmd += [ "-i" ] if fix else [ "--dry-run", "--Werror" ]
		cmd.append( path )
		result = Run( cmd, check=False )
		if result.returncode != 0:
			if fix:
				Fail( f"clang-format failed on {path}:\n{result.stdout}" )
			badFiles += 1
			print( result.stdout.rstrip() )
	if fix:
		Log( "Formatting applied to changed lines. Review the diff." )
	elif badFiles:
		Log( f"\nclang-format: {badFiles} file(s) need formatting. Run with --fix-format to fix the changed lines." )
	else:
		Log( "clang-format: clean." )
	return badFiles


def PathRegex( relPath ):
	return r"[\\/]".join( re.escape( part ) for part in Path( relPath ).parts ) + "$"


def main():
	parser = argparse.ArgumentParser( description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter )
	parser.add_argument( "files", nargs="*", help="limit the check to these files (default: all changed files)" )
	parser.add_argument( "--base", help="branch/commit to diff against (default: merge-base with origin/main or main; HEAD = uncommitted only)" )
	parser.add_argument( "--all-lines", action="store_true", help="report diagnostics on all lines of the checked files, not just changed lines" )
	parser.add_argument( "--preset", help="CMake configure preset providing toolchain/triplet (default: <platform>-release)" )
	parser.add_argument( "--build-dir", default=".cmake-build-tidy", help="Ninja build directory (default: .cmake-build-tidy)" )
	parser.add_argument( "--reconfigure", action="store_true", help="force re-running CMake configure" )
	parser.add_argument( "--vcpkg-installed-dir", help="existing vcpkg_installed directory to reuse" )
	parser.add_argument( "--no-reuse-vcpkg", action="store_true", help="don't reuse another build's vcpkg_installed" )
	parser.add_argument( "--with-simplygon", dest="with_simplygon", action="store_true", default=None )
	parser.add_argument( "--without-simplygon", dest="with_simplygon", action="store_false" )
	parser.add_argument( "--clang-tidy", help="path to clang-tidy executable" )
	parser.add_argument( "--clang-format", help="path to clang-format executable" )
	formatGroup = parser.add_mutually_exclusive_group()
	formatGroup.add_argument( "--fix-format", action="store_true", help="apply clang-format to the changed lines instead of only checking them" )
	formatGroup.add_argument( "--skip-format", action="store_true", help="don't run the clang-format check" )
	formatGroup.add_argument( "--format-only", action="store_true", help="only run the clang-format check (no build, no clang-tidy)" )
	parser.add_argument( "-j", "--jobs", type=int, default=os.cpu_count() or 4 )
	args = parser.parse_args()
	args.preset = args.preset or DefaultPreset()

	# Resolve file arguments against the caller's working directory, before changing to the repo root.
	files = []
	for f in args.files:
		try:
			files.append( Path( f ).resolve().relative_to( REPO_ROOT ).as_posix() )
		except ValueError:
			Fail( f"{f} is not inside the repository ({REPO_ROOT})" )
	args.files = files

	os.chdir( REPO_ROOT )
	buildDir = ( REPO_ROOT / args.build_dir ).resolve()
	tidyDbDir = buildDir / "clang-tidy-db"

	changes = SelectChanges( args )
	if not changes:
		Log( "No changed C++ files under src/ or include/. Nothing to check." )
		return 0

	vsPath = FindVisualStudio() if IS_WINDOWS else None
	if IS_WINDOWS and not vsPath:
		Fail( "Visual Studio with C++ tools not found (vswhere)" )

	formatCode = 0
	if not args.skip_format:
		clangFormat = FindLlvmTool( "clang-format", args.clang_format, vsPath )
		formatCode = 1 if CheckFormat( clangFormat, changes, args.fix_format ) else 0
		if args.format_only:
			return formatCode
		if args.fix_format:
			# Formatting can split, join or move lines, so the pre-format ranges no longer match the files.
			changes = SelectChanges( args )
		print()

	tidyCode = RunTidy( args, changes, vsPath, buildDir, tidyDbDir )
	if formatCode and tidyCode == 0:
		Log( "Failing because of clang-format violations above." )
	return max( formatCode, tidyCode )


def RunTidy( args, changes, vsPath, buildDir, tidyDbDir ):
	headers = [ p for p in changes if Path( p ).suffix.lower() in HEADER_EXTS ]

	def SourcesToCheck( forcedIncluders ):
		sources = { p for p in changes if Path( p ).suffix.lower() in SOURCE_EXTS and ( REPO_ROOT / p ).exists() }
		if headers:
			sources |= IncludersOf( headers, forcedIncluders )
		return sources

	# Toolchain + compile database
	env = LoadVcvarsEnvironment( vsPath ) if IS_WINDOWS else dict( os.environ )
	clangTidy = FindLlvmTool( "clang-tidy", args.clang_tidy, vsPath, env.get( "PATH" ) )

	if args.reconfigure or not ( buildDir / "build.ninja" ).exists():
		Configure( args, buildDir, env )
	BuildGeneratedHeaders( buildDir, env )
	inDatabase, forcedIncluders = WriteCleanCompileDatabase( buildDir, tidyDbDir )
	sources = SourcesToCheck( forcedIncluders )

	missing = [ s for s in sources if NormPath( REPO_ROOT / s ) not in inDatabase ]
	if missing and not args.reconfigure:
		Log( "Some files are not in the compile database (new files?); re-running CMake ..." )
		Run( [ "cmake", str( buildDir ) ], env=env )
		inDatabase, forcedIncluders = WriteCleanCompileDatabase( buildDir, tidyDbDir )
		sources = SourcesToCheck( forcedIncluders )
		missing = [ s for s in sources if NormPath( REPO_ROOT / s ) not in inDatabase ]
	for path in sorted( missing ):
		Log( f"warning: {path} is not part of any build target; skipping" )
	sources = sorted( s for s in sources if s not in missing )
	if not sources:
		Log( "No compilable files to check." )
		return 0

	headerFilter = "|".join( PathRegex( h ) for h in headers ) if headers else ""
	changedByPath = { NormPath( REPO_ROOT / p ): r for p, r in changes.items() }

	Log( f"Running {clangTidy.name} on {len( sources )} file(s) with {args.jobs} job(s)"
		 f" ({'all lines' if args.all_lines else 'changed lines only'}) ..." )
	findings, seen, failedRuns = [], set(), []
	with concurrent.futures.ThreadPoolExecutor( max_workers=args.jobs ) as pool:
		futures = [ pool.submit( RunClangTidy, clangTidy, tidyDbDir, s, headerFilter, env ) for s in sources ]
		for done, future in enumerate( concurrent.futures.as_completed( futures ), 1 ):
			source, returnCode, output = future.result()
			Log( f"[{done}/{len( sources )}] {source}" )
			diagnostics = ParseDiagnostics( output )
			if returnCode != 0 and not any( d[2] == "error" for d in diagnostics ):
				failedRuns.append( ( source, output ) )
			for file, line, kind, text in diagnostics:
				if kind != "error":
					ranges = changedByPath.get( NormPath( file ), False ) if file else False
					if ranges is False:
						continue
					if ranges is not None and not any( start <= line <= end for start, end in ranges ):
						continue
				key = text[0]
				if key not in seen:
					seen.add( key )
					findings.append( ( kind, "\n".join( text ) ) )

	for source, output in failedRuns:
		print( f"\nclang-tidy failed on {source}:\n{output[-2000:]}" )

	if findings:
		print()
		for _, text in findings:
			print( text )
		errors = sum( 1 for kind, _ in findings if kind == "error" )
		scope = "checked files" if args.all_lines else "changed code"
		print( f"\nclang-tidy: {len( findings ) - errors} warning(s), {errors} error(s) in {scope}." )
		return 1
	if failedRuns:
		return 2
	Log( f"\nclang-tidy: clean ({len( sources )} file(s) checked)." )
	return 0


if __name__ == "__main__":
	sys.exit( main() )
