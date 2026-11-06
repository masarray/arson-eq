// Offline browser smoke. No npm, network, WebAudio or new host.
const {spawnSync}=require('node:child_process');
const {resolve}=require('node:path');
const fs=require('node:fs');
const browser=process.env.CHROME || ['/usr/bin/chromium','/usr/bin/google-chrome','/usr/bin/chromium-browser'].find(fs.existsSync);
if(!browser){console.log('Chromium absent: native-only smoke; screenshot gate skipped');process.exit(0)}
const page='file://'+resolve(__dirname,'index.html')+'?smoke=1';
const r=spawnSync(browser,['--headless','--no-sandbox','--disable-gpu','--disable-dev-shm-usage','--virtual-time-budget=1500','--dump-dom',page],{encoding:'utf8',timeout:30000,maxBuffer:4*1024*1024});
if(r.status!==0||!r.stdout.includes('data-smoke="PASS"')){
 console.error('ConsoleUI contract smoke failed',r.stdout.match(/data-smoke="[^"]+"/)?.[0],r.stderr.slice(-900));process.exit(1);
}
console.log('ConsoleUI native ACK+readback / stale-epoch / offline fail-closed browser smoke PASS');
