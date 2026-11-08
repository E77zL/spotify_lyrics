'use strict';
const fs = require('fs'), path = require('path'), crypto = require('crypto');
const { Readable, Transform } = require('stream');
const { pipeline } = require('stream/promises');
// Recent Node versions can honor the user's existing proxy configuration for
// fetch. Do not silently bypass it when downloading GitHub release assets.
const proxy = process.env.HTTPS_PROXY || process.env.https_proxy || process.env.HTTP_PROXY || process.env.http_proxy;
if (proxy && process.allowedNodeEnvironmentFlags.has('--use-env-proxy') &&
    !process.execArgv.includes('--use-env-proxy') && process.env.NODE_USE_ENV_PROXY !== '1') {
  const child = require('child_process').spawnSync(process.execPath,
    ['--use-env-proxy', __filename, ...process.argv.slice(2)], { stdio: 'inherit', env: process.env });
  if (child.error) throw child.error;
  process.exit(child.status || (child.status === 0 ? 0 : 1));
}
const root = path.resolve(__dirname,'..');
const cache = path.resolve(process.argv[2] || path.join(root,'.cache'));
const dependencies = require('../dependencies.lock.json');
async function digest(file) {
  const hash = crypto.createHash('sha256');
  for await (const chunk of fs.createReadStream(file)) hash.update(chunk);
  return hash.digest('hex');
}
(async()=>{
  fs.mkdirSync(cache,{recursive:true});
  for (const entry of dependencies) {
    if (path.basename(entry.name)!==entry.name) throw Error('Invalid dependency name');
    const file=path.join(cache,entry.name), temporary=file+'.download';
    if (fs.existsSync(file)) {
      if (await digest(file)!==entry.sha256) throw Error('Cached dependency hash mismatch: '+entry.name);
      console.log('Verified '+entry.name); continue;
    }
    console.log('Downloading '+entry.name);
    const response=await fetch(entry.url,{signal:AbortSignal.timeout(180000)});
    if(!response.ok) throw Error(entry.name+': HTTP '+response.status);
    const hash=crypto.createHash('sha256');
    try {
      await pipeline(Readable.fromWeb(response.body),new Transform({transform(chunk,encoding,done){hash.update(chunk);done(null,chunk);}}),fs.createWriteStream(temporary));
      if(hash.digest('hex')!==entry.sha256) throw Error('Downloaded hash mismatch: '+entry.name);
      fs.renameSync(temporary,file);
    } finally { if(fs.existsSync(temporary))fs.unlinkSync(temporary); }
  }
})().catch(error=>{console.error(error.message);process.exitCode=1;});
