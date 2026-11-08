'use strict';
// MIT. Small, fail-closed changes to the SHA256-pinned Marketplace 1.0.12 release.
const fs=require('fs'),vm=require('vm');
function patch(source){
 const replace=(before,after)=>{
  if(source.split(before).length!==2)throw Error('Marketplace patch no longer matches: '+before);
  source=source.replace(before,after);
 };
 const start=source.indexOf(',async function(){console.debug("Preloading extensions and themes...")');
 if(start<0||!source.trimEnd().endsWith('}()})()}();'))throw Error('Marketplace preload boundary changed');
 // Preserve the normal installed-extension loader. Remove only catalog preloading,
 // including its global sessionStorage.clear(), requests and worker creation.
 source=source.slice(0,start)+'})()}();\n';
 replace('const v=await dy();',
  'wh.setItem(g.localTheme,Spicetify.Config.current_theme||"");const v=Wg(g.installedExtensions,[]).length||wh.getItem(g.themeInstalled)?await dy():null;');
 replace('else window.navigator.onLine?',
  'else if(Wg(g.installedExtensions,[]).length||wh.getItem(g.themeInstalled))window.navigator.onLine?');
 new vm.Script(source,{filename:'marketplace-extension.js'});
 return '// Marketplace 1.0.12, MIT: CharlieS1103 + theRealPadster.\n'+
  '// Spotify Lyrics integration: on-demand catalog; no global session cache reset or idle CDN probe.\n'+source;
}
module.exports={patch};
if(require.main===module){
 const file=process.argv[2];fs.writeFileSync(file,patch(fs.readFileSync(file,'utf8')));
 console.log('Patched Marketplace startup; installed extensions/themes remain enabled.');
}
