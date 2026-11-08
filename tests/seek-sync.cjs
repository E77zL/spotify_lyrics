'use strict';
// Reproduce the real Spicetify contract: seek() returns before getProgress() changes.
const assert=require('assert/strict'),fs=require('fs'),path=require('path'),vm=require('vm');
const source=fs.readFileSync(path.join(__dirname,'../extension/spotify-lyrics-hud.js'),'utf8');
let now=10000,position=10000,playing=false,socket,unload,clockId=0;
const timers=new Map(),sent=[],seeks=[],listeners=new Map();
const player={data:{item:{uri:'spotify:track:first',metadata:{title:'First',artist_name:'Artist'}}},
 getProgress:()=>position,getDuration:()=>180000,isPlaying:()=>playing,
 seek:value=>{seeks.push(value);},addEventListener:(key,fn)=>listeners.set(key,fn),removeEventListener(){}};
class FakeSocket{
 static OPEN=1;
 constructor(){socket=this;this.readyState=1;}
 send(data){sent.push(JSON.parse(data));}
 close(){this.readyState=3;this.onclose?.();}
}
const sandbox={Date:{now:()=>now},Number,Math,WebSocket:FakeSocket,document:{},
 setTimeout:(fn,ms)=>{timers.set(++clockId,{fn,at:now+ms});return clockId;},clearTimeout:id=>timers.delete(id),
 Spicetify:{Player:player,Playbar:{Button:class{}}},window:{addEventListener:(event,fn)=>{if(event==='beforeunload')unload=fn;}}};
sandbox.window.Spicetify=sandbox.Spicetify;vm.runInNewContext(source,sandbox);socket.onopen();
const playback=()=>sent.filter(m=>m.type==='playback').at(-1).snapshot;
const command=deltaMs=>socket.onmessage({data:JSON.stringify({type:'playerAction',action:'seek',deltaMs})});
const poll=ms=>{now+=ms;for(const[id,timer]of [...timers])if(timer.at<=now){timers.delete(id);timer.fn();}};
const sample=()=>socket.onmessage({data:'{"type":"getPlayback"}'});
(async()=>{
 command(5000);assert.equal(seeks.at(-1),15000);assert.equal(playback().positionSec,15);assert(playback().seek);
 const count=sent.length;poll(50);assert.equal(sent.length,count,'Waiting for acknowledgement must not stream extra snapshots');
 sample();assert.equal(playback().positionSec,15,'Periodic playback cannot restore the stale clock');
 command(5000);assert.equal(seeks.at(-1),20000);assert.equal(playback().positionSec,20,'Rapid presses accumulate');
 command(-5000);assert.equal(seeks.at(-1),15000);assert.equal(playback().positionSec,15,'Direction changes use the pending target');
 position=15000;poll(50);assert.equal(playback().positionSec,15);assert.equal(timers.size,0,'Confirmation stops polling');
 playing=true;command(5000);poll(200);sample();assert.equal(playback().positionSec,20.2,'Pending clock follows playing audio');
 playing=false;listeners.get('onplaypause')();poll(200);sample();assert.equal(playback().positionSec,20.2,'Pending clock stops on pause');
 position=20200;poll(50);assert.equal(timers.size,0);
 let rejectOld;player.seek=value=>{seeks.push(value);return new Promise((resolve,reject)=>{rejectOld=reject;});};
 command(5000);player.seek=value=>{seeks.push(value);};command(5000);rejectOld(Error('Old request failed'));await new Promise(setImmediate);
 assert.equal(playback().positionSec,30.2,'An old rejection cannot undo a newer target');
 player.seek=()=>Promise.reject(Error('Disconnected'));command(-5000);await new Promise(setImmediate);
 assert.equal(playback().positionSec,20.2,'A failed latest seek rolls back');assert.equal(timers.size,0);
 player.seek=()=>{};command(5000);poll(1600);assert.equal(playback().positionSec,20.2,'An unconfirmed command expires');assert.equal(timers.size,0);
 command(5000);player.data.item={uri:'spotify:track:second',metadata:{title:'Second',artist_name:'Artist'}};position=7000;
 listeners.get('songchange')();assert.equal(playback().positionSec,7);assert.equal(timers.size,0,'Track changes cancel predictions');
 position=179000;command(5000);assert.equal(playback().positionSec,180,'Clamp the track end');
 position=180000;poll(50);position=1000;command(-5000);assert.equal(playback().positionSec,0,'Clamp the track start');
 unload();assert.equal(timers.size,0,'Unload cancels temporary work');
 console.log('PASS immediate seek snapshots, stale-cache suppression, rapid/reverse seeks, play/pause clock, acknowledgement, rejection, timeout, track changes, endpoints and cleanup');
})().catch(error=>{console.error(error);process.exitCode=1;});
