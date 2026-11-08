'use strict';
const assert=require('assert/strict'),fs=require('fs'),path=require('path'),vm=require('vm'),crypto=require('crypto');
const source=fs.readFileSync(path.resolve(__dirname,'../.build/payload/spotify-lyrics-hud.js'),'utf8');
let socket,button,menu,route;const sent=[];let title='Wait 4 U',artist='Westwood';
let position=1200,volume=0.5,toggles=0;const seeks=[],volumes=[];
class FakeSocket {
 static OPEN=1;
 constructor(){socket=this;this.readyState=1;}
 send(raw){const m=JSON.parse(raw);sent.push(m);if(m.type==='request')queueMicrotask(()=>{
  let result;if(m.op==='md5')result=crypto.createHash('md5').update(m.args).digest('hex');
  else if(m.args.url.includes('lrclib.net'))result={trackName:title,artistName:artist,duration:180,syncedLyrics:'[00:01.00]First line\n[00:02.00]Second line'};
  else result={};this.onmessage?.({data:JSON.stringify({type:'reply',id:m.id,result})});
 });}
 close(){this.readyState=3;this.onclose?.();}
}
const player={data:{item:{uri:'spotify:track:fixture',metadata:{title,artist_name:artist}}},isPlaying:()=>true,getProgress:()=>position,getDuration:()=>180000,getVolume:()=>volume,
 seek:value=>{position=value;seeks.push(value);},setVolume:value=>{volume=value;volumes.push(value);},togglePlay:()=>{toggles++;},addEventListener(){},removeEventListener(){}};
const sandbox={URL,TextDecoder,Uint8Array,atob,AbortController,console:{error(){}},setTimeout,clearTimeout,WebSocket:FakeSocket,document:{body:{},querySelector(){return null;}},Spicetify:{Player:player,Playbar:{Button:class {constructor(label,icon,click){this.click=click;this.element={};button=this;}}}},window:{addEventListener(){}}};
sandbox.Spicetify.Config={custom_apps:['marketplace']};
sandbox.Spicetify.Platform={History:{push:value=>{route=value;}}};
sandbox.Spicetify.Menu={Item:class{constructor(label,enabled,click){this.label=label;this.click=click;menu=this;}register(){this.registered=true;}deregister(){this.registered=false;}}};
sandbox.window.Spicetify=sandbox.Spicetify;vm.runInNewContext(source,sandbox);socket.onopen();
const wait=()=>new Promise(r=>setTimeout(r,30));
(async()=>{await wait();assert(menu.registered);assert.equal(menu.label,'Customization');menu.click();assert.equal(route,'/marketplace');let lyrics=sent.find(m=>m.type==='lyrics');assert.equal(lyrics.key,JSON.stringify([title,artist,180]));assert.equal(lyrics.lines.length,2);assert.equal(lyrics.source,'lrclib');
 button.click();assert.deepEqual(sent.at(-1),{type:'setEnabled',enabled:true});
 const before=sent.filter(m=>m.type==='request').length;socket.onmessage({data:'{"type":"getPlayback"}'});await wait();assert.equal(sent.filter(m=>m.type==='request').length,before);
 title='Another track';player.data.item.metadata.title=title;socket.onmessage({data:'{"type":"getPlayback"}'});await wait();assert.equal(sent.at(-1).key,JSON.stringify([title,artist,180]));
 const action=async values=>{socket.onmessage({data:JSON.stringify({type:'playerAction',...values})});await new Promise(r=>setTimeout(r,70));};
 await action({action:'seek',deltaMs:-5000});assert.equal(seeks.at(-1),0);
 position=179000;await action({action:'seek',deltaMs:5000});assert.equal(seeks.at(-1),180000);
 volume=0.99;await action({action:'volume',delta:0.05});assert.equal(volumes.at(-1),1);
 volume=0.01;await action({action:'volume',delta:-0.05});assert.equal(volumes.at(-1),0);
 await action({action:'toggle'});assert.equal(toggles,1);
 await action({action:'seek',deltaMs:99999});await action({action:'volume',delta:1});await action({action:'unknown'});
 assert.equal(seeks.length,2);assert.equal(volumes.length,2);assert.equal(toggles,1);
 player.seek=()=>Promise.reject(Error('Device disconnected'));await action({action:'seek',deltaMs:-5000});
 console.log('PASS Spotify playback actions, progress/volume endpoints, invalid commands and disconnected devices');
 console.log('PASS browser provider bundle: original strict matching, native RPC, current-track identity, cache, Spotify A toggle and playback bridge');
})().catch(e=>{console.error(e);process.exitCode=1;});
