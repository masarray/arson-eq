/* ArSonEQ Studio web canvas. Adapted interaction vocabulary from user's MaSonKuPik EQ.
 * WebView contains NO audio DSP, no Web Audio, no libraries, no network requests.
 * Backend owns authoritative host parameters and transfer functions.
 */
(() => {
"use strict";
const $=id=>document.getElementById(id);
const canvas=$("eqCanvas"), wrap=$("graphWrap"), ctx=canvas.getContext("2d",{alpha:false});
const TYPES=["Bell","Low Shelf","High Shelf","Low Cut","High Cut","Notch","Band Pass","Tilt Shelf","Flat Tilt"];
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
const hasGain=t=>!isCut(t)&&t!==5&&t!==6;
const isFiniteNumber=n=>typeof n==="number"&&Number.isFinite(n);
const noteFor=hz=>{if(!isFiniteNumber(hz)||hz<=0)return "—";const midi=Math.round(69+12*Math.log2(hz/440));return ["C","C♯","D","D♯","E","F","F♯","G","G♯","A","A♯","B"][(midi+1200)%12]+(Math.floor(midi/12)-1);};
const backend=window.__JUCE__?.backend;
let bands=Array.from({length:MAX},(_,i)=>({id:i,enabled:false,type:0,route:0,freq:1000,gain:0,q:1,slope:0,dynRange:0,dynThreshold:-24}));
let presetNames=[],activePreset=-1,presetBusy=false;
let dynamicState={active:false,l:[],r:[],activity:Array(24).fill(0),gainDb:Array(24).fill(0)};
let curves={l:[],r:[],cross:[],band:[]},spec={pre:[],post:[],range:90,peakL:0,peakR:0,preVisible:true,postVisible:true},samplerate=48000;
let selected=-1,hovered=-1,showSpectrum=true,dragging=null,pendingDrag=null,lastValidPick=null;
const selection=new Set(),history=[],redoHistory=[];
const FIELDS=["enabled","type","route","freq","gain","q","slope","dynRange","dynThreshold"];
let editSnapshot=null,historyReplay=false;
const snapshot=()=>bands.map(b=>FIELDS.map(f=>b[f]));
const refreshUndo=()=>{$("undoBtn").disabled=!history.length;$("redoBtn").disabled=!redoHistory.length;};
const beginEdit=()=>{if(!editSnapshot&&!historyReplay)editSnapshot=snapshot();};
const finishEdit=()=>{
  if(!editSnapshot)return;
  const after=snapshot(),before=editSnapshot;editSnapshot=null;
  if(after.some((v,i)=>v.some((x,j)=>x!==before[i][j]))){
    history.push({before,after});if(history.length>64)history.shift();
    redoHistory.length=0;
  }
  refreshUndo();
};
const undoRedo=undo=>{
  if(dragging||editSnapshot)return;
  const src=undo?history:redoHistory,dst=undo?redoHistory:history;
  const entry=src.pop();if(!entry)return;
  const values=undo?entry.before:entry.after,changes=[];
  historyReplay=true;
  values.forEach((row,id)=>row.forEach((value,k)=>{
    const field=FIELDS[k];
    if(bands[id][field]!==value){bands[id][field]=value;changes.push({id,field,value});}
  }));
  historyReplay=false;
  if(changes.length)emit({kind:"bulkEdit",entries:changes,gesture:true});
  dst.push(entry);refreshUndo();renderInspector();renderRail();placeInspector();changed();
};
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
  if(!Number.isFinite(value)||id<0||id>=MAX||!FIELDS.includes(field))return;
  if(bands[id][field]===value)return;
  if(gesture)beginEdit();
  updateBandLocal(id,field,value);
  if(activePreset>=0){activePreset=-1;$("presetSelect").value="-1";}
  emit({kind:"edit",id,field,value,gesture});
  if(gesture)finishEdit();
};
const placeInspector=()=>{
  const card=$("inspector");
  if(card.hidden||selected<0)return;
  const height=card.classList.contains("expanded")?Math.min(142,h-40):70;
  const width=Math.min(484,Math.max(270,plotWidth(w)-28));
  const b=bands[selected],x=hzX(b.freq,w),y=dbY(isCut(b.type)?0:b.gain,h);
  let left=clamp(x-width*.5,12,Math.max(12,plotWidth(w)-width-12));
  let top=h-height-13;
  if(y>=top-20){
    if(y-height-27>=35)top=y-height-27;
    else if(y+height+27<h-13)top=y+27;
    else left=x<plotWidth(w)*.5?Math.max(12,plotWidth(w)-width-12):12;
  }
  card.style.left=Math.round(left)+"px";
  card.style.top=Math.round(clamp(top,32,Math.max(32,h-height-8)))+"px";
  card.style.bottom="auto";
};
const select=(i,additive=false)=>{
  const id=(i>=0&&i<MAX)?i:-1;
  if(additive&&id>=0){
    if(selection.has(id))selection.delete(id);else selection.add(id);
    selected=selection.size?[...selection].at(-1):-1;
  }else{selected=id;selection.clear();if(id>=0)selection.add(id);}
  $("bandList").hidden=true;$("quickHelp").hidden=true;
  emit({kind:"select",id:selected});
  renderInspector();renderRail();placeInspector();changed();
};
const addBand=(x,y)=>{
  const idx=bands.findIndex(b=>!b.enabled);
  if(idx<0)return;
  beginEdit();
  const freq=xHz(x,w),gain=yDb(y,h);
  bands[idx]={...bands[idx],enabled:true,type:0,route:0,freq,gain,q:1,slope:0,dynRange:0,dynThreshold:-24};
  emit({kind:"add",id:idx,freq,gain});
  select(idx);finishEdit();
};
const removeSelected=()=>{
  if(selected<0)return;
  beginEdit();
  const entries=[...selection].filter(id=>bands[id]?.enabled).map(id=>({id,field:"enabled",value:0}));
  for(const e of entries)bands[e.id].enabled=false;
  if(entries.length)emit({kind:"bulkEdit",entries,gesture:true});
  select(-1);finishEdit();
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
    chip.textContent=(selection.has(i)?"◆ ":"")+String(i+1).padStart(2,"0")+" · "+formatHz(bands[i].freq);
    chip.onclick=e=>select(i,!!(e.ctrlKey||e.metaKey));rail.appendChild(chip);
  }
};
const renderInspector=()=>{
  const b=selected>=0?bands[selected]:null;
  $("inspector").hidden=!b;
  $("emptyInspector").hidden=true;
  $("bandInspector").hidden=!b;
  if(!b)return;
  $("activeLabel").textContent=["STEREO","LEFT","RIGHT","MID","SIDE"][b.route]||"STEREO";
  $("bandTitle").textContent=(selection.size>1?selection.size+" BANDS · ":"BAND ")+
    String(selected+1).padStart(2,"0");
  $("bandDot").style.background=col(selected);
  $("enableBtn").textContent=b.enabled?"● ON":"○ OFF";
  $("enableBtn").classList.toggle("off",!b.enabled);
  $("typeSelect").value=String(b.type);
  $("typeButton").textContent=TYPES[b.type]||"Bell";
  $("slopeSelect").value=String(b.slope);
  const setValue=(id,v)=>{const input=$(id);if(document.activeElement!==input)input.value=String(v);};
  setValue("freqInput",Math.round(b.freq*10)/10);
  setValue("gainInput",Math.round(b.gain*100)/100);
  setValue("qInput",Math.round(b.q*100)/100);
  $("gainInput").disabled=!hasGain(b.type);
  $("gainKnob").style.opacity=$("gainInput").disabled?".35":"1";
  $("slopeSelect").disabled=!isCut(b.type);
  $("noteLabel").textContent=noteFor(b.freq);
  $("typeGlyph").textContent=["⌒","◡","◠","╱","╲","⌄","◇","⇅","↕"][b.type]||"⌒";
  $("shapePicker").querySelectorAll("button").forEach(btn=>
    btn.classList.toggle("active",Number(btn.dataset.shape)===b.type));
  const dynamicOK=b.type===0||b.type===1||b.type===2;
  $("dynRangeInput").disabled=!dynamicOK;
  $("dynThresholdInput").disabled=!dynamicOK;
  setValue("dynRangeInput",Math.round((b.dynRange||0)*10)/10);
  setValue("dynThresholdInput",Math.round((b.dynThreshold??-24)*10)/10);
  updateDynamicMeter();
  const knobAngle=(v,min,max,log=false)=>{
    const t=log?(Math.log(v/min)/Math.log(max/min)):(v-min)/(max-min);
    return (-140+280*clamp(t,0,1)).toFixed(1)+"deg";
  };
  $("freqKnob").style.setProperty("--angle",knobAngle(b.freq,20,20000,true));
  $("gainKnob").style.setProperty("--angle",knobAngle(b.gain,-24,24));
  $("qKnob").style.setProperty("--angle",knobAngle(b.q,.1,24,true));
  $("routeGroup").querySelectorAll("button").forEach(btn=>
    btn.classList.toggle("active",Number(btn.dataset.route)===b.route));
  // Inspector is a fixed-geometry compositor overlay, not a flex child.
};
const updateDynamicMeter=()=>{
  const b=selected>=0?bands[selected]:null;
  if(!b)return;
  const eligible=(b.type===0||b.type===1||b.type===2)&&Math.abs(b.dynRange)>0.01;
  const gain=eligible?(dynamicState.gainDb?.[selected]||0):0;
  const activity=eligible?(dynamicState.activity?.[selected]||0):0;
  $("dynStatus").textContent=eligible?(gain>0?"+":"")+gain.toFixed(1)+" dB":"OFF";
  $("dynStatus").classList.toggle("active",eligible&&activity>0.015);
  $("dynMeterFill").style.width=(100*clamp(activity,0,1)).toFixed(1)+"%";
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
  const staticCurve=curves.l;
  if(!staticCurve?.length||staticCurve.length<2)return;
  const live=dynamicState.active && dynamicState.l?.length===staticCurve.length;
  const arr=live?dynamicState.l:staticCurve;
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
  if(live){
    // Static reference is deliberately subdued; live effective IIR response
    // comes from native DSP activity (no fake sound-reactive animation).
    if(responsePath(staticCurve)){
      ctx.strokeStyle="#c2a4788f";ctx.lineWidth=1.05;ctx.stroke();
    }
    const pw=plotWidth(w);
    ctx.beginPath();
    arr.forEach((v,i)=>{const x=i*pw/(arr.length-1),y=dbY(v,h);
      if(!i)ctx.moveTo(x,y);else ctx.lineTo(x,y);});
    for(let i=staticCurve.length-1;i>=0;--i){
      ctx.lineTo(i*pw/(staticCurve.length-1),dbY(staticCurve[i],h));
    }
    ctx.closePath();ctx.fillStyle="rgba(161,90,202,.27)";ctx.fill();
  }
  ctx.lineWidth=2.1;ctx.strokeStyle=live?"#f4d57d":"#f0cc6b";ctx.lineJoin="round";
  ctx.shadowBlur=3;ctx.shadowColor="#ecb84f69";
  if(responsePath(arr))ctx.stroke();
  ctx.shadowBlur=0;
  const rightCurve=live?dynamicState.r:curves.r;
  const differ=arr?.length===rightCurve?.length &&
    arr.some((v,i)=>Math.abs(v-rightCurve[i])>.15);
  if(differ){
    ctx.strokeStyle="#75d7d7";ctx.lineWidth=1.2;
    if(responsePath(rightCurve))ctx.stroke();
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
    const wet=dynamicState.active?(dynamicState.activity?.[i]||0):0;
    const dynamicDelta=(b.type===0||b.type===1||b.type===2)&&Math.abs(b.dynRange)>0.01?
      (dynamicState.gainDb?.[i]||0):0;
    const focus=selection.has(i)||hovered===i;
    ctx.save();
    // Animated dynamic marker is separate from the STATIC draggable handle,
    // so clicking it never moves the band or changes graph coordinates.
    if(Math.abs(dynamicDelta)>0.08){
      const activeY=dbY(b.gain+dynamicDelta,h);
      ctx.beginPath();ctx.moveTo(x,y);ctx.lineTo(x,activeY);
      ctx.strokeStyle=col(i);ctx.lineWidth=2.0;ctx.globalAlpha=.8;ctx.stroke();
      ctx.beginPath();ctx.arc(x,activeY,3.3+wet*1.4,0,Math.PI*2);
      ctx.fillStyle="#f8d77e";ctx.fill();ctx.globalAlpha=1;
    }
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
  if(oldW!==w||oldH!==h){placeInspector();changed();}
};
new ResizeObserver(resize).observe(wrap);
window.addEventListener("resize",resize);
const updatePresetMenu=()=>{
  const menu=$("presetMenu");
  if(!menu||menu.children.length===presetNames.length+1){
    if(menu)for(let i=0;i<menu.children.length;++i)
      menu.children[i].classList.toggle("selected",Number(menu.children[i].dataset.index)===activePreset);
    return;
  }
  menu.replaceChildren();
  const all=["Custom / Init",...presetNames];
  all.forEach((name,pos)=>{
    const index=pos-1,button=document.createElement("button");
    button.type="button";button.dataset.index=String(index);
    button.textContent=name;
    button.classList.toggle("selected",index===activePreset);
    button.onclick=()=>{
      hidePresetMenu();
      if(index>=0)presets.load(index);
    };
    menu.appendChild(button);
  });
};
const hidePresetMenu=()=>{
  $("presetMenu").hidden=true;
  $("presetButton").setAttribute("aria-expanded","false");
};
const hideTypeMenu=()=>{
  $("typeMenu").hidden=true;
  $("typeButton").setAttribute("aria-expanded","false");
};
const hideExtraMenus=()=>{hidePresetMenu();hideTypeMenu();};
function applyHostState(data){
  if(!data||!Array.isArray(data.bands)||data.bands.length!==MAX)return;
  connected=true;samplerate=isFiniteNumber(data.rate)?data.rate:samplerate;
  bands=data.bands.map((b,i)=>({
    id:i,enabled:!!b.enabled,type:clamp(b.type|0,0,8),route:clamp(b.route|0,0,4),
    freq:clamp(Number(b.freq)||1000,20,20000),gain:clamp(Number(b.gain)||0,-24,24),
    q:clamp(Number(b.q)||1,.1,24),slope:clamp(b.slope|0,0,3),
    dynRange:clamp(Number(b.dynRange)||0,-12,12),
    dynThreshold:clamp(Number(b.dynThreshold??-24),-60,0)
  }));
  if(dragging)for(const [id,v] of dragging.values){
    if(bands[id]){bands[id].freq=v.freq;bands[id].gain=v.gain;}
  }
  if(data.curve)curves=data.curve;
  // Invalidates stale live response while C++ detector/filter topology changes.
  dynamicState={...dynamicState,active:false,l:[],r:[]};
  // Match Pro-EQ style 12 dB default range, but expand if actual host gain
  // exceeds 12 dB so handles/response are never silently clipped.
  const peak=Math.max(0,...bands.filter(b=>b.enabled).map(b=>Math.abs(b.gain)),
    ...(curves.l||[]).map(Math.abs),...(curves.r||[]).map(Math.abs));
  rangeDb=clamp(Math.ceil((peak+1)/3)*3,12,24);
  $("stateInfo").textContent="24 BANDS / "+(samplerate/1000).toFixed(1)+" kHz / ZERO LATENCY";
  $("bridgeStatus").textContent="NATIVE DSP CONNECTED";
  if(Array.isArray(data.presets)&&data.presets.length){
    presetNames=data.presets.map(String);
    if($("presetSelect").options.length!==presetNames.length+1){
      $("presetSelect").replaceChildren();
      const custom=document.createElement("option");custom.value="-1";custom.textContent="Custom / Init";
      $("presetSelect").appendChild(custom);
      presetNames.forEach((name,index)=>{
        const option=document.createElement("option");
        option.value=String(index);option.textContent=name;
        $("presetSelect").appendChild(option);
      });
    }
  }
  activePreset=Number.isInteger(data.presetIndex)?data.presetIndex:-1;
  $("presetSelect").value=String(activePreset);
  $("presetButton").textContent=activePreset>=0?(presetNames[activePreset]||"Custom / Init"):"Custom / Init";
  updatePresetMenu();
  renderInspector();renderRail();if(!dragging)placeInspector();changed();
}
function applyDynamics(data){
  if(!data||!Array.isArray(data.activity)||data.activity.length!==MAX)return;
  if(data.active && (!Array.isArray(data.l)||!Array.isArray(data.r)))return;
  dynamicState={active:!!data.active,
    l: data.active?data.l:[],r:data.active?data.r:[],
    activity:data.activity.map(v=>clamp(Number(v)||0,0,1)),
    gainDb:Array.isArray(data.gainDb)?data.gainDb.map(v=>Number(v)||0):Array(MAX).fill(0)};
  updateDynamicMeter();changed();
}
function applySpectrum(data){
  if(!data||!Array.isArray(data.pre)||!Array.isArray(data.post))return;
  spec=data;hasSpectrum=true;if(showSpectrum)changed();
}
if(backend?.addEventListener){
  backend.addEventListener("eq-state",applyHostState);
  backend.addEventListener("eq-spectrum",applySpectrum);
  backend.addEventListener("eq-dynamics",applyDynamics);
  emit({kind:"ready"});
}else{
  $("bridgeStatus").textContent="WEB PREVIEW MODE — NO AUDIO ENGINE";
  curves.l=Array(256).fill(0);curves.r=Array(256).fill(0);
}
// Native-style right click: actions operate on real host parameters.
const hideContext=()=>{$("bandContextMenu").hidden=true;};
canvas.addEventListener("contextmenu",e=>{
  e.preventDefault();
  const p=mapLocal(e),id=bandHit(p.x,p.y);
  if(id>=0&&!selection.has(id))select(id);
  const menu=$("bandContextMenu");
  const rect={width:184,height:185};
  menu.style.left=clamp(p.x,8,Math.max(8,plotWidth(w)-rect.width-8))+"px";
  menu.style.top=clamp(p.y,28,Math.max(28,h-rect.height-8))+"px";
  menu.dataset.x=String(p.x);menu.dataset.y=String(p.y);
  menu.querySelectorAll("button").forEach(btn=>{
    if(btn.dataset.eqMenu==="add")btn.hidden=id>=0;
    else if(["edit","toggle","remove"].includes(btn.dataset.eqMenu))btn.hidden=id<0;
  });
  menu.hidden=false;
});
$("bandContextMenu").addEventListener("click",e=>{
  const action=e.target.closest("[data-eq-menu]")?.dataset.eqMenu;
  if(!action)return;
  const menu=$("bandContextMenu");
  if(action==="edit"&&selected>=0){renderInspector();placeInspector();}
  else if(action==="dynamic"&&selected>=0){
    const b=bands[selected];
    if(b.type===0||b.type===1||b.type===2){
      setParam(selected,"dynRange",Math.abs(b.dynRange)>0.01?0:-6);
      $("inspector").classList.add("expanded");
      $("advancedBtn").textContent="LESS ▴";placeInspector();
    }
  }
  else if(action==="toggle"&&selected>=0){
    beginEdit();
    const entries=[...selection].map(id=>({id,field:"enabled",value:bands[id].enabled?0:1}));
    for(const change of entries)bands[change.id].enabled=!!change.value;
    emit({kind:"bulkEdit",entries,gesture:true});
    finishEdit();renderInspector();changed();
  }else if(action==="remove")removeSelected();
  else if(action==="add")addBand(Number(menu.dataset.x),Number(menu.dataset.y));
  else if(action==="all"){
    selection.clear();for(const id of active())selection.add(id);
    selected=selection.size?[...selection].at(-1):-1;
    emit({kind:"select",id:selected});renderInspector();renderRail();placeInspector();changed();
  }
  hideContext();
});
canvas.addEventListener("pointerdown",e=>{
  hideContext();hideExtraMenus();
  if(e.button!==0)return;
  const p=mapLocal(e),id=bandHit(p.x,p.y);
  if(id<0){select(-1);return;}
  lastValidPick={id,x:p.x,y:p.y,at:Date.now()};
  if(e.ctrlKey||e.metaKey){select(id,true);return;}
  if(!selection.has(id))select(id);
  else if(selected!==id){selected=id;emit({kind:"select",id});renderInspector();placeInspector();}
  beginEdit();
  const ids=selection.has(id)?[...selection]:[id];
  const start=new Map(ids.map(i=>[i,{freq:bands[i].freq,gain:bands[i].gain}]));
  const fields=ids.map(i=>({id:i,fields:["freq",...(hasGain(bands[i].type)?["gain"]:[])]}));
  dragging={id,pointer:e.pointerId,x:p.x,y:p.y,start,values:new Map(start),
    fields,group:ids.length>1,precision:!!e.shiftKey};
  canvas.setPointerCapture(e.pointerId);
  if(ids.length===1)emit({kind:"begin",id,fields:fields[0].fields});
  else emit({kind:"gestureMany",begin:true,items:fields});
});
canvas.addEventListener("pointermove",e=>{
  const p=mapLocal(e);
  $("hoverReadout").textContent=formatHz(xHz(p.x,w))+" Hz  •  "+noteFor(xHz(p.x,w))+"  •  "+number(yDb(p.y,h))+" dB";
  if(dragging&&dragging.pointer===e.pointerId){
    const d=dragging,precision=e.shiftKey||d.precision?0.17:1;
    const dx=(p.x-d.x)*precision,dy=(p.y-d.y)*precision;
    const ratio=Math.exp(dx/plotWidth(w)*Math.log(F1/F0)),delta=-dy/(h*.5)*rangeDb;
    const entries=[];
    for(const [id,v] of d.start){
      const freq=clamp(v.freq*ratio,F0,F1);
      const gain=!hasGain(bands[id].type)?v.gain:clamp(v.gain+delta,-24,24);
      d.values.set(id,{freq,gain});
      bands[id].freq=freq;bands[id].gain=gain;
      entries.push({id,field:"freq",value:freq});
      if(hasGain(bands[id].type))entries.push({id,field:"gain",value:gain});
    }
    pendingDrag=entries;
    if(!canvas.dataset.dragRaf){canvas.dataset.dragRaf="1";requestAnimationFrame(flushDrag);}
    renderInspector();changed();return;
  }
  const hit=bandHit(p.x,p.y);
  if(hit!==hovered){hovered=hit;changed();}
  const tooltip=$("graphTooltip");
  if(hit>=0){
    const band=bands[hit];
    tooltip.textContent="B"+(hit+1)+"  "+formatHz(band.freq)+" Hz  "+number(band.gain)+" dB  · Q "+number(band.q);
    tooltip.style.left=clamp(p.x+14,10,w-190)+"px";
    tooltip.style.top=clamp(p.y-50,8,h-45)+"px";tooltip.hidden=false;
  }else tooltip.hidden=true;
});
const flushDrag=()=>{
  canvas.dataset.dragRaf="";
  if(!pendingDrag)return;
  const entries=pendingDrag;pendingDrag=null;
  if(dragging&&!dragging.group){
    for(const e of entries)emit({kind:"edit",...e,gesture:false});
  }else emit({kind:"bulkEdit",entries,gesture:false});
  renderRail();
};
const finishDrag=e=>{
  if(!dragging||dragging.pointer!==e.pointerId)return;
  flushDrag();
  const d=dragging;dragging=null;
  if(d.group)emit({kind:"gestureMany",begin:false,items:d.fields});
  else emit({kind:"end",id:d.id,fields:d.fields[0].fields});
  finishEdit();placeInspector();changed();
};
canvas.addEventListener("pointerup",finishDrag);
canvas.addEventListener("pointercancel",finishDrag);
canvas.addEventListener("pointerleave",()=>{hovered=-1;$("graphTooltip").hidden=true;changed();});
canvas.addEventListener("dblclick",e=>{
  const p=mapLocal(e);
  let id=bandHit(p.x,p.y);
  // Coordinates remain stable; narrow fallback for double click hit testing.
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
  if(!selection.has(id))select(id);
},{passive:false});
const ctrl=(id,field,converter=Number)=>{
  $(id).addEventListener("change",()=>{
    if(selected<0)return;const v=converter($(id).value);
    if(Number.isFinite(v))setParam(selected,field,v);
  });
};
ctrl("typeSelect","type",Number);ctrl("slopeSelect","slope",Number);
ctrl("dynRangeInput","dynRange",v=>clamp(Number(v),-12,12));
ctrl("dynThresholdInput","dynThreshold",v=>clamp(Number(v),-60,0));
ctrl("freqInput","freq",v=>clamp(Number(v),20,20000));
ctrl("gainInput","gain",v=>clamp(Number(v),-24,24));
ctrl("qInput","q",v=>clamp(Number(v),.1,24));
$("enableBtn").onclick=()=>{if(selected>=0)setParam(selected,"enabled",bands[selected].enabled?0:1);};
$("removeBtn").onclick=removeSelected;
$("routeGroup").addEventListener("click",e=>{
  const btn=e.target.closest("[data-route]");
  if(selected>=0&&btn)setParam(selected,"route",Number(btn.dataset.route));
});
// A dark, original app-owned popup avoids WebView2's white native <select> menu.
const typeMenu=$("typeMenu");
TYPES.forEach((name,type)=>{
  const button=document.createElement("button");
  button.type="button";button.dataset.type=String(type);
  button.textContent=name;button.title=name;
  button.onclick=()=>{
    hideTypeMenu();
    if(selected>=0)setParam(selected,"type",type);
  };
  typeMenu.appendChild(button);
});
$("typeButton").onclick=()=>{
  hidePresetMenu();
  const isOpen=$("typeMenu").hidden;
  $("typeMenu").hidden=!isOpen;
  $("typeButton").setAttribute("aria-expanded",String(isOpen));
  if(isOpen)typeMenu.querySelectorAll("button").forEach(btn=>
    btn.classList.toggle("selected",selected>=0&&Number(btn.dataset.type)===bands[selected].type));
};
$("presetButton").onclick=()=>{
  hideTypeMenu();
  const isOpen=$("presetMenu").hidden;
  $("presetMenu").hidden=!isOpen;
  $("presetButton").setAttribute("aria-expanded",String(isOpen));
  if(isOpen)updatePresetMenu();
};
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
const presets={
  navigate(delta){
    if(!presetNames.length)return;
    const start=activePreset<0?(delta>0?-1:0):activePreset;
    const next=(start+delta+presetNames.length)%presetNames.length;
    this.load(next);
  },
  load(index){
    if(presetBusy||index<0||index>=presetNames.length)return;
    presetBusy=true;
    emit({kind:"presetLoad",index});
  }
};
$("prevPreset").onclick=()=>presets.navigate(-1);
$("nextPreset").onclick=()=>presets.navigate(1);
$("presetSelect").addEventListener("change",()=>{
  const value=Number($("presetSelect").value);
  if(value>=0)presets.load(value);
});
$("savePreset").onclick=()=>{
  const index=Number($("saveSlot").value);
  if(index>=8&&index<16)emit({kind:"presetSave",index});
};
if(backend?.addEventListener)backend.addEventListener("eq-preset-result",result=>{
  presetBusy=false;
  if(!result?.ok){
    $("bridgeStatus").textContent="PRESET NOT AVAILABLE / SAVE FAILED";
    $("presetSelect").value=String(activePreset);
  }else{
    activePreset=Number(result.index);
    $("presetSelect").value=String(activePreset);
  }
});
$("shapePicker").addEventListener("click",e=>{
  const button=e.target.closest("[data-shape]");
  if(selected>=0&&button)setParam(selected,"type",Number(button.dataset.shape));
});
$("advancedBtn").onclick=()=>{hideTypeMenu();
  const expanded=$("inspector").classList.toggle("expanded");
  $("advancedBtn").textContent=expanded?"LESS ▴":"MORE ▾";
  $("advancedBtn").setAttribute("aria-expanded",String(expanded));
  placeInspector();
};
$("undoBtn").onclick=()=>undoRedo(true);
$("redoBtn").onclick=()=>undoRedo(false);
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
    beginEdit();
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
    gesture=null;finishEdit();
  };
  knob.addEventListener("pointerup",end);
  knob.addEventListener("pointercancel",end);
  knob.addEventListener("lostpointercapture",end);
};
attachKnob("freqKnob","freq");
attachKnob("gainKnob","gain");
attachKnob("qKnob","q");
window.addEventListener("keydown",e=>{
  if(e.target?.matches?.("input,select,textarea"))return;
  const ctrl=e.ctrlKey||e.metaKey,key=e.key.toLowerCase();
  if(ctrl&&(key==="z"||key==="y")){e.preventDefault();undoRedo(key==="z"&&!e.shiftKey);return;}
  if(ctrl&&key==="a"){
    e.preventDefault();selection.clear();for(const id of active())selection.add(id);
    selected=selection.size?[...selection].at(-1):-1;
    emit({kind:"select",id:selected});renderInspector();renderRail();placeInspector();changed();return;
  }
  if(key==="escape"){hideContext();hideExtraMenus();select(-1);return;}
  if((key==="delete"||key==="backspace")&&selected>=0){e.preventDefault();removeSelected();}
});
resize();renderInspector();renderRail();refreshUndo();changed();
})();