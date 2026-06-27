#!/usr/bin/env python3

import os
import queue
import re
import shutil
import subprocess
import sys
import tempfile
import threading
import time
from collections import deque
from pathlib import Path


TEST_COMPLETE = "TEST_COMPLETE"
TEST_TIMEOUT_SECONDS = 30

ROOT = Path(__file__).resolve().parent.parent
ROM = ROOT / "build/u/ge007.u.z64"
TEST_CASES_HEADER = ROOT / "src/practice/practice_tests.h"
DOCKER_IMAGE = os.environ.get("GOLDENEYE_DOCKER_IMAGE", "goldeneye")


def read_test_cases():
    start_marker = "// --- start test cases ---"
    end_marker = "// --- end test cases ---"
    define_pattern = re.compile(r"^\s*#define\s+([A-Za-z_][A-Za-z0-9_]*)\b")
    test_cases = []
    inside_test_cases = False
    found_start = False
    found_end = False

    for line in TEST_CASES_HEADER.read_text().splitlines():
        if line.strip() == start_marker:
            found_start = True
            inside_test_cases = True
            continue
        if line.strip() == end_marker:
            found_end = True
            inside_test_cases = False
            break
        if inside_test_cases:
            match = define_pattern.match(line)
            if match:
                test_cases.append(match.group(1))

    if not found_start or not found_end:
        raise ValueError(
            f"test case markers are missing from {TEST_CASES_HEADER}"
        )
    if not test_cases:
        raise ValueError(f"no test case defines found in {TEST_CASES_HEADER}")
    if len(test_cases) != len(set(test_cases)):
        raise ValueError(f"duplicate test case defines found in {TEST_CASES_HEADER}")

    return test_cases


def emulator_command():
    configured_emulator = os.environ.get("ARES")
    if configured_emulator:
        return [configured_emulator, str(ROM)]

    emulator = shutil.which("ares")
    if emulator:
        return [emulator, str(ROM)]

    macos_emulator = Path("/Applications/ares.app/Contents/MacOS/ares")
    if macos_emulator.is_file():
        return [str(macos_emulator), str(ROM)]

    return None


def build_test(test_case):
    jobs = os.cpu_count() or 1
    command = [
        "docker",
        "run",
        "--rm",
        "-v",
        f"{ROOT}:/home/dev",
        DOCKER_IMAGE,
        "make",
        f"-j{jobs}",
        "DEV=1",
        f"TEST_CASE={test_case}",
    ]
    with tempfile.TemporaryFile(mode="w+") as build_log:
        try:
            result = subprocess.run(
                command,
                cwd=ROOT,
                stdout=build_log,
                stderr=subprocess.STDOUT,
            )
        except OSError as error:
            print(f"Could not start build: {error}", file=sys.stderr)
            return False
        if result.returncode == 0:
            return True

        build_log.seek(0)
        print("Build failed:", file=sys.stderr)
        for line in deque(build_log):
            print(line, end="", file=sys.stderr)
        return False


def stream_output(process, output_queue):
    assert process.stdout is not None
    for line in process.stdout:
        output_queue.put(line)
    output_queue.put(None)


def stop_emulator(process):
    if process.poll() is not None:
        return

    process.terminate()
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait()


def run_test(command):
    process = subprocess.Popen(
        command,
        cwd=ROOT,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,
    )
    output_queue = queue.Queue()
    output_thread = threading.Thread(
        target=stream_output, args=(process, output_queue), daemon=True
    )
    output_thread.start()
    deadline = time.monotonic() + TEST_TIMEOUT_SECONDS

    try:
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                return False, f"timed out after {TEST_TIMEOUT_SECONDS}s"

            try:
                line = output_queue.get(timeout=remaining)
            except queue.Empty:
                return False, f"timed out after {TEST_TIMEOUT_SECONDS}s"

            if line is None:
                return False, f"emulator exited with status {process.wait()}"

            print(line, end="", flush=True)
            if TEST_COMPLETE in line:
                return True, "completed"
    finally:
        stop_emulator(process)
        output_thread.join(timeout=1)


def print_summary(results):
    print("\nTest summary")
    print("------------")
    for test_case, passed, detail in results:
        status = "PASS" if passed else "FAIL"
        print(f"{status:4}  {test_case}: {detail}")

    passed_count = sum(passed for _, passed, _ in results)
    print(f"\n{passed_count} passed, {len(results) - passed_count} failed")


def main():
    try:
        test_cases = read_test_cases()
    except (OSError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1

    command = emulator_command()
    if command is None:
        print(
            "error: ares was not found; install it, add it to PATH, or set ARES",
            file=sys.stderr,
        )
        return 1

    results = []
    try:
        for test_case in test_cases:
            print(f"\n=== {test_case}: building ===", flush=True)
            if not build_test(test_case):
                results.append((test_case, False, "build failed"))
                continue

            print(f"=== {test_case}: running ===", flush=True)
            try:
                passed, detail = run_test(command)
            except OSError as error:
                passed, detail = False, f"could not start emulator: {error}"
            results.append((test_case, passed, detail))
    except KeyboardInterrupt:
        print("\nTest run interrupted.", file=sys.stderr)
        return 130

    print_summary(results)
    return 0 if all(passed for _, passed, _ in results) else 1


if __name__ == "__main__":
    sys.exit(main())
