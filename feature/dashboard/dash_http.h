// Patternflow Dashboard - the settings page at http://patternflow.local/dashboard
//   - location: the browser looks the place up, the panel stores coordinates only
//   - night mode: sleep from / until
//   - GIFs: the browser decodes and converts them, the panel stores the clips
// Nothing personal is ever compiled into the firmware.
#pragma once
#include <Arduino.h>
#include <FFat.h>
#include "../../src/core_loop_sync.h"
#include "../../src/core_patterns_http.h"
#include "dash_accel.h"
#include "dash_clocks.h"
#include "dash_gifs.h"
#include "dash_state.h"
#include "dash_night.h"
#include "dash_weather.h"

namespace DashHttp {

inline WebServer& server() { return PatternflowPatternsHttp::server(); }

static const char PAGE[] PROGMEM = R"HTML(<!doctype html><html><head><meta charset="utf-8">
<script src="/pf-console.js"></script>
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Patternflow - Dashboard</title>
<style>:root{--cream:#0C0B09;--ink:#EDE7DB;--muted:#8A8272;--faint:#5A5546;--rule:#242118;--led:#FF5C2E;--ok:#57B87F;--panel:#131110;--lift:#1B1914;
--sans:'Inter',ui-sans-serif,system-ui,sans-serif;--mono:'JetBrains Mono',ui-monospace,Menlo,monospace}
*{box-sizing:border-box}body{margin:0;background:var(--cream);color:var(--ink);font:14px/1.5 var(--sans);-webkit-font-smoothing:antialiased;max-width:960px;margin:0 auto;padding:28px 20px 64px}
h1{font-size:15px;font-weight:600;margin:0 0 4px}h2{font-size:11px;font-weight:600;letter-spacing:.09em;text-transform:uppercase;color:var(--muted);margin:28px 0 10px;padding-top:16px;border-top:1px solid var(--rule)}
input,button,select{font:inherit;font-size:12px;height:34px;padding:0 10px;border-radius:2px;border:1px solid var(--rule);background:var(--panel);color:var(--ink)}
button{cursor:pointer;border-color:var(--led);color:var(--ink)}button:hover{background:var(--lift)}button.del{border-color:var(--rule);color:var(--muted)}
button:disabled{opacity:.35;cursor:default}ul{list-style:none;padding:0}li{margin:.3rem 0;display:flex;gap:.5rem;align-items:center}li .grow{flex:1}
.muted{color:var(--muted)}canvas{image-rendering:pixelated;background:#000;border:1px solid var(--rule);margin:.5rem .5rem 0 0}
label{display:inline-flex;gap:.4rem;align-items:center;margin:.3rem 1rem .3rem 0;color:var(--muted)}b{font-family:var(--mono);font-weight:500}</style></head><body>
<h1>Dashboard</h1>

<h2>Location</h2><p class="muted">For the weather screens.</p>
<p>Now: <b id="now">…</b> <span id="wx" class="muted"></span></p>
<form id="f"><input id="q" placeholder="City, e.g. Utrecht" required> <button>Search</button></form><ul id="r"></ul>

<h2>World clocks</h2><p class="muted">Up to four, on the dashboard's world clock screen. Daylight saving time comes from your browser's time zone database.</p>
<ul id="clocks"></ul>
<form id="cf"><input id="cq" placeholder="Add a city, e.g. Tokyo"> <button>Search</button></form><ul id="cr"></ul>
<p><button id="csave">Save clocks</button> <span id="cmsg" class="muted"></span></p>

<h2>Night mode</h2><p class="muted">The whole panel sleeps between these times. Any knob wakes it; at night it goes back to sleep after 10 minutes.</p>
<form id="nf"><label><input type="checkbox" id="non"> On</label>
<label>from <input type="time" id="ns"></label><label>until <input type="time" id="ne"></label> <button>Save</button></form>
<p id="nmsg" class="muted"></p>

<h2>Orientation</h2><p class="muted" id="ostat">…</p>
<p><label><input type="checkbox" id="oauto"> Turn with the panel (needs the accelerometer)</label><br>
<label><input type="checkbox" id="oflip"> Also turn Patternflow's patterns when the panel is upside down</label></p>
<p class="muted">Setting up the accelerometer: hang the panel upright (portrait) and press <i>This is upright</i>. If the dashboard then reads upside down in portrait or landscape, press the matching button.</p>
<p><button id="oup">This is upright</button> <button id="ofp">Portrait is upside down</button> <button id="ofl">Landscape is upside down</button></p>

<h2>GIFs</h2><p class="muted">Shown between the dashboard screens. <span id="space"></span></p>
<ul id="gifs"></ul>
<p><input type="file" id="file" accept="image/gif"></p>
<p><label>Fit <select id="fit"><option value="contain">whole GIF, black bars</option><option value="cover">fill, crop</option></select></label>
<label><input type="checkbox" id="sharp" checked> Sharp (pixel art)</label>
<label><input type="checkbox" id="split"> Split wide GIFs in portrait: left half on top, right half below</label></p>
<p><label>Name <input id="name" maxlength="24" placeholder="e.g. nyan-cat"></label></p>
<div><canvas id="pp" width="64" height="128" style="width:96px;height:192px"></canvas><canvas id="pl" width="128" height="64" style="width:192px;height:96px"></canvas></div>
<p><button id="up" disabled>Upload</button> <span id="gmsg" class="muted"></span></p>

<script>
const $=id=>document.getElementById(id);
const hm=m=>String(Math.floor(m/60)).padStart(2,'0')+':'+String(m%60).padStart(2,'0');
async function status(first){const s=await (await fetch('/api/dashboard')).json();
 $('now').textContent=s.place?`${s.place} (${s.lat.toFixed(3)}, ${s.lon.toFixed(3)})`:'not set';
 $('wx').textContent=s.error?`last fetch: ${s.error}`:(s.updated?`weather ${s.updated} s old`:'');
 if(first){clocks=s.clocks.split('\n').filter(Boolean).map(l=>{const[name,spec,label]=l.split('|');return{name,spec,label};});drawClocks();}
 if(first){$('non').checked=s.night.on;$('ns').value=hm(s.night.start);$('ne').value=hm(s.night.end);}
 $('space').textContent=`${(s.free/1048576).toFixed(1)} MB free.`;
 $('gifs').innerHTML='';for(const g of s.gifs){const li=document.createElement('li');li.innerHTML=`<span class="grow">${g}</span>`;
  const b=document.createElement('button');b.className='del';b.textContent='Delete';b.onclick=async()=>{await fetch('/api/dashboard/gif/delete?name='+g,{method:'POST'});status();};
  li.append(b);$('gifs').append(li);}
 if(!s.gifs.length)$('gifs').innerHTML='<li class="muted">No GIFs yet.</li>';}
$('f').onsubmit=async e=>{e.preventDefault();$('r').innerHTML='';
 const j=await (await fetch('https://geocoding-api.open-meteo.com/v1/search?count=5&name='+encodeURIComponent($('q').value))).json();
 for(const p of j.results||[]){const b=document.createElement('button');b.textContent=[p.name,p.admin1,p.country_code].filter(Boolean).join(', ');
  b.onclick=async()=>{await fetch('/api/dashboard',{method:'POST',body:new URLSearchParams({lat:p.latitude,lon:p.longitude,place:p.name})});$('r').innerHTML='';status();};
  const li=document.createElement('li');li.append(b);$('r').append(li);}
 if(!(j.results||[]).length)$('r').textContent='Nothing found.';};
$('nf').onsubmit=async e=>{e.preventDefault();
 const r=await fetch('/api/dashboard/night',{method:'POST',body:new URLSearchParams({on:$('non').checked?1:0,start:$('ns').value,end:$('ne').value})});
 $('nmsg').textContent=r.ok?'Saved.':'Could not save.';};

// ---- world clocks: the zone rule is derived from the browser's time zone database
function zoneSpec(zone) {
  const fmt = new Intl.DateTimeFormat('en-US', {timeZone: zone, hourCycle: 'h23', year: 'numeric', month: 'numeric', day: 'numeric', hour: 'numeric', minute: 'numeric'});
  const off = t => { const p = fmt.formatToParts(new Date(t)), g = k => +p.find(x => x.type === k).value;
    return Math.round((Date.UTC(g('year'), g('month') - 1, g('day'), g('hour'), g('minute')) - Math.floor(t / 60000) * 60000) / 60000); };
  const y = new Date().getUTCFullYear(), t0 = Date.UTC(y, 0, 1), t1 = Date.UTC(y + 1, 0, 1), changes = [];
  let prev = off(t0);
  for (let t = t0 + 3600e3; t <= t1; t += 3600e3) {
    const o = off(t); if (o === prev) continue;
    let lo = t - 3600e3, hi = t;  // narrow the switch down to the minute
    while (hi - lo > 60e3) { const mid = lo + Math.floor((hi - lo) / 120e3) * 60e3; if (off(mid) === prev) lo = mid; else hi = mid; }
    changes.push({t: hi, from: prev, to: o}); prev = o;
  }
  const s = changes.find(c => c.to > c.from), e = changes.find(c => c.to < c.from);
  if (!s || !e) return `${prev},0`;  // no daylight saving time
  const std = Math.min(s.from, e.to), dst = s.to - s.from;
  const dim = (yy, m) => new Date(Date.UTC(yy, m, 0)).getUTCDate();
  const when = (yy, r, from) => { const firstWd = new Date(Date.UTC(yy, r.m - 1, 1)).getUTCDay();
    let d = 1 + (r.wd - firstWd + 7) % 7 + 7 * (r.w - 1); while (d > dim(yy, r.m)) d -= 7;
    return Date.UTC(yy, r.m - 1, d) + (r.min - from) * 60e3; };
  const holds = (r, c) => { for (let yy = y; yy < y + 7; yy++) { const t = when(yy, r, c.from); if (off(t - 60e3) !== c.from || off(t) !== c.to) return false; } return true; };
  const rule = c => { const l = new Date(c.t + c.from * 60e3), min = l.getUTCHours() * 60 + l.getUTCMinutes();
    for (const k of [0, -1, 1, -2, 2, -3, 3]) {  // describe the moment from a weekday k days away
      const a = new Date(l.getTime() + k * 864e5); if (a.getUTCMonth() !== l.getUTCMonth()) continue;
      const m = a.getUTCMonth() + 1, d = a.getUTCDate(), wd = a.getUTCDay();
      const weeks = d + 7 > dim(a.getUTCFullYear(), m) ? [5, Math.ceil(d / 7)] : [Math.ceil(d / 7)];
      for (const w of weeks) { const r = {m, w, wd, min: min - k * 1440}; if (holds(r, c)) return `${m}.${w}.${wd}/${r.min}`; }
    }
    return null; };
  const rs = rule(s), re = rule(e);
  if (!rs || !re) return null;  // a rule this format cannot express
  return `${std},${dst},${rs},${re}`;
}
let clocks=null;
const clockTime=z=>{try{return new Intl.DateTimeFormat('en-GB',{timeZone:z,hour:'2-digit',minute:'2-digit'}).format(new Date());}catch(e){return '';}};
function drawClocks(){$('clocks').innerHTML='';clocks.forEach((c,i)=>{const li=document.createElement('li');
 const n=document.createElement('input');n.value=c.name;n.maxLength=10;n.style.width='8rem';n.oninput=()=>{c.name=n.value.toUpperCase().replace(/[^ -~]|["\\|]/g,'');};
 const t=document.createElement('span');t.className='grow muted';t.textContent=`${c.label||'?'} ${c.label?clockTime(c.label):''}${c.note?' · '+c.note:''}`;
 const b=document.createElement('button');b.className='del';b.textContent='Remove';b.onclick=()=>{clocks.splice(i,1);drawClocks();};
 li.append(n,t,b);$('clocks').append(li);});
 if(!clocks.length)$('clocks').innerHTML='<li class="muted">No world clocks: the screen is skipped.</li>';
 $('cq').disabled=clocks.length>=4;}
$('cf').onsubmit=async e=>{e.preventDefault();$('cr').innerHTML='';
 const j=await (await fetch('https://geocoding-api.open-meteo.com/v1/search?count=5&name='+encodeURIComponent($('cq').value))).json();
 for(const p of j.results||[]){const b=document.createElement('button');b.textContent=[p.name,p.admin1,p.country_code].filter(Boolean).join(', ')+' · '+p.timezone;
  b.onclick=()=>{let spec=zoneSpec(p.timezone),note='';
   if(!spec){const f=new Intl.DateTimeFormat('en-US',{timeZone:p.timezone,timeZoneName:'longOffset'}).formatToParts(new Date()).find(x=>x.type==='timeZoneName').value;
    const m=f.match(/([+-])(\d+):?(\d*)/);spec=`${m?(m[1]==='-'?-1:1)*(+m[2]*60+(+m[3]||0)):0},0`;note='daylight saving time not followed';}
   clocks.push({name:p.name.toUpperCase().normalize('NFD').replace(/[^ -~]|["\\|]/g,'').slice(0,10),spec,label:p.timezone,note});
   $('cr').innerHTML='';$('cq').value='';drawClocks();};
  const li=document.createElement('li');li.append(b);$('cr').append(li);}
 if(!(j.results||[]).length)$('cr').textContent='Nothing found.';};
$('csave').onclick=async()=>{const text=clocks.map(c=>`${c.name}|${c.spec}|${c.label}`).join('\n');
 const r=await fetch('/api/dashboard/clocks',{method:'POST',body:new URLSearchParams({clocks:text})});
 $('cmsg').textContent=r.ok?'Saved.':'Could not save: '+await r.text();};
setInterval(()=>clocks&&drawClocks(),30000);

// ---- GIF decoding (GIF87a/89a: LZW, local palettes, transparency, disposal, interlacing)
function lzw(data,minCode,n){const out=new Uint8Array(n);let op=0;const clear=1<<minCode,eoi=clear+1;let size=minCode+1,next=eoi+1;
 const prefix=new Int16Array(4096),suffix=new Uint8Array(4096),stack=new Uint8Array(4097);for(let c=0;c<clear;c++){prefix[c]=-1;suffix[c]=c;}
 let old=-1,first=0,bits=0,acc=0,i=0;
 while(op<n){while(bits<size){if(i>=data.length)return out;acc|=data[i++]<<bits;bits+=8;}
  const code=acc&((1<<size)-1);acc>>=size;bits-=size;
  if(code===clear){size=minCode+1;next=eoi+1;old=-1;continue;}if(code===eoi)break;
  if(old<0){out[op++]=suffix[code];old=first=code;continue;}
  let sp=0,cur=code;if(code>=next){stack[sp++]=first;cur=old;}
  while(cur>=clear){stack[sp++]=suffix[cur];cur=prefix[cur];}stack[sp++]=first=suffix[cur];
  while(sp&&op<n)out[op++]=stack[--sp];
  if(next<4096){prefix[next]=old;suffix[next]=first;next++;if(next===(1<<size)&&size<12)size++;}old=code;}
 return out;}
function decodeGif(buf){const b=new Uint8Array(buf);if(b[0]!==71||b[1]!==73||b[2]!==70)throw new Error('not a GIF');
 let p=6;const u16=()=>{p+=2;return b[p-2]|b[p-1]<<8;};const W=u16(),H=u16(),fl=b[p];p+=3;
 let gct=null;if(fl&128){const n=3*(2<<(fl&7));gct=b.subarray(p,p+n);p+=n;}
 const cv=new Uint8ClampedArray(W*H*4),frames=[];let ext={delay:100,trans:-1,disp:0};
 while(p<b.length){const t=b[p++];if(t===0x3B)break;
  if(t===0x21){const label=b[p++];if(label===0xF9){p++;const f=b[p++];ext.disp=f>>2&7;const d=u16()*10;ext.delay=d<20?100:d;const ti=b[p++];ext.trans=f&1?ti:-1;p++;}
   else{let s;while((s=b[p++]))p+=s;}continue;}
  if(t!==0x2C)break;
  const x=u16(),y=u16(),w=u16(),h=u16(),f=b[p++];let ct=gct;if(f&128){const n=3*(2<<(f&7));ct=b.subarray(p,p+n);p+=n;}
  const minCode=b[p++],chunks=[];let s,len=0;while((s=b[p++])){chunks.push(b.subarray(p,p+s));p+=s;len+=s;}
  const data=new Uint8Array(len);let o=0;for(const c of chunks){data.set(c,o);o+=c.length;}
  const idx=lzw(data,minCode,w*h);let rows=[...Array(h).keys()];
  if(f&64){rows=[];for(const[s0,st]of[[0,8],[4,8],[2,4],[1,2]])for(let r=s0;r<h;r+=st)rows.push(r);}
  const saved=ext.disp===3?cv.slice():null;
  for(let i=0;i<w*h;i++){const ci=idx[i];if(ci===ext.trans||!ct)continue;const X=x+i%w,Y=y+rows[Math.floor(i/w)];if(X>=W||Y>=H)continue;
   const q=(Y*W+X)*4;cv[q]=ct[ci*3];cv[q+1]=ct[ci*3+1];cv[q+2]=ct[ci*3+2];cv[q+3]=255;}
  frames.push({rgba:cv.slice(),delay:ext.delay});
  if(ext.disp===2)for(let r=0;r<h;r++)for(let c=0;c<w;c++){if(x+c<W&&y+r<H)cv.fill(0,((y+r)*W+x+c)*4,((y+r)*W+x+c)*4+4);}
  else if(saved)cv.set(saved);
  ext={delay:100,trans:-1,disp:0};}
 return{W,H,frames};}

// ---- convert to clips: scale to 64x128 and 128x64, map to the 6x7x6 palette
const G7=[0,43,85,128,170,213,255];
function convert(gif,tw,th,fit,sharp,split){let fr=gif.frames;const step=Math.ceil(fr.length/120);
 // split: a wide GIF becomes its left half above its right half (half as wide, twice as tall)
 const half=Math.floor(gif.W/2),SW=split?half:gif.W,SH=split?gif.H*2:gif.H;
 const raw=document.createElement('canvas');raw.width=gif.W;raw.height=gif.H;const rc=raw.getContext('2d');
 const src=document.createElement('canvas');src.width=SW;src.height=SH;const sc=src.getContext('2d');
 const dst=document.createElement('canvas');dst.width=tw;dst.height=th;const dc=dst.getContext('2d');dc.imageSmoothingEnabled=!sharp;
 const s=fit==='cover'?Math.max(tw/SW,th/SH):Math.min(tw/SW,th/SH),dw=SW*s,dh=SH*s;
 const out=[];for(let i=0;i<fr.length;i+=step){let delay=0;for(let k=i;k<Math.min(i+step,fr.length);k++)delay+=fr[k].delay;
  rc.putImageData(new ImageData(fr[i].rgba,gif.W,gif.H),0,0);sc.clearRect(0,0,SW,SH);
  if(split){sc.drawImage(raw,0,0,half,gif.H,0,0,half,gif.H);sc.drawImage(raw,half,0,half,gif.H,0,gif.H,half,gif.H);}else sc.drawImage(raw,0,0);
  dc.fillStyle='#000';dc.fillRect(0,0,tw,th);dc.drawImage(src,(tw-dw)/2,(th-dh)/2,dw,dh);
  const px=dc.getImageData(0,0,tw,th).data,ix=new Uint8Array(tw*th);
  for(let j=0;j<tw*th;j++){const r=px[j*4],g=px[j*4+1],b=px[j*4+2];ix[j]=Math.round(r/51)*42+Math.round(g/42.5)*6+Math.round(b/51);}
  out.push({ix,delay:Math.min(65535,delay)});}
 return{w:tw,h:th,frames:out};}
function clipBytes(c){const n=c.frames.length,buf=new Uint8Array(12+2*n+n*c.w*c.h),dv=new DataView(buf.buffer);
 buf.set([68,71,70,49]);dv.setUint16(4,c.w,true);dv.setUint16(6,c.h,true);dv.setUint16(8,n,true);
 c.frames.forEach((f,i)=>{dv.setUint16(12+2*i,f.delay,true);buf.set(f.ix,12+2*n+i*c.w*c.h);});return buf;}
let gif=null,clips=null,anim=null;
function preview(){if(!gif)return;clips={p:convert(gif,64,128,$('fit').value,$('sharp').checked,$('split').checked),l:convert(gif,128,64,$('fit').value,$('sharp').checked,false)};
 const kb=Math.round((clipBytes(clips.p).length+clipBytes(clips.l).length)/1024);
 $('gmsg').textContent=`${clips.p.frames.length} frames, ${kb} KB`;$('up').disabled=false;
 clearTimeout(anim);let i=0;const tick=()=>{for(const[k,c]of[['pp',clips.p],['pl',clips.l]]){const f=c.frames[i%c.frames.length],img=new ImageData(c.w,c.h);
   for(let j=0;j<c.w*c.h;j++){const v=f.ix[j];img.data[j*4]=Math.floor(v/42)*51;img.data[j*4+1]=G7[Math.floor(v/6)%7];img.data[j*4+2]=v%6*51;img.data[j*4+3]=255;}
   $(k).getContext('2d').putImageData(img,0,0);}
  anim=setTimeout(tick,clips.p.frames[i%clips.p.frames.length].delay);i++;};tick();}
$('file').onchange=async()=>{const f=$('file').files[0];if(!f)return;
 try{gif=decodeGif(await f.arrayBuffer());}catch(e){$('gmsg').textContent='Could not read this GIF.';return;}
 $('split').checked=gif.W>=1.6*gif.H;$('name').value=f.name.replace(/\.gif$/i,'').toLowerCase().replace(/[^a-z0-9-]+/g,'-').replace(/^-|-$/g,'').slice(0,24);preview();};
$('fit').onchange=preview;$('split').onchange=preview;$('sharp').onchange=preview;
$('up').onclick=async()=>{const name=$('name').value;if(!/^[a-z0-9-]{1,24}$/.test(name)){$('gmsg').textContent='Name: a-z, 0-9 and - only.';return;}
 $('up').disabled=true;for(const o of['p','l']){$('gmsg').textContent=`Uploading ${o==='p'?'portrait':'landscape'}…`;
  const fd=new FormData();fd.append('clip',new Blob([clipBytes(clips[o])]),'clip.dgf');
  const r=await fetch(`/api/dashboard/gif?name=${name}&o=${o}`,{method:'POST',body:fd});
  if(!r.ok){$('gmsg').textContent='Upload failed: '+(await r.text());$('up').disabled=false;return;}}
 $('gmsg').textContent='Uploaded.';status();};
const ONAMES=['landscape','portrait','landscape, upside down','portrait, upside down'];
async function ostatus(first){const s=await (await fetch('/api/dashboard/orientation')).json();
 if(first){$('oauto').checked=s.auto;$('oflip').checked=s.flip;}
 $('ostat').textContent=s.present?`Accelerometer found. Showing ${ONAMES[s.orientation]}; it reads ${s.sensed<0?'flat or moving':ONAMES[s.sensed]} (x ${s.g[0].toFixed(2)}, y ${s.g[1].toFixed(2)}, z ${s.g[2].toFixed(2)} g).`
  :`No accelerometer connected. Showing ${ONAMES[s.orientation]}; turn K2 on the dashboard to rotate by hand.`;
 for(const id of['oup','ofp','ofl'])$(id).disabled=!s.present;}
async function orient(body){await fetch('/api/dashboard/orientation',{method:'POST',body:new URLSearchParams(body)});ostatus(false);}
$('oauto').onchange=()=>orient({auto:$('oauto').checked?1:0});$('oflip').onchange=()=>orient({flip:$('oflip').checked?1:0});
$('oup').onclick=()=>orient({action:'upright'});$('ofp').onclick=()=>orient({action:'flipportrait'});$('ofl').onclick=()=>orient({action:'fliplandscape'});
status(true);ostatus(true);setInterval(()=>status(false),10000);setInterval(()=>ostatus(false),2000);
</script></body></html>)HTML";

inline void sendJson(int code, const String& json) {
  server().sendHeader("Cache-Control", "no-store");
  server().send(code, "application/json", json);
}

inline void handlePage() {
  server().sendHeader("Cache-Control", "no-store");
  server().send_P(200, "text/html", PAGE);
}

inline void handleGet() {
  const bool has = DashWeather::hasLocation();
  // The clock text is JSON-safe apart from its newlines (names and labels are filtered on the way in)
  String clocksJson;
  (void)PFLoopSync::run([&] { clocksJson = DashClocks::format(); });
  clocksJson.replace("\n", "\\n");
  char head[260 + 4 * 128];
  snprintf(head, sizeof head,
           "{\"place\":\"%s\",\"lat\":%.4f,\"lon\":%.4f,\"updated\":%u,\"error\":\"%s\","
           "\"night\":{\"on\":%s,\"start\":%d,\"end\":%d},\"free\":%u,\"clocks\":\"%s\",\"gifs\":[",
           has ? DashWeather::place : "", has ? DashWeather::lat : 0.0f, has ? DashWeather::lon : 0.0f,
           DashWeather::updatedAtMs ? (unsigned)((millis() - DashWeather::updatedAtMs) / 1000) : 0u,
           DashWeather::lastError, DashNight::enabled ? "true" : "false", DashNight::startMin, DashNight::endMin,
           (unsigned)(FFat.totalBytes() - FFat.usedBytes()), clocksJson.c_str());
  String json = head;
  // The clip list belongs to the loop task: copy it there.
  String list;
  (void)PFLoopSync::run([&] {
    if (DashGifs::listDirty) DashGifs::rescan();
    for (int i = 0; i < DashGifs::count; i++) {
      if (i) list += ',';
      list += '"';
      list += DashGifs::names[i];
      list += '"';
    }
  });
  json += list;
  json += "]}";
  sendJson(200, json);
}

inline void handleLocation() {
  if (!server().hasArg("lat") || !server().hasArg("lon")) {
    sendJson(400, "{\"error\":\"lat and lon required\"}");
    return;
  }
  const float la = server().arg("lat").toFloat(), lo = server().arg("lon").toFloat();
  if (la < -90 || la > 90 || lo < -180 || lo > 180) {
    sendJson(400, "{\"error\":\"out of range\"}");
    return;
  }
  // Keep only characters the panel can draw and JSON can carry unescaped.
  const String name = server().arg("place");
  String clean;
  for (size_t i = 0; i < name.length() && clean.length() < 38; i++) {
    const char c = name[i];
    if (c >= 0x20 && c < 0x7f && c != '"' && c != '\\') clean += c;
  }
  DashWeather::saveSettings(la, lo, clean.c_str());
  DashWeather::requestFetch();
  handleGet();
}

inline void handleClocks() {
  const String text = server().arg("clocks");
  if (text.length() > 4 * 128) {
    sendJson(400, "{\"error\":\"too long\"}");
    return;
  }
  int n = 0, lines = 0;
  for (size_t i = 0; i < text.length(); i++) lines += text[i] == '\n';
  if (text.length()) lines++;
  (void)PFLoopSync::run([&] {
    n = DashClocks::parseAll(text.c_str());
    DashClocks::save();
  });
  if (n != lines && lines <= DashClocks::MAX) {
    sendJson(400, "{\"error\":\"some clocks could not be read\"}");
    return;
  }
  sendJson(200, String("{\"ok\":true,\"count\":") + n + "}");
}

inline void handleNight() {
  const int start = DashNight::parseClock(server().arg("start").c_str());
  const int end = DashNight::parseClock(server().arg("end").c_str());
  if (start < 0 || end < 0) {
    sendJson(400, "{\"error\":\"times as HH:MM\"}");
    return;
  }
  const bool on = server().arg("on") == "1";
  (void)PFLoopSync::run([&] { DashNight::save(on, start, end); });
  sendJson(200, "{\"ok\":true}");
}

// ---- GIF clip upload: streamed to a temporary file, checked, then renamed
inline File upFile;
inline bool upFailed = false;
inline char upError[48] = "";
inline uint32_t upBytes = 0;

inline void tmpPath(char* out, size_t n, char o) { snprintf(out, n, "%s/.upload.%c.tmp", DashGifs::DIR, o); }

inline void handleUpload() {
  HTTPUpload& up = server().upload();
  const String name = server().arg("name");
  const String o = server().arg("o");
  char tmp[64];
  tmpPath(tmp, sizeof tmp, o == "p" ? 'p' : 'l');
  if (up.status == UPLOAD_FILE_START) {
    upFailed = false;
    upError[0] = 0;
    upBytes = 0;
    if (!DashGifs::validName(name.c_str()) || (o != "p" && o != "l")) {
      upFailed = true;
      snprintf(upError, sizeof upError, "bad name or orientation");
      return;
    }
    if (!DashGifs::ensureDir()) {
      upFailed = true;
      snprintf(upError, sizeof upError, "cannot create %s", DashGifs::DIR);
      return;
    }
    upFile = FFat.open(tmp, FILE_WRITE);
    if (!upFile) {
      upFailed = true;
      snprintf(upError, sizeof upError, "cannot write");
    }
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (upFailed || !upFile) return;
    if (upFile.write(up.buf, up.currentSize) != up.currentSize) {
      upFailed = true;
      snprintf(upError, sizeof upError, "storage full");
    }
    upBytes += up.currentSize;
  } else if (up.status == UPLOAD_FILE_END || up.status == UPLOAD_FILE_ABORTED) {
    if (upFile) upFile.close();
    if (up.status == UPLOAD_FILE_ABORTED) {
      upFailed = true;
      snprintf(upError, sizeof upError, "upload aborted");
    }
    if (upFailed) FFat.remove(tmp);
  }
}

inline void handleUploadDone() {
  const String name = server().arg("name");
  const bool portrait = server().arg("o") == "p";
  char tmp[64], dst[64];
  tmpPath(tmp, sizeof tmp, portrait ? 'p' : 'l');
  if (upFailed) {
    sendJson(400, String("{\"error\":\"") + upError + "\"}");
    return;
  }
  // Check the header against the size before anything can play it.
  File f = FFat.open(tmp, FILE_READ);
  uint8_t head[12] = {};
  const bool read = f && f.read(head, 12) == 12;
  const uint32_t size = f ? f.size() : 0;
  if (f) f.close();
  const int w = head[4] | head[5] << 8, h = head[6] | head[7] << 8, frames = head[8] | head[9] << 8;
  const bool ok = read && memcmp(head, "DGF1", 4) == 0 && w == (portrait ? 64 : 128) && h == (portrait ? 128 : 64) &&
                  frames >= 1 && frames <= DashGifs::MAX_FRAMES && size == 12u + 2u * frames + (uint32_t)frames * w * h;
  if (!ok) {
    FFat.remove(tmp);
    sendJson(400, "{\"error\":\"not a valid clip\"}");
    return;
  }
  DashGifs::path(dst, sizeof dst, name.c_str(), portrait);
  (void)PFLoopSync::run([&] {
    if (strcmp(DashGifs::player.name, name.c_str()) == 0) DashGifs::player.stop();
    FFat.remove(dst);
    FFat.rename(tmp, dst);
    DashGifs::listDirty = true;
  });
  Serial.printf("[DASH] gif %s (%s): %d frames\n", name.c_str(), portrait ? "portrait" : "landscape", frames);
  sendJson(200, "{\"ok\":true}");
}

inline void handleDelete() {
  const String name = server().arg("name");
  if (!DashGifs::validName(name.c_str())) {
    sendJson(400, "{\"error\":\"bad name\"}");
    return;
  }
  (void)PFLoopSync::run([&] { DashGifs::removeClip(name.c_str()); });
  sendJson(200, "{\"ok\":true}");
}

inline void handleOrientationGet() {
  char json[200];
  const float x = DashAccel::gx, y = DashAccel::gy, z = DashAccel::gz;
  snprintf(json, sizeof json,
           "{\"present\":%s,\"auto\":%s,\"flip\":%s,\"orientation\":%d,\"sensed\":%d,\"g\":[%.2f,%.2f,%.2f]}",
           DashAccel::present ? "true" : "false", DashAccel::autoRotate ? "true" : "false",
           DashAccel::flipPatterns ? "true" : "false", DashState::orientation,
           DashAccel::present ? DashAccel::orientationOf(x, y, z) : -1, x, y, z);
  sendJson(200, json);
}

inline void handleOrientationPost() {
  const String action = server().arg("action");
  bool ok = true;
  (void)PFLoopSync::run([&] {
    if (server().hasArg("auto")) DashAccel::autoRotate = server().arg("auto") == "1";
    if (server().hasArg("flip")) DashAccel::flipPatterns = server().arg("flip") == "1";
    if (action == "upright") ok = DashAccel::calibrateUpright();
    if (action == "flipportrait") DashAccel::uprightSign = -DashAccel::uprightSign;
    if (action == "fliplandscape") DashAccel::landscapeSign = -DashAccel::landscapeSign;
    DashAccel::save();
    // Apply right away instead of waiting for the next turn
    const int o = DashAccel::orientationOf(DashAccel::gx, DashAccel::gy, DashAccel::gz);
    if (DashAccel::present && DashAccel::autoRotate && o >= 0) DashState::orientation = o;
  });
  if (!ok) {
    sendJson(400, "{\"error\":\"hold the panel upright and still\"}");
    return;
  }
  handleOrientationGet();
}

inline void registerRoutes() {
  server().on("/api/dashboard/orientation", HTTP_GET, handleOrientationGet);
  server().on("/api/dashboard/orientation", HTTP_POST, handleOrientationPost);
  server().on("/dashboard", HTTP_GET, handlePage);
  server().on("/api/dashboard", HTTP_GET, handleGet);
  server().on("/api/dashboard", HTTP_POST, handleLocation);
  server().on("/api/dashboard/night", HTTP_POST, handleNight);
  server().on("/api/dashboard/clocks", HTTP_POST, handleClocks);
  server().on("/api/dashboard/gif", HTTP_POST, handleUploadDone, handleUpload);
  server().on("/api/dashboard/gif/delete", HTTP_POST, handleDelete);
}

}  // namespace DashHttp
