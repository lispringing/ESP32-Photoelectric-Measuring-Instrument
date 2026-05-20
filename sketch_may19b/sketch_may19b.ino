#include <WiFi.h>
#include <WebServer.h>

const char* AP_SSID     = "ESP32-Light";
const char* AP_PASSWORD = "12345678";

// 引脚定义（完全不变）
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

// 光照滤波
int getSmoothData() {
  buf[pos++] = analogRead(AO_PIN);
  if(pos >= FILTER_NUM) pos = 0;
  long sum = 0;
  for(int i=0;i<FILTER_NUM;i++) sum += buf[i];
  return sum / FILTER_NUM;
}

// 反射滤波
int getSmoothReflect() {
  refBuf[refPos++] = analogRead(REFLECT_PIN);
  if(refPos >= FILTER_NUM) refPos = 0;
  long sum = 0;
  for(int i=0;i<FILTER_NUM;i++) sum += refBuf[i];
  return sum / FILTER_NUM;
}

// 计算勒克斯
float getLux(int val) {
  if (val < 10) val = 10;
  return (4095.0 / val) * 4.2;
}

// 数据接口
void getData() {
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
  json += "\"reflect\":" + String(reflectPercent);
  json += "}";
  server.send(200, "application/json", json);
}

// 主页面（导航100%可点击版）
void webPage() {
  String html = "";
  
  // 头部
  html += "<!DOCTYPE html><html lang='zh-CN'><head>";
  html += "<meta charset='utf-8'>";
  html += "<meta name='viewport' content='width=device-width,initial-scale=1.0,user-scalable=no'>";
  html += "<title>光照综合监测</title>";
  
  // CSS样式
  html += "<style>";
  html += "*{margin:0;padding:0;box-sizing:border-box;font-family:system-ui,sans-serif}";
  html += "body{background:#121212;color:#fff;padding-bottom:85px;min-height:100vh}";
  html += ".main{max-width:430px;margin:0 auto;padding:15px}";
  html += ".card{background:#1e1e1e;border-radius:20px;padding:26px;margin-bottom:25px}";
  html += ".card-title{font-size:18px;color:#90caf9;font-weight:600;border-left:4px solid #2196f3;padding-left:12px;margin-bottom:20px}";
  html += ".lux-num{font-size:54px;color:#ffd700;text-align:center;margin:16px 0}";
  html += ".light-num{font-size:44px;color:#4cd964;text-align:center;margin:16px 0}";
  html += ".ref-num{font-size:44px;color:#ff69b4;text-align:center;margin:16px 0}";
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
  // 采集按钮样式
  html += ".record-btn{width:100%;height:50px;border-radius:12px;border:none;font-size:16px;font-weight:500;margin-bottom:20px;cursor:pointer}";
  html += ".btn-start{background:#2196f3;color:white}";
  html += ".btn-stop{background:#f44336;color:white}";
  html += "</style></head><body>";

  // 页面内容
  html += "<div class='main'>";
  
  // 数据页
  html += "<div class='page active' id='p1'>";
  html += "<div class='card'><div class='card-title'>一、标准光照亮度(LUX)</div><div class='lux-num' id='lux'>0.0 lx</div><div class='desc'>国际标准勒克斯光照强度值</div></div>";
  html += "<div class='card'><div class='card-title'>二、环境相对光强</div><div class='light-num' id='val'>0 / 4095</div><div class='bar-box'><div class='bar' id='pct'></div></div><div class='state' id='tip'>--</div></div>";
  html += "<div class='card'><div class='card-title'>三、物体红外反射率</div><div class='ref-num' id='ref'>0 %</div><div class='desc'>数值越高 表面反射能力越强</div></div>";
  html += "</div>";

  // 记录页（加了采集按钮）
  html += "<div class='page' id='p2'>";
  html += "<div class='card'>";
  html += "<div class='card-title'>实时数据波形记录</div>";
  // 波形采集按钮
  html += "<button class='record-btn btn-start' id='recordBtn'>点击开始采集波形</button>";
  html += "<div class='wave-item'><div class='wave-name'>光照亮度波形</div><canvas class='wave-canvas' id='c1'></canvas></div>";
  html += "<div class='wave-item'><div class='wave-name'>相对光强波形</div><canvas class='wave-canvas' id='c2'></canvas></div>";
  html += "<div class='wave-item'><div class='wave-name'>反射率波形</div><canvas class='wave-canvas' id='c3'></canvas></div>";
  html += "</div></div>";

  // 设置页
  html += "<div class='page' id='p3'>";
  html += "<div class='card'><div class='card-title'>系统功能设置</div><div class='set-text'>";
  html += "1. 本设备支持光照强度、相对光强、红外反射率实时采集监测<br><br>";
  html += "2. 波形页面自动记录数据变化曲线，直观查看数值波动趋势<br><br>";
  html += "3. 可后期拓展：色温检测、显色指数、薄膜透光率检测模块<br><br>";
  html += "4. 设备默认采样频率400ms，内置8级数据滤波平滑降噪<br><br>";
  html += "5. 开发板供电不足时，可使用闲置GPIO引脚输出3.3V拓展供电";
  html += "</div></div></div>";
  html += "</div>";

  // 底部导航
  html += "<div class='nav'>";
  html += "<div class='nav-item active'><div class='nav-icon'>📊</div><div>数据</div></div>";
  html += "<div class='nav-item'><div class='nav-icon'>📝</div><div>记录</div></div>";
  html += "<div class='nav-item'><div class='nav-icon'>⚙️</div><div>设置</div></div>";
  html += "</div>";

  // JavaScript逻辑
  html += "<script>";
  html += "document.addEventListener('DOMContentLoaded', function() {";
  html += "const pages=document.querySelectorAll('.page');";
  html += "const navs=document.querySelectorAll('.nav-item');";
  html += "const recordBtn=document.getElementById('recordBtn');";
  
  // 页面切换
  html += "function switchPage(index) {";
  html += "pages.forEach(p=>p.classList.remove('active'));";
  html += "navs.forEach(n=>n.classList.remove('active'));";
  html += "pages[index].classList.add('active');";
  html += "navs[index].classList.add('active');";
  html += "}";
  
  html += "navs.forEach((item, index) => {";
  html += "item.addEventListener('click', () => switchPage(index));";
  html += "});";
  
  // 波形变量
  html += "const maxPoints=60;";
  html += "let dataLux=[], dataLight=[], dataRef=[];";
  html += "let isRecording=false;";
  html += "let recordTimer=null;";
  html += "let c1=document.getElementById('c1'), c2=document.getElementById('c2'), c3=document.getElementById('c3');";
  
  html += "function resizeCanvas() {";
  html += "c1.width=c1.offsetWidth; c1.height=c1.offsetHeight;";
  html += "c2.width=c2.offsetWidth; c2.height=c2.offsetHeight;";
  html += "c3.width=c3.offsetWidth; c3.height=c3.offsetHeight;";
  html += "}";
  html += "resizeCanvas();";
  html += "window.addEventListener('resize', resizeCanvas);";
  
  html += "let ctx1=c1.getContext('2d'), ctx2=c2.getContext('2d'), ctx3=c3.getContext('2d');";
  
  // 绘制波形
  html += "function drawWave(ctx, arr, color) {";
  html += "ctx.clearRect(0,0,ctx.canvas.width,ctx.canvas.height);";
  html += "if(arr.length===0)return;";
  html += "ctx.beginPath();";
  html += "ctx.strokeStyle=color;";
  html += "ctx.lineWidth=2.5;";
  html += "let step=ctx.canvas.width/maxPoints;";
  html += "arr.forEach((val,i)=>{let y=ctx.canvas.height-(val/100*ctx.canvas.height);i==0?ctx.moveTo(i*step,y):ctx.lineTo(i*step,y);});";
  html += "ctx.stroke();";
  html += "}";
  
  html += "function limit(v, min, max) {return v<min?min:(v>max?max:v);}";
  
  // 数据更新（只在采集时运行）
  html += "function updateData() {";
  html += "fetch('/d').then(r=>r.json()).then(res=>{";
  html += "document.getElementById('lux').innerText=res.lux+' lx';";
  html += "document.getElementById('val').innerText=res.val+' / 4095';";
  html += "document.getElementById('pct').style.width=res.pct+'%';";
  html += "document.getElementById('ref').innerText=res.reflect+' %';";
  html += "if(res.sta==1){document.getElementById('tip').innerText='当前环境光线偏弱';document.getElementById('tip').style.color='#ff9800';}";
  html += "else{document.getElementById('tip').innerText='当前环境光线充足';document.getElementById('tip').style.color='#4cd964';}";
  // 只有开启采集才保存波形数据
  html += "if(isRecording){";
  html += "let luxRatio=limit(res.lux/1200*100,0,100);";
  html += "dataLux.push(luxRatio); dataLight.push(res.pct); dataRef.push(res.reflect);";
  html += "if(dataLux.length>maxPoints)dataLux.shift();";
  html += "if(dataLight.length>maxPoints)dataLight.shift();";
  html += "if(dataRef.length>maxPoints)dataRef.shift();";
  html += "drawWave(ctx1,dataLux,'#ffd700');";
  html += "drawWave(ctx2,dataLight,'#4cd964');";
  html += "drawWave(ctx3,dataRef,'#ff69b4');";
  html += "}";
  html += "});";
  html += "}";
  
  // 按钮切换采集状态
  html += "recordBtn.addEventListener('click',function(){";
  html += "isRecording=!isRecording;";
  html += "if(isRecording){";
  html += "recordBtn.innerText='停止采集并清空波形';";
  html += "recordBtn.className='record-btn btn-stop';";
  html += "}else{";
  html += "dataLux=[];dataLight=[];dataRef=[];";
  html += "drawWave(ctx1,dataLux,'#ffd700');";
  html += "drawWave(ctx2,dataLight,'#4cd964');";
  html += "drawWave(ctx3,dataRef,'#ff69b4');";
  html += "recordBtn.innerText='点击开始采集波形';";
  html += "recordBtn.className='record-btn btn-start';";
  html += "}";
  html += "});";
  
  // 基础数据始终刷新，波形只在采集时记录
  html += "setInterval(updateData, 400);";
  html += "});";
  html += "</script></body></html>";

  server.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);
  pinMode(DO_PIN, INPUT);
  pinMode(REFLECT_PIN, INPUT);
  
  for(int i=0;i<FILTER_NUM;i++) {
    buf[i] = 0;
    refBuf[i] = 0;
  }

  WiFi.softAP(AP_SSID, AP_PASSWORD);
  IPAddress ip = WiFi.softAPIP();
  Serial.println("");
  Serial.println("WiFi启动成功！");
  Serial.print("SSID: ");
  Serial.println(AP_SSID);
  Serial.print("访问地址: ");
  Serial.println(ip);

  server.on("/", webPage);
  server.on("/d", getData);
  server.begin();
  Serial.println("Web服务器已启动");
}

void loop() {
  server.handleClient();
  smoothVal = getSmoothData();
  smoothRef = getSmoothReflect();
  lightState = digitalRead(DO_PIN);
  delay(150);
}