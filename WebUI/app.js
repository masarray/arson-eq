/* ArSonEQ Studio web canvas. Adapted interaction vocabulary from user's MaSonKuPik EQ.
 * WebView contains NO audio DSP, no Web Audio, no libraries, no network requests.
 * Backend owns authoritative host parameters and transfer functions.
 */
(() => {
"use strict";
const $=id=>document.getElementById(id);
const canvas=$("eqCanvas"), wrap=$("graphWrap"), ctx=canvas.getContext("2d",{alpha:false});
const TYPES=["Bell","Low Shelf","High Shelf","Low Cut","High Cut","Notch"];
const COLORS=["#ff9473","#e5b855","#a99aff","#77d5a4","#53ccd2","#b58eea","#eb87a9","#78b6e9"];
const F0=20,F1=20000,RANGE=18,MAX=24;
const clamp=(x,a,b)=>Math.max(a,Math.min(b,x));
const hzX=(f,w)=>Math.log(clamp(f,F0,F1)/F0)/Math.log(F1/F0)*w;
const xHz=(x,w)=>F0*Math.pow(F1/F0,clamp(x/w,0,1));
const dbY=(v,h)=>h*0.5-clamp(v,-RANGE,RANGE)/RANGE*h*0.5;
const yDb=(y,h)=>clamp((h*0.5-y)/(h*0.5)*RANGE,-RANGE,RANGE);
const isCut=t=>t===3||t===4;
const isFiniteNumber=n=>typeof n==="number"&&Number.isFinite(n);
const noteFor=hz=>{if(!isFiniteNumber(hz)||hz<=0)return "—";const midi=Math.round(69+12*Math.log2(hz/440));return ["C","C♯","D","D♯","E","F","F♯","G","G♯","A","A♯","B"][(midi+1200)%12]+(Math.floor(midi/12)-1);};
const backend=window.__JUCE__?.backend;
let bands=Array.from({length:MAX},(_,i)=>({id:i,enabled:false,type:0,route:0,freq:1000,gain:0,q:1,slope:0}));
let curves={l:[],r:[],cross:[],band:[]},spec={pre:[],post:[]},samplerate=48000;
let selected=-1,hovered=-1,showSpectrum=true,dragging=null,pendingDrag=null;
let w=1,h=1,dpr=1,dirty=false,connected=false,hasSpectrum=false;
const emit=payload=>{if(backend?.emitEvent)backend.emitEvent("eq-command",payload);};
const changed=()=>{if(!dirty){dirty=true;requestAnimationFrame(()=>{dirty=false;draw();});}};
const col=i=>COLORS[i%COLORS.length];
const active=()=>bands.map((b,i)=>b.enabled?i:-1).filter(i=>i>=0);
const updateBandLocal=(id,field,value)=>{
  if(id<0||id>=MAX||!Number.isFinite(value))return;
  bands[id][field]=value;
  renderInspector();changed();
};
const setParam=(id,field,value,gesture=true)=>{
  if(!Number.isFinite(value)||id<0||id>=MAX)return;
  updateBandLocal(id,field,value);
  emit({kind:"edit",id,field,value,gesture});
};
const select=i=>{
  selected=(i>=0&&i<MAX)?i:-1;
  $("bandList").hidden=true;
  $("quickHelp").hidden=true;
  emit({kind:"select",id:selected});
  renderInspector();renderRail();changed();
};
const addBand=(x,y)=>{
  const idx=bands.findIndex(b=>!b.enabled);
  if(idx<0)return;
  const freq=xHz(x,w),gain=yDb(y,h);
  bands[idx]={...bands[idx],enabled:true,type:0,route:0,freq,gain,q:1,slope:0};
  emit({kind:"add",id:idx,freq,gain});
  select(idx);
};
const removeSelected=()=>{
  if(selected<0)return;
  setParam(selected,"enabled",0);
  select(-1);
};
const bandHit=(x,y)=>{
  let choice=-1,best=16*16;
  for(let i=0;i<MAX;i++){
    const b=bands[i];if(!b.enabled)continue;
    const cx=hzX(b.freq,w),cy=dbY(isCut(b.type)?0:b.gain,h);
    const ds=(cx-x)**2+(cy-y)**2;
    if(ds<best){choice=i;best=ds;}
  }
  return choice;
};
const formatHz=f=>f>=1000?(f/1000).toFixed(f%1000===0?0:1)+"k":Math.round(f)+"";
const number=n=>Number.isFinite(n)?n.toFixed(1):"—";
const renderRail=()=>{
  const list=active();
  $("bandCount").textContent=list.length+" / 24";
  const rail=$("bandRail");rail.replaceChildren();
  for(const i of list){
    const chip=document.createElement("button");
    chip.type="button";chip.className="band-chip on"+(selected===i?" selected":"");
    chip.style.setProperty("--chip",col(i));
    chip.textContent=String(i+1).padStart(2,"0")+" · "+formatHz(bands[i].freq);
    chip.onclick=()=>select(i);rail.appendChild(chip);
  }
};
const positionInspector=()=>{
  if(selected<0||$("inspector").hidden)return;
  const b=bands[selected],panel=$("inspector");
  const pw=panel.offsetWidth||466,ph=panel.offsetHeight||174;
  const x=hzX(b.freq,w),y=dbY(isCut(b.type)?0:b.gain,h);
  const px=clamp(x-pw*.5,10,Math.max(10,w-pw-10));
  const roomBelow=h-y-22,roomAbove=y-30;
  let py=roomBelow>=ph+10?y+19:roomAbove>=ph+10?y-ph-19:h-ph-12;
  py=clamp(py,33,Math.max(33,h-ph-12));
  panel.style.left=Math.round(px)+"px";
  panel.style.top=Math.round(py)+"px";
};
const renderInspector=()=>{
  const b=selected>=0?bands[selected]:null;
  $("inspector").hidden=!b;
  $("emptyInspector").hidden=true;
  $("bandInspector").hidden=!b;
  if(!b)return;
  $("activeLabel").textContent=["STEREO","LEFT","RIGHT","MID","SIDE"][b.route]||"STEREO";
  $("bandTitle").textContent="BAND "+String(selected+1).padStart(2,"0");
  $("bandDot").style.background=col(selected);
  $("enableBtn").textContent=b.enabled?"● ON":"○ OFF";
  $("enableBtn").classList.toggle("off",!b.enabled);
  $("typeSelect").value=String(b.type);
  $("slopeSelect").value=String(b.slope);
  const setValue=(id,v)=>{const input=$(id);if(document.activeElement!==input)input.value=String(v);};
  setValue("freqInput",Math.round(b.freq*10)/10);
  setValue("gainInput",Math.round(b.gain*100)/100);
  setValue("qInput",Math.round(b.q*100)/100);
  $("gainInput").disabled=isCut(b.type)||b.type===5;
  $("slopeSelect").disabled=!isCut(b.type);
  $("noteLabel").textContent=noteFor(b.freq);
  $("routeGroup").querySelectorAll("button").forEach(btn=>
    btn.classList.toggle("active",Number(btn.dataset.route)===b.route));
  positionInspector();
};
const mapLocal=e=>{const r=canvas.getBoundingClientRect();return{x:clamp(e.clientX-r.left,0,r.width),y:clamp(e.clientY-r.top,0,r.height)};};
const drawSmooth=(arr,color,lineWidth=2,fill=false,alpha=1)=>{
  if(!arr||arr.length<2)return;
  ctx.beginPath();
  for(let i=0;i<arr.length;i++){
    const x=i/(arr.length-1)*w,y=dbY(arr[i],h);
    if(i===0)ctx.moveTo(x,y);else ctx.lineTo(x,y);
  }
  if(fill){
    ctx.lineTo(w,h/2);ctx.lineTo(0,h/2);ctx.closePath();
    const grad=ctx.createLinearGradient(0,0,0,h);
    grad.addColorStop(0,"rgba(69,199,218,.15)");
    grad.addColorStop(.5,"rgba(69,199,218,.035)");
    grad.addColorStop(1,"rgba(69,199,218,.09)");
    ctx.fillStyle=grad;ctx.fill();
  }
  ctx.beginPath();
  for(let i=0;i<arr.length;i++){
    const x=i/(arr.length-1)*w,y=dbY(arr[i],h);
    if(i===0)ctx.moveTo(x,y);else ctx.lineTo(x,y);
  }
  ctx.strokeStyle=color;ctx.lineWidth=lineWidth;ctx.globalAlpha=alpha;
  ctx.lineJoin="round";ctx.stroke();ctx.globalAlpha=1;
};
const drawSpectrum=(values,color,fill)=>{
  if(!values?.length)return;
  const floor=-104,ceil=-14;
  ctx.beginPath();
  for(let i=0;i<values.length;i++){
    const f=F0*Math.pow(F1/F0,i/Math.max(1,values.length-1));
    const x=hzX(f,w), v=clamp((values[i]-floor)/(ceil-floor),0,1);
    // The analyzer dB mapping is independent from EQ gain axis.
    const y=h-(v*v*.83)*h;
    if(i===0)ctx.moveTo(x,y);else ctx.lineTo(x,y);
  }
  if(fill){
    ctx.lineTo(w,h);ctx.lineTo(0,h);ctx.closePath();
    const grad=ctx.createLinearGradient(0,0,0,h);
    grad.addColorStop(0,"rgba(90,196,163,.18)");
    grad.addColorStop(1,"rgba(66,134,117,.012)");
    ctx.fillStyle=grad;ctx.fill();
  }else{
    ctx.strokeStyle=color;ctx.lineWidth=.85;ctx.globalAlpha=.5;ctx.stroke();ctx.globalAlpha=1;
  }
};
const draw=()=>{
  if(w<2||h<2)return;
  ctx.save();ctx.setTransform(dpr,0,0,dpr,0,0);
  ctx.fillStyle="#181820";ctx.fillRect(0,0,w,h);
  const bg=ctx.createLinearGradient(0,0,0,h);
  bg.addColorStop(0,"rgba(29,34,43,.34)");
  bg.addColorStop(1,"rgba(7,14,21,.53)");
  ctx.fillStyle=bg;ctx.fillRect(0,0,w,h);
  ctx.font="10px Consolas,monospace";
  for(const f of [20,30,40,50,70,100,200,300,500,700,1000,2000,3000,5000,7000,10000,20000]){
    const x=hzX(f,w);
    ctx.strokeStyle=[20,100,1000,10000].includes(f)?"#38414b":"#293039";
    ctx.beginPath();ctx.moveTo(x,0);ctx.lineTo(x,h);ctx.lineWidth=.65;ctx.stroke();
    if([30,50,100,200,500,1000,2000,5000,10000].includes(f)){
      ctx.textAlign="center";ctx.fillStyle="#8d9ca9";ctx.fillText(formatHz(f),x,h-9);
    }
  }
  for(let db=-18;db<=18;db+=6){
    const y=dbY(db,h);ctx.beginPath();ctx.moveTo(0,y);ctx.lineTo(w,y);
    ctx.lineWidth=db===0?1.3:.65;
    ctx.strokeStyle=db===0?"#a28347":"#30353d";ctx.stroke();
    ctx.fillStyle=db===0?"#f0c76b":"#93a0ad";ctx.textAlign="right";
    ctx.fillText((db>0?"+":"")+db,w-8,clamp(y-7,22,h-22));
  }
  if(showSpectrum){drawSpectrum(spec.post,"#71cfb3",true);drawSpectrum(spec.pre,"#7897ab",false);}
  if(selected>=0&&bands[selected]?.enabled&&curves.band?.length){
    drawSmooth(curves.band,col(selected),1.3,true,.55);
  }
  drawSmooth(curves.l,"#70e9d0",2.0,true,.98);
  // Only show two channel response lines when they differ materially.
  const distinct=curves.l?.length===curves.r?.length&&curves.l?.some((v,i)=>Math.abs(v-curves.r[i])>.12);
  if(distinct)drawSmooth(curves.r,"#f1bf74",1.3,false,.87);
  // The physical crossfeed curve is hidden to avoid a misleading -108dB line.
  for(let i=0;i<MAX;i++){
    const b=bands[i];if(!b.enabled)continue;
    const x=hzX(b.freq,w),y=dbY(isCut(b.type)?0:b.gain,h);
    ctx.save();
    const on=selected===i||hovered===i;
    ctx.shadowBlur=on?15:6;ctx.shadowColor=col(i);
    ctx.fillStyle=col(i);ctx.beginPath();ctx.arc(x,y,on?6.5:5.5,0,Math.PI*2);ctx.fill();
    ctx.shadowBlur=0;ctx.lineWidth=on?1.5:1;
    ctx.strokeStyle=on?"#f8fbff":"#1d242d";
    ctx.beginPath();ctx.arc(x,y,on?9:7.5,0,Math.PI*2);ctx.stroke();
    // Color identifies the band; numbers belong in its floating inspector.
    if(on){
      ctx.font="10px Consolas,monospace";ctx.textAlign="center";
      ctx.fillStyle="#e9f5fa";ctx.fillText(String(i+1),x,y-13);
    }
    ctx.restore();
  }
  ctx.restore();
};
const resize=()=>{
  const r=wrap.getBoundingClientRect(),oldW=w,oldH=h;
  w=Math.max(1,r.width);h=Math.max(1,r.height);
  dpr=Math.min(2,window.devicePixelRatio||1);
  const cw=Math.max(1,Math.round(w*dpr)),ch=Math.max(1,Math.round(h*dpr));
  if(canvas.width!==cw||canvas.height!==ch){canvas.width=cw;canvas.height=ch;}
  if(oldW!==w||oldH!==h){positionInspector();changed();}
};
new ResizeObserver(resize).observe(wrap);
window.addEventListener("resize",resize);
function applyHostState(data){
  if(!data||!Array.isArray(data.bands)||data.bands.length!==MAX)return;
  connected=true;samplerate=isFiniteNumber(data.rate)?data.rate:samplerate;
  bands=data.bands.map((b,i)=>({
    id:i,enabled:!!b.enabled,type:clamp(b.type|0,0,5),route:clamp(b.route|0,0,4),
    freq:clamp(Number(b.freq)||1000,20,20000),gain:clamp(Number(b.gain)||0,-24,24),
    q:clamp(Number(b.q)||1,.1,24),slope:clamp(b.slope|0,0,3)
  }));
  if(dragging){ const b=bands[dragging.id];if(b){b.freq=dragging.freq;b.gain=dragging.gain;} }
  if(data.curve){curves=data.curve;}
  $("stateInfo").textContent="24 BANDS  /  "+(samplerate/1000).toFixed(1)+" kHz  /  ZERO LATENCY";
  $("bridgeStatus").textContent="NATIVE DSP CONNECTED";
  renderInspector();renderRail();changed();
}
function applySpectrum(data){
  if(!data||!Array.isArray(data.pre)||!Array.isArray(data.post))return;
  spec=data;hasSpectrum=true;if(showSpectrum)changed();
}
if(backend?.addEventListener){
  backend.addEventListener("eq-state",applyHostState);
  backend.addEventListener("eq-spectrum",applySpectrum);
  emit({kind:"ready"});
}else{
  $("bridgeStatus").textContent="WEB PREVIEW MODE — NO AUDIO ENGINE";
  curves.l=Array(256).fill(0);curves.r=Array(256).fill(0);
}
canvas.addEventListener("pointerdown",e=>{
  if(e.button!==0)return;
  const p=mapLocal(e);const id=bandHit(p.x,p.y);
  if(id<0){select(-1);return;}
  select(id);
  const b=bands[id];
  dragging={id,pointer:e.pointerId,freq:b.freq,gain:b.gain};
  canvas.setPointerCapture(e.pointerId);
  emit({kind:"begin",id,fields:["freq",...(!isCut(b.type)&&b.type!==5?["gain"]:[])]});
});
canvas.addEventListener("pointermove",e=>{
  const p=mapLocal(e); const f=xHz(p.x,w);
  $("hoverReadout").textContent=formatHz(f)+" Hz  •  "+noteFor(f)+"  •  "+number(yDb(p.y,h))+" dB";
  if(dragging && dragging.pointer===e.pointerId){
    dragging.freq=f;
    if(!isCut(bands[dragging.id].type)&&bands[dragging.id].type!==5) dragging.gain=yDb(p.y,h);
    pendingDrag={id:dragging.id,freq:dragging.freq,gain:dragging.gain};
    // At most one native command bundle per animation frame.
    if(!canvas.dataset.dragRaf){
      canvas.dataset.dragRaf="1";
      requestAnimationFrame(flushDrag);
    }
    changed();return;
  }
  const hit=bandHit(p.x,p.y);
  if(hit!==hovered){hovered=hit;changed();}
  const tooltip=$("graphTooltip");
  if(hit>=0){
    const b=bands[hit];
    tooltip.textContent="B"+(hit+1)+"  "+formatHz(b.freq)+" Hz  "+number(b.gain)+" dB  ·  Q "+number(b.q);
    tooltip.style.left=clamp(p.x+14,10,w-190)+"px";
    tooltip.style.top=clamp(p.y-50,8,h-45)+"px";
    tooltip.hidden=false;
  }else tooltip.hidden=true;
});
const flushDrag=()=>{
  canvas.dataset.dragRaf="";
  if(!pendingDrag)return;
  const v=pendingDrag;pendingDrag=null;
  setParam(v.id,"freq",v.freq,false);
  if(!isCut(bands[v.id].type)&&bands[v.id].type!==5)setParam(v.id,"gain",v.gain,false);
  renderRail();
};
const finishDrag=e=>{
  if(!dragging||dragging.pointer!==e.pointerId)return;
  // Always commit the final pointer location before closing the host gesture.
  flushDrag();
  const d=dragging;
  dragging=null;
  emit({kind:"end",id:d.id,fields:["freq",...(!isCut(bands[d.id].type)&&bands[d.id].type!==5?["gain"]:[])]});
  changed();
};
canvas.addEventListener("pointerup",finishDrag);
canvas.addEventListener("pointercancel",finishDrag);
canvas.addEventListener("pointerleave",()=>{hovered=-1;$("graphTooltip").hidden=true;changed();});
canvas.addEventListener("dblclick",e=>{
  const p=mapLocal(e),id=bandHit(p.x,p.y);
  if(id>=0){select(id);removeSelected();}
  else addBand(p.x,p.y);
});
canvas.addEventListener("wheel",e=>{
  const p=mapLocal(e),id=bandHit(p.x,p.y);
  if(id<0)return;
  e.preventDefault();
  const b=bands[id],direction=e.deltaY<0?1:-1;
  if(isCut(b.type))setParam(id,"slope",clamp(b.slope+direction,0,3));
  else setParam(id,"q",clamp(b.q*Math.pow(1.14,direction),.1,24));
  select(id);
},{passive:false});
const ctrl=(id,field,converter=Number)=>{
  $(id).addEventListener("change",()=>{
    if(selected<0)return;const v=converter($(id).value);
    if(Number.isFinite(v))setParam(selected,field,v);
  });
};
ctrl("typeSelect","type",Number);ctrl("slopeSelect","slope",Number);
ctrl("freqInput","freq",v=>clamp(Number(v),20,20000));
ctrl("gainInput","gain",v=>clamp(Number(v),-24,24));
ctrl("qInput","q",v=>clamp(Number(v),.1,24));
$("enableBtn").onclick=()=>{if(selected>=0)setParam(selected,"enabled",bands[selected].enabled?0:1);};
$("removeBtn").onclick=removeSelected;
$("routeGroup").addEventListener("click",e=>{
  const btn=e.target.closest("[data-route]");
  if(selected>=0&&btn)setParam(selected,"route",Number(btn.dataset.route));
});
$("viewBtn").onclick=()=>{
  showSpectrum=!showSpectrum;
  $("spectrumStatus").textContent=showSpectrum?"ON":"OFF";
  $("viewBtn").classList.toggle("active",showSpectrum);
  changed();
};
$("bandsBtn").onclick=()=>{
  $("quickHelp").hidden=true;
  $("bandList").hidden=!$("bandList").hidden;
};
$("helpBtn").onclick=()=>{
  $("bandList").hidden=true;
  $("quickHelp").hidden=!$("quickHelp").hidden;
};
$("closeInspector").onclick=()=>select(-1);
window.addEventListener("keydown",e=>{
  if(e.target.matches("input,select"))return;
  if((e.key==="Delete"||e.key==="Backspace")&&selected>=0){
    e.preventDefault();removeSelected();
  }
});
resize();renderInspector();renderRail();changed();
})();