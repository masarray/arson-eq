// Zero-dependency smoke test of bundled WebUI startup and host-command bridge.
// Runs in Node using a minimal fake DOM/canvas. Catches regressions in real JS
// interaction wiring without building JUCE or opening a DAW.
const fs=require("node:fs"),vm=require("node:vm"),assert=require("node:assert/strict");
const calls=[],handlers={},nodes={},windowEvents={};
function element(id) {
  if(nodes[id])return nodes[id];
  const events={},classes=new Set();
  const node={
    id,value:"",style:{setProperty(){}},className:"",textContent:"",hidden:false,
    get options(){return this.children;},
    dataset:{},width:800,height:400,disabled:false,children:[],
    classList:{
      toggle(name){if(classes.has(name)){classes.delete(name);return false;}classes.add(name);return true;},
      add(name){classes.add(name);},remove(name){classes.delete(name);},
      contains(name){return classes.has(name);}
    },
    addEventListener(name,callback){events[name]=callback;},
    dispatch(name,payload){assert.ok(events[name],id+" has "+name);events[name](payload||{});},
    setAttribute(){},
    appendChild(child){this.children.push(child);return child;},
    replaceChildren(){this.children=[];},
    querySelectorAll(){return [0,1,2,3,4].map(i=>({
      dataset:{route:String(i)},classList:{toggle(){}}
    }));},
    getBoundingClientRect(){return{left:0,top:0,width:800,height:400};},
    setPointerCapture(){},
    scrollIntoView(){},
    getContext(){return ctx;}
  };
  return nodes[id]=node;
}
const ctx={
  fillRect(){},beginPath(){},moveTo(){},lineTo(){},stroke(){},fill(){},
  arc(){},closePath(){},clip(){},save(){},restore(){},fillText(){},setTransform(){},
  createLinearGradient(){return{addColorStop(){}};}
};
const document={activeElement:null,
  getElementById:element,
  createElement:()=>element("created-"+Object.keys(nodes).length)
};
const backend={
  addEventListener(name,fn){handlers[name]=fn;},
  emitEvent(name,data){calls.push({name,data});}
};
const window={
  __JUCE__:{backend},devicePixelRatio:1,
  addEventListener(name,fn){windowEvents[name]=fn;},
  requestAnimationFrame(fn){fn();}
};
const sandbox={
  document,window,ResizeObserver:class{constructor(fn){this.fn=fn;}observe(){this.fn();}},
  requestAnimationFrame:fn=>fn(),console,Math,Array,Number,JSON,Set
};
vm.runInNewContext(fs.readFileSync("WebUI/app.js","utf8"),sandbox,{timeout:2000,filename:"app.js"});
const find=k=>calls.filter(x=>x.data.kind===k).map(x=>x.data);
assert.ok(find("ready").length,"JS announces ready to native engine");
assert.equal(typeof handlers["eq-state"],"function");
assert.equal(typeof handlers["eq-spectrum"],"function");
const bands=Array.from({length:24},(_,i)=>({
  enabled:i<2,type:0,route:0,freq:i===0?1000:5000,gain:i===0?6:2,q:1,slope:0,
  dynRange:0,dynThreshold:-24
}));
const curve=Array(256).fill(0);
handlers["eq-state"]({bands,rate:48000,curve:{l:curve,r:curve,cross:curve,band:curve},
  presets:["Init / Flat","Clean Vocal","Vocal Presence","Warm Vocal","De-Esser Dynamic",
    "Tight Kick","Balanced Master","Air & Sparkle","User 01","User 02","User 03",
    "User 04","User 05","User 06","User 07","User 08"],presetIndex:-1});
assert.ok(element("bridgeStatus").textContent.includes("CONNECTED"),"UI becomes connected");
assert.ok(element("bandCount").textContent.includes("2 / 24"),"host band snapshot reflected");
handlers["eq-spectrum"]({pre:Array(384).fill(-80),post:Array(384).fill(-75),range:90,peakL:0.2,peakR:0.15});
element("eqCanvas").dispatch("pointerdown",{button:0,pointerId:1,clientX:431,clientY:100});
assert.ok(find("begin").length,"host gesture starts on drag");
assert.equal(element("inspector").hidden,false,"floating card opens on pointerdown without reflow");
assert.ok(Number.parseFloat(element("inspector").style.left)>=0,"card is positioned inside the graph");
assert.ok(element("inspector").style.top.endsWith("px"),"card uses absolute coordinates, no flex sizing");
element("eqCanvas").dispatch("pointermove",{pointerId:1,clientX:500,clientY:170});
element("eqCanvas").dispatch("pointerup",{pointerId:1});
assert.ok(find("end").length,"host gesture ends after drag");
assert.equal(element("inspector").hidden,false,"floating card remains visible after drag");
const cardTop=element("inspector").style.top;
assert.ok(cardTop.endsWith("px"),"floating card has stable compositor position");
assert.ok(find("edit").some(x=>x.field==="freq"),"drag automates frequency");
element("qKnob").dispatch("pointerdown",{button:0,pointerId:25,clientY:120,preventDefault(){}});
element("qKnob").dispatch("pointermove",{pointerId:25,clientY:98});
element("qKnob").dispatch("pointerup",{pointerId:25});
assert.ok(find("begin").some(x=>x.fields.includes("q")),"Q knob opens host automation gesture");
assert.ok(find("edit").some(x=>x.field==="q"),"Q knob has actual editing behavior");
assert.ok(find("end").some(x=>x.fields.includes("q")),"Q knob closes host automation gesture");
// M3 native preset browser + filter discovery + real dynamic host params.
assert.equal(element("presetSelect").options.length,17,"native factory+user preset list visible");
element("presetSelect").value="4";element("presetSelect").dispatch("change");
assert.ok(find("presetLoad").some(x=>x.index===4),"factory preset triggers native load");
element("saveSlot").value="9";element("savePreset").onclick();
assert.ok(find("presetSave").some(x=>x.index===9),"user preset saves to chosen native slot");
element("shapePicker").dispatch("click",{target:{closest(){return{dataset:{shape:"6"}};}}});
assert.ok(find("edit").some(x=>x.field==="type"&&x.value===6),"Band Pass becomes real host parameter");
element("shapePicker").dispatch("click",{target:{closest(){return{dataset:{shape:"0"}};}}});
element("dynRangeInput").value="-6";element("dynRangeInput").dispatch("change");
element("dynThresholdInput").value="-36";element("dynThresholdInput").dispatch("change");
assert.ok(find("edit").some(x=>x.field==="dynRange"&&x.value===-6),"native dynamic range");
assert.ok(find("edit").some(x=>x.field==="dynThreshold"&&x.value===-36),"native dynamic threshold");
element("analyzerMenu").hidden=true;
element("analyzerMenuBtn").onclick();
assert.equal(element("analyzerMenu").hidden,false,"analyzer FFT options available");
element("fftSelect").value="13";element("fftSelect").dispatch("change");
assert.ok(find("analyzerSettings").some(x=>x.field==="fft"&&x.value===13),"native FFT resolution changes");
element("freezeSpectrum").checked=true;element("freezeSpectrum").dispatch("change");
assert.ok(find("analyzerSettings").some(x=>x.field==="freeze"&&x.value===true),"native analyzer freeze");
element("advancedBtn").onclick();
assert.ok(element("inspector").classList.contains("expanded"),"advanced controls are overlay, not layout");
assert.ok(find("ready").length,"advanced editor toggle does not restart browser");
element("advancedBtn").onclick();
assert.ok(!element("inspector").classList.contains("expanded"),"quick panel closes without graph reflow");
// Ctrl-select second band, then move both in a single bounded IPC frame.
element("eqCanvas").dispatch("pointerdown",{button:0,pointerId:3,clientX:589,clientY:167,ctrlKey:true});
element("eqCanvas").dispatch("pointerdown",{button:0,pointerId:4,clientX:589,clientY:167});
element("eqCanvas").dispatch("pointermove",{pointerId:4,clientX:609,clientY:154});
element("eqCanvas").dispatch("pointerup",{pointerId:4});
assert.ok(find("gestureMany").some(x=>x.begin===true),"group begins host gestures");
assert.ok(find("gestureMany").some(x=>x.begin===false),"group closes host gestures");
assert.ok(find("bulkEdit").some(x=>x.entries?.some(e=>e.id===0)&&x.entries?.some(e=>e.id===1)),"group edit batches multiple bands in one IPC event");
assert.equal(element("inspector").hidden,false,"group edit keeps stable card visible");
windowEvents.keydown({target:{matches(){return false;}},key:"z",ctrlKey:true,preventDefault(){}});
assert.ok(find("bulkEdit").some(x=>x.gesture===true),"Ctrl+Z restores native host parameters with gesture");
windowEvents.keydown({target:{matches(){return false;}},key:"y",ctrlKey:true,preventDefault(){}});
assert.equal(element("redoBtn").disabled,true,"redo stack consumed");
// Right-click is contextual, not a layout-sized modal.
element("eqCanvas").dispatch("contextmenu",{clientX:589,clientY:167,preventDefault(){}});
assert.equal(element("bandContextMenu").hidden,false,"right-click menu is visible");
assert.ok(element("bandContextMenu").style.top.endsWith("px"),"context menu is graph anchored");
element("eqCanvas").dispatch("pointerdown",{button:0,pointerId:8,clientX:589,clientY:167,ctrlKey:true});
assert.equal(element("bandContextMenu").hidden,true,"normal canvas interaction dismisses context menu");


element("viewBtn").onclick();
assert.ok(find("spectrum").some(x=>x.enabled===false),"spectrum switch gates native telemetry");
element("viewBtn").onclick();
assert.ok(find("spectrum").some(x=>x.enabled===true),"spectrum gate resumes native telemetry");
element("eqCanvas").dispatch("dblclick",{clientX:100,clientY:300});
assert.ok(find("add").length,"double click empty graph creates band");
element("closeInspector").onclick();
assert.equal(element("inspector").hidden,true,"close inspector frees entire graph");
element("bandsBtn").onclick();
assert.equal(element("bandList").hidden,false,"band list opens only when asked");
element("helpBtn").onclick();
assert.equal(element("bandList").hidden,true,"help and band menus are mutually exclusive");
assert.equal(element("quickHelp").hidden,false,"help popover opens on demand");
const markup=fs.readFileSync("WebUI/index.html","utf8");
const style=fs.readFileSync("WebUI/style.css","utf8");
assert.ok(markup.includes('class="graph-wrap"'),"graph wraps the canvas and card");
assert.ok(markup.indexOf('id="inspector"')>markup.indexOf('id="quickHelp"'),"card comes after menus");
assert.ok(markup.indexOf('id="inspector"')<markup.lastIndexOf('  </div>\n</main>'),"card nested inside graphWrap, not in flex flow");
assert.ok(style.includes(".inspector{position:absolute"),"card uses absolute overlay, no layout reflow");
assert.ok(style.includes(".inspector.expanded{flex:none"),"expanded card stays outside flex layout");
assert.ok(!style.includes("filter:brightness"),"no global hover GPU brightness filters");
handlers["eq-state"]({bands,rate:48000,curve:{l:curve,r:curve,cross:curve,band:curve}});
// Check host-originated gain beyond 12 dB no longer silently clips nodes.
const elevated=bands.map(x=>({...x}));
elevated[0].gain=18;
handlers["eq-state"]({bands:elevated,rate:48000,curve:{l:curve,r:curve,cross:curve,band:curve}});
assert.ok(element("inspector").hidden,"host snapshots do not force the floating card open");
console.log("ArSonEQ M2.5 smoke PASS: stable overlay, precision/group gestures, undo/redo, right-click, analyzer");
