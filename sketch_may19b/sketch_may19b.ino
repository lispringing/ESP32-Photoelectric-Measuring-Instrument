#include <WiFi.h>
#include <WebServer.h>

const char* AP_SSID     = "ESP32-Light";
const char* AP_PASSWORD = "12345678";

// 光敏引脚
const int DO_PIN = 4;
const int AO_PIN = 5;

// 反射率传感器 TCRT5000 → 改为 GPIO6
const int REFLECT_PIN = 6;

WebServer server(80);

// 滑动平均滤波
#define FILTER_NUM 15
int buf[FILTER_NUM];
int pos = 0;
int smoothVal = 0;
bool lightState = false;

// 反射率滤波
int refBuf[FILTER_NUM];
int refPos = 0;
int smoothRef = 0;

// 滤波计算（光照）
int getSmoothData() {
  buf[pos++] = analogRead(AO_PIN);
  if(pos >= FILTER_NUM) pos = 0;
  long sum = 0;
  for(int i=0;i<FILTER_NUM;i++) sum += buf[i];
  return sum / FILTER_NUM;
}

// 滤波计算（反射）
int getSmoothReflect() {
  refBuf[refPos++] = analogRead(REFLECT_PIN);
  if(refPos >= FILTER_NUM) refPos = 0;
  long sum = 0;
  for(int i=0;i<FILTER_NUM;i++) sum += refBuf[i];
  return sum / 15;
}

// ADC转勒克斯LUX
float getLux(int val) {
  if (val < 10) val = 10;
  return (4095.0 / val) * 4.2;
}

// JSON数据接口
void getData() {
  // 光照处理
  int correctVal = 4095 - smoothVal;
  int per = map(correctVal, 0, 4095, 0, 100);
  per = constrain(per, 0, 100);
  float lux = getLux(smoothVal);

  // 反射率 正确反向映射
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

// 网页主页 三卡片布局
void webPage() {
  String html = R"HTML(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
<title>光照综合监测</title>
<style>
  *{margin:0;padding:0;box-sizing:border-box;font-family:system-ui,sans-serif}
  body{background:#121212;color:#fff;padding:20px;min-height:100vh}
  .container{max-width:420px;margin:0 auto;gap:20px;display:flex;flex-direction:column}
  .card{background:#1e1e1e;border-radius:18px;padding:25px}
  .card-title{font-size:18px;color:#90caf9;margin-bottom:18px;font-weight:600;border-left:4px solid #2196f3;padding-left:10px}
  
  .lux-num{font-size:52px;color:#ffd700;text-align:center;margin:15px 0}
  .adc-num{font-size:42px;color:#4cd964;text-align:center;margin:15px 0}
  .ref-num{font-size:42px;color:#ff69b4;text-align:center;margin:15px 0}
  
  .desc{text-align:center;color:#aaa;font-size:14px}
  .bar-box{width:100%;height:18px;background:#333;border-radius:9px;overflow:hidden;margin:10px 0}
  .bar{height:100%;background:#4cd964;width:0%;transition:0.4s ease}
  .status-tip{margin-top:12px;text-align:center;font-size:16px}
</style>
</head>
<body>
<div class="container">
  <div class="card">
    <div class="card-title">一、标准光照亮度(LUX)</div>
    <div class="lux-num" id="luxShow">0.0 lx</div>
    <div class="desc">国际标准勒克斯光照强度值</div>
  </div>

  <div class="card">
    <div class="card-title">二、环境相对光强</div>
    <div class="adc-num" id="adcShow">0 / 4095</div>
    <div class="bar-box"><div class="bar" id="pctBar"></div></div>
    <div class="status-tip" id="stateTip">--</div>
  </div>

  <div class="card">
    <div class="card-title">三、物体红外反射率</div>
    <div class="ref-num" id="refShow">0 %</div>
    <div class="desc">数值越高 → 反射越强</div>
  </div>
</div>

<script>
function refreshData(){
  fetch("/d")
  .then(res=>res.json())
  .then(d=>{
    document.getElementById("luxShow").innerText = d.lux + " lx";
    document.getElementById("adcShow").innerText = d.val + " / 4095";
    document.getElementById("pctBar").style.width = d.pct + "%";
    document.getElementById("refShow").innerText = d.reflect + " %";
    
    if(d.sta === 1){
      document.getElementById("stateTip").innerText = "当前环境光线偏弱";
      document.getElementById("stateTip").style.color = "#ff9800";
    }else{
      document.getElementById("stateTip").innerText = "当前环境光线充足";
      document.getElementById("stateTip").style.color = "#4cd964";
    }
  })
}
setInterval(refreshData, 400);
</script>
</body>
</html>
  )HTML";
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
  server.on("/", webPage);
  server.on("/d", getData);
  server.begin();
}

void loop() {
  server.handleClient();
  smoothVal = getSmoothData();
  smoothRef = getSmoothReflect();
  lightState = digitalRead(DO_PIN);
  delay(150);
}