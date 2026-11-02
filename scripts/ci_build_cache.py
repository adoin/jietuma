"""Verified, cross-tag Qt SDK and incremental-build artifacts for Windows releases."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess
import tarfile
import tempfile
import zipfile

SCHEMA = 1
QT_VERSION = '6.11.0'
MAX_UNPACKED = 12 * 1024**3
ROOT = Path(__file__).resolve().parents[1]


def digest(path: Path) -> str:
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024*1024), b''):
            h.update(block)
    return h.hexdigest()


def key(kind: str, arch: str, toolchain: str = '', root: Path = ROOT) -> str:
    files = ['cpp/scripts/build_static_qt.ps1']
    if kind == 'build':
        files += ['cpp/CMakeLists.txt', 'cpp/scripts/build.ps1', 'scripts/ci_build_cache.py']
    h = hashlib.sha256(f'{SCHEMA}|{kind}|{arch}|{QT_VERSION}|static-crt|cxx20'.encode())
    for name in files:
        h.update(name.encode())
        h.update((root/name).read_bytes().replace(b'\r\n', b'\n'))
    if kind == 'build':
        h.update((toolchain + '|' + os.path.normcase(str(root.resolve()))).encode())
    return f'ci-{kind}-v{SCHEMA}-{arch}-{h.hexdigest()[:24]}'


def safe_relative(name: str) -> PurePosixPath:
    path = PurePosixPath(name)
    if not name or path.is_absolute() or '..' in path.parts or '\\' in name or ':' in name:
        raise ValueError('Unsafe cache path')
    return path


def check_destination(root: Path, destination: Path, kind: str) -> None:
    allowed = (root/'cpp'/('.sdk' if kind == 'qt' else 'build')).resolve()
    actual = destination.resolve()
    if actual == allowed or not actual.is_relative_to(allowed):
        raise ValueError('Cache destination must be inside the designated SDK/build directory')


def tracked_sources(root: Path) -> dict:
    names = subprocess.check_output(['git','ls-files','-z'], cwd=root).decode('utf-8').split('\0')
    result = {}
    for name in names:
        if not name:
            continue
        path = root/Path(*safe_relative(name).parts)
        if path.is_file() and not path.is_symlink():
            result[name] = {'sha256':digest(path), 'mtime_ns':path.stat().st_mtime_ns}
    return result


def restore_source_times(root: Path, sources: dict) -> int:
    count = 0
    for name, info in sources.items():
        path = root/Path(*safe_relative(name).parts)
        if path.is_file() and not path.is_symlink() and digest(path) == info['sha256']:
            os.utime(path, ns=(path.stat().st_atime_ns, int(info['mtime_ns'])))
            count += 1
    return count


def validate_tree(source: Path, kind: str, arch: str) -> None:
    if kind == 'qt':
        required = ['lib/cmake/Qt6/Qt6Config.cmake','lib/Qt6Core.lib','lib/Qt6Svg.lib',
                    f'sbom/qtbase-{QT_VERSION}.spdx',f'sbom/qtsvg-{QT_VERSION}.spdx',
                    f'sbom/qtimageformats-{QT_VERSION}.spdx']
        if arch == 'x64':
            required.append('bin/moc.exe')
    else:
        required = ['CMakeCache.txt']
    if not all((source/name).is_file() for name in required):
        raise ValueError(f'Incomplete {kind} tree')


def pack(source: Path, bundle: Path, kind: str, arch: str, cache_key: str, root: Path = ROOT) -> None:
    validate_tree(source, kind, arch)
    bundle.mkdir(parents=True, exist_ok=True)
    payload = bundle/'payload.tar.gz'
    with tarfile.open(payload, 'w:gz', compresslevel=1, dereference=True) as archive:
        for path in sorted(source.rglob('*')):
            relative = path.relative_to(source)
            if path.is_symlink() or relative.name == '.build.lock':
                continue
            # Root checkout credentials are outside source. Never export Git config/hooks
            # from FetchContent checkouts either; public remotes are re-established below.
            if '.git' in relative.parts and ('hooks' in relative.parts or path.name == 'config'):
                continue
            if path.is_file():
                archive.add(path, arcname='tree/'+relative.as_posix(), recursive=False)
    manifest = {'schema':SCHEMA,'kind':kind,'arch':arch,'key':cache_key,
                'qt_version':QT_VERSION,'workspace':os.path.normcase(str(root.resolve())),
                'payload_sha256':digest(payload)}
    if kind == 'build':
        manifest['sources'] = tracked_sources(root)
        # Only public dependency repository URLs; no cached HTTP authorization headers.
        remotes = {}
        for git_dir in source.rglob('.git'):
            if git_dir.is_dir():
                result = subprocess.run(['git','remote','get-url','origin'],cwd=git_dir.parent,
                                        capture_output=True,text=True,encoding='utf-8')
                url = result.stdout.strip()
                if result.returncode == 0 and re.fullmatch(r'https://github\.com/[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+(?:\.git)?',url):
                    remotes[git_dir.relative_to(source).as_posix()] = url
        manifest['dependency_remotes'] = remotes
    (bundle/'manifest.json').write_text(json.dumps(manifest),encoding='utf-8')
    print(f'Packed {kind}/{arch}: {payload.stat().st_size//1024//1024} MiB')


def unpack(bundle: Path, destination: Path, kind: str, arch: str, cache_key: str, root: Path = ROOT) -> None:
    check_destination(root,destination,kind)
    metadata_path=bundle/'manifest.json'
    if metadata_path.stat().st_size > 8*1024*1024:
        raise ValueError('Oversized cache manifest')
    manifest=json.loads(metadata_path.read_text(encoding='utf-8'))
    expected={'schema':SCHEMA,'kind':kind,'arch':arch,'key':cache_key,'qt_version':QT_VERSION,
              'workspace':os.path.normcase(str(root.resolve()))}
    if any(manifest.get(name)!=value for name,value in expected.items()):
        raise ValueError('Cache compatibility mismatch')
    payload=bundle/'payload.tar.gz'
    if digest(payload)!=manifest['payload_sha256']:
        raise ValueError('Cache payload checksum mismatch')
    if destination.exists():
        raise ValueError('Refusing to replace an existing cache destination')
    destination.parent.mkdir(parents=True,exist_ok=True)
    # Stage beside the destination: no cross-volume move and no overwrite/delete of work.
    with tempfile.TemporaryDirectory(prefix='.restore-',dir=destination.parent) as temporary:
        staging=Path(temporary)
        with tarfile.open(payload,'r:gz') as archive:
            members=archive.getmembers()
            if len(members)>200000 or sum(m.size for m in members)>MAX_UNPACKED:
                raise ValueError('Oversized cache archive')
            for member in members:
                path=safe_relative(member.name)
                if path.parts[0]!='tree' or not (member.isfile() or member.isdir()):
                    raise ValueError('Unsafe cache archive entry')
            archive.extractall(staging,filter='data')
        tree=staging/'tree'
        validate_tree(tree,kind,arch)
        for path,url in manifest.get('dependency_remotes',{}).items():
            git_dir=tree/Path(*safe_relative(path).parts)
            if not git_dir.is_dir() or not re.fullmatch(r'https://github\.com/[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+(?:\.git)?',url):
                raise ValueError('Invalid cached dependency remote')
            (git_dir/'config').write_text('[core]\n repositoryformatversion = 0\n bare = false\n[remote "origin"]\n url = '+url+'\n fetch = +refs/heads/*:refs/remotes/origin/*\n',encoding='utf-8')
        os.replace(tree,destination)
    if kind=='build':
        count=restore_source_times(root,manifest['sources'])
        print(f'Restored timestamps for {count} unchanged tracked files')


def gh_json(endpoint: str, paginate: bool = False):
    command=['gh','api']+(['--paginate','--slurp'] if paginate else [])+[endpoint]
    return json.loads(subprocess.check_output(command,text=True,encoding='utf-8'))


def trusted_run(run: dict, repository_id: int) -> bool:
    if run.get('head_repository',{}).get('id')!=repository_id or run.get('status')!='completed':
        return False
    path=run.get('path','').split('@')[0]
    if path=='.github/workflows/build.yml':
        return run.get('event')=='push' and run.get('head_branch','').startswith('release-')
    return (path=='.github/workflows/sdk-cache-export.yml' and run.get('event')=='workflow_dispatch'
            and run.get('head_branch')=='feat/plugin-ecosystem'
            and run.get('actor',{}).get('id')==run.get('repository',{}).get('owner',{}).get('id'))


def restore_remote(repository: str,destination: Path,kind: str,arch: str,cache_key: str) -> bool:
    repository_id=gh_json(f'repos/{repository}')['id']
    pages=gh_json(f'repos/{repository}/actions/artifacts?per_page=100&name={cache_key}',True)
    artifacts=sorted((a for page in pages for a in page['artifacts']
                      if a['name']==cache_key and not a['expired']),key=lambda a:a['id'],reverse=True)
    for artifact in artifacts[:6]:
        run=gh_json(f"repos/{repository}/actions/runs/{artifact['workflow_run']['id']}")
        if not trusted_run(run,repository_id) or artifact['size_in_bytes']>3*1024**3:
            continue
        try:
            with tempfile.TemporaryDirectory(prefix='ci-cache-',dir=os.environ.get('RUNNER_TEMP') or os.environ.get('TEMP')) as temporary:
                directory=Path(temporary);download=directory/'artifact.zip'
                with download.open('wb') as stream:
                    subprocess.run(['gh','api',f"repos/{repository}/actions/artifacts/{artifact['id']}/zip"],stdout=stream,check=True)
                expected=artifact.get('digest')
                if expected and expected!='sha256:'+digest(download):
                    raise ValueError('Artifact digest mismatch')
                with zipfile.ZipFile(download) as archive:
                    if sorted(archive.namelist())!=['manifest.json','payload.tar.gz']:
                        raise ValueError('Unexpected artifact files')
                    for entry in archive.infolist():
                        if entry.file_size>3*1024**3:
                            raise ValueError('Oversized artifact')
                        archive.extract(entry,directory)
                unpack(directory,destination,kind,arch,cache_key)
            print(f'Reused {kind}/{arch} from run {run["id"]}')
            return True
        except (ValueError,OSError,tarfile.TarError,zipfile.BadZipFile,subprocess.CalledProcessError) as error:
            print(f'Cache candidate skipped: {error}')
    return False


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command',choices=['key','pack','unpack','restore'])
    parser.add_argument('--kind',choices=['qt','build'],required=True)
    parser.add_argument('--arch',choices=['x64','arm64'],required=True)
    parser.add_argument('--toolchain',default='')
    parser.add_argument('--source',type=Path)
    parser.add_argument('--destination',type=Path)
    parser.add_argument('--bundle',type=Path)
    parser.add_argument('--github-output',type=Path)
    parser.add_argument('--repo',default=os.environ.get('GITHUB_REPOSITORY'))
    args=parser.parse_args();cache_key=key(args.kind,args.arch,args.toolchain)
    outputs={'key':cache_key}
    if args.command=='pack':
        pack(args.source,args.bundle,args.kind,args.arch,cache_key)
    elif args.command=='unpack':
        unpack(args.bundle,args.destination,args.kind,args.arch,cache_key)
    elif args.command=='restore':
        if not args.repo or not re.fullmatch(r'[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+',args.repo):parser.error('Invalid repository')
        outputs['hit']=str(restore_remote(args.repo,args.destination,args.kind,args.arch,cache_key)).lower()
    if args.github_output:
        with args.github_output.open('a',encoding='utf-8') as stream:
            for name,value in outputs.items():stream.write(f'{name}={value}\n')
    print(json.dumps(outputs))


if __name__=='__main__': main()
