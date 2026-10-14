# SPDX-License-Identifier: GPL-2.0-or-later
"""Shared developer-tool boundaries; these tools are never installed by Meson."""
import contextlib
import os
from pathlib import Path
import shutil
import tempfile

ROOT = Path(__file__).resolve().parents[2]


@contextlib.contextmanager
def fresh_output(destination, source=None):
    """Publish complete output once; never overwrite existing developer data."""
    destination = Path(destination).absolute()
    if source is not None:
        root = Path(source).resolve()
        resolved = destination.resolve()
        protected = ('.git', '.github', 'fvwm', 'libs', 'modules', 'bin',
                     'perllib', 'doc', 'po', 'default-config', 'tests')
        for name in protected:
            boundary = root / name
            if resolved == boundary or boundary in resolved.parents:
                raise ValueError(f'output must be outside source components: {destination}')
    if destination.exists() or destination.is_symlink():
        raise ValueError(f"output already exists: {destination}")
    destination.parent.mkdir(parents=True, exist_ok=True)
    stage = Path(tempfile.mkdtemp(prefix='.fvwm-maint-', dir=destination.parent))
    try:
        yield stage
        # Developer workspaces are trusted. Refuse accidental concurrent reuse.
        if destination.exists() or destination.is_symlink():
            raise ValueError(f"output appeared during generation: {destination}")
        os.rename(stage, destination)
    finally:
        if stage.exists():
            shutil.rmtree(stage)


def source_file(root, name):
    root = Path(root).resolve()
    path = root / name
    if path.is_symlink() or root not in path.resolve().parents:
        raise ValueError(f"source escapes root or is a symlink: {name}")
    if not path.is_file():
        raise ValueError(f"missing source: {name}")
    return path
