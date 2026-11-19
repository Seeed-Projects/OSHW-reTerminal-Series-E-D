// Keep FS.h ahead of TFT_eSPI.h: Seeed_GFX defines FS_NO_GLOBALS, and
// WebServer.h uses the unqualified FS type.
#include <FS.h>
#include <SD.h>
#include <WiFi.h>
#include <WebServer.h>

#include "upload_mode.h"

#define UPLOG Serial1

namespace upload_mode {
namespace {

// ----- tunables --------------------------------------------------------------
constexpr uint16_t  kHttpPort       = 80;
constexpr uint32_t  kRedrawIdleMs   = 2500;                     // redraw status when quiet
constexpr uint32_t  kIdleExitMs     = 10UL * 60UL * 1000UL;      // 0 disables
constexpr uint32_t  kExitHoldMs     = 1500;                      // hold to leave
constexpr size_t    kMaxUploadBytes = 24UL * 1024UL * 1024UL;    // per file
constexpr size_t    kMaxListFiles   = 200;
constexpr float     kPeakLimitMb    = 7.4f;

// ----- state -----------------------------------------------------------------
EPaper    *g_panel = nullptr;
WebServer  g_server(kHttpPort);
Config     g_cfg;                       // filled by run() before anything else

#define g_dir           g_cfg.image_dir
#define g_password      g_cfg.password
#define g_exit_hint     g_cfg.exit_hint
#define g_bytes_per_px  g_cfg.bytes_per_px
#define g_exit_pin      g_cfg.exit_key_pin
#define g_buzzer_pin    g_cfg.buzzer_pin

struct UploadState {
  File    file;
  String  path;
  size_t  written = 0;
  bool    active  = false;
  bool    failed  = false;
  String  error;
  String  saved_name;
  int     w = 0;
  int     h = 0;
} g_up;

size_t   g_photo_count   = 0;
int      g_next_index    = 1;
uint32_t g_last_activity = 0;
uint32_t g_last_end_ms   = 0;
bool     g_dirty         = false;
String   g_last_msg      = "WAITING FOR UPLOAD";

// ----- small helpers ---------------------------------------------------------
static String dir_join(const char *dir, const String &name) {
  String p(dir && dir[0] ? dir : "/");
  if (!p.endsWith("/")) p += '/';
  p += name;
  return p;
}

static String base_name(const String &path) {
  const int slash = path.lastIndexOf('/');
  return slash >= 0 ? path.substring(slash + 1) : path;
}

static bool is_supported_image(const String &name) {
  String low = name;
  low.toLowerCase();
  return low.endsWith(".jpg") || low.endsWith(".jpeg") ||
         low.endsWith(".bmp") || low.endsWith(".png");
}

static void beep(int freq = 1500, int ms = 60) {
  if (g_buzzer_pin >= 0) tone(g_buzzer_pin, freq, ms);
}

// Rebuild the photo count and the next free NNN_ prefix in one pass.
static void scan_dir_stats() {
  size_t count = 0;
  int    max_idx = 0;

  File dir = SD.open(g_dir);
  if (dir && dir.isDirectory()) {
    File entry = dir.openNextFile();
    while (entry) {
      if (!entry.isDirectory()) {
        String name = base_name(entry.name());
        String low = name;
        low.toLowerCase();
        if (is_supported_image(low)) {
          ++count;
          int value = 0, i = 0;
          while (i < (int)name.length() && i < 4 &&
                 name[i] >= '0' && name[i] <= '9') {
            value = value * 10 + (name[i] - '0');
            ++i;
          }
          if (i > 0 && i < (int)name.length() && name[i] == '_' && value > max_idx) {
            max_idx = value;
          }
        }
      }
      entry.close();
      entry = dir.openNextFile();
    }
    dir.close();
  }

  g_photo_count = count;
  g_next_index  = max_idx + 1;
  UPLOG.printf("[upload] dir '%s': %u photo(s), next index %03d\n",
               g_dir, (unsigned)count, g_next_index);
  UPLOG.flush();
}

// ASCII-only stem; the numeric prefix carries the ordering anyway.
static String sanitize_stem(const String &filename) {
  String base = base_name(filename);
  String ext  = ".jpg";
  const int dot = base.lastIndexOf('.');
  if (dot > 0) {
    String e = base.substring(dot);
    e.toLowerCase();
    if (e == ".jpg" || e == ".jpeg" || e == ".png" || e == ".bmp") ext = e;
    base = base.substring(0, dot);
  }
  String out;
  for (size_t i = 0; i < base.length() && out.length() < 40; ++i) {
    const char c = base[i];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_') {
      out += c;
    } else if (c == ' ') {
      out += '_';
    }
  }
  if (out.length() == 0) out = "photo";
  return out + ext;
}

static float peak_mb(int w, int h) {
  const double n = (double)w * (double)h;
  return (float)(((double)g_cfg.sprite_bytes + (double)g_bytes_per_px * n) /
                 (1024.0 * 1024.0));
}

// Largest box with the panel's aspect ratio (even sides) whose dither peak still
// fits the configured budget. The upload page scales phone photos down to this.
static void budget_box(int *out_w, int *out_h) {
  const double usable    = (double)g_cfg.budget_bytes - (double)g_cfg.sprite_bytes;
  const double budget_px = usable / (double)g_bytes_per_px;
  const double panel_px  = (double)g_cfg.panel_w * (double)g_cfg.panel_h;
  double k = (budget_px > 0.0 && panel_px > 0.0) ? sqrt(budget_px / panel_px) : 1.0;
  if (k > 1.0) k = 1.0;
  int w = ((int)(g_cfg.panel_w * k)) & ~1;
  int h = ((int)(g_cfg.panel_h * k)) & ~1;
  if (w < 320) w = 320;
  if (h < 240) h = 240;
  *out_w = w;
  *out_h = h;
}

// ----- header sniffing (dimensions only) -------------------------------------
static uint16_t be16(const uint8_t *p) { return (uint16_t)(((uint16_t)p[0] << 8) | p[1]); }
static uint32_t be32(const uint8_t *p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
         ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static bool peek_dims(const String &path, int *out_w, int *out_h) {
  File f = SD.open(path.c_str(), FILE_READ);
  if (!f) return false;

  uint8_t b[64];
  const size_t n = f.read(b, sizeof(b));

  if (n >= 24 && b[0] == 0x89 && b[1] == 'P' && b[2] == 'N' && b[3] == 'G') {
    *out_w = (int)be32(b + 16);
    *out_h = (int)be32(b + 20);
    f.close();
    return true;
  }

  if (n >= 26 && b[0] == 'B' && b[1] == 'M') {
    int32_t w = (int32_t)((uint32_t)b[18] | ((uint32_t)b[19] << 8) |
                          ((uint32_t)b[20] << 16) | ((uint32_t)b[21] << 24));
    int32_t h = (int32_t)((uint32_t)b[22] | ((uint32_t)b[23] << 8) |
                          ((uint32_t)b[24] << 16) | ((uint32_t)b[25] << 24));
    if (h < 0) h = -h;
    *out_w = (int)w;
    *out_h = (int)h;
    f.close();
    return true;
  }

  if (n >= 4 && b[0] == 0xFF && b[1] == 0xD8) {   // JPEG: walk to the first SOF
    f.seek(2);
    uint8_t m[2];
    while (f.read(m, 2) == 2) {
      if (m[0] != 0xFF) continue;
      const uint8_t marker = m[1];
      if (marker == 0xFF) { f.seek(f.position() - 1); continue; }
      if (marker == 0xD8 || marker == 0x01 ||
          (marker >= 0xD0 && marker <= 0xD7)) {
        continue;
      }
      if (marker == 0xD9 || marker == 0xDA) break;
      uint8_t lenb[2];
      if (f.read(lenb, 2) != 2) break;
      const uint16_t len = be16(lenb);
      const bool is_sof = (marker >= 0xC0 && marker <= 0xCF) &&
                          marker != 0xC4 && marker != 0xC8 && marker != 0xCC;
      if (is_sof) {
        uint8_t sof[5];
        if (f.read(sof, 5) != 5) break;
        *out_h = be16(sof + 1);
        *out_w = be16(sof + 3);
        f.close();
        return true;
      }
      if (len < 2) break;
      f.seek(f.position() + (len - 2));
    }
    f.close();
    return false;
  }

  f.close();
  return false;
}

// ----- HTTP: status page -----------------------------------------------------
static const char kPage[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="zh-CN"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>E1003 照片传输</title>
<style>
:root{color-scheme:light dark}
*{box-sizing:border-box}
body{margin:0 auto;padding:22px 18px 40px;max-width:820px;font:16px/1.6 -apple-system,"Segoe UI","Microsoft YaHei",sans-serif}
h1{font-size:21px;margin:0 0 4px}
.sub{opacity:.65;font-size:14px;margin:0}
#drop{margin-top:18px;border:2px dashed #8a8a8a;border-radius:12px;padding:44px 20px;text-align:center;cursor:pointer;transition:.15s}
#drop.on{border-color:#22b377;background:rgba(34,179,119,.10)}
#drop b{font-size:18px}
.row{display:flex;align-items:center;gap:12px;padding:7px 0;border-bottom:1px solid rgba(128,128,128,.25);font-size:14px}
.name{flex:1;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.bar{width:110px;height:6px;background:rgba(128,128,128,.25);border-radius:3px;overflow:hidden;flex:none}
.bar i{display:block;height:100%;width:0;background:#22b377;transition:width .1s}
.st{width:190px;text-align:right;flex:none}
.ok{color:#22a06b}.bad{color:#d14343}.warn{color:#c98500}
h2{font-size:15px;margin:26px 0 4px;opacity:.8}
#list{max-height:280px;overflow:auto}
</style></head><body>
<h1>reTerminal E 系列 · 照片传输</h1>
<p class="sub">已连上 <b id="ssid">…</b>。拖拽或选择照片即可写入 SD 卡的 /img 目录。</p>
<div id="drop"><b>拖拽照片到这里</b><br><span class="sub">或点击选择，支持多选、可直接调起手机相机</span></div>
<input id="picker" type="file" accept="image/*" multiple hidden>
<div id="queue"></div>
<p class="sub" id="hint" style="margin-top:14px">正在读取板子的尺寸上限…</p>
<label class="sub" style="display:block;margin-top:8px"><input type="checkbox" id="cover" checked> 按屏幕比例裁剪（会裁掉多余的边）</label>
<label class="sub" style="display:block;margin-top:4px"><input type="checkbox" id="raw"> 上传原图（不缩放、不裁剪，用于已经调好尺寸的图）</label>
<h2>卡上的照片 <span id="count"></span></h2>
<div id="list"></div>
<p class="sub" style="margin-top:26px">传完后长按机身 <b>右键</b> 约 2 秒退出，板子会重启并开始轮播。</p>
<script>
const $=s=>document.querySelector(s);
const drop=$('#drop'),picker=$('#picker'),queue=$('#queue');
let MAXW=1180,MAXH=885;
drop.onclick=()=>picker.click();
picker.onchange=()=>{uploadAll([...picker.files]);picker.value=''};
['dragenter','dragover'].forEach(ev=>drop.addEventListener(ev,e=>{e.preventDefault();drop.classList.add('on')}));
['dragleave','drop'].forEach(ev=>drop.addEventListener(ev,e=>{e.preventDefault();drop.classList.remove('on')}));
drop.addEventListener('drop',e=>uploadAll([...e.dataTransfer.files]));

async function uploadAll(files){
  files=files.filter(f=>f.type.startsWith('image/')||/\.(jpe?g|png|bmp)$/i.test(f.name));
  for(const f of files){
    const row=document.createElement('div');row.className='row';
    row.innerHTML='<span class="name"></span><span class="bar"><i></i></span><span class="st">准备中</span>';
    row.querySelector('.name').textContent=f.name;
    queue.prepend(row);
    const bar=row.querySelector('i'),st=row.querySelector('.st');
    let blob=f,name=f.name,tag='';
    if(!$('#raw').checked){
      st.textContent='缩放中…';
      const out=await downscale(f);
      if(out&&out.error){st.className='st bad';st.textContent=out.error;continue}
      if(out&&out.blob){blob=out.blob;name=out.name;tag=' · '+out.ow+'×'+out.oh+'→'+out.w+'×'+out.h;}
    }
    try{
      const r=await send(blob,name,p=>{bar.style.width=(p*100)+'%';st.textContent=Math.round(p*100)+'%'});
      if(!r.ok){st.className='st bad';st.textContent=r.error||'失败';continue}
      const kb=(r.bytes/1024).toFixed(0)+' KB';
      const info=tag?tag:(r.w?(' · '+r.w+'×'+r.h):'');
      if(r.too_big){st.className='st warn';st.textContent=kb+info+' · ⚠ 太大';}
      else{st.className='st ok';st.textContent='✓ '+kb+info;}
    }catch(e){st.className='st bad';st.textContent='网络错误';}
  }
  refresh();
}

// Downscale on the phone/laptop before the bytes go over Wi-Fi. Keeps the
// board's PSRAM budget in check without a JPEG encoder on the ESP32.
function downscale(f){
  return new Promise(async res=>{
    let bmp;
    try{
      if(window.createImageBitmap){
        try{bmp=await createImageBitmap(f,{imageOrientation:'from-image'})}
        catch(e){bmp=await createImageBitmap(f)}
      }else{
        bmp=await new Promise((ok,bad)=>{
          const im=new Image();
          im.onload=()=>ok(im);im.onerror=bad;
          im.src=URL.createObjectURL(f);
        });
      }
    }catch(e){res({error:'无法解码（HEIC？请先转成 JPEG）'});return}
    const ow=bmp.width,oh=bmp.height;
    const cover=$('#cover').checked;
    let w,h,sx,sy,sw,sh;
    if(cover){
      // Crop to the panel aspect ratio. The firmware does not upscale to full screen.
      const s=Math.max(MAXW/ow,MAXH/oh);
      w=MAXW;h=MAXH;
      sw=w/s;sh=h/s;sx=(ow-sw)/2;sy=(oh-sh)/2;
    }else{
      if(ow<=MAXW&&oh<=MAXH){res(null);return}
      const s=Math.min(MAXW/ow,MAXH/oh);
      w=Math.max(2,Math.round(ow*s));h=Math.max(2,Math.round(oh*s));
      sx=0;sy=0;sw=ow;sh=oh;
    }
    const c=document.createElement('canvas');c.width=w;c.height=h;
    const x=c.getContext('2d');
    x.fillStyle='#fff';x.fillRect(0,0,w,h);
    x.imageSmoothingEnabled=true;x.imageSmoothingQuality='high';
    x.drawImage(bmp,sx,sy,sw,sh,0,0,w,h);
    if(bmp.close)bmp.close();
    c.toBlob(b=>{
      if(!b){res({error:'压缩失败'});return}
      const base=(f.name.replace(/\.[^.]+$/,'').replace(/[^\w-]+/g,'_').slice(0,40))||'photo';
      res({blob:b,name:base+'.jpg',w:w,h:h,ow:ow,oh:oh});
    },'image/jpeg',0.92);
  });
}

function send(f,name,onProgress){
  return new Promise((res,rej)=>{
    const fd=new FormData();fd.append('file',f,name);
    const xhr=new XMLHttpRequest();
    xhr.open('POST','/upload');
    xhr.upload.onprogress=e=>{if(e.lengthComputable)onProgress(e.loaded/e.total)};
    xhr.onload=()=>{try{res(JSON.parse(xhr.responseText))}catch(e){rej(e)}};
    xhr.onerror=()=>rej(new Error('net'));
    xhr.send(fd);
  });
}
async function refresh(){
  try{
    const r=await (await fetch('/api/list',{cache:'no-store'})).json();
    if(r.maxW&&r.maxH){MAXW=r.maxW;MAXH=r.maxH}
    $('#hint').textContent='照片会先缩到 '+MAXW+'×'+MAXH+'；勾选“按屏幕比例裁剪”时按这个比例裁剪，不勾则保留完整画面。固件不会把图片放大到整屏，四周会有留白。';
    $('#ssid').textContent=r.ssid||'E1003-Photo';
    $('#count').textContent='（'+r.count+' 张，剩余 '+(r.free/1048576).toFixed(0)+' MB）';
    $('#list').innerHTML=r.files.map(f=>{
      const warn=f.big?'<span class="st warn">⚠ 会被跳过</span>':'<span class="st"></span>';
      const dim=f.w?(' · '+f.w+'×'+f.h):'';
      return '<div class="row"><span class="name">'+f.n+'</span><span>'+(f.s/1024).toFixed(0)+' KB'+dim+'</span>'+warn+'</div>';
    }).join('')||'<div class="row"><span class="name">（空）</span></div>';
  }catch(e){}
}
refresh();
</script></body></html>)rawliteral";

static void handle_root() {
  g_last_activity = millis();
  g_server.sendHeader("Cache-Control", "no-store");
  g_server.send_P(200, "text/html; charset=utf-8", kPage);
}

static void handle_not_found() {
  g_last_activity = millis();
  g_server.send(404, "text/plain; charset=utf-8", "not found");
}

static void handle_list() {
  g_last_activity = millis();

  int bw = 0, bh = 0;
  budget_box(&bw, &bh);

  String json;
  json.reserve(64 + 96 * 16);
  char buf[128];

  snprintf(buf, sizeof(buf),
           "{\"count\":%u,\"free\":%llu,\"total\":%llu,\"maxW\":%d,\"maxH\":%d,"
           "\"ssid\":\"%s\",",
           (unsigned)g_photo_count,
           (unsigned long long)(SD.totalBytes() - SD.usedBytes()),
           (unsigned long long)SD.totalBytes(), bw, bh,
           g_cfg.ssid ? g_cfg.ssid : "");
  json += buf;
  json += "\"files\":[";

  File dir = SD.open(g_dir);
  size_t listed = 0;
  if (dir && dir.isDirectory()) {
    File entry = dir.openNextFile();
    while (entry && listed < kMaxListFiles) {
      if (!entry.isDirectory()) {
        const String name = base_name(entry.name());
        if (is_supported_image(name)) {
          const String full = dir_join(g_dir, name);
          int w = 0, h = 0;
          const bool known = peek_dims(full, &w, &h);
          const float peak = known ? peak_mb(w, h) : 0.0f;
          const bool big = known && peak > kPeakLimitMb;

          if (listed) json += ',';
          snprintf(buf, sizeof(buf),
                   "{\"n\":\"%s\",\"s\":%lu,\"w\":%d,\"h\":%d,\"peak\":%.2f,\"big\":%s}",
                   name.c_str(), (unsigned long)entry.size(), w, h, peak,
                   big ? "true" : "false");
          json += buf;
          ++listed;
        }
      }
      entry.close();
      entry = dir.openNextFile();
    }
    dir.close();
  }
  json += "]}";
  g_server.send(200, "application/json", json);
}

// ----- HTTP: upload ----------------------------------------------------------
// ESP32 core 3.x hands the chunk callback no arguments; the parser state is
// available through WebServer::upload(). The common example signature
// `void handleUpload(HTTPUpload&)` no longer compiles against THandlerFunction.
static void on_upload_chunk() {
  HTTPUpload &up = g_server.upload();

  if (up.status == UPLOAD_FILE_START) {
    g_last_activity = millis();
    g_up.active  = true;
    g_up.failed  = false;
    g_up.written = 0;
    g_up.w      = 0;
    g_up.h      = 0;
    g_up.error  = "";
    g_up.saved_name = "";

    const String stem = sanitize_stem(up.filename);
    char prefix[8];
    bool found = false;
    // Only create a new file. Never open an existing path with FILE_WRITE,
    // because that would silently truncate an older photo on the card.
    for (int idx = g_next_index; idx <= 9999; ++idx) {
      snprintf(prefix, sizeof(prefix), "%03d", idx);
      const String candidate = dir_join(g_dir, String(prefix) + "_" + stem);
      if (!SD.exists(candidate.c_str())) {
        g_up.path    = candidate;
        g_next_index = idx;
        found        = true;
        break;
      }
    }
    if (!found) {
      g_up.failed = true;
      g_up.error  = "SD 卡上没有可用的文件名";
      UPLOG.println("[upload] no free filename");
      UPLOG.flush();
      return;
    }
    g_up.file = SD.open(g_up.path.c_str(), FILE_WRITE);
    if (!g_up.file) {
      g_up.failed = true;
      g_up.error  = "无法在 SD 卡上创建文件";
      UPLOG.printf("[upload] open failed: %s\n", g_up.path.c_str());
      UPLOG.flush();
    } else {
      UPLOG.printf("[upload] receiving -> %s\n", g_up.path.c_str());
      UPLOG.flush();
    }
    return;
  }

  if (up.status == UPLOAD_FILE_WRITE) {
    g_last_activity = millis();
    if (g_up.failed) return;

    if (g_up.written + up.currentSize > kMaxUploadBytes) {
      g_up.failed = true;
      g_up.error  = "文件超过 24 MB";
      if (g_up.file) g_up.file.close();
      SD.remove(g_up.path.c_str());
      return;
    }
    const size_t n = g_up.file.write(up.buf, up.currentSize);
    g_up.written += n;
    if (n != (size_t)up.currentSize) {
      g_up.failed = true;
      g_up.error  = "SD 卡写入失败（空间不足？）";
      if (g_up.file) g_up.file.close();
      SD.remove(g_up.path.c_str());
    }
    return;
  }

  if (up.status == UPLOAD_FILE_END || up.status == UPLOAD_FILE_ABORTED) {
    if (g_up.file) g_up.file.close();

    if (up.status == UPLOAD_FILE_ABORTED) {
      g_up.failed = true;
      g_up.error  = "传输被中断";
    } else if (!g_up.failed && g_up.written == 0) {
      g_up.failed = true;
      g_up.error  = "空文件";
    }

    if (g_up.failed) {
      if (g_up.path.length()) SD.remove(g_up.path.c_str());
      UPLOG.printf("[upload] failed: %s (%s)\n", g_up.path.c_str(), g_up.error.c_str());
    } else {
      int w = 0, h = 0;
      if (peek_dims(g_up.path, &w, &h)) { g_up.w = w; g_up.h = h; }
      ++g_next_index;
      ++g_photo_count;
      g_up.saved_name = base_name(g_up.path);
      g_last_msg = "SAVED: " + g_up.saved_name;
      UPLOG.printf("[upload] saved %s  %lu bytes  %dx%d  peak~%.2f MB\n",
                   g_up.path.c_str(), (unsigned long)g_up.written, g_up.w, g_up.h,
                   (g_up.w && g_up.h) ? peak_mb(g_up.w, g_up.h) : 0.0f);
    }
    UPLOG.flush();

    g_up.active   = false;
    g_last_end_ms = millis();
    g_dirty       = true;
  }
}

static void handle_upload_done() {
  g_last_activity = millis();

  String json = "{\"ok\":";
  json += g_up.failed ? "false" : "true";
  if (g_up.failed) {
    json += ",\"error\":\"" + g_up.error + "\"}";
  } else {
    char buf[128];
    const float peak = (g_up.w && g_up.h) ? peak_mb(g_up.w, g_up.h) : 0.0f;
    snprintf(buf, sizeof(buf),
             ",\"name\":\"%s\",\"bytes\":%lu,\"w\":%d,\"h\":%d,\"peak\":%.2f,\"too_big\":%s}",
             g_up.saved_name.c_str(), (unsigned long)g_up.written,
             g_up.w, g_up.h, peak, (peak > kPeakLimitMb) ? "true" : "false");
    json += buf;
  }
  g_server.send(200, "application/json", json);

  g_last_end_ms = millis();
  g_dirty       = true;
}

// ----- status screen ---------------------------------------------------------
static void draw_status() {
  if (!g_panel) return;

  EPaper &d = *g_panel;
  const int W = d.width();
  const int H = d.height();
  const int cx = W / 2;

  d.fillSprite(g_cfg.color_bg);
  d.setTextDatum(MC_DATUM);
  d.setTextFont(1);

  d.setTextColor(g_cfg.color_fg, g_cfg.color_bg, true);
  d.setTextSize(7);
  d.drawString("WI-FI UPLOAD MODE", cx, 120);

  d.setTextSize(5);
  d.drawString(String("AP  : ") + WiFi.softAPSSID(), cx, 300);
  d.drawString(String("PASS: ") +
                   ((g_password && g_password[0]) ? g_password : "(open)"),
               cx, 390);
  d.drawString(String("OPEN: http://") + WiFi.softAPIP().toString(), cx, 480);

  char info[96];
  const uint64_t free_bytes = SD.totalBytes() - SD.usedBytes();
  snprintf(info, sizeof(info), "PHOTOS: %u      FREE: %llu MB",
           (unsigned)g_photo_count, (unsigned long long)(free_bytes / (1024ULL * 1024ULL)));
  d.setTextColor(g_cfg.color_muted, g_cfg.color_bg, true);
  d.setTextSize(4);
  d.drawString(info, cx, 660);

  d.setTextColor(g_cfg.color_fg, g_cfg.color_bg, true);
  d.drawString(g_last_msg, cx, 760);

  d.setTextColor(g_cfg.color_muted, g_cfg.color_bg, true);
  d.setTextSize(4);
  d.drawString(g_exit_hint, cx, H - 150);

  UPLOG.println("[upload] status screen refresh");
  UPLOG.flush();
  d.update();
}

}  // namespace

// ----- main loop -------------------------------------------------------------
void run(EPaper &panel, const Config &cfg) {
  g_panel = &panel;
  g_cfg   = cfg;

  // Defensive defaults for anything the sketch left at zero.
  if (!g_cfg.image_dir || !g_cfg.image_dir[0]) g_cfg.image_dir = "/";
  if (!g_cfg.password) g_cfg.password = "";
  if (!g_cfg.exit_hint || !g_cfg.exit_hint[0]) g_cfg.exit_hint = "HOLD KEY 2s TO EXIT";
  if (!g_cfg.panel_w || !g_cfg.panel_h) {
    g_cfg.panel_w = panel.width();
    g_cfg.panel_h = panel.height();
  }
  if (!g_cfg.sprite_bytes) {
    g_cfg.sprite_bytes = (uint32_t)g_cfg.panel_w * (uint32_t)g_cfg.panel_h / 2;
  }
  if (!g_cfg.bytes_per_px)  g_cfg.bytes_per_px = 4;
  if (!g_cfg.budget_bytes)  g_cfg.budget_bytes = 7u * 1024u * 1024u;

  pinMode(g_exit_pin, INPUT_PULLUP);
  if (g_buzzer_pin >= 0) {
    pinMode(g_buzzer_pin, OUTPUT);
    digitalWrite(g_buzzer_pin, LOW);
  }

  if (strcmp(g_dir, "/") != 0 && !SD.exists(g_dir)) {
    UPLOG.printf("[upload] creating folder %s -> %s\n",
                 g_dir, SD.mkdir(g_dir) ? "ok" : "failed");
  }
  scan_dir_stats();

  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);

  const bool ok = (g_cfg.password[0]) ? WiFi.softAP(g_cfg.ssid, g_cfg.password)
                                            : WiFi.softAP(g_cfg.ssid);
  if (!ok) {
    UPLOG.println("[upload] softAP FAILED -- restarting in 5 s");
    UPLOG.flush();
  } else {
    UPLOG.printf("[upload] AP '%s' up, http://%s\n",
                 g_cfg.ssid, WiFi.softAPIP().toString().c_str());
    UPLOG.flush();
  }

  g_server.on("/", HTTP_GET, handle_root);
  g_server.on("/api/list", HTTP_GET, handle_list);
  g_server.on("/upload", HTTP_POST, handle_upload_done, on_upload_chunk);
  g_server.onNotFound(handle_not_found);
  g_server.begin();

  g_last_activity = millis();
  g_last_end_ms   = millis();
  g_last_msg      = ok ? "WAITING FOR UPLOAD" : "AP START FAILED";
  draw_status();
  beep(1800, 70);

  // The key that opened this mode is still held. Wait for the release, or the
  // exit hold would fire ~1.5 s later and bounce straight back to the slideshow.
  const uint32_t release_deadline = millis() + 8000UL;
  while (digitalRead(g_exit_pin) == LOW &&
         (int32_t)(release_deadline - millis()) > 0) {
    delay(10);
  }

  bool     was_down = (digitalRead(g_exit_pin) == LOW);
  uint32_t down_ms  = millis();

  for (;;) {
    g_server.handleClient();

    // Hold the middle key to leave upload mode.
    const bool down = (digitalRead(g_exit_pin) == LOW);
    if (down && !was_down) down_ms = millis();
    if (down && was_down && (uint32_t)(millis() - down_ms) >= kExitHoldMs) {
      UPLOG.println("[upload] exit key held -- restarting");
      UPLOG.flush();
      beep(1200, 60);
      delay(80);
      beep(900, 90);
      delay(150);
      ESP.restart();
    }
    was_down = down;

    // Refresh the e-paper only when a transfer has finished and things are
    // quiet; a full Gray16 refresh blocks for ~2.2 s.
    if (g_dirty && !g_up.active &&
        (uint32_t)(millis() - g_last_end_ms) > kRedrawIdleMs) {
      g_dirty = false;
      draw_status();
    }

    if (kIdleExitMs &&
        (uint32_t)(millis() - g_last_activity) > kIdleExitMs &&
        WiFi.softAPgetStationNum() == 0) {
      UPLOG.println("[upload] idle timeout -- restarting");
      UPLOG.flush();
      ESP.restart();
    }

    delay(2);
  }
}

}  // namespace upload_mode