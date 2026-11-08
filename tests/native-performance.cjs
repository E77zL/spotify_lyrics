'use strict';
// Optional, local before/after benchmark. Build both DLLs with -TestMode first.
// node tests/native-performance.cjs <baseline-version.dll> [result.json]
const fs=require('fs'),path=require('path'),crypto=require('crypto'),readline=require('readline');
const {spawn}=require('child_process'),WebSocket=require('ws');
const root=path.resolve(__dirname,'..');
const baseline=process.argv[2];
if(!baseline)throw Error('Supply a baseline DLL built with -TestMode');
const delay=ms=>new Promise(resolve=>setTimeout(resolve,ms));
async function measure(label,dll,fullscreen,port){
 const dir=path.join(root,'.build','performance-'+label+(fullscreen?'-fullscreen':'-normal'));
 fs.mkdirSync(path.join(dir,'LyricsHUD'),{recursive:true});
 fs.mkdirSync(path.join(dir,'spotify-lyrics-hud'),{recursive:true});
 for(const [from,to] of [[dll,'version.dll'],[path.join(root,'.build/native-qa/SpotifyBridgeTest.exe'),'SpotifyBridgeTest.exe']])
  fs.copyFileSync(from,path.join(dir,to));
 const token=crypto.randomBytes(32).toString('hex');
 fs.writeFileSync(path.join(dir,'connection.json'),JSON.stringify({port,token}));
 fs.writeFileSync(path.join(dir,'LyricsHUD/bridge.ini'),Buffer.concat([Buffer.from([255,254]),Buffer.from('[LyricsHUD]\r\nEnabled=1\r\nInstallRoot='+dir+'\r\n','utf16le')]));
 fs.writeFileSync(path.join(dir,'spotify-lyrics-hud/settings.json'),JSON.stringify({hudEnabled:false,fontSize:fullscreen?72:26}));
 fs.writeFileSync(path.join(dir,'spotify-lyrics-hud/window-state.json'),JSON.stringify({x:100,y:40,width:770,height:223}));
 const child=spawn(path.join(dir,'SpotifyBridgeTest.exe'),[],{env:{...process.env,APPDATA:dir,HUD_TEST_DIR:dir},stdio:['pipe','pipe','ignore']});
 const lines=readline.createInterface({input:child.stdout}),pending=[];
 lines.on('line',line=>pending.shift()?.(line));
 const command=text=>new Promise(resolve=>{pending.push(resolve);child.stdin.write(text+'\n');});
 const health=async()=>{const response=await fetch('http://127.0.0.1:'+port+'/health',{headers:{'X-Hud-Token':token}});if(!response.ok)throw Error('Health '+response.status);return response.json();};
 let ws;
 try{
  let ready=false;
  for(let i=0;i<80;i++){try{await health();ready=true;break;}catch{await delay(100);}}
  if(!ready)throw Error('Fixture did not start');
  const startup=JSON.parse(await command('resources'));
  ws=new WebSocket('ws://127.0.0.1:'+port+'/control?token='+token,{origin:'https://xpui.app.spotify.com'});
  await new Promise((resolve,reject)=>{ws.once('open',resolve);ws.once('error',reject);});
  const snapshot={found:true,title:'Performance fixture',artist:'Native HUD',durationSec:180,positionSec:10,isPlaying:true};
  const key=JSON.stringify([snapshot.title,snapshot.artist,180]);
  const send=value=>ws.send(JSON.stringify(value));
  send({type:'playback',snapshot});
  send({type:'lyrics',key,source:'fixture',lines:Array.from({length:40},(_,i)=>({time:i*4,text:'A long lyric line glides smoothly across the rounded glass window '+i}))});
  send({type:'setEnabled',enabled:true});await delay(350);
  if(fullscreen)await command('fullscreen');
  await command('typography');await delay(1500); // Keep toolbar visible, including outside the pointer.
  const start=await health(),before=JSON.parse(await command('resources'));
  const samples=[];
  for(let i=0;i<20;i++){await delay(200);samples.push(await health());}
  const after=JSON.parse(await command('resources')),end=await health();
  const seconds=end.frameTimeSec-start.frameTimeSec;
  send({type:'setEnabled',enabled:false});await delay(800);
  const hidden=JSON.parse(await command('resources')),hiddenHealth=await health();
  const result={version:end.version,scenario:fullscreen?'fullscreen':'normal',bounds:end.bounds,
   cpuMsPerSecond:+((after.cpuMs-before.cpuMs)/seconds).toFixed(2),
   privateMiB:+(after.privateBytes/1048576).toFixed(2),
   disabledStartupPrivateMiB:+(startup.privateBytes/1048576).toFixed(2),
   hiddenPrivateMiB:+(hidden.privateBytes/1048576).toFixed(2),
   hiddenRenderSurfaceBytes:hiddenHealth.renderSurfaceBytes??null,
   fps:+((end.frameCount-start.frameCount)/seconds).toFixed(2),
   meanSampledPaintMs:+(samples.reduce((sum,h)=>sum+h.lastPaintMs,0)/samples.length).toFixed(3),
   toolbarRasters:end.uiTextPaintCount===undefined?null:end.uiTextPaintCount-start.uiTextPaintCount,
   compositionBackdrop:!!end.glassHwnd,glassError:end.glassError??null};
  console.log(label+': '+JSON.stringify(result));return result;
 }finally{
  ws?.terminate();lines.close();child.kill();await new Promise(resolve=>child.once('exit',resolve));
 }
}
(async()=>{
 const results=[];let port=27960;
 for(const fullscreen of [false,true])for(const [label,dll] of [['baseline',path.resolve(baseline)],['current',path.join(root,'.build/native-qa/version.dll')]])
  results.push(await measure(label,dll,fullscreen,port++));
 if(process.argv[3])fs.writeFileSync(process.argv[3],JSON.stringify({method:'Serial isolated native host, 1.5s warmup, 4s animated sample; toolbar visible; whole fixture process CPU/private memory. Excludes Spotify and DWM.',results},null,2)+'\n');
})().catch(error=>{console.error(error);process.exitCode=1;});
