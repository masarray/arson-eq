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
const F0=20,F1=20000,MAX=24,RIGHT_RAIL=63;
let rangeDb=12;
const plotWidth=width=>Math.max(1,width-RIGHT_RAIL);
const clamp=(x,a,b)=>Math.max(a,Math.min(b,x));
const hzX=(f,width)=>Math.log(clamp(f,F0,F1)/F0)/Math.log(F1/F0)*plotWidth(width);
const xHz=(x,width)=>F0*Math.pow(F1/F0,clamp(x/plotWidth(width),0,1));
const dbY=(v,h)=>h*0.5-clamp(v,-rangeDb,rangeDb)/rangeDb*h*0.5;
const yDb=(y,h)=>clamp((h*0.5-y)/(h*0.5)*rangeDb,-rangeDb,rangeDb);
const isCut=t=>t===3||t===4;
const isFiniteNumber=n=>typeof n==="number"&&Number.isFinite(n);
const noteFor=hz=>{if(!isFiniteNumber(hz)||hz<=0)return "—";const midi=Math.round(69+12*Math.log2(hz/440));return ["C","C♯","D","D♯","E","F","F♯","G","G♯","A","A♯","B"][(midi+1200)%12]+(Math.floor(midi/12)-1);};
const backend=window.__JUCE__?.backend;
let bands=Array.from({length:MAX},(_,i)=>({id:i,enabled:false,type:0,route:0,freq:1000,gain:0,q:1,slope:0}));
let curves={l:[],r:[],cross:[],band:[]},spec={pre:[],post:[],range:90,peakL:0,peakR:0,preVisible:true,postVisible:true},samplerate=48000;
let selected=-1,hovered=-1,showSpectrum=true,dragging=null,pendingDrag=null,lastValidPick=null;
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
  $("gainKnob").style.opacity=$("gainInput").disabled?".35":"1";
  $("slopeSelect").disabled=!isCut(b.type);
  $("noteLabel").textContent=noteFor(b.freq);
  $("typeGlyph").textContent=["⌒","◡","◠","╱","╲","⌄"][b.type]||"⌒";
  const knobAngle=(v,min,max,log=false)=>{
    const t=log?(Math.log(v/min)/Math.log(max/min)):(v-min)/(max-min);
    return (-140+280*clamp(t,0,1)).toFixed(1)+"deg";
  };
  $("freqKnob").style.setProperty("--angle",knobAngle(b.freq,20,20000,true));
  $("gainKnob").style.setProperty("--angle",knobAngle(b.gain,-24,24));
  $("qKnob").style.setProperty("--angle",knobAngle(b.q,.1,24,true));
  $("routeGroup").querySelectorAll("button").forEach(btn=>
    btn.classList.toggle("active",Number(btn.dataset.route)===b.route));
  // Dock is a flex sibling of the graph, never an absolute canvas overlay.
};
const mapLocal=e=>{const r=canvas.getBoundingClientRect();return{x:clamp(e.clientX-r.left,0,plotWidth(r.width)),y:clamp(e.clientY-r.top,0,r.height)};};
// Canvas renderer: only native DSP response data are graphed. No JS biquads.
const responsePath=arr=>{
  const pathWidth=plotWidth(w);
  if(!arr||arr.length<2)return false;
  ctx.beginPath();
  for(let i=0;i<arr.length;i++){
    const x=pathWidth*i/(arr.length-1),y=dbY(arr[i],h);
    if(i===0)ctx.moveTo(x,y);else ctx.lineTo(x,y);
  }
  return true;
};
const drawResponse=()=>{
  const arr=curves.l;
  if(!arr?.length||arr.length<2)return;
  const baseline=h*.5,pw=plotWidth(w);
  // Boost and attenuation each get an independently clipped fill.
  // The underlying music spectrum remains visible through both layers.
  const fillHalf=(upper)=>{
    ctx.save();
    ctx.beginPath();
    if(upper){ctx.moveTo(0,0);ctx.lineTo(pw,0);ctx.lineTo(pw,baseline);ctx.lineTo(0,baseline);}
    else {ctx.moveTo(0,baseline);ctx.lineTo(pw,baseline);ctx.lineTo(pw,h);ctx.lineTo(0,h);}
    ctx.closePath();ctx.clip();
    if(responsePath(arr)){
      ctx.lineTo(pw,baseline);ctx.lineTo(0,baseline);ctx.closePath();
      const grad=ctx.createLinearGradient(0,upper?0:baseline,0,upper?baseline:h);
      if(upper){
        grad.addColorStop(0,"rgba(102,218,123,.24)");
        grad.addColorStop(1,"rgba(64,185,103,.06)");
      }else{
        grad.addColorStop(0,"rgba(171,120,59,.13)");
        grad.addColorStop(1,"rgba(160,79,79,.23)");
      }
      ctx.fillStyle=grad;ctx.fill();
    }
    ctx.restore();
  };
  fillHalf(true);fillHalf(false);
  ctx.lineWidth=2.1;ctx.strokeStyle="#f0cc6b";ctx.lineJoin="round";
  ctx.shadowBlur=3;ctx.shadowColor="#ecb84f69";
  if(responsePath(arr))ctx.stroke();
  ctx.shadowBlur=0;
  const differ=curves.l?.length===curves.r?.length &&
      curves.l?.some((v,i)=>Math.abs(v-curves.r[i])>.15);
  if(differ){
    ctx.strokeStyle="#75d7d7";ctx.lineWidth=1.2;
    if(responsePath(curves.r))ctx.stroke();
  }
};
const drawSelectedBand=()=>{
  if(selected<0||!bands[selected]?.enabled||!curves.band?.length)return;
  if(responsePath(curves.band)){
    ctx.strokeStyle=col(selected);ctx.globalAlpha=.78;
    ctx.lineWidth=1.2;ctx.stroke();ctx.globalAlpha=1;
  }
};
// Spectrum values are real C++ FFT dBFS with visualization-only tilt.
const spectrumY=value=>{
  const range=spec.range===60||spec.range===120?spec.range:90;
  return h*(1-clamp(value/range+1,0,1));
};
const spectrumPath=values=>{
  if(!values?.length)return false;
  ctx.beginPath();
  const pw=plotWidth(w);
  for(let i=0;i<values.length;++i){
    const x=pw*i/(values.length-1),y=spectrumY(values[i]);
    if(i===0)ctx.moveTo(x,y);else ctx.lineTo(x,y);
  }
  return true;
};
const drawSpectrum=(values,fill)=>{
  if(!spectrumPath(values))return;
  const pw=plotWidth(w);
  if(fill){
    ctx.lineTo(pw,h);ctx.lineTo(0,h);ctx.closePath();
    const grad=ctx.createLinearGradient(0,0,0,h);
    grad.addColorStop(0,"rgba(200,200,211,.28)");
    grad.addColorStop(.52,"rgba(158,160,171,.20)");
    grad.addColorStop(1,"rgba(88,92,106,.11)");
    ctx.fillStyle=grad;ctx.fill();
  }
  if(spectrumPath(values)){
    ctx.strokeStyle=fill?"#c3c4cc88":"#959dad9c";
    ctx.lineWidth=fill?1.25:.9;ctx.stroke();
  }
};
const drawMeters=()=>{
  const right=plotWidth(w),x=right+9,trackH=h-38,yTop=12,barW=9;
  const peak=[spec.peakL||0,spec.peakR||0];
  for(let channel=0;channel<2;++channel){
    const bx=x+channel*12;
    ctx.fillStyle="#101822";ctx.fillRect(bx,yTop,barW,trackH);
    const level=clamp((20*Math.log10(Math.max(peak[channel],1.e-8))+90)/90,0,1);
    const height=trackH*level;
    const grad=ctx.createLinearGradient(0,yTop+trackH,0,yTop);
    grad.addColorStop(0,"#4acfa3");grad.addColorStop(.80,"#dfca61");grad.addColorStop(1,"#e66664");
    ctx.fillStyle=grad;ctx.fillRect(bx,yTop+trackH-height,barW,height);
    ctx.fillStyle="#a5b4c0";ctx.font="9px Consolas,monospace";ctx.textAlign="center";
    ctx.fillText(channel===0?"L":"R",bx+barW/2,h-7);
  }
};
const draw=()=>{
  if(w<2||h<2)return;
  const pw=plotWidth(w);
  ctx.save();ctx.setTransform(dpr,0,0,dpr,0,0);
  ctx.fillStyle="#17151c";ctx.fillRect(0,0,w,h);
  const ground=ctx.createLinearGradient(0,0,0,h);
  ground.addColorStop(0,"#211d27");ground.addColorStop(.55,"#1d1b22");ground.addColorStop(1,"#151a24");
  ctx.fillStyle=ground;ctx.fillRect(0,0,pw,h);
  ctx.fillStyle="#0f141c";ctx.fillRect(pw,0,w-pw,h);
  ctx.font="10px Consolas,monospace";
  for(const hz of [20,30,40,50,70,100,200,300,500,700,1000,2000,3000,5000,7000,10000,20000]){
    const x=hzX(hz,w);
    const major=[20,100,1000,10000].includes(hz);
    ctx.beginPath();ctx.moveTo(x,0);ctx.lineTo(x,h);
    ctx.lineWidth=major?.9:.5;ctx.strokeStyle=major?"#403a46":"#302d36";ctx.stroke();
    if([30,50,100,200,500,1000,2000,5000,10000,20000].includes(hz)){
      ctx.textAlign="center";ctx.fillStyle="#98919e";
      ctx.fillText(formatHz(hz),clamp(x,18,pw-35),h-8);
    }
  }
  const tickStep=rangeDb<=15?3:6;
  for(let db=-rangeDb;db<=rangeDb;db+=tickStep){
    const y=dbY(db,h);
    ctx.beginPath();ctx.moveTo(0,y);ctx.lineTo(pw,y);
    ctx.lineWidth=db===0?1.25:.65;
    ctx.strokeStyle=db===0?"#987b4b":"#37313b";ctx.stroke();
    ctx.textAlign="right";ctx.font="10px Consolas,monospace";
    ctx.fillStyle=db===0?"#f0c86d":"#a9a0a8";
    ctx.fillText((db>0?"+":"")+db,pw-6,clamp(y+3,13,h-17));
  }
  // Right meter and analyzer level reference, physically separate from EQ dB gain axis.
  const range=spec.range===60||spec.range===120?spec.range:90;
  ctx.beginPath();ctx.moveTo(pw+.5,0);ctx.lineTo(pw+.5,h);
  ctx.strokeStyle="#4a444d";ctx.stroke();
  ctx.textAlign="right";ctx.font="9px Consolas,monospace";
  for(let level=0;level>=-range;level-=20){
    const y=h*(-level)/range;
    ctx.fillStyle="#838c98";ctx.fillText(String(level),w-5,clamp(y+5,13,h-22));
  }
  if(showSpectrum){
    if(spec.postVisible!==false){ctx.globalAlpha=.89;drawSpectrum(spec.post,true);}
    if(spec.preVisible!==false){ctx.globalAlpha=.72;drawSpectrum(spec.pre,false);}
    ctx.globalAlpha=1;
  }
  drawMeters();
  drawSelectedBand();
  drawResponse();
  for(let i=0;i<MAX;++i){
    const b=bands[i];if(!b.enabled)continue;
    const x=hzX(b.freq,w),y=dbY(isCut(b.type)?0:b.gain,h);
    const focus=selected===i||hovered===i;
    ctx.save();
    ctx.shadowBlur=focus?9:3;ctx.shadowColor=col(i);
    ctx.beginPath();ctx.arc(x,y,focus?6.2:5.4,0,Math.PI*2);
    ctx.fillStyle=col(i);ctx.fill();
    ctx.shadowBlur=0;ctx.lineWidth=focus?2:1.5;
    ctx.strokeStyle="#e7e3df";ctx.stroke();
    ctx.beginPath();ctx.arc(x,y,focus?8.8:7.3,0,Math.PI*2);
    ctx.strokeStyle=focus?"#e9e6df91":"#ffffff20";ctx.lineWidth=.9;ctx.stroke();
    if(focus){
      ctx.textAlign="center";ctx.font="10px Consolas,monospace";
      ctx.fillStyle="#f0ece8";ctx.fillText(String(i+1),x,Math.max(19,y-16));
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
  if(oldW!==w||oldH!==h)changed();
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
  if(data.curve)curves=data.curve;
  // Match Pro-EQ style 12 dB default range, but expand if actual host gain
  // exceeds 12 dB so handles/response are never silently clipped.
  const peak=Math.max(0,...bands.filter(b=>b.enabled).map(b=>Math.abs(b.gain)),
    ...(curves.l||[]).map(Math.abs),...(curves.r||[]).map(Math.abs));
  rangeDb=clamp(Math.ceil((peak+1)/3)*3,12,24);
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
  lastValidPick={id,x:p.x,y:p.y,at:Date.now()};
  // Opening the bottom dock resizes the graph; defer selection until
  // release so its coordinate system cannot jump DURING the pointer drag.
  const b=bands[id];
  dragging={id,pointer:e.pointerId,freq:b.freq,gain:b.gain,selectOnUp:selected!==id};
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
  if(d.selectOnUp)select(d.id);
  changed();
};
canvas.addEventListener("pointerup",finishDrag);
canvas.addEventListener("pointercancel",finishDrag);
canvas.addEventListener("pointerleave",()=>{hovered=-1;$("graphTooltip").hidden=true;changed();});
canvas.addEventListener("dblclick",e=>{
  const p=mapLocal(e);
  let id=bandHit(p.x,p.y);
  // 2nd click can land off-node after the first click reveals the dock.
  // Preserve a recent first hit only for the SAME small pointer location.
  if(id<0 && lastValidPick && Date.now()-lastValidPick.at<550 &&
     Math.hypot(p.x-lastValidPick.x,p.y-lastValidPick.y)<20)
    id=lastValidPick.id;
  lastValidPick=null;
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
// M2.4 analyzer controls are only editor telemetry; no host audio parameters altered.
const analyzerFields=[["fftSelect","fft"],["speedSelect","speed"],["tiltSelect","tilt"],["rangeSelect","range"]];
const emitSetting=(field,value)=>emit({kind:"analyzerSettings",field,value});
for(const [id,field] of analyzerFields)$(id).addEventListener("change",()=>{
  const value=Number($(id).value);
  if(Number.isFinite(value))emitSetting(field,value);
});
for(const [id,field] of [["showPre","pre"],["showPost","post"],["freezeSpectrum","freeze"]])
  $(id).addEventListener("change",()=>emitSetting(field,!!$(id).checked));
$("analyzerMenuBtn").onclick=()=>{
  $("quickHelp").hidden=true;$("bandList").hidden=true;
  $("analyzerMenu").hidden=!$("analyzerMenu").hidden;
};
$("advancedBtn").onclick=()=>{
  const expanded=$("inspector").classList.toggle("expanded");
  $("advancedBtn").textContent=expanded?"LESS ▴":"MORE ▾";
  $("advancedBtn").setAttribute("aria-expanded",String(expanded));
};
$("viewBtn").onclick=()=>{
  showSpectrum=!showSpectrum;
  $("spectrumStatus").textContent=showSpectrum?"ON":"OFF";
  $("viewBtn").classList.toggle("active",showSpectrum);
  emit({kind:"spectrum",enabled:showSpectrum});
  changed();
};
$("bandsBtn").onclick=()=>{
  $("quickHelp").hidden=true;$("analyzerMenu").hidden=true;
  $("bandList").hidden=!$("bandList").hidden;
};
$("helpBtn").onclick=()=>{
  $("bandList").hidden=true;$("analyzerMenu").hidden=true;
  $("quickHelp").hidden=!$("quickHelp").hidden;
};
$("closeInspector").onclick=()=>select(-1);
const attachKnob=(id,field)=>{
  const knob=$(id),lo=field==="freq"?20:field==="gain"?-24:.1;
  const hi=field==="freq"?20000:field==="gain"?24:24;
  let gesture=null;
  knob.addEventListener("pointerdown",e=>{
    if(selected<0||e.button!==0||(field==="gain"&&$("gainInput").disabled))return;
    e.preventDefault();
    gesture={pointer:e.pointerId,id:selected,startY:e.clientY,start:bands[selected][field]};
    knob.setPointerCapture(e.pointerId);
    emit({kind:"begin",id:selected,fields:[field]});
  });
  knob.addEventListener("pointermove",e=>{
    if(!gesture||gesture.pointer!==e.pointerId)return;
    const dy=gesture.startY-e.clientY;
    const value=field==="gain"?gesture.start+dy*.12:
      gesture.start*Math.exp(dy*.012);
    setParam(gesture.id,field,clamp(value,lo,hi),false);
  });
  const end=e=>{
    if(!gesture||gesture.pointer!==e.pointerId)return;
    emit({kind:"end",id:gesture.id,fields:[field]});
    gesture=null;
  };
  knob.addEventListener("pointerup",end);
  knob.addEventListener("pointercancel",end);
  knob.addEventListener("lostpointercapture",end);
};
attachKnob("freqKnob","freq");
attachKnob("gainKnob","gain");
attachKnob("qKnob","q");
window.addEventListener("keydown",e=>{
  if(e.target.matches("input,select"))return;
  if((e.key==="Delete"||e.key==="Backspace")&&selected>=0){
    e.preventDefault();removeSelected();
  }
});
resize();renderInspector();renderRail();changed();
})();