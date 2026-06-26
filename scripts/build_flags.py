"""Pre-build script: inject git commit hash as compile-time define."""
import subprocess

Import("env")

try:
    git_hash = subprocess.check_output(
        ["git", "rev-parse", "--short", "HEAD"],
        stderr=subprocess.DEVNULL
    ).decode("ascii").strip()
    dirty = subprocess.check_output(
        ["git", "diff", "--quiet"],
        stderr=subprocess.DEVNULL
    ).returncode != 0
    git_version = git_hash + ("-dirty" if dirty else "")
except Exception:
    git_version = "unknown"

env.Append(BUILD_FLAGS=[f'-D GIT_COMMIT_HASH=\\"{git_version}\\"'])
print(f"[build_flags] GIT_COMMIT_HASH = {git_version}")
