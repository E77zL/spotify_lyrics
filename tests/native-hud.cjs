'use strict';
const assert=require('assert/strict'),fs=require('fs'),path=require('path'),crypto=require('crypto'),{spawn}=require('child_process');
const WebSocket=require('ws');
const root=path.resolve(__dirname,'..'),qa=path.join(root,'.build/native-test');
fs.mkdirSync(path.join(qa,'LyricsHUD'),{recursive:true});fs.mkdirSync(path.join(qa,'spotify-lyrics-hud'),{recursive:true});
for(const file of ['version.dll','SpotifyBridgeTest.exe'])fs.copyFileSync(path.join(root,'.build/native-qa',file),path.join(qa,file));
const token=crypto.randomBytes(32).toString('hex'),port=27949;
fs.writeFileSync(path.join(qa,'connection.json'),JSON.stringify({token,port}));
fs.writeFileSync(path.join(qa,'LyricsHUD/bridge.ini'),Buffer.concat([Buffer.from([255,254]),Buffer.from('[LyricsHUD]\r\nEnabled=1\r\nInstallRoot='+qa+'\r\n','utf16le')]));
fs.writeFileSync(path.join(qa,'spotify-lyrics-hud/settings.json'),JSON.stringify({hudEnabled:false,transparency:0,opacityCurve:2}));
const child=spawn(path.join(qa,'SpotifyBridgeTest.exe'),[],{env:{...process.env,HUD_TEST_DIR:qa,APPDATA:qa},stdio:['pipe','pipe','ignore']});
const click=command=>new Promise((resolve,reject)=>{
 const timeout=setTimeout(()=>reject(Error('Fixture command timed out: '+command)),30000);
 child.stdout.once('data',data=>{clearTimeout(timeout);resolve(data.toString().trim());});child.stdin.write(command+'\n');
});
const delay=ms=>new Promise(r=>setTimeout(r,ms));
const health=async()=>{let r=await fetch('http://127.0.0.1:'+port+'/health',{headers:{'X-Hud-Token':token}});assert.equal(r.status,200);return r.json();};
(async()=>{let h;
 try{
  for(let i=0;i<50;i++){try{h=await health();break;}catch{await delay(100);}}
  assert(h,'Native server started');assert.equal(h.pid,child.pid);assert.equal(h.windowPid,child.pid);assert.equal(h.integration,'spotify-native');
  assert.equal(h.version,require('../package.json').version);
  assert.equal(h.glassUpdateCount,1,'Initialize backdrop once without window-region reentrancy');
  assert.equal(await click('frame'),'custom','The alpha edge must not receive a second DWM rounding');
  assert.equal(h.renderSurfaceBytes,0,'Disabled startup should not allocate raster surfaces');
  assert.equal(h.glassHwnd,0,'Disabled startup should not allocate a composition backdrop');
  assert.equal(JSON.parse(fs.readFileSync(path.join(qa,'host.json'))).forwarding,true);
  assert.equal((await fetch('http://127.0.0.1:'+port+'/health')).status,403);
  const rejected=new WebSocket('ws://127.0.0.1:'+port+'/control?token='+token,{origin:'https://example.com'});
  await new Promise((resolve,reject)=>{rejected.on('open',()=>reject(Error('Unexpected foreign origin')));rejected.on('error',resolve);});
  const ws=new WebSocket('ws://127.0.0.1:'+port+'/control?token='+token,{origin:'https://xpui.app.spotify.com'}),messages=[];
  ws.on('message',b=>messages.push(JSON.parse(b)));await new Promise((resolve,reject)=>{ws.on('open',resolve);ws.on('error',reject);});
  const send=j=>ws.send(JSON.stringify(j));
  const receive=async id=>{for(let i=0;i<100;i++){const message=messages.find(m=>m.id===id&&m.type==='reply');if(message)return message;await delay(20);}throw Error('Reply timed out');};
  send({type:'request',id:1,op:'md5',args:'abc'});assert.equal((await receive(1)).result,'900150983cd24fb0d6963f7d28e17f72');
  send({type:'request',id:2,op:'fetch',args:{url:'https://example.com/'}});assert.match((await receive(2)).error,/not allowed/);
  const snapshot={found:true,title:'Wait 4 U',artist:'Westwood',durationSec:180,positionSec:10,isPlaying:true};const key=JSON.stringify([snapshot.title,snapshot.artist,180]);
  send({type:'playback',snapshot});send({type:'setEnabled',enabled:true});await delay(700);
  const searchingFrames=(await health()).frameCount;await delay(500);
  assert((await health()).frameCount-searchingFrames<=2,'Playing without lyrics should not repaint continuously');
  send({type:'setEnabled',enabled:false});
  send({type:'playback',snapshot});
  send({type:'lyrics',key,source:'test',lines:[{time:0,text:'First'},{time:11,text:'Second'}]});
  await delay(600);h=await health();assert.equal(h.lineCount,2);assert(h.positionSec>=10.2&&h.positionSec<11.3);
  send({type:'lyrics',key:'stale',source:'bad',lines:[]});await delay(300);assert.equal((await health()).lineCount,2);
  send({type:'setEnabled',enabled:true});await delay(300);await click('typography');await delay(400);const frameStart=await health();await delay(2000);const frameEnd=await health();
  const fps=(frameEnd.frameCount-frameStart.frameCount)/(frameEnd.frameTimeSec-frameStart.frameTimeSec);
  assert(fps>50&&fps<67,'Animation frame cadence: '+fps.toFixed(1));console.log('PASS measured native animation cadence: '+fps.toFixed(1)+' fps');
  assert.equal(frameEnd.uiSurfaceHeight,32);assert.equal(frameEnd.uiTextPaintCount,frameStart.uiTextPaintCount,'Reuse toolbar text while lyrics animate');
  assert.equal(await click('hitBlank'),'hit','A 0% background must still receive hover and drag in blank areas');
  send({type:'playback',snapshot:{...snapshot,positionSec:42,isPlaying:false}});await delay(500);assert.equal((await health()).positionSec,42);
  send({type:'setEnabled',enabled:true});await delay(300);h=await health();assert(h.visible);assert.equal(h.activeIndex,1);assert.equal(h.painted,true,JSON.stringify(h));assert.equal(h.paintError,0);assert(h.exStyle & 0x80);assert(h.exStyle & 0x08000000);assert.equal(h.exStyle & 0x40000,0);
  assert.equal(h.textRenderer,'DirectWrite grayscale');assert.equal(h.antialias,50);
  await delay(800);const settled=(await health()).frameCount;await delay(600);
  assert((await health()).frameCount-settled<=2,'Paused HUD must not repaint continuously');
  const idleFrames=(await health()).frameCount;
  for(let i=0;i<20;i++)send({type:'playback',snapshot:{...snapshot,positionSec:42,isPlaying:false}});
  await delay(500);assert.equal((await health()).frameCount,idleFrames,'Repeated paused snapshots must not dirty the surface');
  const normal=(await health()).bounds;
  await click('keyLeft');await delay(100);assert.equal(messages.filter(m=>m.type==='playerAction').length,0,'Normal overlay ignores playback shortcuts');
  await click('begin');await delay(300);const dragStart=await health();assert(dragStart.sizingMoving);
  const moveLatency=await click('moveBurst');await delay(300);const moved=await health();
  assert.equal(moved.glassUpdateCount,dragStart.glassUpdateCount,'No compositor reconfiguration per mouse report');
  assert(moved.frameCount-dragStart.frameCount<=2,'Paused movement reuses the existing layered surface');
  await click('end');await delay(300);assert.deepEqual((await health()).bounds,normal);
  await click('resizeBegin');await delay(300);assert((await health()).sizingMoving);
  const resizeLatency=await click('resizeBurst');await delay(300);const resized=await health();
  assert.equal(resized.glassUpdateCount,dragStart.glassUpdateCount,'Defer region/backdrop updates while resizing');
  assert(resized.frameCount-moved.frameCount<512,'Resize bursts coalesce into frame cadence');
  await click('end');await delay(400);h=await health();assert.equal(h.sizingMoving,false);
  assert.equal(h.glassUpdateCount,dragStart.glassUpdateCount,'Keep backdrop enabled throughout capture and release');
  assert.deepEqual(h.bounds,normal);assert.equal(h.paintError,0);
  const savedBounds=JSON.parse(fs.readFileSync(path.join(qa,'spotify-lyrics-hud/window-state.json')));
  assert.deepEqual(savedBounds,normal);
  await click('begin');await click('step');await delay(300);h=await health();
  assert.equal(h.bounds.x,normal.x+80);assert.equal(h.bounds.y,normal.y+40);
  await click('cancel');await delay(300);
  assert.equal((await health()).sizingMoving,false);assert.deepEqual((await health()).bounds,normal);
  await click('resizeBegin');await click('step');await delay(300);h=await health();
  assert.equal(h.bounds.width,normal.width+80);assert.equal(h.bounds.height,normal.height+40);
  await click('maxStep');await delay(300);h=await health();assert.equal(h.bounds.width,2000);assert.equal(h.bounds.height,1000);
  await click('minStep');await delay(300);h=await health();assert.equal(h.bounds.width,420);assert.equal(h.bounds.height,110);
  await click('cancel');await delay(300);assert.deepEqual((await health()).bounds,normal);
  console.log('PASS 512 mouse-message move/resize bursts, cached movement, continuous glass, capture release, Escape and saved bounds:',moveLatency,resizeLatency);
  await click('fullscreen');await delay(400);h=await health();assert(h.fullscreen);
  assert(h.bounds.width>=normal.width&&h.bounds.height>normal.height);
  assert(h.keyboardFocus,'Fullscreen HUD owns keyboard focus: '+JSON.stringify(h));
  for(const key of ['Left','Right','Up','Down','Space','SpaceRepeat'])await click('key'+key);
  await delay(200);assert.deepEqual(messages.filter(m=>m.type==='playerAction'),[
   {type:'playerAction',action:'seek',deltaMs:-5000},{type:'playerAction',action:'seek',deltaMs:5000},
   {type:'playerAction',action:'volume',delta:0.05},{type:'playerAction',action:'volume',delta:-0.05},
   {type:'playerAction',action:'toggle'}],'Fullscreen keys and Space repeat suppression');
  send({type:'lyrics',key,source:'test',lines:Array.from({length:70},(_,i)=>({time:i,text:'Seek line '+i}))});
  send({type:'playback',snapshot:{...snapshot,positionSec:10,isPlaying:false}});await delay(300);
  const beforeSeekLayout=(await health()).lyricLayoutCount;
  async function animatedSeek(position){
   const start=(await health()).scrollIndex;
   send({type:'playback',snapshot:{...snapshot,positionSec:position,isPlaying:false,seek:true}});
   let moving;
   for(let i=0;i<25;i++){
    h=await health();
    if(h.activeIndex===position){moving=h;break;}
    await delay(20);
   }
   assert(moving,'Seek updates the active lyric without waiting for the animation');
   assert(moving.scrollIndex>Math.min(start,position)&&moving.scrollIndex<Math.max(start,position),
    'Seek has intermediate animation frames in both directions, including jumps over six rows: '+JSON.stringify(moving));
   return moving;
  }
  for(const position of [15,10,25]){
   await animatedSeek(position);
   await delay(700);h=await health();assert.equal(h.activeIndex,position);
   assert.equal(h.scrollIndex,position,'Seek animation settles at the requested row');
   assert(h.activeEmphasis>0.999,'Target highlighting settles with the normal playback easing');
   assert.equal(h.lyricLayoutCount,beforeSeekLayout,'Seeking reuses lyric layouts');
  }
  await animatedSeek(5);
  await animatedSeek(30);
  await delay(700);h=await health();assert.equal(h.scrollIndex,30,'Reversing during an animation retargets the current position');
  const seekSettled=h.frameCount;await delay(500);
  assert((await health()).frameCount-seekSettled<=2,'Paused seek animation stops rendering once settled');
  send({type:'playback',snapshot:{...snapshot,positionSec:20,isPlaying:false}});await delay(300);
  h=await health();assert.equal(h.scrollIndex,20,'External timeline jumps also bypass the old transition');
  send({type:'lyrics',key,source:'test',lines:[{time:0,text:'First'},{time:11,text:'Second'}]});
  send({type:'playback',snapshot:{...snapshot,positionSec:42,isPlaying:false}});await delay(300);
  console.log('PASS paused fullscreen seek: immediate target, animated forward/back/large jumps, mid-animation reversal, cached layouts and idle rendering');
  await click('topBegin');await click('end');await delay(200);assert((await health()).fullscreen,'Clicking top edge must not restore');
  await click('typography');await delay(300);h=await health();assert(h.flyoutHwnd);
  assert(h.flyout.y>=h.bounds.y+32&&h.flyout.y+h.flyout.height<=h.bounds.y+h.bounds.height);
  await click('fullscreen');await delay(300);h=await health();assert.equal(h.fullscreen,false);assert.deepEqual(h.bounds,normal);assert.equal(h.flyoutHwnd,0);
  await click('fullscreen');await delay(300);await click('topBegin');await click('step');await delay(300);
  h=await health();assert(!h.fullscreen&&h.sizingMoving,'Top drag restores and starts a normal drag: '+JSON.stringify(h));
  assert.equal(h.bounds.width,normal.width);assert.equal(h.bounds.height,normal.height);
  const restored=h.bounds;await click('step');await delay(300);h=await health();
  assert.equal(h.bounds.x,restored.x+80);assert.equal(h.bounds.y,restored.y+40);
  await click('end');await delay(300);assert(!(await health()).sizingMoving);
  // Put the fixture back at its original bounds for the remaining layout tests.
  await click('layoutRestoreNormal');await delay(300);
  console.log('PASS fullscreen playback shortcuts, repeat suppression, click threshold and restore-follow drag');
  send({type:'lyrics',key,source:'test',lines:[{time:0,text:'Previous lyric'},
   {time:11,text:'A phrase that wraps without clipping or ellipses. '.repeat(12)}]});
  await click('layoutNarrow');await delay(800);const narrow=await health();
  assert(narrow.activeWrappedRows>5,JSON.stringify(narrow));
  assert(narrow.activeTextHeight<=(narrow.bounds.height-32)*0.64-12);
  await click('layoutWide');await delay(400);const wide=await health();
  assert(wide.activeWrappedRows<narrow.activeWrappedRows,'Wide window reflows into fewer rows');
  await click('layoutShort');await delay(400);const short=await health();
  assert(short.activeWrappedRows>=2 && short.activeFontSize<wide.activeFontSize);
  assert(short.activeTextHeight<=(short.bounds.height-32)*0.64-12,'Short viewport fits the whole active phrase');
  await click('layoutNarrow');send({type:'playback',snapshot:{...snapshot,positionSec:42,isPlaying:true}});
  await delay(500);const cachedLayout=(await health()).lyricLayoutCount;await delay(600);
  assert.equal((await health()).lyricLayoutCount,cachedLayout,'Animation must reuse wrapped layouts');
  await click('layoutRestore');await delay(400);assert.deepEqual((await health()).bounds,normal);
  console.log('PASS more than five wrapped rows, width reflow, short viewport fitting and cached multiline animation');
  await click('fullscreen');await delay(300);
  send({type:'setEnabled',enabled:false});await delay(500);assert.equal((await health()).visible,false);
  assert.equal((await health()).fullscreen,false);assert.deepEqual((await health()).bounds,normal);
  assert.equal((await health()).renderSurfaceBytes,0,'Hide releases large raster buffers');
  const layoutBeforeReopen=(await health()).lyricLayoutCount;
  send({type:'setEnabled',enabled:true});await delay(500);h=await health();
  assert(h.visible && h.painted && h.renderSurfaceBytes>0,'Reopen recreates render surfaces');
  assert.equal(h.lyricLayoutCount,layoutBeforeReopen,'Reopen retains cached lyric layouts');
  send({type:'setEnabled',enabled:false});await delay(500);
  console.log('PASS fullscreen monitor bounds, usable flyout, restore and disable restoration');
  ws.close();console.log('PASS native host PID, real version API forwarding, authentication/origin, MD5, allowlist, stale-track rejection, interpolation/pause/seek, fade and hide');
 }finally{child.kill();await new Promise(r=>child.once('exit',r));}
 await assert.rejects(health());console.log('PASS native host termination removes overlay and server without helper cleanup');
})().catch(e=>{console.error(e);process.exitCode=1;});
