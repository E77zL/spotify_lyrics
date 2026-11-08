'use strict';
const path=require('path'),{spawnSync}=require('child_process');
const root=path.resolve(__dirname,'..');
for(const test of ['FullscreenTest.exe','AppearanceTest.exe','CornersTest.exe']){const r=spawnSync(path.join(root,'.build/native-qa',test),[],{cwd:root,stdio:'inherit'});if(r.status!==0)process.exit(r.status||1);}
for(const test of ['version','cache','matching','providers','native-renderer','seek-sync','native-hud']){const r=spawnSync(process.execPath,[path.join(root,'tests',test+'.cjs')],{cwd:root,stdio:'inherit',env:{...process.env,NODE_PATH:path.join(root,'.build/node_modules')}});if(r.status!==0)process.exit(r.status||1);}
