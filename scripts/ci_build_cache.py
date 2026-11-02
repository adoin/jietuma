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


def sdk_msvc_version(source: Path) -> tuple[int,...]:
    text=(source/'mkspecs/qconfig.pri').read_text(encoding='utf-8')
    result=[]
    for part in ['MAJOR','MINOR','PATCH']:
        match=re.search(rf'^QT_MSVC_{part}_VERSION\s*=\s*(\d+)',text,re.M)
        if not match:raise ValueError('Missing SDK compiler version')
        result.append(int(match[1]))
    return tuple(result)


def installed_msvc_version() -> tuple[int,...]:
    import ctypes as c
    program_files=os.environ.get('ProgramFiles(x86)','C:/Program Files (x86)')
    vswhere=Path(program_files)/'Microsoft Visual Studio/Installer/vswhere.exe'
    install=Path(subprocess.check_output([str(vswhere),'-latest','-products','*','-property','installationPath'],text=True,encoding='utf-8').strip())
    toolset=(install/'VC/Auxiliary/Build/Microsoft.VCToolsVersion.default.txt').read_text().strip()
    compiler=install/'VC/Tools/MSVC'/toolset/'bin/Hostx64/x64/cl.exe'
    api=c.WinDLL('version')
    api.GetFileVersionInfoSizeW.argtypes=[c.c_wchar_p,c.c_void_p]
    api.GetFileVersionInfoW.argtypes=[c.c_wchar_p,c.c_uint,c.c_uint,c.c_void_p]
    api.VerQueryValueW.argtypes=[c.c_void_p,c.c_wchar_p,c.POINTER(c.c_void_p),c.POINTER(c.c_uint)]
    length=api.GetFileVersionInfoSizeW(str(compiler),None)
    if not length:raise ValueError('Cannot identify installed MSVC')
    data=c.create_string_buffer(length)
    if not api.GetFileVersionInfoW(str(compiler),0,length,data):raise ValueError('Cannot read MSVC version')
    pointer=c.c_void_p();size=c.c_uint()
    if not api.VerQueryValueW(data,'\\',c.byref(pointer),c.byref(size)):raise ValueError('Cannot parse MSVC version')
    fields=c.cast(pointer,c.POINTER(c.c_uint32))
    return fields[2]>>16,fields[2]&0xffff,fields[3]>>16


def digest(path: Path) -> str:
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024*1024), b''):
            h.update(block)
    return h.hexdigest()


def key(kind: str, arch: str, toolchain: str = '', root: Path = ROOT) -> str:
    files = ['cpp/scripts/build_static_qt.ps1'] if kind!='rust' else ['rust_libs/Cargo.toml','.github/workflows/rust.yml']
    if kind == 'build':
        files += ['cpp/CMakeLists.txt', 'cpp/scripts/build.ps1', 'scripts/ci_build_cache.py']
    elif kind == 'rust':
        files += ['scripts/ci_build_cache.py']
    profile='rust-tests|lto=false|codegen16' if kind=='rust' else 'static-crt|cxx20'
    h = hashlib.sha256(f'{SCHEMA}|{kind}|{arch}|{QT_VERSION}|{profile}'.encode())
    for name in files:
        h.update(name.encode())
        h.update((root/name).read_bytes().replace(b'\r\n', b'\n'))
    if kind in {'build','rust'}:
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
        required = ['lib/cmake/Qt6/Qt6Config.cmake','lib/Qt6Core.lib','lib/Qt6Svg.lib','mkspecs/qconfig.pri',
                    f'sbom/qtbase-{QT_VERSION}.spdx',f'sbom/qtsvg-{QT_VERSION}.spdx',
                    f'sbom/qtimageformats-{QT_VERSION}.spdx']
        if arch == 'x64':
            required.append('bin/moc.exe')
    elif kind=='build':
        required = ['CMakeCache.txt']
    else:
        required = ['target/.rustc_info.json']
    if not all((source/name).is_file() for name in required):
        raise ValueError(f'Incomplete {kind} tree')


def pack(source: Path, bundle: Path, kind: str, arch: str, cache_key: str, root: Path = ROOT) -> None:
    validate_tree(source, kind, arch)
    bundle.mkdir(parents=True, exist_ok=True)
    payload = bundle/'payload.tar.gz'
    with tarfile.open(payload, 'w:gz', compresslevel=1, dereference=True) as archive:
        for path in sorted(source.rglob('*')):
            relative = path.relative_to(source)
            if path.is_symlink() or relative.name in {'.build.lock','credentials','credentials.toml'}:
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
    if kind in {'build','rust'}:
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


def unpack(bundle: Path, destination: Path, kind: str, arch: str, cache_key: str, root: Path = ROOT, require_msvc: bool = False) -> None:
    check_destination(root,destination,kind)
    metadata_path=bundle/'manifest.json'
    if metadata_path.stat().st_size > 8*1024*1024:
        raise ValueError('Oversized cache manifest')
    manifest=json.loads(metadata_path.read_text(encoding='utf-8'))
    if not isinstance(manifest,dict):
        raise ValueError('Invalid cache manifest')
    expected={'schema':SCHEMA,'kind':kind,'arch':arch,'key':cache_key,'qt_version':QT_VERSION}
    if kind in {'build','rust'}:
        expected['workspace']=os.path.normcase(str(root.resolve()))
    if any(manifest.get(name)!=value for name,value in expected.items()):
        raise ValueError('Cache compatibility mismatch')
    if kind in {'build','rust'}:
        sources=manifest.get('sources')
        if not isinstance(sources,dict):raise ValueError('Invalid cache source table')
        for name,info in sources.items():
            safe_relative(name)
            if (not isinstance(info,dict) or not re.fullmatch(r'[a-f0-9]{64}',str(info.get('sha256','')))
                    or not isinstance(info.get('mtime_ns'),int) or not 0<=info['mtime_ns']<2**63):
                raise ValueError('Invalid cache source fingerprint')
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
        if kind=='qt' and require_msvc and installed_msvc_version()<sdk_msvc_version(tree):
            raise ValueError('Cached static Qt requires a newer MSVC toolset')
        if kind=='qt':
            # Qt's CMake exports are relative; host tools also need a relative prefix.
            (tree/'bin').mkdir(exist_ok=True)
            (tree/'bin/qt.conf').write_text('[Paths]\nPrefix=..\n',encoding='utf-8')
        for path,url in manifest.get('dependency_remotes',{}).items():
            git_dir=tree/Path(*safe_relative(path).parts)
            if git_dir.name!='.git' or not git_dir.is_dir() or not re.fullmatch(r'https://github\.com/[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+(?:\.git)?',url):
                raise ValueError('Invalid cached dependency remote')
            (git_dir/'config').write_text('[core]\n repositoryformatversion = 0\n bare = false\n[remote "origin"]\n url = '+url+'\n fetch = +refs/heads/*:refs/remotes/origin/*\n',encoding='utf-8')
        os.replace(tree,destination)
    if kind in {'build','rust'}:
        count=restore_source_times(root,manifest['sources'])
        print(f'Restored timestamps for {count} unchanged tracked files')


def gh_json(endpoint: str, paginate: bool = False):
    command=['gh','api']+(['--paginate','--slurp'] if paginate else [])+[endpoint]
    return json.loads(subprocess.check_output(command,text=True,encoding='utf-8'))


def trusted_run(run: dict, repository_id: int) -> bool:
    if (run.get('head_repository',{}).get('id')!=repository_id
            or run.get('status')!='completed' or run.get('conclusion')!='success'):
        return False
    path=run.get('path','').split('@')[0]
    if path=='.github/workflows/build.yml':
        return run.get('event')=='push' and run.get('head_branch','').startswith('release-')
    return (path=='.github/workflows/sdk-cache-export.yml' and run.get('event')=='workflow_dispatch'
            and run.get('head_branch')=='feat/plugin-ecosystem'
            and run.get('actor',{}).get('id')==run.get('repository',{}).get('owner',{}).get('id'))


def restore_remote(repository: str,destination: Path,kind: str,arch: str,cache_key: str,require_msvc: bool = False) -> bool:
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
                unpack(directory,destination,kind,arch,cache_key,require_msvc=require_msvc)
            print(f'Reused {kind}/{arch} from run {run["id"]}')
            return True
        except (ValueError,OSError,tarfile.TarError,zipfile.BadZipFile,subprocess.CalledProcessError) as error:
            print(f'Cache candidate skipped: {error}')
    return False


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command',choices=['key','pack','unpack','restore'])
    parser.add_argument('--kind',choices=['qt','build','rust'],required=True)
    parser.add_argument('--arch',choices=['x64','arm64'],required=True)
    parser.add_argument('--toolchain',default='')
    parser.add_argument('--source',type=Path)
    parser.add_argument('--destination',type=Path)
    parser.add_argument('--bundle',type=Path)
    parser.add_argument('--github-output',type=Path)
    parser.add_argument('--repo',default=os.environ.get('GITHUB_REPOSITORY'))
    parser.add_argument('--require-msvc',action='store_true',help='Only restore static Qt usable by the installed compiler')
    args=parser.parse_args();cache_key=key(args.kind,args.arch,args.toolchain)
    outputs={'key':cache_key}
    if args.command=='pack':
        pack(args.source,args.bundle,args.kind,args.arch,cache_key)
    elif args.command=='unpack':
        unpack(args.bundle,args.destination,args.kind,args.arch,cache_key,require_msvc=args.require_msvc)
    elif args.command=='restore':
        if not args.repo or not re.fullmatch(r'[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+',args.repo):parser.error('Invalid repository')
        try:
            hit=restore_remote(args.repo,args.destination,args.kind,args.arch,cache_key,args.require_msvc)
        except subprocess.CalledProcessError as error:
            print(f'Cache service unavailable, falling back to a source build: {error}')
            hit=False
        outputs['hit']=str(hit).lower()
    if args.github_output:
        with args.github_output.open('a',encoding='utf-8') as stream:
            for name,value in outputs.items():stream.write(f'{name}={value}\n')
    print(json.dumps(outputs))


if __name__=='__main__': main()
