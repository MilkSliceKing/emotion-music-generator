#ifndef WEB_RESOURCES_H
#define WEB_RESOURCES_H

namespace WebResources {

const char* INDEX_HTML = R"rawliteral(<!DOCTYPE html>
<html lang="zh">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Emotion Music Remote</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:-apple-system,"Segoe UI",Roboto,sans-serif;background:#0f0f1a;color:#eee;min-height:100vh}
.container{max-width:640px;margin:0 auto;padding:12px}
h1{text-align:center;font-size:20px;color:#e94560;padding:12px 0}
.card{background:#1a1a2e;border-radius:12px;padding:14px;margin:10px 0}
.stream-wrap{position:relative;width:100%;background:#000;border-radius:8px;overflow:hidden}
.stream-wrap img{width:100%;display:block}
.emotion-big{text-align:center;font-size:36px;font-weight:bold;margin:8px 0;text-transform:capitalize}
.conf-row{display:flex;align-items:center;margin:3px 0}
.conf-row .lbl{width:70px;text-align:right;font-size:12px;margin-right:6px}
.conf-row .bar-bg{flex:1;height:16px;background:#16213e;border-radius:8px;overflow:hidden}
.conf-row .bar-fill{height:100%;border-radius:8px;transition:width .3s}
.conf-row .pct{width:42px;font-size:12px;margin-left:4px}
.params{display:grid;grid-template-columns:1fr 1fr;gap:6px;font-size:13px}
.params span:nth-child(odd){color:#aaa}
.btn-row{display:flex;gap:10px;justify-content:center;flex-wrap:wrap}
.btn{padding:10px 20px;border:none;border-radius:8px;font-size:14px;cursor:pointer;color:#fff;transition:opacity .2s}
.btn:hover{opacity:.8}
.btn-play{background:#e94560}
.btn-auto{background:#0f3460}
.btn-mode{background:#533483}
.btn-pause{background:#e97f45}
.status-line{text-align:center;font-size:12px;color:#888;margin-top:6px}
canvas{width:100%;height:120px;border-radius:6px;background:#16213e}
</style>
</head>
<body>
<div class="container">
<h1>Emotion Music Remote</h1>
<div class="card">
 <div class="stream-wrap"><img id="stream" src="/stream" alt="Live"></div>
</div>
<div class="card">
 <div class="emotion-big" id="emo">--</div>
 <div id="confs"></div>
</div>
<div class="card">
 <div class="params" id="params"></div>
</div>
<div class="card">
 <div class="btn-row">
  <button class="btn btn-play" onclick="api('/api/play')">Play</button>
  <button class="btn btn-pause" onclick="api('/api/pause')">Stop</button>
  <button class="btn btn-auto" onclick="api('/api/auto')">Auto</button>
  <button class="btn btn-mode" onclick="api('/api/mode')">Mode</button>
 </div>
</div>
<div class="card">
 <h3 style="font-size:14px;margin-bottom:6px">Emotion Timeline</h3>
 <canvas id="tl" width="620" height="120"></canvas>
</div>
<div class="status-line" id="status">Connecting...</div>
</div>
<script>
var tlData=[];
var emoColors={Angry:'#ff4444',Disgust:'#ff8800',Fear:'#cc44cc',Happy:'#ffee44',Sad:'#4488ff',Surprised:'#ff8844',Neutral:'#aaaaaa'};
function api(u){fetch(u,{method:'POST'}).catch(function(){})}
function update(){
 fetch('/api/status').then(function(r){return r.json()}).then(function(d){
  document.getElementById('emo').textContent=d.emotion;
  document.getElementById('emo').style.color=emoColors[d.emotion]||'#fff';
  var labels=['Angry','Disgust','Fear','Happy','Sad','Surprise','Neutral'];
  var colors=['#ff4444','#ff8800','#cc44cc','#ffee44','#4488ff','#ff8844','#aaaaaa'];
  var h='';
  for(var i=0;i<7;i++){
   var p=(d.confidences[i]*100).toFixed(1);
   h+='<div class="conf-row"><span class="lbl">'+labels[i]+'</span>'
    +'<div class="bar-bg"><div class="bar-fill" style="width:'+p+'%;background:'+colors[i]+'"></div></div>'
    +'<span class="pct">'+p+'%</span></div>';
  }
  document.getElementById('confs').innerHTML=h;
  var mp=d.music_params||{};
  document.getElementById('params').innerHTML=
   '<span>Key</span><span>'+(mp.key||'-')+' '+(mp.scale||'-')+'</span>'
   +'<span>Tempo</span><span>'+(mp.tempo||'-')+' BPM</span>'
   +'<span>Mode</span><span>'+(d.mode||'-')+'</span>'
   +'<span>FPS</span><span>'+(d.fps||'-')+'</span>';
  tlData.push({t:Date.now(),e:d.emotion});
  if(tlData.length>300)tlData.shift();
  drawTL();
  document.getElementById('status').textContent=
   d.is_playing?'Playing ('+d.mode+')':'Idle | Auto:'+(d.music_enabled?'ON':'OFF');
 }).catch(function(e){
  document.getElementById('status').textContent='Connection lost...';
 });
}
function drawTL(){
 var c=document.getElementById('tl');
 var ctx=c.getContext('2d');
 var w=c.width,h=c.height;
 ctx.clearRect(0,0,w,h);
 if(tlData.length<2)return;
 var dur=60000;
 var now=Date.now();
 for(var i=1;i<tlData.length;i++){
  var x1=((tlData[i-1].t-now+dur)/dur)*w;
  var x2=((tlData[i].t-now+dur)/dur)*w;
  if(x2<0)continue;
  var ci=0;
  var labels=['Angry','Disgust','Fear','Happy','Sad','Surprised','Neutral'];
  for(var j=0;j<7;j++){if(labels[j]==tlData[i].e){ci=j;break}}
  var y=h/2-40+ci*12;
  ctx.beginPath();
  ctx.moveTo(Math.max(0,x1),y);
  ctx.lineTo(x2,y);
  ctx.strokeStyle=emoColors[tlData[i].e]||'#888';
  ctx.lineWidth=3;
  ctx.stroke();
 }
}
setInterval(update,1000);
</script>
</body>
</html>
)rawliteral";

} // namespace WebResources

#endif // WEB_RESOURCES_H
