/* ArSonMix Console EQ GUI: display/control layer only. No audio, AudioContext,
 * FFT, WebAudio, VST host, device access, or network. The browser may calculate
 * a sample response only in explicit ?designPreview=1 fixture mode. A native
 * host MUST deliver both accepted bands and native calculated curveDb[].
 */
(() => {
'use strict';
const $=id=>document.getElementById(id);
const KEY=[
  ['HPF','HIGH PASS',80,'#6ec7dc','LowCut'],
  ['LS','LOW SHELF',160,'#87b7e5','LowShelf'],
  ['1','BELL 1',250,'#78d2b1','Bell'],
  ['2','BELL 2',500,'#d2bb84','Bell'],
  ['3','BELL 3',1200,'#8caae9','Bell'],
  ['4','BELL 4',3200,'#dfad82','Bell'],
  ['5','BELL 5',7500,'#c9a5de','Bell'],
  ['HS','HIGH SHELF',10000,'#8dc9c9','HighShelf'],
  ['LPF','LOW PASS',18000,'#e1b58d','HighCut']
];
const N=9,pi=Math.PI,F0=20,F1=20000,GMAX=18,STEPS=257;
const canvas=$('eqPlot'),ctx=canvas.getContext('2d',{alpha:false});
const rail=$('filterRail');
const designPreview=new URLSearchParams(location.search).has('designPreview');
const clamp=(x,a,b)=>Math.min(b,Math.max(a,x));
const defaults=()=>KEY.map((k,i)=>({enabled:false,frequencyHz:k[2],gainDb:0,q:.7071067811865476,slopeDbPerOct:24}));
const isCut=i=>i===0||i===8;
const state={mode:designPreview?'design':'offline',sampleRate:48000,channelId:'1',deviceEpoch:'0',revision:'0',channelName:'CH 01 · VOCAL',bands:defaults(),curveDb:null,selected:3,pending:null,requestCounter:0,drag:null,previewDraft:null,readout:'Drag a point to adjust · wheel changes Q'};
let viewWidth=400,viewHeight=300,dpr=1,needsFrame=false;
const hex=/^(0|[1-9][0-9]*)$/;
const validUint=v=>typeof v==='string'&&v.length<=20&&hex.test(v)&&BigInt(v)<=18446744073709551615n;
const validBand=(b,i,sr)=>b&&typeof b.enabled==='boolean'&&Number.isFinite(b.frequencyHz)&&b.frequencyHz>=20&&b.frequencyHz<=Math.min(20000,sr*.45)&&Number.isFinite(b.gainDb)&&b.gainDb>=-18&&b.gainDb<=18&&Number.isFinite(b.q)&&b.q>=.1&&b.q<=24&&[12,24,48].includes(b.slopeDbPerOct)&&(!isCut(i)||b.gainDb===0);
const sameBand=(a,b)=>a&&b&&a.enabled===b.enabled&&a.frequencyHz===b.frequencyHz&&a.gainDb===b.gainDb&&a.q===b.q&&a.slopeDbPerOct===b.slopeDbPerOct;
const round=(v,n=1)=>Number(v.toFixed(n));
const copy=b=>({...b});
const labelHz=f=>f>=1000?`${round(f/1000,1)}k`:`${Math.round(f)}`;
const xf=f=>39+Math.log(clamp(f,F0,F1)/F0)/Math.log(F1/F0)*(viewWidth-81);
const fx=x=>F0*Math.pow(F1/F0,clamp((x-39)/(viewWidth-81),0,1));
const yg=db=>(viewHeight/2)-(clamp(db,-GMAX,GMAX)/GMAX)*(viewHeight*.39);
const gy=y=>clamp((viewHeight*.5-y)/(viewHeight*.39)*GMAX,-18,18);
const maxFrequency=()=>Math.min(20000,state.sampleRate*.45);
const draftBands=()=>state.previewDraft?state.bands.map((b,i)=>i===state.selected?state.previewDraft:b):state.bands;
const bandEnabledCount=()=>state.bands.reduce((v,b)=>v+(b.enabled?1:0),0);

// Only used for the expressly labeled design fixture. Native mode NEVER
// computes a browser response; the real Engine provides curveDb from audio-
// owner-safe control-side snapshots. Biquad math here is display-only.
function previewResponse(bands, rate) {
  const butter={1:[.7071067811865476],2:[.541196100146197,1.306562964876377],4:[.509795579104159,.601344886935045,.899976223136416,2.562915447741506]};
  const calc=(type,hz,gain,q)=>{
    const w=2*pi*hz/rate,s=Math.sin(w),c=Math.cos(w),A=Math.pow(10,gain/40),alpha=s/(2*q);
    const S=clamp(q,.25,1),root=2*Math.sqrt(A)*(.5*s*Math.sqrt((A+1/A)*(1/S-1)+2));
    let b0=1,b1=0,b2=0,a0=1,a1=0,a2=0;
    if(type==='Bell'){b0=1+alpha*A;b1=-2*c;b2=1-alpha*A;a0=1+alpha/A;a1=-2*c;a2=1-alpha/A;}
    else if(type==='LowCut'){b0=(1+c)/2;b1=-(1+c);b2=(1+c)/2;a0=1+alpha;a1=-2*c;a2=1-alpha;}
    else if(type==='HighCut'){b0=(1-c)/2;b1=1-c;b2=(1-c)/2;a0=1+alpha;a1=-2*c;a2=1-alpha;}
    else if(type==='LowShelf'){b0=A*((A+1)-(A-1)*c+root);b1=2*A*((A-1)-(A+1)*c);b2=A*((A+1)-(A-1)*c-root);a0=(A+1)+(A-1)*c+root;a1=-2*((A-1)+(A+1)*c);a2=(A+1)+(A-1)*c-root;}
    else if(type==='HighShelf'){b0=A*((A+1)+(A-1)*c+root);b1=-2*A*((A-1)+(A+1)*c);b2=A*((A+1)+(A-1)*c-root);a0=(A+1)-(A-1)*c+root;a1=2*((A-1)-(A+1)*c);a2=(A+1)-(A-1)*c-root;}
    return [b0/a0,b1/a0,b2/a0,a1/a0,a2/a0];
  };
  const entries=[];
  bands.forEach((b,i)=>{
    if(!b.enabled)return;
    const qs=isCut(i)?butter[b.slopeDbPerOct/12]:[b.q];
    for(const q of qs)entries.push(calc(KEY[i][4],b.frequencyHz,b.gainDb,q));
  });
  const out=[];
  for(let k=0;k<STEPS;k++){
    const hz=F0*Math.pow(F1/F0,k/(STEPS-1)),w=2*pi*hz/rate;
    const c=Math.cos(w),s=Math.sin(w),c2=Math.cos(2*w),s2=Math.sin(2*w);
    let sum=0;
    for(const [b0,b1,b2,a1,a2] of entries){
      const nr=b0+b1*c+b2*c2,ni=-b1*s-b2*s2,dr=1+a1*c+a2*c2,di=-a1*s-a2*s2;
      sum+=10*Math.log10(Math.max(1e-20,(nr*nr+ni*ni)/Math.max(1e-20,dr*dr+di*di)));
    }
    out.push(clamp(sum,-100,100));
  }
  return out;
}

function renderStatus(message) {
  const demo=state.mode==='design',live=state.mode==='native';
  const label=$('connectionLabel');
  label.textContent=demo?'DESIGN PREVIEW · NO AUDIO':live?'NATIVE SNAPSHOT':'NOT CONNECTED';
  label.classList.toggle('live',live&&!state.pending);
  $('statusDot').className='status-dot'+(live&&!state.pending?' live':state.pending?' pending':'');
  $('statusText').textContent=message||(demo?'DESIGN PREVIEW · NO AUDIO / no mixer changes':live?(state.pending?'Waiting for native ACK + readback':'Native response · accepted snapshot'):'Native offline · controls disabled');
  $('plotMode').textContent=demo?'DESIGN FIXTURE · NOT AUDIO':live?(state.curveDb?'CURVE FROM NATIVE DSP':'NATIVE CURVE UNAVAILABLE'):'NO AUDIO ENGINE CONNECTED';
  $('sampleRate').textContent=Number(state.sampleRate/1000).toFixed(state.sampleRate%1000===0?0:1)+' kHz';
}
function renderRail(){
  for(const button of rail.children){
    const index=Number(button.dataset.slot),b=state.bands[index];
    button.classList.toggle('selected',index===state.selected);
    button.dataset.enabled=String(b.enabled);
    button.setAttribute('aria-current',index===state.selected?'true':'false');
    button.title=KEY[index][1]+(b.enabled?' · enabled':' · bypassed')+' · click to select, double-click to toggle';
    button.disabled=state.mode==='offline';
  }
}
function renderInspector(){
  const index=state.selected,b=state.previewDraft||state.bands[index],editable=state.mode!=='offline'&&!state.pending;
  $('selectedTitle').innerHTML='';
  $('selectedTitle').append(document.createTextNode(KEY[index][1]+' '));
  const suffix=document.createElement('span');suffix.textContent=KEY[index][0];$('selectedTitle').append(suffix);
  $('selectedMark').style.background=KEY[index][3];
  $('frequencyInput').value=round(b.frequencyHz,1);
  $('gainInput').value=round(b.gainDb,2);
  $('qInput').value=round(b.q,2);
  $('slopeInput').value=String(b.slopeDbPerOct);
  $('gainInput').disabled=!editable||isCut(index);
  $('qInput').disabled=!editable||isCut(index);
  $('slopeInput').disabled=!editable||!isCut(index);
  $('frequencyInput').disabled=!editable;
  $('bandToggle').disabled=!editable;
  $('bandToggle').setAttribute('aria-pressed',String(b.enabled));
  $('bandToggle').setAttribute('aria-label',(b.enabled?'Disable':'Enable')+' '+KEY[index][1]);
  $('powerText').textContent=b.enabled?'ON':'OFF';
  $('flatBtn').disabled=!editable;
}
function refresh(){renderRail();renderInspector();renderStatus();drawLater();}
function selectBand(i){if(i<0||i>=N)return;state.selected=i;state.previewDraft=null;refresh();}
function sendBand(i,band){
  if(state.mode==='offline'||state.pending||!validBand(band,i,state.sampleRate))return false;
  if(state.mode==='design'){
    state.bands[i]=copy(band);
    state.curveDb=previewResponse(state.bands,state.sampleRate);
    refresh();return true;
  }
  const bridge=window.__ARSON_CONSOLE_EQ_BRIDGE__;
  if(!bridge||typeof bridge.send!=='function')return false;
  const requestId='ceq-'+(++state.requestCounter),expected=copy(band);
  const payload={schemaVersion:1,kind:'eqSetBand',requestId,channelId:state.channelId,
    deviceEpoch:state.deviceEpoch,baseRevision:state.revision,slot:i,parameters:expected};
  state.pending={requestId,slot:i,expected,ackRevision:null,epoch:state.deviceEpoch,channelId:state.channelId};
  armTimeout(requestId);
  try{bridge.send(payload);}catch(e){state.pending=null;renderStatus('Native command rejected by transport');refresh();return false;}
  refresh();renderStatus('PENDING · wait for native ACK and matching readback');
  return true;
}
function updateSelected(field,value){
  if(state.mode==='offline'||state.pending||!Number.isFinite(value))return false;
  const i=state.selected,b=copy(state.bands[i]);b[field]=value;
  if(!validBand(b,i,state.sampleRate))return false;
  return sendBand(i,b);
}
function armTimeout(requestId) {
  window.setTimeout(()=>{
    if(state.pending?.requestId!==requestId)return;
    state.pending=null;refresh();renderStatus('TIMEOUT · EQ state not confirmed by native');
  },4000);
}
function resetFlat(){
  if(state.mode==='offline'||state.pending)return;
  if(state.mode==='design') {
    state.bands=state.bands.map(b=>({...b,enabled:false}));
    state.curveDb=previewResponse(state.bands,state.sampleRate);
    refresh();return;
  }
  // Native must implement atomic transaction; never emit nine unrelated edits.
  const bridge=window.__ARSON_CONSOLE_EQ_BRIDGE__;
  if(!bridge||typeof bridge.send!=='function')return;
  const requestId='ceq-'+(++state.requestCounter);
  state.pending={requestId,allFlat:true,ackRevision:null,epoch:state.deviceEpoch,channelId:state.channelId};
  armTimeout(requestId);
  try{bridge.send({schemaVersion:1,kind:'eqResetFlat',requestId,channelId:state.channelId,
    deviceEpoch:state.deviceEpoch,baseRevision:state.revision});}
  catch(e){state.pending=null;refresh();renderStatus('Native FLAT unavailable');return;}
  refresh();renderStatus('PENDING · waiting for atomic EQ reset confirmation');
}

// Strong validation: all nine fixed-type bands and current native response
// are accepted together, never preserve another ChannelId's curve on switch.
function setSnapshot(s){
  if(!s||s.schemaVersion!==1||!validUint(s.channelId)||!validUint(s.deviceEpoch)||!validUint(s.revision)||
     !Number.isFinite(s.sampleRate)||s.sampleRate<8000||s.sampleRate>192000||
     !Array.isArray(s.bands)||s.bands.length!==N||
     !s.bands.every((b,i)=>validBand(b,i,s.sampleRate))||
     (s.curveDb!==null&&s.curveDb!==undefined&&(!Array.isArray(s.curveDb)||s.curveDb.length<65||s.curveDb.length>1024||!s.curveDb.every(v=>Number.isFinite(v)&&v>=-100&&v<=100))))return false;
  if(state.mode==='native'&&s.channelId===state.channelId&&s.deviceEpoch===state.deviceEpoch&&BigInt(s.revision)<BigInt(state.revision))return false;
  if(state.mode==='native'&&s.channelId===state.channelId&&BigInt(s.deviceEpoch)<BigInt(state.deviceEpoch))return false;
  const changedIdentity=state.mode!=='native'||s.channelId!==state.channelId||s.deviceEpoch!==state.deviceEpoch;
  if(changedIdentity)state.pending=null;
  state.mode='native';state.channelId=s.channelId;state.deviceEpoch=s.deviceEpoch;state.revision=s.revision;state.sampleRate=s.sampleRate;
  state.channelName=typeof s.channelName==='string'&&s.channelName.length<80?s.channelName:'CH '+s.channelId;
  $('channelTitle').textContent=state.channelName;
  state.bands=s.bands.map(copy);state.curveDb=s.curveDb||null;state.previewDraft=null;
  if(state.pending&&state.pending.ackRevision!==null&&
      BigInt(s.revision)>=BigInt(state.pending.ackRevision)&&
      (state.pending.allFlat?s.bands.every(b=>!b.enabled):sameBand(s.bands[state.pending.slot],state.pending.expected)))state.pending=null;
  refresh();return true;
}
function ack(a){
  const p=state.pending;
  if(!p||!a||a.schemaVersion!==1||a.requestId!==p.requestId||
     a.channelId!==p.channelId||a.deviceEpoch!==p.epoch)return false;
  if(a.status!=='accepted'){
    state.pending=null;refresh();renderStatus('REJECTED / STALE · native did not apply EQ change');return true;
  }
  if(!validUint(a.revision)||BigInt(a.revision)<BigInt(state.revision))return false;
  p.ackRevision=a.revision;
  // Requires a fresh accepted native revision and exact requested parameters.
  // An ACK alone can never repaint desired state as though audio were updated.
  refresh();renderStatus('ACK received · awaiting matching native readback');return true;
}
function disconnect(){state.mode='offline';state.pending=null;state.curveDb=null;state.bands=defaults();state.deviceEpoch='0';state.revision='0';state.previewDraft=null;refresh();}

function resize(){
  const r=canvas.getBoundingClientRect();
  if(!r.width||!r.height)return;
  viewWidth=Math.max(120,r.width);viewHeight=Math.max(100,r.height);dpr=Math.min(2,window.devicePixelRatio||1);
  canvas.width=Math.round(viewWidth*dpr);canvas.height=Math.round(viewHeight*dpr);
  ctx.setTransform(dpr,0,0,dpr,0,0);drawLater();
}
function drawLater(){if(needsFrame)return;needsFrame=true;requestAnimationFrame(()=>{needsFrame=false;draw();});}
function draw(){
  const w=viewWidth,h=viewHeight;ctx.clearRect(0,0,w,h);
  ctx.fillStyle='#111d28';ctx.fillRect(0,0,w,h);
  const stops=[20,50,100,200,500,1000,2000,5000,10000,20000];
  ctx.textBaseline='middle';ctx.font='10px Consolas, monospace';
  stops.forEach((f,i)=>{
    const x=xf(f);ctx.strokeStyle=i%2?'#293943':'#344651';ctx.globalAlpha=i%2?.55:.7;
    ctx.beginPath();ctx.moveTo(x,26);ctx.lineTo(x,h-30);ctx.stroke();ctx.globalAlpha=1;
    ctx.fillStyle='#7892a2';ctx.textAlign='center';ctx.fillText(f>=1000?(f/1000)+'k':String(f),x,h-17);
  });
  [-18,-12,-6,0,6,12,18].forEach(db=>{
    const y=yg(db);ctx.setLineDash(db===0?[4,4]:[]);ctx.lineWidth=db===0?1.15:.65;
    ctx.strokeStyle=db===0?'#65818d':'#29404e';ctx.beginPath();ctx.moveTo(39,y);ctx.lineTo(w-42,y);ctx.stroke();ctx.setLineDash([]);
    ctx.textAlign='right';ctx.fillStyle=db===0?'#b5c9d3':'#6f8998';ctx.fillText((db>0?'+':'')+db,31,y);
    ctx.textAlign='left';ctx.fillText('dB',w-33,y);
  });
  // Accepted host response is not assumed if missing. Flat is displayed only
  // as the disabled baseline; the visible caption warns if native curve absent.
  const curve=state.mode==='design'?(state.previewDraft?previewResponse(draftBands(),state.sampleRate):state.curveDb):state.curveDb;
  if(curve&&curve.length>=65){
    ctx.beginPath();curve.forEach((db,i)=>{const x=39+i/(curve.length-1)*(w-81),y=yg(db);i?ctx.lineTo(x,y):ctx.moveTo(x,y);});
    ctx.lineTo(w-42,yg(0));ctx.lineTo(39,yg(0));ctx.closePath();
    const grd=ctx.createLinearGradient(0,yg(18),0,yg(-18));grd.addColorStop(0,'#b78d4750');grd.addColorStop(.5,'#b78d4712');grd.addColorStop(1,'#2fa7bb24');ctx.fillStyle=grd;ctx.fill();
    ctx.beginPath();curve.forEach((db,i)=>{const x=39+i/(curve.length-1)*(w-81),y=yg(db);i?ctx.lineTo(x,y):ctx.moveTo(x,y);});
    ctx.lineWidth=2.1;ctx.strokeStyle='#ebc47e';ctx.shadowColor='#f0c47735';ctx.shadowBlur=7;ctx.stroke();ctx.shadowBlur=0;
  }else{
    ctx.strokeStyle='#9cb2be';ctx.lineWidth=1.2;ctx.beginPath();ctx.moveTo(39,yg(0));ctx.lineTo(w-42,yg(0));ctx.stroke();
  }
  const bands=draftBands();
  bands.forEach((b,i)=>{
    const x=xf(b.frequencyHz),y=yg(isCut(i)?0:b.gainDb),picked=state.selected===i,r=picked?10:7;
    ctx.beginPath();ctx.arc(x,y,r+3,0,2*pi);ctx.fillStyle='#0b151e';ctx.fill();
    ctx.beginPath();ctx.arc(x,y,r,0,2*pi);ctx.fillStyle=b.enabled?KEY[i][3]:'#25333b';
    ctx.strokeStyle=picked?'#d5f2ff':KEY[i][3];ctx.lineWidth=picked?1.8:1;ctx.fill();ctx.stroke();
    ctx.font=(picked?'600 ':'')+'9px Inter, Segoe UI, sans-serif';ctx.textAlign='center';ctx.textBaseline='middle';ctx.fillStyle=b.enabled?'#0c2734':picked?'#e8f4f8':'#acbcc4';ctx.fillText(KEY[i][0].length>1?KEY[i][0]:String(i-1),x,y+.2);
  });
  ctx.textBaseline='alphabetic';
}
function locate(e){const r=canvas.getBoundingClientRect();return {x:(e.clientX-r.left)*viewWidth/r.width,y:(e.clientY-r.top)*viewHeight/r.height};}
function pick(x,y){let index=-1,distance=20;for(let i=0;i<N;i++){const b=state.bands[i],d=Math.hypot(xf(b.frequencyHz)-x,yg(isCut(i)?0:b.gainDb)-y);if(d<distance){distance=d;index=i;}}return index;}
function mouseDown(e){if(e.button!==0||state.mode==='offline'||state.pending)return;const p=locate(e),i=pick(p.x,p.y);if(i<0)return;selectBand(i);state.drag={pointerId:e.pointerId,startX:p.x,startY:p.y,moved:false};canvas.setPointerCapture?.(e.pointerId);}
function mouseMove(e){const p=locate(e),idx=state.selected;if(state.drag&&e.pointerId===state.drag.pointerId){
  const drag=state.drag;if(Math.hypot(p.x-drag.startX,p.y-drag.startY)>3)drag.moved=true;
  if(!drag.moved)return;
  const candidate=copy(state.previewDraft||state.bands[idx]);candidate.frequencyHz=clamp(round(fx(p.x),1),20,maxFrequency());
  if(!isCut(idx))candidate.gainDb=round(gy(p.y),1);
  if(!candidate.enabled)candidate.enabled=true;
  state.previewDraft=candidate;
  $('plotReadout').textContent=labelHz(candidate.frequencyHz)+' Hz  ·  '+(candidate.gainDb>0?'+':'')+candidate.gainDb+' dB · EDIT NOT COMMITTED';
  renderInspector();drawLater();return;
 }
 const i=pick(p.x,p.y);const b=i>=0?state.bands[i]:null;
 $('plotReadout').textContent=b?KEY[i][1]+' · '+labelHz(b.frequencyHz)+' Hz'+(isCut(i)?'':(' · '+b.gainDb.toFixed(1)+' dB')):'Drag a point to adjust · wheel changes Q';
}
function mouseUp(e){if(!state.drag||e.pointerId!==state.drag.pointerId)return;const draft=state.previewDraft,idx=state.selected;state.drag=null;state.previewDraft=null;if(draft)sendBand(idx,draft);else refresh();}
function mouseWheel(e){if(state.mode==='offline'||state.pending)return;const p=locate(e),i=pick(p.x,p.y);if(i<0)return;selectBand(i);e.preventDefault();const b=copy(state.bands[i]);if(isCut(i)){const slopes=[12,24,48],next=clamp(slopes.indexOf(b.slopeDbPerOct)+(e.deltaY<0?1:-1),0,2);b.slopeDbPerOct=slopes[next];}else b.q=clamp(round(b.q*(e.deltaY<0?1.1:1/1.1),2),.1,24);sendBand(i,b);}
function doubleClick(e){if(state.mode==='offline'||state.pending)return;const p=locate(e),i=pick(p.x,p.y);if(i>=0){selectBand(i);sendBand(i,{...state.bands[i],enabled:!state.bands[i].enabled});}}

KEY.forEach((k,i)=>{
  const btn=document.createElement('button');btn.className='filter-key';btn.type='button';btn.dataset.slot=String(i);btn.style.setProperty('--band-color',k[3]);btn.innerHTML='<span class="filter-swatch"></span><span>'+k[0]+'</span>';btn.setAttribute('aria-label','Select '+k[1]);
  btn.addEventListener('click',()=>selectBand(i));btn.addEventListener('dblclick',()=>{if(state.mode==='offline'||state.pending)return;sendBand(i,{...state.bands[i],enabled:!state.bands[i].enabled});});rail.append(btn);
});
['frequencyInput','gainInput','qInput','slopeInput'].forEach(id=>$(id).addEventListener('change',e=>{
  const field={frequencyInput:'frequencyHz',gainInput:'gainDb',qInput:'q',slopeInput:'slopeDbPerOct'}[id];
  const ok=updateSelected(field,Number(e.target.value));if(!ok){renderInspector();renderStatus('Invalid or unacknowledged filter edit');}
}));
$('bandToggle').addEventListener('click',()=>{const b=state.bands[state.selected];sendBand(state.selected,{...b,enabled:!b.enabled});});
$('flatBtn').addEventListener('click',resetFlat);
canvas.addEventListener('pointerdown',mouseDown);canvas.addEventListener('pointermove',mouseMove);canvas.addEventListener('pointerup',mouseUp);canvas.addEventListener('pointercancel',()=>{state.drag=null;state.previewDraft=null;refresh();});canvas.addEventListener('wheel',mouseWheel,{passive:false});canvas.addEventListener('dblclick',doubleClick);
window.addEventListener('resize',resize);
if(window.ResizeObserver)new ResizeObserver(resize).observe(canvas);
if(state.mode==='design'){
  state.bands[0].enabled=true;
  state.bands[3].enabled=true;state.bands[3].gainDb=-3.5;state.bands[3].q=1.2;
  state.bands[5].enabled=true;state.bands[5].gainDb=2.7;state.bands[5].q=1.1;
  state.bands[7].enabled=true;state.bands[7].gainDb=1.7;
  state.bands[8].enabled=true;state.bands[8].frequencyHz=18000;
  state.curveDb=previewResponse(state.bands,state.sampleRate);
}
window.ArSonConsoleEQ=Object.freeze({setSnapshot,ack,disconnect,getDiagnostics:()=>({mode:state.mode,channelId:state.channelId,deviceEpoch:state.deviceEpoch,revision:state.revision,pending:state.pending?.requestId||null,selected:state.selected,activeBands:bandEnabledCount(),hasNativeCurve:state.mode==='native'&&!!state.curveDb}),
  // Read-only contract introspection for native implementers / QA tests.
  protocolVersion:1});
resize();refresh();

// Browser QA exercised only via explicit ?smoke=1. The file is NOT loaded
// by the native mixer and never contacts a network or audio device.
if(new URLSearchParams(location.search).has('smoke')){
 try{
  const check=(v,why)=>{if(!v)throw new Error(why);};
  check(state.mode==='offline','default fail-closed');
  check($('frequencyInput').disabled,'offline disables edits');
  const good={schemaVersion:1,channelId:'10',deviceEpoch:'22',revision:'4',sampleRate:48000,
    channelName:'Vocal <script>not markup</script>',bands:defaults(),curveDb:Array.from({length:257},()=>0)};
  check(setSnapshot(good),'good native accepted');
  check($('channelTitle').textContent.includes('<script>'),'name text only');
  check(!setSnapshot({...good,revision:'3'}),'older snapshot rejected');
  check(!setSnapshot({...good,bands:[...good.bands.slice(0,8)]}),'8 bands rejected');
  check(!setSnapshot({...good,curveDb:Array(257).fill(NaN)}),'invalid curve rejected');
  const sent=[];window.__ARSON_CONSOLE_EQ_BRIDGE__={send:x=>sent.push(x)};
  const expect={...state.bands[3],enabled:true,gainDb:3};
  check(sendBand(3,expect),'command sent');check(sent.length===1,'single command');
  check(state.bands[3].gainDb===0,'no optimistic applied state');
  check(!ack({schemaVersion:1,requestId:'wrong',channelId:'10',deviceEpoch:'22',revision:'5',status:'accepted'}),'stale ack denied');
  check(ack({schemaVersion:1,requestId:sent[0].requestId,channelId:'10',deviceEpoch:'22',revision:'5',status:'accepted'}),'matching ack');
  check(state.pending!==null,'ACK is not enough');
  check(setSnapshot({...good,revision:'5'}),'readback');check(state.pending!==null,'mismatched readback not accepted');
  check(setSnapshot({...good,revision:'6',bands:good.bands.map((b,i)=>i===3?expect:b)}),'matching readback');
  check(state.pending===null&&state.bands[3].gainDb===3,'ACK + readback finalized');
  const next={...good,deviceEpoch:'23',revision:'1',channelId:'11',bands:defaults()};
  check(setSnapshot(next),'new epoch');check(state.bands[3].gainDb===0,'no per-channel state leakage');
  disconnect();check(state.mode==='offline'&&$('frequencyInput').disabled,'disconnect fail closed');
  document.body.dataset.smoke='PASS';
 }catch(e){document.body.dataset.smoke='FAIL:'+String(e.message);}
}
})();
