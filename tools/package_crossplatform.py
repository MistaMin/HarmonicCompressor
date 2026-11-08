#!/usr/bin/env python3
"""Package actual Windows/Linux Release artifacts; fail if a promised format is absent."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import tarfile
import zipfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--platform', choices=['windows', 'linux'], required=True)
    parser.add_argument('--artefacts', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    version = (root / 'VERSION').read_text().strip()
    name = f'HarmonicCompressor-{version}-{args.platform}-x86_64'
    stage = args.out / name
    stage.mkdir(parents=True, exist_ok=False)
    installs = []
    for fmt, suffix in [('VST3', '.vst3'), ('CLAP', '.clap'), ('LV2', '.lv2'),
                        ('Standalone', '.exe' if args.platform == 'windows' else '')]:
        source = args.artefacts / fmt / ('Harmonic Compressor' + suffix)
        if not source.exists():
            raise SystemExit(f'Missing required {fmt}: {source}')
        if args.platform == 'windows':
            # The manager copies paths relative to Program Files. Keep the ZIP
            # root identical to those destinations rather than a portable wrapper.
            folder = {'VST3': 'Common Files/VST3', 'CLAP': 'Common Files/CLAP',
                      'LV2': 'Common Files/LV2', 'Standalone': 'OpenGrid/Harmonic Compressor'}[fmt]
        else:
            folder = 'plugins' if fmt != 'Standalone' else 'standalone'
        destination = stage / folder / source.name
        destination.parent.mkdir(parents=True, exist_ok=True)
        if source.is_dir():
            shutil.copytree(source, destination, symlinks=True)
        else:
            shutil.copy2(source, destination)
        archive_root = stage if args.platform == 'windows' else args.out
        installs.append({'format': fmt.lower(), 'path': destination.relative_to(archive_root).as_posix()})
    docs = stage / 'OpenGrid/Harmonic Compressor/Documentation' if args.platform == 'windows' else stage
    docs.mkdir(parents=True, exist_ok=True)
    for source in [root / 'output/pdf/HarmonicCompressor-User-Manual.pdf',
                   root / 'LICENSE', root / 'LICENSE-BINARIES.txt', root / 'RELEASE_NOTES.md',
                   root / 'THIRD_PARTY_LICENSES.md']:
        shutil.copy2(source, docs / source.name)
    shutil.copytree(root / 'Licenses', docs / 'Licenses')
    if args.platform == 'windows':
        archive = args.out / (name + '.zip')
        with zipfile.ZipFile(archive, 'w', compression=zipfile.ZIP_DEFLATED) as output:
            for path in sorted(stage.rglob('*')):
                output.write(path, path.relative_to(stage))
    else:
        archive = args.out / (name + '.tar.gz')
        with tarfile.open(archive, 'w:gz') as output:
            output.add(stage, arcname=name)
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    Path(str(archive) + '.sha256').write_text(f'{digest}  {archive.name}\n')
    metadata = {'os': args.platform, 'arch': 'x86_64',
                'min_os': '10.0' if args.platform == 'windows' else '24.04',
                'build_class': 'modern', 'installs': installs}
    if args.platform == 'linux':
        metadata['distribution'] = 'Ubuntu 24.04 / glibc 2.39'
    Path(str(archive) + '.installs.json').write_text(json.dumps(metadata, indent=2) + '\n')
    print(archive)


if __name__ == '__main__':
    main()
