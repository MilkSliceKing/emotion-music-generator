#ifndef WEB_RESOURCES_H
#define WEB_RESOURCES_H

namespace WebResources {

const char* INDEX_HTML = R"rawliteral(<!DOCTYPE html>
<html lang="zh">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>情绪音乐生成器 - 远程控制</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:-apple-system,"Segoe UI","Microsoft YaHei",Roboto,sans-serif;background:#0f0f1a;color:#eee;min-height:100vh}
.container{max-width:640px;margin:0 auto;padding:12px}
h1{text-align:center;font-size:20px;color:#e94560;padding:12px 0}
.card{background:#1a1a2e;border-radius:12px;padding:14px;margin:10px 0}
.stream-wrap{position:relative;width:100%;background:#000;border-radius:8px;overflow:hidden}
.stream-wrap img{width:100%;display:block}
.emotion-big{text-align:center;font-size:36px;font-weight:bold;margin:8px 0}
.conf-row{display:flex;align-items:center;margin:3px 0}
.conf-row .lbl{width:50px;text-align:right;font-size:12px;margin-right:6px}
.conf-row .bar-bg{flex:1;height:16px;background:#16213e;border-radius:8px;overflow:hidden}
.conf-row .bar-fill{height:100%;border-radius:8px;transition:width .3s}
.conf-row .pct{width:42px;font-size:12px;margin-left:4px}
.params{display:grid;grid-template-columns:1fr 1fr;gap:6px;font-size:13px}
.params span:nth-child(odd){color:#aaa}
.btn-row{display:flex;gap:8px;justify-content:center;flex-wrap:wrap}
.btn{padding:10px 18px;border:none;border-radius:8px;font-size:13px;cursor:pointer;color:#fff;transition:all .15s}
.btn:hover{opacity:.85;transform:scale(1.03)}
.btn:active{transform:scale(.97)}
.btn-play{background:#e94560}
.btn-stop{background:#e97f45}
.btn-auto-on{background:#2d8f4e}
.btn-auto-off{background:#0f3460}
.btn-mode-synth{background:#533483}
.btn-mode-local{background:#8f2d5e}
.status-line{text-align:center;font-size:12px;color:#888;margin-top:6px}
canvas{width:100%;height:120px;border-radius:6px;background:#16213e}
.tabs{display:flex;gap:4px;margin-bottom:8px}
.tab{padding:6px 14px;border:1px solid #333;border-radius:6px;cursor:pointer;font-size:12px;color:#888}
.tab.active{background:#1a1a2e;color:#eee;border-color:#e94560}
.log-box{background:#111;border-radius:6px;padding:8px;font-size:11px;font-family:monospace;max-height:200px;overflow-y:auto;color:#aaa;white-space:pre-wrap}
.summary{font-size:13px;line-height:1.6}
.summary b{color:#e94560}
</style>
</head>
<body>
<div class="container">
<h1>情绪音乐生成器</h1>

<div class="card">
 <div class="stream-wrap"><img id="stream" src="/stream" alt="实时画面"></div>
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
  <button class="btn btn-play" id="btnPlay" onclick="doPlay()">播放</button>
  <button class="btn btn-stop" onclick="api('/api/pause')">停止</button>
  <button class="btn btn-auto-on" id="btnAuto" onclick="api('/api/auto')">自动: 开</button>
  <button class="btn btn-mode-synth" id="btnMode" onclick="api('/api/mode')">模式: 合成</button>
 </div>
 <div class="status-line" id="status">连接中...</div>
</div>

<div class="card">
 <h3 style="font-size:14px;margin-bottom:8px">情绪时间线</h3>
 <canvas id="tl" width="620" height="120"></canvas>
</div>

<div class="card">
 <div class="tabs">
  <div class="tab active" onclick="showTab(this,'tabLog')">日志</div>
  <div class="tab" onclick="showTab(this,'tabSummary');loadSummary()">日报</div>
  <div class="tab" onclick="showTab(this,'tabWeek');loadWeek()">周报</div>
 </div>
 <div id="tabLog"><div class="log-box" id="logBox">加载中...</div></div>
 <div id="tabSummary" style="display:none"><div class="summary" id="summaryBox">加载中...</div></div>
 <div id="tabWeek" style="display:none"><div class="summary" id="weekBox">加载中...</div></div>
</div>
</div>

<script>
var tlData=[];
var emoMap={Angry:'愤怒',Disgust:'厌恶',Fear:'恐惧',Happy:'开心',Sad:'悲伤',Surprised:'惊讶',Neutral:'中性'};
var emoColors={Angry:'#ff4444',Disgust:'#ff8800',Fear:'#cc44cc',Happy:'#ffee44',Sad:'#4488ff',Surprised:'#ff8844',Neutral:'#aaaaaa'};
var curState={is_playing:false,music_enabled:true,mode:'synth'};

function api(u){fetch(u,{method:'POST'}).catch(function(){})}
function doPlay(){api('/api/play')}

function update(){
 fetch('/api/status').then(function(r){return r.json()}).then(function(d){
  curState=d;
  var cn=emoMap[d.emotion]||d.emotion;
  document.getElementById('emo').textContent=cn;
  document.getElementById('emo').style.color=emoColors[d.emotion]||'#fff';

  var labels=['Angry','Disgust','Fear','Happy','Sad','Surprise','Neutral'];
  var cnLabels=['愤怒','厌恶','恐惧','开心','悲伤','惊讶','中性'];
  var colors=['#ff4444','#ff8800','#cc44cc','#ffee44','#4488ff','#ff8844','#aaaaaa'];
  var h='';
  for(var i=0;i<7;i++){
   var p=(d.confidences[i]*100).toFixed(1);
   h+='<div class="conf-row"><span class="lbl">'+cnLabels[i]+'</span>'
    +'<div class="bar-bg"><div class="bar-fill" style="width:'+p+'%;background:'+colors[i]+'"></div></div>'
    +'<span class="pct">'+p+'%</span></div>';
  }
  document.getElementById('confs').innerHTML=h;

  var mp=d.music_params||{};
  var scaleMap={major:'大调',minor:'小调',blues:'布鲁斯'};
  var scaleCn=scaleMap[mp.scale]||mp.scale||'-';
  document.getElementById('params').innerHTML=
   '<span>调性</span><span>'+(mp.key||'-')+' '+scaleCn+'</span>'
   +'<span>速度</span><span>'+(mp.tempo||'-')+' BPM</span>'
   +'<span>模式</span><span>'+(d.mode==='synth'?'合成':'本地音乐')+'</span>'
   +'<span>帧率</span><span>'+(d.fps?d.fps.toFixed(1):'-')+'</span>';

  var ba=document.getElementById('btnAuto');
  ba.textContent='自动: '+(d.music_enabled?'开':'关');
  ba.className='btn '+(d.music_enabled?'btn-auto-on':'btn-auto-off');
  var bm=document.getElementById('btnMode');
  bm.textContent='模式: '+(d.mode==='synth'?'合成':'本地');
  bm.className='btn '+(d.mode==='synth'?'btn-mode-synth':'btn-mode-local');
  var bp=document.getElementById('btnPlay');
  bp.textContent=d.is_playing?'播放中...':'播放';

  tlData.push({t:Date.now(),e:d.emotion});
  if(tlData.length>300)tlData.shift();
  drawTL();
  document.getElementById('status').textContent=
   (d.face_detected?'已检测到人脸':'未检测到人脸')+' | 帧率: '+(d.fps?d.fps.toFixed(0):'-');
 }).catch(function(e){
  document.getElementById('status').textContent='连接断开...';
 });
}

function drawTL(){
 var c=document.getElementById('tl');
 var ctx=c.getContext('2d');
 var w=c.width,h=c.height;
 ctx.clearRect(0,0,w,h);
 if(tlData.length<2)return;
 var dur=60000;var now=Date.now();
 var labels=['Angry','Disgust','Fear','Happy','Sad','Surprised','Neutral'];
 var cnLabels=['愤怒','厌恶','恐惧','开心','悲伤','惊讶','中性'];
 for(var i=1;i<tlData.length;i++){
  var x1=((tlData[i-1].t-now+dur)/dur)*w;
  var x2=((tlData[i].t-now+dur)/dur)*w;
  if(x2<0)continue;
  var ci=6;
  for(var j=0;j<7;j++){if(labels[j]==tlData[i].e){ci=j;break}}
  var y=h/2-42+ci*13;
  ctx.beginPath();ctx.moveTo(Math.max(0,x1),y);ctx.lineTo(x2,y);
  ctx.strokeStyle=emoColors[tlData[i].e]||'#888';ctx.lineWidth=4;ctx.stroke();
 }
 for(var j=0;j<7;j++){
  ctx.fillStyle=['#ff4444','#ff8800','#cc44cc','#ffee44','#4488ff','#ff8844','#aaaaaa'][j];
  ctx.fillRect(w-60,h/2-42+j*13-4,8,8);
  ctx.fillStyle='#999';ctx.font='9px sans-serif';
  ctx.fillText(cnLabels[j],w-48,h/2-42+j*13+4);
 }
}

function showTab(el,id){
 var tabs=document.querySelectorAll('.tab');
 tabs.forEach(function(t){t.classList.remove('active')});
 el.classList.add('active');
 ['tabLog','tabSummary','tabWeek'].forEach(function(tid){
  document.getElementById(tid).style.display=tid===id?'block':'none';
 });
}

function loadSummary(){
 var d=new Date();var ds=d.getFullYear()+'-'+String(d.getMonth()+1).padStart(2,'0')+'-'+String(d.getDate()).padStart(2,'0');
 fetch('/api/summary?date='+ds).then(function(r){return r.json()}).then(function(d){
  if(d.error){document.getElementById('summaryBox').textContent=d.error;return}
  var labels=['Angry','Disgust','Fear','Happy','Sad','Surprised','Neutral'];
  var cnLabels=['愤怒','厌恶','恐惧','开心','悲伤','惊讶','中性'];
  var colors=['#ff4444','#ff8800','#cc44cc','#ffee44','#4488ff','#ff8844','#aaaaaa'];
  var h='<b>日期:</b> '+d.date+'<br><b>检测总次数:</b> '+d.total_detections;
  var domCn=emoMap[d.dominant_emotion]||d.dominant_emotion;
  h+='<br><b>主导情绪:</b> <span style="color:'+(emoColors[d.dominant_emotion]||'#fff')+'">'+domCn+'</span>';
  if(d.percentages){
   h+='<br><br>';
   for(var i=0;i<7;i++){
    var k=labels[i];var v=d.percentages[k]||0;
    h+='<span style="color:'+colors[i]+'">'+cnLabels[i]+'</span>: '+v.toFixed(1)+'% &nbsp;';
   }
  }
  h+='<br><br><b>情绪切换:</b> '+(d.transitions||0)+'次 <b>触发播放:</b> '+(d.music_triggered_count||0)+'次';
  document.getElementById('summaryBox').innerHTML=h;
 }).catch(function(e){document.getElementById('summaryBox').textContent='加载失败'});
}

function loadWeek(){
 fetch('/api/summary?range=week').then(function(r){return r.json()}).then(function(d){
  if(!d.days){document.getElementById('weekBox').textContent='暂无数据';return}
  var h='';
  d.days.forEach(function(day){
   var dom=day.dominant||'N/A';
   var domCn=emoMap[dom]||dom;
   var col=emoColors[dom]||'#888';
   h+='<b>'+day.date+'</b>: '+day.total+'次检测, 主导: <span style="color:'+col+'">'+domCn+'</span>';
   if(day.distribution){
    h+=' (';
    var labels=['Angry','Disgust','Fear','Happy','Sad','Surprised','Neutral'];
    var cnLabels=['愤怒','厌恶','恐惧','开心','悲伤','惊讶','中性'];
    var parts=[];
    for(var i=0;i<7;i++){
     var v=day.distribution[labels[i]]||0;
     if(v>0)parts.push(cnLabels[i]+':'+v);
    }
    h+=parts.join(', ');
    h+=')';
   }
   h+='<br>';
  });
  document.getElementById('weekBox').innerHTML=h||'暂无数据';
 }).catch(function(e){document.getElementById('weekBox').textContent='加载失败'});
}

setInterval(update,1000);
update();
loadLog();
function loadLog(){
 fetch('/api/log').then(function(r){return r.text()}).then(function(t){
  document.getElementById('logBox').textContent=t||'暂无日志';
  var box=document.getElementById('logBox');box.scrollTop=box.scrollHeight;
 }).catch(function(){});
}
setInterval(loadLog,10000);
</script>
</body>
</html>
)rawliteral";

} // namespace WebResources

#endif // WEB_RESOURCES_H
