import subprocess
import sys


def git_version():
    version = (
        subprocess.check_output(
            ["git", "describe", "--tags", "--abbrev=0"], stderr=subprocess.STDOUT
        )
        .strip()
        .decode("utf-8")
    )
    return version


def pyproject_version():
    try:
        import tomllib

        with open("pyproject.toml", "rb") as f:
            pyproject_data = tomllib.load(f)
        version = pyproject_data["project"]["version"]
        return version
    except ImportError:
        # primitive parsing for Python < 3.11
        with open("pyproject.toml", "r", encoding="utf-8") as f:
            for line in f:
                if line.strip().startswith("version ="):
                    version = line.split("=", 1)[1].strip().strip('"').strip("'")
                    return version
    raise RuntimeError("Could not determine version from pyproject.toml")


if __name__ == "__main__":
    assert (
        git_version() == "v" + pyproject_version()
    ), f"Version mismatch: git version '{git_version()}' != pyproject.toml version '{pyproject_version()}'"
    print("Version check passed:", git_version())
