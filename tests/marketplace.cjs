'use strict';
const assert=require('assert/strict'),fs=require('fs'),path=require('path'),vm=require('vm');
const source=fs.readFileSync(path.join(__dirname,'../.build/marketplace-bundle/marketplace-dist/extension.js'),'utf8');
class Storage{
 constructor(entries=[]){this.data=new Map(entries);}
 get length(){return this.data.size;}
 key(i){return [...this.data.keys()][i]??null;}
 getItem(k){return this.data.get(k)??null;}
 setItem(k,v){this.data.set(k,String(v));}
 removeItem(k){this.data.delete(k);}
 clear(){throw Error('Global session storage must never be cleared');}
}
async function boot(installed){
 const key='marketplace:installed:fixture';
 const local=new Storage(installed?[
  ['marketplace:installed-extensions',JSON.stringify([key])],
  [key,JSON.stringify({extensionURL:'https://raw.githubusercontent.com/example/test/main/test.js',manifest:{main:'test.js'}})]
 ]:[]);
 const session=new Storage([['other-extension','preserve']]),requests=[],scripts=[],notifications=[];
 const element=()=>({style:{},classList:{add(){},remove(){}},setAttribute(){},remove(){},appendChild(){}});
 const sandbox={Promise,Blob,URL,TextEncoder,TextDecoder,setTimeout,clearTimeout,setInterval,clearInterval,
  navigator:{userAgent:'Chrome/131',onLine:true,language:'en'},
  localStorage:local,sessionStorage:session,addEventListener(){},removeEventListener(){},
  document:{createElement:element,querySelector:()=>null,body:{appendChild:e=>scripts.push(e)}},
  console:{log(){},debug(){},warn(){},error(){}},
  fetch:async url=>{requests.push(url);return{type:'opaqueredirect'};},
  Spicetify:{React:{createContext:()=>({})},ReactDOM:{},LocalStorage:local,Config:{current_theme:'marketplace',extensions:[]},showNotification:(...args)=>notifications.push(args)}};
 sandbox.window=sandbox; sandbox.self=sandbox;
 await vm.runInNewContext(source,sandbox,{timeout:5000});
 await new Promise(resolve=>setTimeout(resolve,150));
 assert.equal(session.getItem('other-extension'),'preserve');
 assert.equal(notifications.length,0,JSON.stringify(notifications));
 assert.equal(sandbox.Marketplace.version,'1.0.12');
 assert.equal(requests.some(url=>url.includes('api.github.com/search')||url.includes('blacklist')),false);
 if(installed){assert(requests.length>0);assert(scripts.some(script=>script.src?.includes('test.js')));}
 else{assert.equal(requests.length,0,'Empty Marketplace should make no startup requests');assert.equal(scripts.filter(script=>script.src).length,0);}
}
(async()=>{await boot(false);await boot(true);console.log('PASS Marketplace: preserve shared cache, no idle networking/catalog preload, retain installed extension loader');})().catch(error=>{console.error(error);process.exitCode=1;});
