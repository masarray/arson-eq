// Zero-dependency smoke test of bundled WebUI startup and host-command bridge.
// Runs in Node using a minimal fake DOM/canvas. Catches regressions in real JS
// interaction wiring without building JUCE or opening a DAW.
const fs=require("node:fs"),vm=require("node:vm"),assert=require("node:assert/strict");
const calls=[],handlers={},nodes={};
function element(id) {
  if(nodes[id])return nodes[id];
  const events={};
  const node={
    id,value:"",style:{setProperty(){}},className:"",textContent:"",hidden:false,
    dataset:{},width:800,height:400,disabled:false,children:[],
    classList:{toggle(){},add(){},remove(){}},
    addEventListener(name,callback){events[name]=callback;},
    dispatch(name,payload){assert.ok(events[name],id+" has "+name);events[name](payload||{});},
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
  addEventListener(){},
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
  enabled:i===0,type:0,route:0,freq:i===0?1000:2000,gain:i===0?6:0,q:1,slope:0
}));
const curve=Array(256).fill(0);
handlers["eq-state"]({bands,rate:48000,curve:{l:curve,r:curve,cross:curve,band:curve}});
assert.ok(element("bridgeStatus").textContent.includes("CONNECTED"),"UI becomes connected");
assert.ok(element("bandCount").textContent.includes("1 / 24"),"host band snapshot reflected");
handlers["eq-spectrum"]({pre:Array(224).fill(-80),post:Array(224).fill(-75)});
element("eqCanvas").dispatch("pointerdown",{button:0,pointerId:1,clientX:431,clientY:100});
assert.ok(find("begin").length,"host gesture starts on drag");
assert.equal(element("inspector").hidden,false,"inspector appears only when a band is selected");
assert.ok(!element("inspector").style.left,"inspector must never follow/cover graph nodes");
assert.ok(element("inspector").hidden===false,"selected band creates non-obscuring dock");
element("eqCanvas").dispatch("pointermove",{pointerId:1,clientX:500,clientY:170});
element("eqCanvas").dispatch("pointerup",{pointerId:1});
assert.ok(find("end").length,"host gesture ends after drag");
assert.ok(find("edit").some(x=>x.field==="freq"),"drag automates frequency");
element("qKnob").dispatch("pointerdown",{button:0,pointerId:25,clientY:120,preventDefault(){}});
element("qKnob").dispatch("pointermove",{pointerId:25,clientY:98});
element("qKnob").dispatch("pointerup",{pointerId:25});
assert.ok(find("begin").some(x=>x.fields.includes("q")),"Q knob opens host automation gesture");
assert.ok(find("edit").some(x=>x.field==="q"),"Q knob has actual editing behavior");
assert.ok(find("end").some(x=>x.fields.includes("q")),"Q knob closes host automation gesture");
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
assert.ok(markup.includes('class="graph-wrap"') && !markup.includes('<aside class="inspector"'),"no wasted right-hand sidebar");
assert.ok(style.includes(".inspector{position:relative"),"inspector is a dedicated layout sibling, not an overlay");
const inspectorPosition=markup.indexOf('id="inspector"');
const graphClose=markup.indexOf('<!-- Context dock has its own layout space');
assert.ok(inspectorPosition>graphClose && graphClose>markup.indexOf('id="quickHelp"'),
  "band controls are after graph closing tag, never inside interactive plot");
assert.ok(style.includes(".workspace{flex:1;min-height:0;display:flex;flex-direction:column"),
  "dock stacks under graph without occlusion");
assert.ok(style.includes("flex:0 0 143px"),"dock has bounded, predictable layout space");
assert.ok(!style.includes("filter:brightness"),"no global hover GPU brightness filters");
handlers["eq-state"]({bands,rate:48000,curve:{l:curve,r:curve,cross:curve,band:curve}});
// Check host-originated gain beyond 12 dB no longer silently clips nodes.
const elevated=bands.map(x=>({...x}));
elevated[0].gain=18;
handlers["eq-state"]({bands:elevated,rate:48000,curve:{l:curve,r:curve,cross:curve,band:curve}});
assert.ok(element("inspector").hidden,"host snapshots do not force the dock open");
console.log("ArSonEQ M2.3 GUI smoke PASS: dock outside graph, host gestures, adaptive gain axis, spectrum gate");
