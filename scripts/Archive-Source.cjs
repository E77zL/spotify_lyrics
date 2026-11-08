'use strict';
const fs=require('fs'),path=require('path'),{execFileSync}=require('child_process');
const root=path.resolve(__dirname,'..'),output=path.resolve(process.argv[2]),archiver=path.resolve(process.argv[3]);
const files=['AGENTS.md','.clang-format','Build.ps1','package.json','dependencies.lock.json','.gitignore','LICENSE','NOTICE.txt','README.md','README.en.md','README.zh-CN.md','CREDITS.md','CHANGELOG.md','RELEASING.md'];
function walk(dir){for(const entry of fs.readdirSync(path.join(root,dir),{withFileTypes:true})){
  const relative=dir+'/'+entry.name;
  if(entry.isDirectory()) {if(!['node_modules'].includes(entry.name))walk(relative);}
  else if(!relative.startsWith('tests/')||relative.endsWith('.cjs'))files.push(relative);
}}
for(const dir of ['app','assets','images','extension','installer','native','licenses','scripts','.github'])if(fs.existsSync(path.join(root,dir)))walk(dir);
for(const entry of fs.readdirSync(path.join(root,'tests'))){if(/\.(?:cjs|cpp|ps1)$/.test(entry))files.push('tests/'+entry);}
for(const file of files){
  if(/(?:^|\/)(?:connection\.json|lyric-offsets\.json|settings\.json|window-state\.json)$/.test(file))throw Error('Private state in source archive');
}
const list=path.join(root,'.build/source-files.txt');fs.writeFileSync(list,files.sort().join('\n')+'\n');
if(fs.existsSync(output))fs.unlinkSync(output);
execFileSync(archiver,['a','-tzip','-mx=5','-bso0','-scsUTF-8',output,'@'+list],{cwd:root,stdio:'inherit'});
console.log('Archived '+files.length+' source/license files.');
