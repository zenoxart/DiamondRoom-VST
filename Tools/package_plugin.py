"""Package complete plugin bundles without dropping Unix executable permissions."""

import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import zipfile


def main():
    platform = sys.argv[1]
    names = {
        "Windows": "DiamondRoom-Windows-x64",
        "macOS": "DiamondRoom-macOS-universal",
        "Linux": "DiamondRoom-Linux-x64",
    }
    name = names[platform]
    stage = Path("build/package") / name
    stage.mkdir(parents=True, exist_ok=True)
    dist = Path("dist")
    dist.mkdir(exist_ok=True)
    products = Path("build/DiamondRoom_artefacts/Release")
    bundles = [("VST3", "Diamond Room.vst3")]
    if platform == "macOS":
        bundles.append(("AU", "Diamond Room.component"))
    for kind, bundle in bundles:
        source = products / kind / bundle
        binary = {
            "Windows": "Contents/x86_64-win/Diamond Room.vst3",
            "macOS": "Contents/MacOS/Diamond Room",
            "Linux": "Contents/x86_64-linux/Diamond Room.so",
        }[platform]
        if not (source / binary).is_file():
            raise RuntimeError(f"Incomplete plugin bundle: {source / binary}")
        shutil.copytree(source, stage / bundle, symlinks=True, dirs_exist_ok=True)

    shutil.copy2("LICENSE", stage / "LICENSE.txt")
    shutil.copy2("JUCE/LICENSE.md", stage / "JUCE-LICENSE.md")
    destinations = {
        "Windows": "VST3: C:\\Program Files\\Common Files\\VST3\\",
        "macOS": "VST3: ~/Library/Audio/Plug-Ins/VST3/\nAU: ~/Library/Audio/Plug-Ins/Components/",
        "Linux": "VST3: ~/.vst3/ (x86_64, glibc 2.35 or newer)",
    }
    commit = os.environ.get("GITHUB_SHA", "local build")
    repository = os.environ.get("GITHUB_REPOSITORY", "zenoxart/DiamondRoom-VST")
    (stage / "INSTALL.txt").write_text(
        f"Diamond Room - {platform}\nCommit: {commit}\n"
        f"Source: https://github.com/{repository}/tree/{commit}\n\n"
        "Close your DAW. Extract this archive and copy each COMPLETE plugin folder to:\n"
        f"{destinations[platform]}\n\n"
        "Restart your DAW and rescan plugins. Keep all bundle contents intact.\n"
        "Development build. macOS bundles are ad-hoc signed, not Apple-notarised.\n",
        encoding="utf-8",
    )
    if platform == "Linux":
        archive = dist / f"{name}.tar.gz"
        with tarfile.open(archive, "w:gz") as output:
            for entry in sorted(stage.iterdir()):
                output.add(entry, arcname=entry.name)
    elif platform == "macOS":
        archive = dist / f"{name}.zip"
        subprocess.run(["ditto", "-c", "-k", "--sequesterRsrc", str(stage), str(archive)], check=True)
    else:
        archive = dist / f"{name}.zip"
        with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as output:
            for entry in sorted(stage.rglob("*")):
                if entry.is_file():
                    output.write(entry, entry.relative_to(stage).as_posix())
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    (dist / f"{name}.sha256").write_text(f"{digest}  {archive.name}\n", encoding="utf-8")
    print(f"Packaged {archive} ({digest})")


if __name__ == "__main__":
    main()
