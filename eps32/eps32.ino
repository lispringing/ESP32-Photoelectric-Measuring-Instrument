#include <WiFi.h>
#include <WebServer.h>

const char* AP_SSID     = "ESP32-Light";
const char* AP_PASSWORD = "12345678";

// 引脚定义
const int DO_PIN = 4;
const int AO_PIN = 5;
const int REFLECT_PIN = 6;

WebServer server(80);

// 滤波参数
#define FILTER_NUM 8
int buf[FILTER_NUM];
int pos = 0;
int smoothVal = 0;
bool lightState = false;

int refBuf[FILTER_NUM];
int refPos = 0;
int smoothRef = 0;

// ========== 仿真变量：色温、色相HSV、透光率 ==========
float colorTemp = 4500.0;   // 2700~6500K
int hueVal = 180;           // 0~360
int satVal = 75;            // 60~95
int briVal = 80;            // 70~90
float transmittance = 62.0; // 0~100% 透光率

// 光谱波段
float spUV = 4.0, spBlue = 20.0, spGreen = 30.0, spRed = 26.0, spIR = 20.0;
float refRate = 50.0;
float refStable = 95.0;

// 光照均值滤波
int getSmoothData() {
  buf[pos++] = analogRead(AO_PIN);
  if(pos >= FILTER_NUM) pos = 0;
  long sum = 0;
  for(int i=0;i<FILTER_NUM;i++) sum += buf[i];
  return sum / FILTER_NUM;
}

// 反射均值滤波
int getSmoothReflect() {
  refBuf[refPos++] = analogRead(REFLECT_PIN);
  if(refPos >= FILTER_NUM) refPos = 0;
  long sum = 0;
  for(int i=0;i<FILTER_NUM;i++) sum += refBuf[i];
  return sum / FILTER_NUM;
}

// 勒克斯换算
float getLux(int val) {
  if (val < 10) val = 10;
  return (4095.0 / val) * 4.2;
}

// ====================== 小幅平滑更新仿真数据 ======================
void updateRandomData() {
  // 色温每次±10以内小幅变动
  colorTemp += random(-10, 11) / 10.0;
  colorTemp = constrain(colorTemp, 2700.0, 6500.0);

  // 色相每次±2缓慢偏移，循环0~360
  hueVal += random(-2, 3);
  if(hueVal < 0) hueVal += 360;
  if(hueVal >= 360) hueVal -= 360;

  // 饱和度小幅±1
  satVal += random(-1, 2);
  satVal = constrain(satVal, 60, 95);

  // 明度小幅±1
  briVal += random(-1, 2);
  briVal = constrain(briVal, 70, 90);

  // 透光率每次±0.2微小波动
  transmittance += random(-2, 3) / 10.0;
  transmittance = constrain(transmittance, 0.0, 100.0);

  // 光谱微小微调，总和维持100%
  spUV += random(-1, 2) / 10.0;
  spBlue += random(-1, 2) / 10.0;
  spGreen += random(-1, 2) / 10.0;
  spRed += random(-1, 2) / 10.0;
  spUV = constrain(spUV, 2.0, 8.0);
  spBlue = constrain(spBlue, 15.0, 25.0);
  spGreen = constrain(spGreen, 25.0, 35.0);
  spRed = constrain(spRed, 20.0, 30.0);
  spIR = 100.0 - spUV - spBlue - spGreen - spRed;
  if(spIR < 0) spIR = 0;

  // 反射参数小幅变化
  refRate = map(smoothRef, 0, 4095, 10, 95) + random(-3, 4)/10.0;
  refStable += random(-1, 2)/10.0;
  refStable = constrain(refStable, 90.0, 99.0);
}

// JSON数据接口
void getData() {
  updateRandomData();
  int correctVal = 4095 - smoothVal;
  int per = map(correctVal, 0, 4095, 0, 100);
  per = constrain(per, 0, 100);
  float lux = getLux(smoothVal);

  int reflectValue = smoothRef;
  int reflectPercent = map(reflectValue, 4095, 1000, 0, 100);
  reflectPercent = constrain(reflectPercent, 0, 100);

  String json = "{";
  json += "\"val\":" + String(correctVal) + ",";
  json += "\"lux\":" + String(lux,1) + ",";
  json += "\"pct\":" + String(per) + ",";
  json += "\"sta\":" + String(lightState ? "1" : "0") + ",";
  json += "\"reflect\":" + String(reflectPercent) + ",";
  json += "\"refRate\":" + String(refRate,1) + ",";
  json += "\"refStable\":" + String(refStable,1) + ",";

  // 新增三大板块数据
  json += "\"colorTemp\":" + String(colorTemp,1) + ",";
  json += "\"hue\":" + String(hueVal) + ",";
  json += "\"sat\":" + String(satVal) + ",";
  json += "\"bri\":" + String(briVal) + ",";
  json += "\"trans\":" + String(transmittance,1) + ",";

  json += "\"spUV\":" + String(spUV,1) + ",";
  json += "\"spBlue\":" + String(spBlue,1) + ",";
  json += "\"spGreen\":" + String(spGreen,1) + ",";
  json += "\"spRed\":" + String(spRed,1) + ",";
  json += "\"spIR\":" + String(spIR,1);
  json += "}";
  server.send(200, "application/json", json);
}

// 网页页面（包含独立色温、色相、透光率三个完整板块）
void webPage() {
  String html = "";
  html += "<!DOCTYPE html><html lang='zh-CN'><head>";
  html += "<meta charset='utf-8'>";
  html += "<meta name='viewport' content='width=device-width,initial-scale=1.0,user-scalable=no'>";
  html += "<title>光照综合监测｜色温色相透光光谱红外分析</title>";
  html += "<style>";
  html += "*{margin:0;padding:0;box-sizing:border-box;font-family:system-ui,sans-serif}";
  html += "body{background:#121212;color:#fff;padding-bottom:85px;min-height:100vh}";
  html += ".main{max-width:430px;margin:0 auto;padding:15px}";
  html += ".card{background:#1e1e1e;border-radius:20px;padding:26px;margin-bottom:25px}";
  html += ".card-title{font-size:18px;color:#90caf9;font-weight:600;border-left:4px solid #2196f3;padding-left:12px;margin-bottom:20px}";
  html += ".lux-num{font-size:54px;color:#ffd700;text-align:center;margin:16px 0}";
  html += ".light-num{font-size:44px;color:#4cd964;text-align:center;margin:16px 0}";
  html += ".ref-num{font-size:44px;color:#ff69b4;text-align:center;margin:16px 0}";
  html += ".temp-num{font-size:44px;color:#ff7875;text-align:center;margin:16px 0}";
  html += ".hue-num{font-size:44px;color:#b37feb;text-align:center;margin:16px 0}";
  html += ".trans-num{font-size:44px;color:#5cdbd3;text-align:center;margin:16px 0}";
  html += ".spec-num{font-size:36px;color:#40a9ff;text-align:center;margin:12px 0}";
  html += ".desc{text-align:center;color:#aaa;font-size:14px}";
  html += ".bar-box{width:100%;height:18px;background:#333;border-radius:9px;overflow:hidden;margin:12px 0}";
  html += ".bar{height:100%;width:0%;background:#4cd964;transition:0.3s}";
  html += ".state{text-align:center;font-size:16px;margin-top:10px}";
  html += ".page{display:none}.page.active{display:block}";
  html += ".wave-item{margin-bottom:22px}";
  html += ".wave-name{font-size:15px;color:#ccc;margin-bottom:10px}";
  html += ".wave-canvas{width:100%;height:120px;background:#1a1a1a;border-radius:12px;display:block}";
  html += ".set-text{line-height:1.9;color:#ccc;font-size:15px}";
  html += ".nav{position:fixed;bottom:0;left:0;right:0;height:72px;background:rgba(28,28,28,0.96);backdrop-filter:blur(12px);border-top-left-radius:22px;border-top-right-radius:22px;max-width:430px;margin:0 auto;display:flex}";
  html += ".nav-item{flex:1;display:flex;flex-direction:column;align-items:center;justify-content:center;color:#888;font-size:12px;cursor:pointer}";
  html += ".nav-item.active{color:#2196f3}";
  html += ".nav-icon{font-size:24px;margin-bottom:4px}";
  html += ".record-btn{width:100%;height:50px;border-radius:12px;border:none;font-size:16px;font-weight:500;margin-bottom:20px;cursor:pointer}";
  html += ".btn-start{background:#2196f3;color:white}";
  html += ".btn-stop{background:#f44336;color:white}";
  html += ".loading-mask{position:fixed;inset:0;background:rgba(0,0,0,.86);display:none;align-items:center;justify-content:center;z-index:9999;padding:18px}";
  html += ".loading-box{width:100%;max-width:360px;background:#1f1f1f;border-radius:16px;padding:22px;text-align:center;box-shadow:0 10px 35px rgba(0,0,0,.45)}";
  html += ".loading-title{font-size:20px;color:#90caf9;font-weight:700;margin-bottom:12px}";
  html += ".loading-status{font-size:15px;color:#ddd;margin-bottom:14px;min-height:22px}";
  html += ".loading-progress{width:100%;height:10px;background:#333;border-radius:99px;overflow:hidden}";
  html += ".loading-bar{height:100%;width:0%;background:linear-gradient(90deg,#2196f3,#4cd964);transition:width .1s linear}";
  html += ".loading-time{margin-top:10px;color:#9e9e9e;font-size:13px}";
  html += ".spec-grid{display:grid;grid-template-columns:1fr 1fr;gap:12px;margin-top:15px}";
  html += ".spec-item{background:#252525;border-radius:12px;padding:15px;text-align:center}";
  html += ".spec-label{font-size:13px;color:#aaa;margin-bottom:6px}";
  html += "</style></head><body>";

  html += "<div class='main'>";
  html += "<div class='page active' id='p1'>";

  // 1.基础光照
  html += "<div class='card'><div class='card-title'>一、标准光照亮度(LUX)</div><div class='lux-num' id='lux'>0.0 lx</div><div class='desc'>国际标准勒克斯光照强度值</div></div>";
  html += "<div class='card'><div class='card-title'>二、环境相对光强</div><div class='light-num' id='val'>0 / 4095</div><div class='bar-box'><div class='bar' id='pct'></div></div><div class='state' id='tip'>--</div></div>";

  // 2.红外反射
  html += "<div class='card'><div class='card-title'>三、红外反射率综合分析</div>";
  html += "<div class='ref-num' id='ref'>0 %</div>";
  html += "<div class='desc'>综合反射率：数值越高表面反光能力越强</div>";
  html += "<div style='margin-top:15px;color:#ccc;font-size:15px;line-height:1.8'>";
  html += "实时反射效率：<span id='refRate' style='color:#ff69b4'>0.0%</span><br/>";
  html += "反射稳定性：<span id='refStable' style='color:#ff69b4'>0.0%</span>";
  html += "</div></div>";

  // ========== 新增板块1：透光率 ==========
  html += "<div class='card'><div class='card-title'>四、介质透光率监测</div>";
  html += "<div class='trans-num' id='transShow'>0.0 %</div>";
  html += "<div class='desc'>材料透光百分比，数值越高透光性能越好</div>";
  html += "</div>";

  // ========== 新增板块2：色温 ==========
  html += "<div class='card'><div class='card-title'>五、光源色温分析</div>";
  html += "<div class='temp-num' id='colorTemp'>0.0 K</div>";
  html += "<div class='desc' id='tempDesc'>色温识别中...</div>";
  html += "</div>";

  // ========== 新增板块3：色相HSV ==========
  html += "<div class='card'><div class='card-title'>六、图像色相色彩分析</div>";
  html += "<div class='hue-num' id='hueShow'>0°</div>";
  html += "<div style='margin-top:15px;color:#ccc;font-size:15px;line-height:1.8;text-align:center'>";
  html += "饱和度：<span id='satShow' style='color:#b37feb'>0%</span>　　";
  html += "明度：<span id='briShow' style='color:#b37feb'>0%</span>";
  html += "</div>";
  html += "<div class='desc' style='margin-top:10px'>HSV色彩空间实时解析</div>";
  html += "</div>";

  // 光谱
  html += "<div class='card'><div class='card-title'>七、光源光谱成分分析</div>";
  html += "<div class='spec-grid'>";
  html += "<div class='spec-item'><div class='spec-label'>紫外波段</div><div class='spec-num' id='spUV'>0.0%</div></div>";
  html += "<div class='spec-item'><div class='spec-label'>蓝光波段</div><div class='spec-num' id='spBlue'>0.0%</div></div>";
  html += "<div class='spec-item'><div class='spec-label'>绿光波段</div><div class='spec-num' id='spGreen'>0.0%</div></div>";
  html += "<div class='spec-item'><div class='spec-label'>红光波段</div><div class='spec-num' id='spRed'>0.0%</div></div>";
  html += "<div class='spec-item' style='grid-column:1/3'><div class='spec-label'>红外波段</div><div class='spec-num' id='spIR'>0.0%</div></div>";
  html += "</div>";
  html += "<div class='desc' style='margin-top:15px'>全波段光谱占比实时监测</div>";
  html += "</div>";
  html += "</div>";

  // 波形记录页（增加透光率波形画布）
  html += "<div class='page' id='p2'>";
  html += "<div class='card'>";
  html += "<div class='card-title'>实时数据波形记录</div>";
  html += "<button class='record-btn btn-start' id='recordBtn'>点击开始采集波形</button>";
  html += "<div class='wave-item'><div class='wave-name'>光照亮度波形</div><canvas class='wave-canvas' id='c1'></canvas></div>";
  html += "<div class='wave-item'><div class='wave-name'>相对光强波形</div><canvas class='wave-canvas' id='c2'></canvas></div>";
  html += "<div class='wave-item'><div class='wave-name'>反射率波形</div><canvas class='wave-canvas' id='c3'></canvas></div>";
  html += "<div class='wave-item'><div class='wave-name'>透光率波形</div><canvas class='wave-canvas' id='c6'></canvas></div>";
  html += "<div class='wave-item'><div class='wave-name'>色温变化波形</div><canvas class='wave-canvas' id='c4'></canvas></div>";
  html += "<div class='wave-item'><div class='wave-name'>色相角度波形</div><canvas class='wave-canvas' id='c5'></canvas></div>";
  html += "</div></div>";

  // 设置页
  html += "<div class='page' id='p3'>";
  html += "<div class='card'><div class='card-title'>系统功能设置</div><div class='set-text'>";
  html += "1.基础：光照强度、相对光强、红外反射<br/>";
  html += "2.三大新增板块：透光率、色温、HSV色相分析<br/>";
  html += "3.光谱多波段解析，波形记录全部维度数据<br/>";
  html += "4.仿真数值平缓小幅渐变，无剧烈跳变<br/>";
  html += "5.8点滑动滤波，150ms底层刷新，400ms网页更新";
  html += "</div></div></div>";
  html += "</div>";

  // 底部导航
  html += "<div class='nav'>";
  html += "<div class='nav-item active'><div class='nav-icon'>📊</div><div>数据</div></div>";
  html += "<div class='nav-item'><div class='nav-icon'>📝</div><div>记录</div></div>";
  html += "<div class='nav-item'><div class='nav-icon'>⚙️</div><div>设置</div></div>";
  html += "</div>";

  // 加载遮罩
  html += "<div class='loading-mask' id='loadingMask'><div class='loading-box'><div class='loading-title'>模块启动中</div><div class='loading-status' id='loadingStatus'>初始化传感器...</div><div class='loading-progress'><div class='loading-bar' id='loadingBar'></div></div><div class='loading-time' id='loadingTime'>0%</div></div></div>";

  // JS逻辑
  html += "<script>";
  html += "document.addEventListener('DOMContentLoaded',function(){";
  html += "const pages=document.querySelectorAll('.page');const navs=document.querySelectorAll('.nav-item');";
  html += "const recordBtn=document.getElementById('recordBtn');const loadingMask=document.getElementById('loadingMask');";
  html += "const loadingBar=document.getElementById('loadingBar');const loadingStatus=document.getElementById('loadingStatus');const loadingTime=document.getElementById('loadingTime');";
  html += "function switchPage(i){pages.forEach(p=>p.classList.remove('active'));navs.forEach(n=>n.classList.remove('active'));pages[i].classList.add('active');navs[i].classList.add('active');}";
  html += "navs.forEach((n,i)=>n.onclick=()=>switchPage(i));";

  html += "const maxPoints=60;let dataLux=[],dataLight=[],dataRef=[],dataTrans=[],dataTemp=[],dataHue=[];";
  html += "let isRecording=false,isBootLoading=false,loadingTimer=null;";
  html += "let c1=document.getElementById('c1'),c2=document.getElementById('c2'),c3=document.getElementById('c3'),c6=document.getElementById('c6'),c4=document.getElementById('c4'),c5=document.getElementById('c5');";
  html += "function resizeCanvas(){c1.width=c1.offsetWidth;c2.width=c2.offsetWidth;c3.width=c3.offsetWidth;c6.width=c6.offsetWidth;c4.width=c4.offsetWidth;c5.width=c5.offsetWidth;";
  html += "c1.height=c1.offsetHeight;c2.height=c2.offsetHeight;c3.height=c3.offsetHeight;c6.height=c6.offsetHeight;c4.height=c4.offsetHeight;c5.height=c5.offsetHeight;}";
  html += "resizeCanvas();window.onresize=resizeCanvas;";
  html += "let ctx1=c1.getContext('2d'),ctx2=c2.getContext('2d'),ctx3=c3.getContext('2d'),ctx6=c6.getContext('2d'),ctx4=c4.getContext('2d'),ctx5=c5.getContext('2d');";
  html += "function drawWave(ctx,arr,color){ctx.clearRect(0,0,ctx.canvas.width,ctx.canvas.height);if(!arr.length)return;ctx.beginPath();ctx.strokeStyle=color;ctx.lineWidth=2.5;let step=ctx.canvas.width/maxPoints;arr.forEach((v,i)=>{let y=ctx.canvas.height-(v/100*ctx.canvas.height);i==0?ctx.moveTo(i*step,y):ctx.lineTo(i*step,y);});ctx.stroke();}";
  html += "function limit(v,min,max){return v<min?min:(v>max?max:v);}";
  html += "function setRecordBtn(start){recordBtn.className='record-btn '+(start?'btn-start':'btn-stop');recordBtn.innerText=start?'开始采集波形':'停止并清空波形';}";
  html += "function clearWave(){dataLux=[];dataLight=[];dataRef=[];dataTrans=[];dataTemp=[];dataHue=[];drawWave(ctx1,dataLux,'#ffd700');drawWave(ctx2,dataLight,'#4cd964');drawWave(ctx3,dataRef,'#ff69b4');drawWave(ctx6,dataTrans,'#5cdbd3');drawWave(ctx4,dataTemp,'#ff7875');drawWave(ctx5,dataHue,'#b37feb');}";
  html += "function showLoading(){isBootLoading=true;loadingMask.style.display='flex';loadingBar.style.width='0%';loadingTime.innerText='0%';let st=['初始化传感器','启动透光模块','启动色温模块','启动色相光谱'];let s=Date.now();loadingTimer=setInterval(()=>{let p=Math.min(100,Math.floor((Date.now()-s)/50));loadingBar.style.width=p+'%';loadingTime.innerText=p+'%';loadingStatus.innerText=st[Math.min(3,Math.floor(p/25))];if(p>=100){clearInterval(loadingTimer);loadingMask.style.display='none';isBootLoading=false;}},50);}";

  // 数据更新逻辑（同步更新透光/色温/色相）
  html += "function updateData(){fetch('/d').then(r=>r.json()).then(res=>{";
  html += "if(isBootLoading){document.getElementById('tip').innerText='启动中';return;}";
  html += "document.getElementById('lux').innerText=res.lux+' lx';";
  html += "document.getElementById('val').innerText=res.val+' / 4095';";
  html += "document.getElementById('pct').style.width=res.pct+'%';";
  html += "document.getElementById('ref').innerText=res.reflect+' %';";
  html += "document.getElementById('refRate').innerText=res.refRate+'%';";
  html += "document.getElementById('refStable').innerText=res.refStable+'%';";
  html += "document.getElementById('tip').innerText=res.sta==1?'光线偏弱':'光线充足';";

  // 透光率更新
  html += "document.getElementById('transShow').innerText=res.trans+' %';";

  // 色温更新+文字说明
  html += "document.getElementById('colorTemp').innerText=res.colorTemp+' K';";
  html += "let ttxt='';if(res.colorTemp<3000)ttxt='暖黄光 温馨';else if(res.colorTemp<4500)ttxt='暖白光 舒适';else if(res.colorTemp<5500)ttxt='正白光 办公';else ttxt='冷白光 高清';";
  html += "document.getElementById('tempDesc').innerText=ttxt;";

  // 色相HSV更新
  html += "document.getElementById('hueShow').innerText=res.hue+'°';";
  html += "document.getElementById('satShow').innerText=res.sat+'%';";
  html += "document.getElementById('briShow').innerText=res.bri+'%';";

  // 光谱
  html += "document.getElementById('spUV').innerText=res.spUV+'%';";
  html += "document.getElementById('spBlue').innerText=res.spBlue+'%';";
  html += "document.getElementById('spGreen').innerText=res.spGreen+'%';";
  html += "document.getElementById('spRed').innerText=res.spRed+'%';";
  html += "document.getElementById('spIR').innerText=res.spIR+'%';";

  // 波形存储绘制
  html += "if(isRecording){let lx=limit(res.lux/1200*100,0,100);dataLux.push(lx);dataLight.push(res.pct);dataRef.push(res.reflect);dataTrans.push(res.trans);";
  html += "let tp=limit((res.colorTemp-2700)/3800*100,0,100);let hu=limit(res.hue/360*100,0,100);dataTemp.push(tp);dataHue.push(hu);";
  html += "[dataLux,dataLight,dataRef,dataTrans,dataTemp,dataHue].forEach(arr=>arr.length>maxPoints&&arr.shift());";
  html += "drawWave(ctx1,dataLux,'#ffd700');drawWave(ctx2,dataLight,'#4cd964');drawWave(ctx3,dataRef,'#ff69b4');drawWave(ctx6,dataTrans,'#5cdbd3');drawWave(ctx4,dataTemp,'#ff7875');drawWave(ctx5,dataHue,'#b37feb');}";
  html += "});}";

  // 采集按钮事件
  html += "recordBtn.onclick=()=>{if(!isRecording){isRecording=true;setRecordBtn(false);showLoading();}else{isRecording=false;clearInterval(loadingTimer);loadingMask.style.display='none';clearWave();setRecordBtn(true);}};";
  html += "setInterval(updateData,400);";
  html += "});</script></body></html>";
  server.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);
  pinMode(DO_PIN, INPUT);
  pinMode(AO_PIN, INPUT);
  pinMode(REFLECT_PIN, INPUT);

  for(int i=0;i<FILTER_NUM;i++) {
    buf[i] = 0;
    refBuf[i] = 0;
  }

  WiFi.softAP(AP_SSID, AP_PASSWORD);
  IPAddress ip = WiFi.softAPIP();
  Serial.println("ESP32 AP启动成功");
  Serial.print("SSID: "); Serial.println(AP_SSID);
  Serial.print("访问IP: "); Serial.println(ip);

  server.on("/", webPage);
  server.on("/d", getData);
  server.begin();
  Serial.println("Web服务已就绪");
}

void loop() {
  server.handleClient();
  smoothVal = getSmoothData();
  smoothRef = getSmoothReflect();
  lightState = digitalRead(DO_PIN);
  updateRandomData();
  delay(150);
}