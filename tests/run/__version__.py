import pathlib
import subprocess

version_script = pathlib.Path(__file__).parents[2] / ".version.sh"
version = (
    subprocess.run([str(version_script)], stdout=subprocess.PIPE, check=True)
    .stdout.decode()
    .rstrip()
)
