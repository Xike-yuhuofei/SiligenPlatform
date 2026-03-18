#include "DispensingVisualizer.h"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace Siligen {
std::string DispensingVisualizer::EscapeJSON(const std::string& str) {
    std::ostringstream oss;
    for (char c : str) {
        switch (c) {
            case '\"':
                oss << "\\\"";
                break;
            case '\\':
                oss << "\\\\";
                break;
            case '\n':
                oss << "\\n";
                break;
            case '\r':
                oss << "\\r";
                break;
            case '\t':
                oss << "\\t";
                break;
            default:
                oss << c;
                break;
        }
    }
    return oss.str();
}

std::string DispensingVisualizer::SerializeToJSON(
    const std::vector<TrajectoryPoint>& trajectory_points,
    const std::vector<Shared::Types::VisualizationTriggerPoint>* planned_triggers) {
    std::ostringstream json;
    json << std::fixed << std::setprecision(3);

    std::vector<Point2D> triggers;
    std::vector<float32> trigger_times;
    const bool use_planned_triggers = planned_triggers != nullptr;
    bool planned_has_time = false;

    if (use_planned_triggers) {
        triggers.reserve(planned_triggers->size());
        trigger_times.reserve(planned_triggers->size());
        planned_has_time = true;
        for (const auto& tp : *planned_triggers) {
            triggers.push_back(tp.position);
            trigger_times.push_back(tp.time_s);
            if (!std::isfinite(tp.time_s)) {
                planned_has_time = false;
            }
        }
    } else {
        triggers.reserve(trajectory_points.size());
        trigger_times.reserve(trajectory_points.size());
    }

    float32 total_dispense_length = 0.0f;
    float32 total_rapid_length = 0.0f;

    struct PreviewSegment {
        bool dispensing = true;
        std::vector<TrajectoryPoint> points;
        float32 length = 0.0f;
        float32 start_time = 0.0f;
        float32 end_time = 0.0f;
    };

    std::vector<PreviewSegment> segments;
    if (!trajectory_points.empty()) {
        PreviewSegment current;
        current.dispensing = trajectory_points.front().enable_position_trigger;
        current.start_time = trajectory_points.front().timestamp;
        current.end_time = trajectory_points.front().timestamp;
        current.points.push_back(trajectory_points.front());
        if (!use_planned_triggers && trajectory_points.front().enable_position_trigger) {
            triggers.push_back(trajectory_points.front().position);
            trigger_times.push_back(trajectory_points.front().timestamp);
        }

        for (size_t i = 1; i < trajectory_points.size(); ++i) {
            const auto& prev = trajectory_points[i - 1];
            const auto& curr = trajectory_points[i];

            if (!use_planned_triggers && curr.enable_position_trigger) {
                triggers.push_back(curr.position);
                trigger_times.push_back(curr.timestamp);
            }

            const float32 dist = prev.position.DistanceTo(curr.position);
            if (current.dispensing) {
                total_dispense_length += dist;
            } else {
                total_rapid_length += dist;
            }
            current.length += dist;
            current.end_time = curr.timestamp;

            const bool curr_dispensing = curr.enable_position_trigger;
            if (curr_dispensing == current.dispensing) {
                current.points.push_back(curr);
            } else {
                current.points.push_back(curr);
                segments.push_back(std::move(current));
                current = PreviewSegment{};
                current.dispensing = curr_dispensing;
                current.start_time = curr.timestamp;
                current.end_time = curr.timestamp;
                current.points.push_back(curr);
            }
        }
        segments.push_back(std::move(current));
    }

    float32 start_time = 0.0f;
    float32 end_time = 0.0f;
    if (!trajectory_points.empty()) {
        start_time = trajectory_points.front().timestamp;
        end_time = trajectory_points.back().timestamp;
    }

    const float32 total_time = (end_time > 0.0f) ? end_time : 0.0f;
    const bool has_segments = !segments.empty();

    json << "{\n";
    json << "  \"metadata\": {\n";
    json << "    \"total_time\": " << total_time << ",\n";
    json << "    \"total_dispensing_length\": " << total_dispense_length << ",\n";
    json << "    \"total_rapid_length\": " << total_rapid_length << ",\n";
    json << "    \"total_triggers\": " << triggers.size() << ",\n";
    if (use_planned_triggers) {
        json << "    \"triggers_source\": \"planned\",\n";
    }
    json << "    \"segment_count\": " << segments.size() << ",\n";
    json << "    \"discontinuity_count\": 0\n";
    json << "  },\n";

    json << "  \"segments\": [\n";
    if (has_segments) {
        for (size_t s = 0; s < segments.size(); ++s) {
            const auto& seg = segments[s];
            const float32 duration = (seg.end_time > seg.start_time) ? (seg.end_time - seg.start_time) : 0.0f;
            json << "    {\n";
            json << "      \"type\": \"" << (seg.dispensing ? "dispensing" : "rapid") << "\",\n";
            json << "      \"length\": " << seg.length << ",\n";
            json << "      \"duration\": " << duration << ",\n";

            json << "      \"points\": [";
            for (size_t j = 0; j < seg.points.size(); ++j) {
                const auto& p = seg.points[j];
                const int trigger_flag = p.enable_position_trigger ? 1 : 0;
                json << "[" << p.position.x << "," << p.position.y << "," << p.velocity << "," << p.timestamp << ","
                     << trigger_flag << "]";
                if (j < seg.points.size() - 1) json << ",";
            }
            json << "],\n";

            if (s == 0) {
                json << "      \"triggers\": [";
                for (size_t j = 0; j < triggers.size(); ++j) {
                    const auto& t = triggers[j];
                    json << "[" << t.x << "," << t.y;
                    if (!use_planned_triggers || planned_has_time) {
                        const float32 trigger_time = (j < trigger_times.size()) ? trigger_times[j] : 0.0f;
                        json << "," << trigger_time;
                    }
                    json << "]";
                    if (j < triggers.size() - 1) json << ",";
                }
                json << "]\n";
            } else {
                json << "      \"triggers\": []\n";
            }

            json << "    }";
            if (s + 1 < segments.size()) {
                json << ",";
            }
            json << "\n";
        }
    }
    json << "  ]\n";
    json << "}";

    return json.str();
}

std::string DispensingVisualizer::GenerateHTMLTemplate(const std::string& json_data,
                                                       const DispensingVisualizationConfig& config) {
    std::ostringstream html;

    html << R"(<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<title>)" << config.title
         << R"(</title>
<style>
* { margin: 0; padding: 0; box-sizing: border-box; }
:root { color-scheme: dark; }
body { font-family: 'Segoe UI', Arial, sans-serif; background: #0b0c0f; color: #d4d4d4; }
.container { display: flex; height: 100vh; height: 100dvh; }
.canvas-panel { flex: 1; padding: 20px; display: flex; flex-direction: column; gap: 8px; min-width: 0; }
.canvas-wrap { flex: 1; min-height: 280px; position: relative; border: 1px solid #1f1f1f; border-radius: 6px; overflow: hidden; background: #000000; }
canvas { width: 100%; height: 100%; display: block; border: none; background: #000000; cursor: crosshair; touch-action: none; }
.controls { margin: 8px 0; display: flex; flex-wrap: wrap; gap: 8px; align-items: center; }
.controls .group { display: flex; gap: 6px; align-items: center; }
button { padding: 8px 14px; background: #0e639c; color: white; border: none; cursor: pointer; border-radius: 3px; }
button:hover { background: #1177bb; }
select, input[type="range"] { background: #1e1e1e; color: #d4d4d4; border: 1px solid #2a2a2a; border-radius: 3px; }
input[type="range"] { flex: 1; }
.toggle { display: inline-flex; gap: 6px; align-items: center; font-size: 0.9em; color: #c8c8c8; }
.hud { position: absolute; top: 10px; right: 10px; background: rgba(0, 0, 0, 0.6); border: 1px solid #2b2b2b; border-radius: 4px; padding: 6px 8px; font-size: 12px; color: #cfcfcf; line-height: 1.4; }
.info-panel { width: 320px; min-width: 240px; padding: 20px; background: #252526; overflow-y: auto; }
.stat { margin: 8px 0; padding: 8px; background: #1e1e1e; border-radius: 3px; }
.stat-label { font-size: 0.85em; color: #858585; }
.stat-value { font-size: 1.1em; font-weight: bold; color: #4ec9b0; word-break: break-all; }
@media (max-width: 960px) {
  .container { flex-direction: column; }
  .canvas-panel { height: 60vh; }
  .info-panel { width: 100%; max-height: 40vh; }
}
</style>
</head>
<body>
<div class="container">
  <div class="canvas-panel">
    <h2>)"
         << config.title << R"(</h2>
    <div class="controls">
      <div class="group">
        <button id="playBtn">播放</button>
        <button id="pauseBtn">暂停</button>
      </div>
      <input type="range" id="timeline" min="0" max="1000" value="0">
      <span id="timeDisplay">0.0s</span>
      <select id="speedCtrl">
        <option value="0.5">0.5x</option>
        <option value="1" selected>1x</option>
        <option value="2">2x</option>
        <option value="5">5x</option>
      </select>
      <div class="group">
        <button id="zoomInBtn">放大</button>
        <button id="zoomOutBtn">缩小</button>
        <button id="zoomResetBtn">重置</button>
      </div>
      <label class="toggle"><input type="checkbox" id="showPath">路径</label>
      <label class="toggle"><input type="checkbox" id="showTriggers" checked>胶点</label>
    </div>
    <div class="canvas-wrap" id="canvasWrap">
      <canvas id="canvas" width=")"
         << config.canvas_width << R"(" height=")" << config.canvas_height << R"("></canvas>
      <div class="hud">
        <div>缩放: <span id="zoomDisplay">1.00x</span></div>
        <div>光标: <span id="cursorPos">-</span></div>
      </div>
    </div>
  </div>
  <div class="info-panel">
    <h3>统计信息</h3>
    <div class="stat"><div class="stat-label">总时间</div><div class="stat-value" id="totalTime">-</div></div>
    <div class="stat"><div class="stat-label">点胶长度</div><div class="stat-value" id="dispLen">-</div></div>
    <div class="stat"><div class="stat-label">空移长度</div><div class="stat-value" id="rapidLen">-</div></div>
    <div class="stat"><div class="stat-label">触发点数</div><div class="stat-value" id="triggers">-</div></div>
    <div class="stat"><div class="stat-label">胶点数量</div><div class="stat-value" id="gluePoints">-</div></div>
    <div class="stat"><div class="stat-label">路径段数</div><div class="stat-value" id="segments">-</div></div>
    <h3 style="margin-top: 20px;">实时状态</h3>
    <div class="stat"><div class="stat-label">当前位置</div><div class="stat-value" id="curPos">-</div></div>
    <div class="stat"><div class="stat-label">当前速度</div><div class="stat-value" id="curVel">-</div></div>
    <div class="stat"><div class="stat-label">已触发次数</div><div class="stat-value" id="curTrig">-</div></div>
  </div>
</div>
<script>
const data = )"
         << json_data << R"(;
const config = {
  dotDiameter: )"
         << config.dot_diameter << R"(,
  showOverlap: )"
         << (config.show_overlap_effect ? "true" : "false") << R"(,
  canvasWidth: )"
         << config.canvas_width << R"(,
  canvasHeight: )"
         << config.canvas_height << R"(
};
const canvas = document.getElementById('canvas');
const canvasWrap = document.getElementById('canvasWrap');
const ctx = canvas.getContext('2d');
const staticCanvas = document.createElement('canvas');
const staticCtx = staticCanvas.getContext('2d');
const glueCanvas = document.createElement('canvas');
const glueCtx = glueCanvas.getContext('2d');

const timelineX = [];
const timelineY = [];
const timelineV = [];
const timelineT = [];
const triggerX = [];
const triggerY = [];
const triggerTimes = [];
let totalTriggerPoints = 0;

let minX = Infinity, minY = Infinity, maxX = -Infinity, maxY = -Infinity;
const forcePlannedTriggers = data.metadata && data.metadata.triggers_source === 'planned';
let hasExplicitTriggers = forcePlannedTriggers;

data.segments.forEach(seg => {
  if (!hasExplicitTriggers && Array.isArray(seg.triggers) && seg.triggers.length) {
    hasExplicitTriggers = true;
  }
  if (!seg.points) return;
  seg.points.forEach(p => {
    const x = p[0];
    const y = p[1];
    const v = p[2];
    const t = p[3];
    timelineX.push(x);
    timelineY.push(y);
    timelineV.push(v);
    timelineT.push(t);
    if (x < minX) minX = x;
    if (x > maxX) maxX = x;
    if (y < minY) minY = y;
    if (y > maxY) maxY = y;
  });
});

if (hasExplicitTriggers) {
  data.segments.forEach(seg => {
    if (!Array.isArray(seg.triggers)) return;
    seg.triggers.forEach(tp => {
      const x = tp[0];
      const y = tp[1];
      triggerX.push(x);
      triggerY.push(y);
      if (tp.length > 2 && isFinite(tp[2])) {
        triggerTimes.push(tp[2]);
      }
    });
  });
} else {
  data.segments.forEach(seg => {
    if (!seg.points) return;
    seg.points.forEach(p => {
      if (p.length > 4 && p[4] === 1) {
        triggerX.push(p[0]);
        triggerY.push(p[1]);
        triggerTimes.push(p[3]);
      }
    });
  });
}

const hasTriggerTimes = triggerTimes.length === triggerX.length && triggerTimes.length > 0;
if (hasTriggerTimes) {
  const combined = triggerX.map((x, i) => ({ x, y: triggerY[i], t: triggerTimes[i] }));
  combined.sort((a, b) => a.t - b.t);
  triggerX.length = 0;
  triggerY.length = 0;
  triggerTimes.length = 0;
  combined.forEach(item => {
    triggerX.push(item.x);
    triggerY.push(item.y);
    triggerTimes.push(item.t);
  });
} else {
  triggerTimes.length = 0;
}

if (!isFinite(minX)) {
  minX = 0;
  minY = 0;
  maxX = 1;
  maxY = 1;
}

const dataCenterX = (minX + maxX) / 2;
const dataCenterY = (minY + maxY) / 2;
const dataWidth = maxX - minX;
const dataHeight = maxY - minY;

const totalTime = (isFinite(data.metadata.total_time) && data.metadata.total_time > 0)
  ? data.metadata.total_time
  : (timelineT.length ? timelineT[timelineT.length - 1] : 0);

totalTriggerPoints = triggerX.length;

let dispensePath = null;
let rapidPath = null;
if (window.Path2D) {
  dispensePath = new Path2D();
  rapidPath = new Path2D();
  data.segments.forEach(seg => {
    if (!seg.points || seg.points.length < 2) return;
    const path = seg.type === 'dispensing' ? dispensePath : rapidPath;
    const p0 = seg.points[0];
    path.moveTo(p0[0], p0[1]);
    for (let i = 1; i < seg.points.length; i++) {
      const p = seg.points[i];
      path.lineTo(p[0], p[1]);
    }
  });
}

const uiState = {
  showPath: document.getElementById('showPath').checked,
  showTriggers: document.getElementById('showTriggers').checked
};

const view = {
  scale: 1,
  offsetX: 0,
  offsetY: 0,
  scaleCss: 1,
  offsetXCss: 0,
  offsetYCss: 0,
  baseOffsetX: 0,
  baseOffsetY: 0,
  baseScale: 1,
  widthCss: 0,
  heightCss: 0,
  dpr: 1
};

let zoomLevel = 1;
let panX = 0;
let panY = 0;
let playing = false;
let currentTime = 0;
let speed = 1;
let lastFrame = 0;
let needsStaticRedraw = true;
let glueDrawnCount = 0;
let needsGlueRedraw = true;
let renderPending = false;
    )";

    html << R"(
const ZOOM_MIN = 0.1;
const ZOOM_MAX = 50;

function clamp(value, min, max) {
  return Math.max(min, Math.min(max, value));
}

function updateView() {
  const rect = canvasWrap.getBoundingClientRect();
  const widthCss = Math.max(1, rect.width || config.canvasWidth);
  const heightCss = Math.max(1, rect.height || config.canvasHeight);
  const dpr = window.devicePixelRatio || 1;

  const nextWidth = Math.floor(widthCss * dpr);
  const nextHeight = Math.floor(heightCss * dpr);
  if (canvas.width !== nextWidth || canvas.height !== nextHeight ||
      staticCanvas.width !== nextWidth || staticCanvas.height !== nextHeight ||
      glueCanvas.width !== nextWidth || glueCanvas.height !== nextHeight) {
    canvas.width = nextWidth;
    canvas.height = nextHeight;
    staticCanvas.width = nextWidth;
    staticCanvas.height = nextHeight;
    glueCanvas.width = nextWidth;
    glueCanvas.height = nextHeight;
  }

  view.widthCss = widthCss;
  view.heightCss = heightCss;
  view.dpr = dpr;

  const margin = 40;
  const safeWidth = Math.max(1, widthCss - 2 * margin);
  const safeHeight = Math.max(1, heightCss - 2 * margin);
  const safeDataWidth = dataWidth > 0 ? dataWidth : 1;
  const safeDataHeight = dataHeight > 0 ? dataHeight : 1;
  view.baseScale = Math.min(safeWidth / safeDataWidth, safeHeight / safeDataHeight);
  if (!isFinite(view.baseScale) || view.baseScale <= 0) view.baseScale = 1;

  view.scaleCss = view.baseScale * zoomLevel;
  view.baseOffsetX = widthCss / 2 - dataCenterX * view.baseScale;
  view.baseOffsetY = heightCss / 2 - dataCenterY * view.baseScale;
  view.offsetXCss = view.baseOffsetX + panX;
  view.offsetYCss = view.baseOffsetY + panY;

  view.scale = view.scaleCss * dpr;
  view.offsetX = view.offsetXCss * dpr;
  view.offsetY = view.offsetYCss * dpr;

  const zoomLabel = document.getElementById('zoomDisplay');
  if (zoomLabel) zoomLabel.textContent = zoomLevel.toFixed(2) + 'x';

  needsStaticRedraw = true;
  needsGlueRedraw = true;
}

function setDataTransform(targetCtx) {
  targetCtx.setTransform(view.scale, 0, 0, -view.scale, view.offsetX, canvas.height - view.offsetY);
}

function dataToScreen(x, y) {
  return [x * view.scale + view.offsetX, canvas.height - (y * view.scale + view.offsetY)];
}

function screenToData(sx, sy) {
  if (!isFinite(view.scaleCss) || view.scaleCss === 0) return null;
  const x = (sx - view.offsetXCss) / view.scaleCss;
  const y = ((view.heightCss - sy) - view.offsetYCss) / view.scaleCss;
  return { x, y };
}

function scheduleRender() {
  if (renderPending) return;
  renderPending = true;
  requestAnimationFrame(() => {
    renderPending = false;
    render(currentTime);
  });
}

function drawDot(ctx2, x, y) {
  const radius = config.dotDiameter / 2;
  ctx2.fillStyle = 'rgba(0, 255, 136, 0.85)';
  ctx2.beginPath();
  ctx2.arc(x, y, radius, 0, 2 * Math.PI);
  ctx2.fill();

  if (config.showOverlap) {
    const gradient = ctx2.createRadialGradient(x, y, 0, x, y, radius * 1.2);
    gradient.addColorStop(0, 'rgba(0, 255, 136, 0.2)');
    gradient.addColorStop(1, 'rgba(0, 255, 136, 0)');
    ctx2.fillStyle = gradient;
    ctx2.beginPath();
    ctx2.arc(x, y, radius * 1.2, 0, 2 * Math.PI);
    ctx2.fill();
  }
}

function upperBound(arr, value) {
  let lo = 0, hi = arr.length;
  while (lo < hi) {
    const mid = (lo + hi) >> 1;
    if (arr[mid] <= value) lo = mid + 1;
    else hi = mid;
  }
  return lo;
}

function timeToIndex(time) {
  if (timelineT.length === 0 || !isFinite(time)) return 0;
  const idx = upperBound(timelineT, time) - 1;
  return clamp(idx, 0, timelineT.length - 1);
}

function getInterpolatedState(time) {
  if (timelineT.length === 0) return null;
  if (!isFinite(time)) {
    return { x: timelineX[0], y: timelineY[0], v: timelineV[0] };
  }

  const last = timelineT.length - 1;
  if (time <= timelineT[0]) {
    return { x: timelineX[0], y: timelineY[0], v: timelineV[0] };
  }
  if (time >= timelineT[last]) {
    return { x: timelineX[last], y: timelineY[last], v: timelineV[last] };
  }

  const idx = upperBound(timelineT, time) - 1;
  const next = Math.min(idx + 1, last);
  const t1 = timelineT[idx];
  const t2 = timelineT[next];
  const ratio = t2 > t1 ? (time - t1) / (t2 - t1) : 0;

  const x = timelineX[idx] + (timelineX[next] - timelineX[idx]) * ratio;
  const y = timelineY[idx] + (timelineY[next] - timelineY[idx]) * ratio;
  const v = timelineV[idx] + (timelineV[next] - timelineV[idx]) * ratio;

  return { x, y, v };
}

function drawPaths(targetCtx) {
  if (uiState.showPath !== true) return;

  const dispWidth = 1.5 / view.scale;
  const rapidWidth = 1.0 / view.scale;

  if (dispensePath && rapidPath) {
    targetCtx.lineJoin = 'round';
    targetCtx.lineCap = 'round';

    targetCtx.strokeStyle = 'rgba(78, 201, 176, 0.45)';
    targetCtx.lineWidth = dispWidth;
    targetCtx.stroke(dispensePath);

    targetCtx.strokeStyle = 'rgba(130, 130, 130, 0.35)';
    targetCtx.lineWidth = rapidWidth;
    targetCtx.stroke(rapidPath);
    return;
  }

  data.segments.forEach(seg => {
    if (!seg.points || seg.points.length < 2) return;
    targetCtx.strokeStyle = seg.type === 'dispensing' ? 'rgba(78, 201, 176, 0.45)' : 'rgba(130, 130, 130, 0.35)';
    targetCtx.lineWidth = seg.type === 'dispensing' ? dispWidth : rapidWidth;
    targetCtx.beginPath();
    const p0 = seg.points[0];
    targetCtx.moveTo(p0[0], p0[1]);
    for (let i = 1; i < seg.points.length; i++) {
      const p = seg.points[i];
      targetCtx.lineTo(p[0], p[1]);
    }
    targetCtx.stroke();
  });
}

function renderStatic() {
  if (!needsStaticRedraw) return;
  staticCtx.setTransform(1, 0, 0, 1, 0, 0);
  staticCtx.clearRect(0, 0, staticCanvas.width, staticCanvas.height);
  if (uiState.showPath) {
    setDataTransform(staticCtx);
    drawPaths(staticCtx);
  }
  needsStaticRedraw = false;
}

function getDispenseCount(time) {
  if (triggerTimes.length === 0) return triggerX.length;
  if (!isFinite(time) || time <= 0) return 0;
  return upperBound(triggerTimes, time);
}

function renderGlue(time) {
  if (!uiState.showTriggers) return;

  const targetCount = getDispenseCount(time);

  if (needsGlueRedraw || targetCount < glueDrawnCount) {
    glueCtx.setTransform(1, 0, 0, 1, 0, 0);
    glueCtx.clearRect(0, 0, glueCanvas.width, glueCanvas.height);
    if (targetCount > 0) {
      setDataTransform(glueCtx);
      for (let i = 0; i < targetCount; i++) {
        drawDot(glueCtx, triggerX[i], triggerY[i]);
      }
    }
    glueDrawnCount = targetCount;
    needsGlueRedraw = false;
    return;
  }

  if (targetCount > glueDrawnCount) {
    setDataTransform(glueCtx);
    for (let i = glueDrawnCount; i < targetCount; i++) {
      drawDot(glueCtx, triggerX[i], triggerY[i]);
    }
    glueDrawnCount = targetCount;
  }
}

function drawCurrentPoint(time) {
  const state = getInterpolatedState(time);
  if (!state) return;
  const x = state.x;
  const y = state.y;
  const coords = dataToScreen(x, y);
  ctx.fillStyle = 'rgba(255, 198, 0, 0.95)';
  ctx.beginPath();
  ctx.arc(coords[0], coords[1], 4 * view.dpr, 0, 2 * Math.PI);
  ctx.fill();
}

function render(time) {
  renderStatic();
  renderGlue(time);
  ctx.setTransform(1, 0, 0, 1, 0, 0);
  ctx.clearRect(0, 0, canvas.width, canvas.height);
  ctx.drawImage(staticCanvas, 0, 0);
  if (uiState.showTriggers) {
    ctx.drawImage(glueCanvas, 0, 0);
  }

  if (timelineX.length === 0 || !isFinite(totalTime) || totalTime <= 0) return;

  if (time <= 0) return;

  drawCurrentPoint(time);
}

function getTriggerCount(time) {
  if (timelineT.length === 0 || !isFinite(totalTime) || totalTime <= 0) return 0;
  return getDispenseCount(time);
}
    )";

    html << R"(
function updateStats() {
  document.getElementById('totalTime').textContent = totalTime.toFixed(1) + 's';
  document.getElementById('dispLen').textContent = data.metadata.total_dispensing_length.toFixed(1) + 'mm';
  document.getElementById('rapidLen').textContent = data.metadata.total_rapid_length.toFixed(1) + 'mm';
  document.getElementById('triggers').textContent = data.metadata.total_triggers.toLocaleString();
  document.getElementById('segments').textContent = data.metadata.segment_count.toLocaleString();
  document.getElementById('gluePoints').textContent = totalTriggerPoints.toLocaleString();
}

function updateTimeUI() {
  const timeDisplay = document.getElementById('timeDisplay');
  if (!isFinite(totalTime) || totalTime <= 0) {
    timeDisplay.textContent = '0.0s';
  } else {
    timeDisplay.textContent = currentTime.toFixed(1) + 's';
  }

  const state = getInterpolatedState(currentTime);
  if (state) {
    document.getElementById('curPos').textContent =
      state.x.toFixed(2) + ', ' + state.y.toFixed(2);
    document.getElementById('curVel').textContent =
      state.v.toFixed(2) + ' mm/s';
  } else {
    document.getElementById('curPos').textContent = '-';
    document.getElementById('curVel').textContent = '-';
  }

  document.getElementById('curTrig').textContent = getTriggerCount(currentTime);
}

function setTime(time) {
  if (!isFinite(totalTime) || totalTime <= 0) {
    currentTime = 0;
  } else {
    currentTime = clamp(time, 0, totalTime);
  }
  const timelineInput = document.getElementById('timeline');
  timelineInput.value = totalTime > 0 ? Math.round((currentTime / totalTime) * 1000) : 0;
  updateTimeUI();
  render(currentTime);
}

function startPlayback() {
  if (playing) return;
  playing = true;
  lastFrame = 0;
  requestAnimationFrame(tick);
}

function stopPlayback() {
  playing = false;
  lastFrame = 0;
}

function tick(timestamp) {
  if (!playing) return;
  if (!lastFrame) lastFrame = timestamp;
  const dt = (timestamp - lastFrame) / 1000;
  lastFrame = timestamp;

  if (isFinite(totalTime) && totalTime > 0) {
    currentTime += dt * speed;
    if (currentTime > totalTime) currentTime = 0;
  } else {
    currentTime = 0;
  }

  updateTimeUI();
  render(currentTime);
  requestAnimationFrame(tick);
}

function zoomAt(newZoom, sx, sy) {
  const before = screenToData(sx, sy);
  zoomLevel = clamp(newZoom, ZOOM_MIN, ZOOM_MAX);
  updateView();
  if (before) {
    panX = sx - before.x * view.scaleCss - view.baseOffsetX;
    panY = (view.heightCss - sy) - before.y * view.scaleCss - view.baseOffsetY;
    updateView();
  }
  scheduleRender();
}

updateStats();
updateView();
setTime(0);

const playBtn = document.getElementById('playBtn');
const pauseBtn = document.getElementById('pauseBtn');
const timelineInput = document.getElementById('timeline');

playBtn.onclick = () => startPlayback();
pauseBtn.onclick = () => stopPlayback();

timelineInput.oninput = e => {
  stopPlayback();
  const ratio = e.target.value / 1000;
  setTime(ratio * totalTime);
};

document.getElementById('speedCtrl').onchange = e => {
  speed = parseFloat(e.target.value);
};

document.getElementById('zoomInBtn').onclick = () => {
  zoomAt(zoomLevel * 1.2, view.widthCss / 2, view.heightCss / 2);
};
document.getElementById('zoomOutBtn').onclick = () => {
  zoomAt(zoomLevel / 1.2, view.widthCss / 2, view.heightCss / 2);
};
document.getElementById('zoomResetBtn').onclick = () => {
  zoomLevel = 1;
  panX = 0;
  panY = 0;
  updateView();
  scheduleRender();
};

document.getElementById('showPath').onchange = e => {
  uiState.showPath = e.target.checked;
  needsStaticRedraw = true;
  scheduleRender();
};
document.getElementById('showTriggers').onchange = e => {
  uiState.showTriggers = e.target.checked;
  needsGlueRedraw = true;
  glueDrawnCount = 0;
  scheduleRender();
};

canvas.addEventListener('wheel', e => {
  e.preventDefault();
  const rect = canvas.getBoundingClientRect();
  const sx = e.clientX - rect.left;
  const sy = e.clientY - rect.top;
  const delta = e.deltaY > 0 ? 0.9 : 1.1;
  zoomAt(zoomLevel * delta, sx, sy);
}, { passive: false });

let isPanning = false;
let panStartX = 0;
let panStartY = 0;
let panOriginX = 0;
let panOriginY = 0;

canvas.addEventListener('pointerdown', e => {
  if (e.button !== 0) return;
  canvas.setPointerCapture(e.pointerId);
  isPanning = true;
  panStartX = e.clientX;
  panStartY = e.clientY;
  panOriginX = panX;
  panOriginY = panY;
});

canvas.addEventListener('pointermove', e => {
  const rect = canvas.getBoundingClientRect();
  const sx = e.clientX - rect.left;
  const sy = e.clientY - rect.top;
  const cursor = screenToData(sx, sy);
  const cursorLabel = document.getElementById('cursorPos');
  if (cursor && cursorLabel) {
    cursorLabel.textContent = cursor.x.toFixed(2) + ', ' + cursor.y.toFixed(2);
  } else if (cursorLabel) {
    cursorLabel.textContent = '-';
  }

  if (!isPanning) return;
  const dx = e.clientX - panStartX;
  const dy = e.clientY - panStartY;
  panX = panOriginX + dx;
  panY = panOriginY + dy;
  updateView();
  scheduleRender();
});

canvas.addEventListener('pointerup', e => {
  if (!isPanning) return;
  isPanning = false;
  canvas.releasePointerCapture(e.pointerId);
});

canvas.addEventListener('pointerleave', () => {
  const cursorLabel = document.getElementById('cursorPos');
  if (cursorLabel) cursorLabel.textContent = '-';
});

if (window.ResizeObserver) {
  const ro = new ResizeObserver(() => {
    updateView();
    scheduleRender();
  });
  ro.observe(canvasWrap);
} else {
  window.addEventListener('resize', () => {
    updateView();
    scheduleRender();
  });
}
</script>
</body>
</html>
)";

    return html.str();
}

bool DispensingVisualizer::GenerateHTML(const std::vector<TrajectoryPoint>& trajectory_points,
                                        const std::string& output_path,
                                        const DispensingVisualizationConfig& config,
                                        const std::vector<Shared::Types::VisualizationTriggerPoint>* planned_triggers) {
    std::string json = SerializeToJSON(trajectory_points, planned_triggers);
    std::string html = GenerateHTMLTemplate(json, config);

    std::ofstream file(output_path, std::ios::binary);
    if (!file) return false;

    const char utf8_bom[] = {'\xEF', '\xBB', '\xBF'};
    file.write(utf8_bom, 3);

    file << html;
    return true;
}

}  // namespace Siligen
