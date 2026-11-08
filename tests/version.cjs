'use strict';
const assert = require('assert/strict'), fs = require('fs'), path = require('path');
const root = path.resolve(__dirname, '..'), version = require('../package.json').version;
assert(/^\d+\.\d+\.\d+$/.test(version));
assert.equal(require('../app/package.json').version, version);
const read = file => fs.readFileSync(path.join(root, file), 'utf8');
const native = read('native/bridge.rc'), installer = read('installer/Installer.cs');
assert(read('native/NativeVersion.h').includes('HudVersion[] = "' + version + '"'));
assert(read('native/NativeHud.cpp').includes('{"version", HudVersion}'));
assert(read('native/NativeNetwork.cpp').includes('Wide(HudVersion)'));
assert(read('Build.ps1').includes("'package.json'"));
assert(native.includes('FILEVERSION ' + version.replaceAll('.', ',') + ',0'));
assert(native.includes('"ProductVersion", "' + version + '"'));
assert(installer.includes('AssemblyFileVersion("' + version + '.0")'));
assert(installer.includes('AssemblyVersion("' + version + '.0")'));
for (const file of ['installer/Installer.cs', 'installer/ConsoleActions.ps1', 'RELEASING.md']) {
  assert(read(file).includes(version), file + ': release version missing');
  assert(!/(?<![\d.])1\.2(?!\.\d)/.test(read(file)), file + ': old release label remains');
}
assert(read('README.md').includes(version)||read('README.md').includes('/releases/latest'),'README links to current release');
assert(read('CHANGELOG.md').includes('## ' + version));
console.log('PASS package, native module, installer, build filenames and release docs all agree on ' + version);
