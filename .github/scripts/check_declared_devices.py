#!/usr/bin/env python3
"""
Fail if the devices ledger_app.toml declares and the devices CI builds disagree.

Why this exists
---------------
ledger_app.toml declared six devices while both matrices in ci-workflow.yml
listed five. nanos was taken out of CI on purpose ("Remove nanos from tests",
10/2025) and left in the manifest, so the repository went on advertising a
device that no pipeline compiled, that no test exercised, and that the current
Speculos cannot even emulate. Two sources of truth drifted apart and nothing
said so.

This is the thing that says so. It is not a build check -- the build already
passes -- it is a check that the manifest still describes what is actually
produced.

Rules
-----
- The declared devices and the build matrix must be the same set. A declared
  device that is never built is a claim the project cannot support; a device
  built but not declared is a stale matrix entry.
- The functional-test matrix must not name a device that is not declared.
- A declared device with no functional test is *reported*, not failed:
  emulator coverage legitimately varies, and forcing them equal would push the
  project to drop a device rather than to state the gap.

Run locally:
    python3 .github/scripts/check_declared_devices.py
"""

import json
import pathlib
import sys
import tomllib

import yaml

ROOT = pathlib.Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "ledger_app.toml"
WORKFLOW = ROOT / ".github" / "workflows" / "ci-workflow.yml"

BUILD_JOB = "ledger_app_build"
TEST_JOB = "ledger_app_test_function"


def normalise(device):
    """The manifest writes "nanos+"; the Ledger workflows write "nanosp"."""
    return device.replace("+", "p")


def declared_devices():
    with MANIFEST.open("rb") as handle:
        manifest = tomllib.load(handle)
    try:
        devices = manifest["app"]["devices"]
    except KeyError:
        sys.exit(f"{MANIFEST}: no [app] devices key")
    return {normalise(device) for device in devices}


def matrix_devices(workflow, job):
    try:
        raw = workflow["jobs"][job]["with"]["run_for_devices"]
    except (KeyError, TypeError):
        sys.exit(f"{WORKFLOW}: job '{job}' has no 'with.run_for_devices'")
    return {normalise(device) for device in json.loads(raw)}


def main():
    workflow = yaml.safe_load(WORKFLOW.read_text())

    declared = declared_devices()
    built = matrix_devices(workflow, BUILD_JOB)
    tested = matrix_devices(workflow, TEST_JOB)

    problems = []
    for device in sorted(declared - built):
        problems.append(f"declared in ledger_app.toml but never built by CI: {device}")
    for device in sorted(built - declared):
        problems.append(f"built by CI but not declared in ledger_app.toml: {device}")
    for device in sorted(tested - declared):
        problems.append(f"functionally tested but not declared: {device}")

    untested = sorted(declared - tested)
    if untested:
        print("note: declared and built, with no functional test: " + ", ".join(untested))

    if problems:
        print()
        print("The declared devices and the CI matrices disagree:")
        for problem in problems:
            print(f"  - {problem}")
        print()
        print(f"  ledger_app.toml : {', '.join(sorted(declared))}")
        print(f"  build matrix    : {', '.join(sorted(built))}")
        print(f"  test matrix     : {', '.join(sorted(tested))}")
        return 1

    print(f"OK: {len(declared)} declared devices, all built: {', '.join(sorted(declared))}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
