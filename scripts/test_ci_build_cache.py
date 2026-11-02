import hashlib
import json
import os
from pathlib import Path
import subprocess
import tarfile
import tempfile
import unittest
from ci_build_cache import key, pack, unpack, restore_source_times, tracked_sources, trusted_run


class CacheTests(unittest.TestCase):
    def setUp(self):
        self.temporary=tempfile.TemporaryDirectory(prefix='reuse-test-')
        self.addCleanup(self.temporary.cleanup)
        self.root=Path(self.temporary.name)
        subprocess.run(['git','init','-q'],cwd=self.root,check=True)
        for name in ['cpp/scripts/build_static_qt.ps1','cpp/CMakeLists.txt','cpp/scripts/build.ps1','scripts/ci_build_cache.py']:
            path=self.root/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_text('recipe\n')
        subprocess.run(['git','add','.'],cwd=self.root,check=True)

    def sdk(self):
        source=self.root/'exported-sdk';source.mkdir()
        for name in ['lib/cmake/Qt6/Qt6Config.cmake','lib/Qt6Core.lib','lib/Qt6Svg.lib','bin/moc.exe',
                     'sbom/qtbase-6.11.0.spdx','sbom/qtsvg-6.11.0.spdx','sbom/qtimageformats-6.11.0.spdx']:
            path=source/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(b'test fixture only')
        return source

    def test_qt_round_trip_and_checksum_rejection(self):
        source=self.sdk();bundle=self.root/'bundle';cache_key=key('qt','x64',root=self.root)
        pack(source,bundle,'qt','x64',cache_key,self.root)
        destination=self.root/'cpp/.sdk/restored'
        unpack(bundle,destination,'qt','x64',cache_key,self.root)
        self.assertEqual((destination/'lib/Qt6Core.lib').read_bytes(),b'test fixture only')
        with (bundle/'payload.tar.gz').open('ab') as stream:stream.write(b'tampered')
        with self.assertRaisesRegex(ValueError,'checksum'):
            unpack(bundle,self.root/'cpp/.sdk/another','qt','x64',cache_key,self.root)

    def test_wrong_architecture_and_outside_destination_are_rejected(self):
        source=self.sdk();bundle=self.root/'bundle';pack(source,bundle,'qt','x64','key',self.root)
        with self.assertRaisesRegex(ValueError,'compatibility'):
            unpack(bundle,self.root/'cpp/.sdk/restored','qt','arm64','key',self.root)
        with self.assertRaisesRegex(ValueError,'destination'):
            unpack(bundle,self.root/'outside','qt','x64','key',self.root)

    def test_unchanged_sources_keep_mtime_but_modified_sources_do_not(self):
        path=self.root/'cpp/CMakeLists.txt';old_time=1_700_000_000_000_000_000
        os.utime(path,ns=(old_time,old_time));snapshot=tracked_sources(self.root)
        os.utime(path,None);restore_source_times(self.root,snapshot)
        self.assertEqual(path.stat().st_mtime_ns,old_time)
        path.write_text('changed');new_time=path.stat().st_mtime_ns
        restore_source_times(self.root,snapshot)
        self.assertEqual(path.stat().st_mtime_ns,new_time)

    def test_archive_traversal_is_rejected_before_writing_destination(self):
        bundle=self.root/'bundle';bundle.mkdir();payload=bundle/'payload.tar.gz'
        with tarfile.open(payload,'w:gz') as archive:
            info=tarfile.TarInfo('tree/../../escape');info.size=0;archive.addfile(info)
        metadata={'schema':1,'kind':'build','arch':'x64','key':'key','qt_version':'6.11.0',
                  'workspace':os.path.normcase(str(self.root.resolve())),
                  'payload_sha256':hashlib.sha256(payload.read_bytes()).hexdigest()}
        (bundle/'manifest.json').write_text(json.dumps(metadata))
        destination=self.root/'cpp/build/restore'
        with self.assertRaises(ValueError):unpack(bundle,destination,'build','x64','key',self.root)
        self.assertFalse(destination.exists());self.assertFalse((self.root/'escape').exists())

    def test_sdk_key_is_stable_across_line_endings_and_build_key_includes_toolchain(self):
        first=key('qt','x64',root=self.root)
        (self.root/'cpp/scripts/build_static_qt.ps1').write_bytes(b'recipe\r\n')
        self.assertEqual(first,key('qt','x64',root=self.root))
        self.assertNotEqual(key('build','x64','MSVC-a',self.root),key('build','x64','MSVC-b',self.root))

    def test_only_trusted_tag_or_owner_export_runs_are_accepted(self):
        run={'head_repository':{'id':7},'status':'completed','path':'.github/workflows/build.yml',
             'event':'push','head_branch':'release-2.3.0'}
        self.assertTrue(trusted_run(run,7))
        self.assertFalse(trusted_run(run|{'event':'pull_request'},7))
        self.assertFalse(trusted_run(run,8))
        run.update(path='.github/workflows/sdk-cache-export.yml',event='workflow_dispatch',
                   head_branch='feat/plugin-ecosystem',actor={'id':3},repository={'owner':{'id':3}})
        self.assertTrue(trusted_run(run,7))
        self.assertFalse(trusted_run(run|{'actor':{'id':4}},7))


if __name__=='__main__':unittest.main()
