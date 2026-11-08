'use strict';
// Bundle the existing, tested providers for Spotify's existing renderer.
const fs=require('fs'),path=require('path');
const root=path.resolve(__dirname,'..');
let modules='';
for(const name of ['lrc','matching','qqmusic','netease','lyrics']){
 let source=fs.readFileSync(path.join(root,'app/src',name+'.js'),'utf8');
 if(name==='qqmusic')source=source.replace("const crypto = require('crypto');",'').replace(/crypto\.createHash\('md5'\)\.update\(([^\n]+)\)\.digest\('hex'\)/,"await __nativeRequest('md5', $1)");
 modules+=JSON.stringify('./'+name)+': function(module,exports,require){\n'+source+'\n},\n';
}
const template=fs.readFileSync(path.join(root,'extension/spotify-lyrics-hud.js'),'utf8');
const runtime=`
  const Buffer = { from: raw => ({ toString: () => new TextDecoder().decode(Uint8Array.from(atob(raw), c => c.charCodeAt(0))) }) };
  const modules = {${modules}
    './http': function(module) { module.exports.fetchJSON = (url, options = {}, timeout = 8000) => __nativeRequest('fetch', {url, method: options.method || 'GET', headers: options.headers || {}, body: options.body || '', timeout}, options.signal, timeout + 2000); }
  };
  const loaded = {};
  function require(name) { if (!loaded[name]) { const module={exports:{}}; modules[name](module,module.exports,require); loaded[name]=module; } return loaded[name].exports; }
  let serial=0, pending=new Map(), lookup=null, lookupKey='', completeKey='';
  const cache=new Map();
  function __nativeRequest(op,args,signal,timeout=10000) {
    signal?.throwIfAborted();
    return new Promise((resolve,reject) => {
      if(!socket || socket.readyState!==WebSocket.OPEN) {reject(Error('HUD disconnected'));return;}
      const id=++serial;
      const finish=(error,result)=>{ const entry=pending.get(id); if(!entry)return; pending.delete(id);clearTimeout(entry.timer);signal?.removeEventListener('abort',entry.abort);error?reject(error):resolve(result); };
      const cancel=()=>{if(socket?.readyState===WebSocket.OPEN)socket.send(JSON.stringify({type:'cancel',id}));};
      const abort=()=>{cancel();finish(signal.reason||Error('Cancelled'));};
      const timer=setTimeout(()=>{cancel();finish(Error('Provider timeout'));},timeout);
      pending.set(id,{finish,timer,abort});signal?.addEventListener('abort',abort,{once:true});
      socket.send(JSON.stringify({type:'request',id,op,args}));
    });
  }
  function resetLookup() {
    lookup?.abort(); lookup=null;lookupKey='';completeKey='';
    for(const entry of [...pending.values()])entry.finish(Error('HUD disconnected'));
  }
  function lookupLyrics(snapshot) {
    if(!snapshot.found) {lookup?.abort();lookup=null;lookupKey='';completeKey='';return;}
    const key=JSON.stringify([snapshot.title,snapshot.artist,Math.round(snapshot.durationSec||0)]);
    if(key===lookupKey||key===completeKey)return;
    lookup?.abort();const controller=new AbortController();lookup=controller;lookupKey=key;completeKey='';
    const publish=result=>{if(controller.signal.aborted||lookupKey!==key||!socket||socket.readyState!==WebSocket.OPEN)return;socket.send(JSON.stringify({type:'lyrics',key,...result}));completeKey=key;};
    if(cache.has(key)){publish(cache.get(key));return;}
    require('./lyrics').fetchLyrics(snapshot.title,snapshot.artist,snapshot.durationSec,controller.signal).then(result=>{
      if(controller.signal.aborted)return;cache.delete(key);cache.set(key,result);while(cache.size>8)cache.delete(cache.keys().next().value);publish(result);
    }).catch(()=>{if(!controller.signal.aborted){lookupKey='';}});
  }
`;
let out=template.replace("  const token =",runtime+"\n  const token =")
 .replace('      lastSample = snapshot;','      lastSample = snapshot;\n      lookupLyrics(snapshot);')
 .replace("          if (state.type === 'state')", "          if (state.type === 'reply') { const entry=pending.get(state.id); if(entry)entry.finish(state.error?Error(state.error):null,state.result); }\n          else if (state.type === 'state')")
 .replace('socket = null; button.active = false;','resetLookup(); socket = null; button.active = false;')
 .replace('    stopped = true;','    resetLookup(); stopped = true;');
const target=process.argv[2]||path.join(root,'.build/payload/spotify-lyrics-hud.js');
fs.mkdirSync(path.dirname(target),{recursive:true});fs.writeFileSync(target,out);console.log('Bundled native renderer providers: '+target);
