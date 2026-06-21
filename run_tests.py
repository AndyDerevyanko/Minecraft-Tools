import os
import sys
import glob
import time
import shutil
import subprocess

SCRIPT_DIR       = os.path.dirname(os.path.abspath(__file__))
BUILDING_CATCHER = os.path.join(SCRIPT_DIR, "c++", "buildingCatcher", "buildingCatcher.exe")

# ---------------------------------------------------------------------------
# C++ compilation
# ---------------------------------------------------------------------------

# g++ location varies by msys2 environment (ucrt64 / mingw64 / clang64); fall back
# to PATH. The old hardcoded mingw64 path silently skipped compilation when absent,
# which left a stale exe running — so probe real locations instead.
def _find_compiler():
    for c in (r"C:\msys64\ucrt64\bin\g++.exe",
              r"C:\msys64\mingw64\bin\g++.exe",
              r"C:\msys64\clang64\bin\g++.exe"):
        if os.path.isfile(c):
            return c
    return shutil.which("g++") or r"C:\msys64\ucrt64\bin\g++.exe"

COMPILER  = _find_compiler()
CPP_FLAGS = ["-std=c++20", "-O2"]

# (display name, source path, output exe path).
# treeRemover is no longer run here — the pipeline consumes the *_trees.world
# files it produced earlier as input.
CPP_TOOLS = [
    ("buildingCatcher",
        os.path.join(SCRIPT_DIR, "c++", "buildingCatcher", "buildingCatcher.cpp"),
        os.path.join(SCRIPT_DIR, "c++", "buildingCatcher", "buildingCatcher.exe")),
]


# ---------------------------------------------------------------------------
# Tee: write every print() to both console and results file simultaneously
# ---------------------------------------------------------------------------

class Tee:
    def __init__(self, file):
        self._file   = file
        self._stdout = sys.__stdout__
    def write(self, data):
        self._stdout.write(data)
        self._file.write(data)
    def flush(self):
        self._stdout.flush()
        self._file.flush()
    def isatty(self):
        return self._stdout.isatty()


# ---------------------------------------------------------------------------
# Parse / validate
# ---------------------------------------------------------------------------

def parse_test(path):
    """Return (cmd_str, inputs) or raise ValueError."""
    with open(path) as f:
        lines = [l.rstrip('\n') for l in f.readlines()]
    while lines and not lines[-1].strip():
        lines.pop()
    if not lines:
        raise ValueError("empty file")
    if not lines[0].lstrip().startswith('>'):
        raise ValueError(f"line 1 must start with '>' (got {lines[0]!r})")
    cmd_str = lines[0].lstrip()[1:].strip()
    return cmd_str, lines[1:]


def validate_test(inputs):
    """Return list of error strings (empty = OK)."""
    errors = []
    if len(inputs) < 6:
        errors.append(f"too few input lines ({len(inputs)})")
        return errors

    world_path = inputs[0].strip()
    if not os.path.isdir(world_path):
        errors.append(f"world path not found: {world_path!r}")

    try:
        upper = int(inputs[2].strip())
        lower = int(inputs[3].strip())
        if upper <= lower:
            errors.append(f"upper_lim ({upper}) must be > lower_lim ({lower})")
    except ValueError:
        errors.append("upper/lower limits must be integers")

    scan_ans = inputs[4].strip().lower()
    if scan_ans not in ('y', 'n'):
        errors.append(f"scan_all answer must be y or n (got {inputs[4]!r})")
        return errors

    if scan_ans == 'n':
        if len(inputs) < 10:
            errors.append(f"scan limits defined but only {len(inputs)} input lines (need 10)")
            return errors
        for i in range(5, 9):
            try:
                int(inputs[i].strip())
            except ValueError:
                errors.append(f"X/Z limit at line {i+2} must be an integer (got {inputs[i]!r})")
        custom_ans = inputs[9].strip().lower()
    else:
        custom_ans = inputs[5].strip().lower()

    if custom_ans not in ('y', 'n'):
        errors.append(f"custom blocks answer must be y or n (got {custom_ans!r})")

    return errors


def dimension_of(inputs):
    """Map the .test dimension-folder line to overworld / nether / end."""
    d = inputs[1].strip().lower()
    if 'dim-1' in d or 'nether' in d:
        return 'nether'
    if 'dim1' in d or 'the_end' in d or d.rstrip('\\/').endswith('end'):
        return 'end'
    return 'overworld'


def test_meta(cmd_str, inputs):
    """Return a dict with all derived paths and commands for the pipeline."""
    py_prefix  = cmd_str.rsplit('worldExtract.py', 1)[0].strip()
    world_base = os.path.basename(inputs[0].strip())
    scan_ans   = inputs[4].strip().lower()
    custom_idx = 9 if scan_ans == 'n' else 5
    custom     = inputs[custom_idx].strip().lower() == 'y'

    def p(*parts): return os.path.join(SCRIPT_DIR, *parts)

    return {
        "world_base":           world_base,
        "world_file":           p(f"{world_base}.world"),
        "obj_file":             p(f"{world_base}.obj"),
        "blockids_file":        p(f"{world_base}.blockIds"),
        "blockprops_file":      p(f"{world_base}.blockProperties"),
        "trees_world_file":     p(f"{world_base}_trees.world"),
        "trees_blockids_file":  p(f"{world_base}_trees.blockIds"),
        "buildings_world_file": p(f"{world_base}_trees_buildings.world"),
        "buildings_obj_file":   p(f"{world_base}_trees_buildings.obj"),
        "dimension":            dimension_of(inputs),
        "custom":               custom,
        "renderer_cmd":         f"{py_prefix} renderer.py",
    }


# ---------------------------------------------------------------------------
# Execution helpers
# ---------------------------------------------------------------------------

def pipe_run(cmd, stdin_lines, cwd):
    """Run cmd with piped stdin. Return (ok, full output text)."""
    data = "\n".join(stdin_lines) + "\n"
    r = subprocess.run(cmd, input=data, capture_output=True, text=True,
                       cwd=cwd, shell=True)
    out = r.stdout
    if r.stderr.strip():
        out += "\n--- stderr ---\n" + r.stderr
    return r.returncode == 0, out


def open_viewer(path):
    os.startfile(path)


def compile_tools():
    """Compile every C++ tool fresh with g++. Return True if all succeeded."""
    if not os.path.isfile(COMPILER):
        print(f"  WARNING: compiler not found at {COMPILER}")
        print(f"           skipping compilation, using existing exes.\n")
        return True

    all_ok = True
    for name, src, exe in CPP_TOOLS:
        if not os.path.isfile(src):
            print(f"  MISSING  {name}  (source not found: {src})")
            all_ok = False
            continue

        cmd = [COMPILER, *CPP_FLAGS, src, "-o", exe, "-lz"]
        r = subprocess.run(cmd, capture_output=True, text=True, cwd=SCRIPT_DIR)
        if r.returncode == 0:
            print(f"  COMPILED {name}  →  {os.path.relpath(exe, SCRIPT_DIR)}")
        else:
            all_ok = False
            print(f"  FAILED   {name}")
            for ln in (r.stderr or r.stdout).strip().splitlines():
                print(f"           {ln}")
    print()
    return all_ok


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    out_path = os.path.join(SCRIPT_DIR, "test_results.txt")
    with open(out_path, "w", encoding="utf-8") as results_file:
        sys.stdout = Tee(results_file)
        try:
            _run()
        finally:
            sys.stdout = sys.__stdout__
    print(f"\nResults saved → {out_path}")


def _run():
    def _test_sort_key(p):
        name = os.path.basename(p).lower()
        # extreme runs first; everything else sorts alphabetically after it
        return (0 if "extreme" in name else 1, name)
    test_paths = sorted(glob.glob(os.path.join(SCRIPT_DIR, "*.test")), key=_test_sort_key)
    if not test_paths:
        print("No .test files found.")
        return

    print(f"Found {len(test_paths)} test file(s).\n")

    # ---- compile pass ----
    print("=" * 55)
    print("COMPILE")
    print("=" * 55)
    if not compile_tools():
        print("Compilation failed — aborting.")
        return

    if not os.path.isfile(BUILDING_CATCHER):
        print(f"  ERROR: buildingCatcher.exe not found at {BUILDING_CATCHER}\n")
        return

    # ---- validation pass ----
    print("=" * 55)
    print("VALIDATION")
    print("=" * 55)
    valid_tests = []
    any_invalid = False
    for path in test_paths:
        name = os.path.basename(path)
        try:
            cmd_str, inputs = parse_test(path)
            errors = validate_test(inputs)
            if errors:
                any_invalid = True
                print(f"  INVALID  {name}")
                for e in errors:
                    print(f"           → {e}")
            else:
                print(f"  OK       {name}")
                valid_tests.append((name, cmd_str, inputs))
        except ValueError as e:
            any_invalid = True
            print(f"  INVALID  {name}  ({e})")

    if not valid_tests:
        print("\nNo valid tests to run.")
        return

    print()
    if any_invalid:
        ans = input("Some tests are invalid — run the valid ones anyway? (y/n): ").strip().lower()
        if ans != 'y':
            return
        print()

    # ---- run pass ----
    results = {}

    for idx, (name, cmd_str, inputs) in enumerate(valid_tests):
        meta = test_meta(cmd_str, inputs)

        print("=" * 55)
        print(f"[{idx+1}/{len(valid_tests)}]  {name}")
        print("=" * 55)

        # ── step 1: use the treeRemover output that already exists ────
        if not os.path.exists(meta["trees_world_file"]):
            print(f"  ^^^ trees world file not found: {os.path.basename(meta['trees_world_file'])} ^^^")
            results[name] = "ERROR  (trees world missing)"
            continue

        # ── step 2: buildingCatcher ───────────────────────────
        bc_inputs = [
            meta["trees_world_file"],
            meta["trees_blockids_file"],
            meta["dimension"],
        ]
        print("  buildingCatcher.exe ...")
        ok, out = pipe_run(BUILDING_CATCHER, bc_inputs, SCRIPT_DIR)
        print(out)
        if not ok or not os.path.exists(meta["buildings_world_file"]):
            print("  ^^^ buildingCatcher FAILED ^^^")
            results[name] = "ERROR  (buildingCatcher failed)"
            continue

        # ── step 3: renderer on buildings world ───────────────
        renderer_inputs = [meta["buildings_world_file"], "y" if meta["custom"] else "n"]
        print("  renderer.py  (buildings) ...")
        ok, out = pipe_run(meta["renderer_cmd"], renderer_inputs, SCRIPT_DIR)
        print(out)
        if not ok or not os.path.exists(meta["buildings_obj_file"]):
            print("  ^^^ renderer (buildings) FAILED ^^^")
            results[name] = "ERROR  (renderer/buildings failed)"
            continue

        # ── step 4: open buildings OBJ ────────────────────────
        # brief pause so the OBJ + MTL are fully flushed before the viewer
        # opens them — otherwise the viewer sometimes loads geometry without
        # the .mtl colours and the file has to be reopened by hand.
        time.sleep(3)
        print(f"  Opening render: {os.path.basename(meta['buildings_obj_file'])} ...")
        open_viewer(meta["buildings_obj_file"])

        # ── step 6: human verdict ─────────────────────────────
        while True:
            ans = input(f"\n  Pass? (y/n): ").strip().lower()
            if ans in ('y', 'n'):
                break
        results[name] = "PASS" if ans == 'y' else "FAIL"
        print()

    # ---- summary ----
    print("=" * 55)
    print("RESULTS")
    print("=" * 55)
    for name, verdict in results.items():
        print(f"  {name:<38} {verdict}")
    passes = sum(1 for v in results.values() if v == "PASS")
    print(f"\n  {passes}/{len(results)} passed")


if __name__ == "__main__":
    main()
