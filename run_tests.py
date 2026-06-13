import os
import sys
import glob
import subprocess

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))


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
        errors.append(f"upper/lower limits must be integers")

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


def test_meta(cmd_str, inputs):
    """Return (py_prefix, world_base, world_file, obj_file, custom, renderer_cmd)."""
    # derive python prefix: "py -3.13 worldExtract.py" → "py -3.13"
    py_prefix = cmd_str.rsplit('worldExtract.py', 1)[0].strip()

    world_base = os.path.basename(inputs[0].strip())
    world_file = os.path.join(SCRIPT_DIR, f"{world_base}.world")
    obj_file   = os.path.join(SCRIPT_DIR, f"{world_base}.obj")

    scan_ans   = inputs[4].strip().lower()
    custom_idx = 9 if scan_ans == 'n' else 5
    custom     = inputs[custom_idx].strip().lower() == 'y'

    renderer_cmd = f"{py_prefix} renderer.py"
    return py_prefix, world_base, world_file, obj_file, custom, renderer_cmd


# ---------------------------------------------------------------------------
# Execution helpers
# ---------------------------------------------------------------------------

def pipe_run(cmd, stdin_lines, cwd):
    """Run cmd (shell string) with piped stdin. Return (ok, stdout+stderr text)."""
    data = "\n".join(stdin_lines) + "\n"
    r = subprocess.run(cmd, input=data, capture_output=True, text=True,
                       cwd=cwd, shell=True)
    combined = r.stdout + ("\n--- stderr ---\n" + r.stderr if r.stderr.strip() else "")
    return r.returncode == 0, combined


def open_viewer(path):
    os.startfile(path)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    test_paths = sorted(glob.glob(os.path.join(SCRIPT_DIR, "*.test")))
    if not test_paths:
        print("No .test files found.")
        return

    print(f"Found {len(test_paths)} test file(s).\n")

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
        _, world_base, world_file, obj_file, custom, renderer_cmd = \
            test_meta(cmd_str, inputs)

        print("=" * 55)
        print(f"[{idx+1}/{len(valid_tests)}]  {name}")
        print("=" * 55)

        # step 1: worldExtract
        print("  worldExtract.py ...", end="", flush=True)
        ok, out = pipe_run(cmd_str, inputs, SCRIPT_DIR)
        if not ok or not os.path.exists(world_file):
            print(" FAILED")
            print(out[-600:])
            results[name] = "ERROR  (worldExtract failed)"
            continue
        print(" done")

        # step 2: renderer
        renderer_inputs = [world_file, "y" if custom else "n"]
        print("  renderer.py ...   ", end="", flush=True)
        ok, out = pipe_run(renderer_cmd, renderer_inputs, SCRIPT_DIR)
        if not ok or not os.path.exists(obj_file):
            print(" FAILED")
            print(out[-600:])
            results[name] = "ERROR  (renderer failed)"
            continue
        print(" done")

        # step 3: open viewer
        print(f"  Opening {os.path.basename(obj_file)} ...")
        open_viewer(obj_file)

        # step 4: human verdict
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

    out_path = os.path.join(SCRIPT_DIR, "test_results.txt")
    with open(out_path, "w") as f:
        for name, verdict in results.items():
            f.write(f"{name:<38} {verdict}\n")
        f.write(f"\n{passes}/{len(results)} passed\n")
    print(f"\n  Results saved → {out_path}")


if __name__ == "__main__":
    main()
