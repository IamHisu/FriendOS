// Thu nhỏ GIF về tối đa 240×240 ngay trên trình duyệt, tự giải mã GIF nên không cần ImageDecoder.
// Phục vụ tại /gif-resize.js, nạp trước home.js.
(function () {
"use strict";
const MAX_BYTES = 1024 * 1024;

function buildPalette(hist) {
    const part = (k, c) => c === 0 ? k >> 10 : c === 1 ? (k >> 5) & 31 : k & 31;
    const used = [];
    for (let k = 0; k < 32768; k++) if (hist[k]) used.push(k);
    const boxes = [used];
    while (boxes.length < 256) {
        let pick = -1, channel = 0, best = 0;
        boxes.forEach((box, i) => {
            if (box.length < 2) return;
            for (let c = 0; c < 3; c++) {
                let lo = 31, hi = 0;
                for (const k of box) { const v = part(k, c); if (v < lo) lo = v; if (v > hi) hi = v; }
                if (hi - lo > best) { best = hi - lo; pick = i; channel = c; }
            }
        });
        if (pick < 0) break;
        const box = boxes[pick].sort((a, b) => part(a, channel) - part(b, channel));
        let total = 0;
        for (const k of box) total += hist[k];
        let acc = 0, cut = box.length - 1;
        for (let i = 0; i < box.length - 1; i++) {
            acc += hist[box[i]];
            if (acc >= total / 2) { cut = i + 1; break; }
        }
        boxes.splice(pick, 1, box.slice(0, cut), box.slice(cut));
    }
    const palette = new Uint8Array(768);
    boxes.forEach((box, i) => {
        let w = 0, r = 0, g = 0, b = 0;
        for (const k of box) {
            const n = hist[k];
            w += n; r += (k >> 10) * n; g += ((k >> 5) & 31) * n; b += (k & 31) * n;
        }
        if (!w) return;
        palette[i * 3] = Math.min(255, Math.round(r / w * 8 + 4));
        palette[i * 3 + 1] = Math.min(255, Math.round(g / w * 8 + 4));
        palette[i * 3 + 2] = Math.min(255, Math.round(b / w * 8 + 4));
    });
    return palette;
}

function makeMapper(palette) {
    const cache = new Int16Array(32768).fill(-1);
    return k => {
        if (cache[k] >= 0) return cache[k];
        const r = (k >> 10) * 8 + 4, g = ((k >> 5) & 31) * 8 + 4, b = (k & 31) * 8 + 4;
        let best = 0, bestD = Infinity;
        for (let i = 0; i < 256; i++) {
            const dr = palette[i * 3] - r, dg = palette[i * 3 + 1] - g, db = palette[i * 3 + 2] - b;
            const d = dr * dr + dg * dg + db * db;
            if (d < bestD) { bestD = d; best = i; }
        }
        return (cache[k] = best);
    };
}

function lzwEncode(pixels) {
    const bytes = [];
    let cur = 0, nbits = 0, size = 9, next = 258, dict = new Map();
    const emit = code => {
        cur |= code << nbits;
        nbits += size;
        while (nbits >= 8) { bytes.push(cur & 255); cur >>>= 8; nbits -= 8; }
    };
    emit(256);
    let prefix = pixels[0];
    for (let i = 1; i < pixels.length; i++) {
        const c = pixels[i], key = (prefix << 8) | c, hit = dict.get(key);
        if (hit !== undefined) { prefix = hit; continue; }
        emit(prefix);
        if (next === 4096) { emit(256); dict = new Map(); size = 9; next = 258; }
        else { if (next >= (1 << size)) size++; dict.set(key, next++); }
        prefix = c;
    }
    emit(prefix);
    emit(257);
    if (nbits > 0) bytes.push(cur & 255);
    return bytes;
}

function encodeGif(w, h, palette, frames) {
    const out = [];
    const u16 = v => out.push(v & 255, v >> 8);
    out.push(71, 73, 70, 56, 57, 97);
    u16(w); u16(h);
    out.push(0xf7, 0, 0);
    for (const v of palette) out.push(v);
    out.push(0x21, 0xff, 0x0b, 78, 69, 84, 83, 67, 65, 80, 69, 50, 46, 48, 3, 1, 0, 0, 0);
    for (const f of frames) {
        out.push(0x21, 0xf9, 4, 0x04);
        u16(f.delay);
        out.push(0, 0, 0x2c);
        u16(0); u16(0); u16(w); u16(h);
        out.push(0, 8);
        const data = lzwEncode(f.pixels);
        for (let i = 0; i < data.length; i += 255) {
            const n = Math.min(255, data.length - i);
            out.push(n);
            for (let j = 0; j < n; j++) out.push(data[i + j]);
        }
        out.push(0);
    }
    out.push(0x3b);
    return new Uint8Array(out);
}

// ---- Bộ giải mã GIF bằng JS thuần (không cần ImageDecoder, chạy được trên http://) ----
function lzwDecode(minCode, data, pixelCount) {
    const out = new Uint8Array(pixelCount);
    const clear = 1 << minCode, eoi = clear + 1;
    const prefix = new Uint16Array(4096), suffix = new Uint8Array(4096), stack = new Uint8Array(4097);
    for (let i = 0; i < clear; i++) suffix[i] = i;
    let size = minCode + 1, next = eoi + 1, prev = -1, first = 0, bits = 0, cur = 0, pos = 0, op = 0;
    while (op < pixelCount) {
        while (bits < size) {
            if (pos >= data.length) return out;
            cur |= data[pos++] << bits;
            bits += 8;
        }
        const code = cur & ((1 << size) - 1);
        cur >>>= size;
        bits -= size;
        if (code === clear) { size = minCode + 1; next = eoi + 1; prev = -1; continue; }
        if (code === eoi) break;
        if (prev === -1) { first = suffix[code]; out[op++] = first; prev = code; continue; }
        let sp = 0, c = code;
        if (code >= next) {
            if (code > next) break; // dữ liệu hỏng
            stack[sp++] = first;
            c = prev;
        }
        while (c >= clear) { stack[sp++] = suffix[c]; c = prefix[c]; }
        first = suffix[c];
        stack[sp++] = first;
        if (next < 4096) {
            prefix[next] = prev;
            suffix[next] = first;
            next++;
            if (next === (1 << size) && size < 12) size++;
        }
        prev = code;
        while (sp > 0 && op < pixelCount) out[op++] = stack[--sp];
    }
    return out;
}

// Trả về từng khung hình đã ghép hoàn chỉnh (RGBA, kích thước bằng màn hình logic của GIF).
// Buffer rgba được dùng lại cho khung kế tiếp nên phải xử lý xong trước khi lấy khung mới.
function* gifFrames(bytes) {
    let p = 0;
    const u8 = () => bytes[p++];
    const u16 = () => { const v = bytes[p] | (bytes[p + 1] << 8); p += 2; return v; };
    const skipBlocks = () => { for (let n = u8(); n; n = u8()) p += n; };
    const sig = String.fromCharCode(...bytes.subarray(0, 6));
    if (sig !== "GIF87a" && sig !== "GIF89a") throw new Error("Không phải file GIF hợp lệ.");
    p = 6;
    const W = u16(), H = u16(), flags = u8();
    p += 2;
    if (!W || !H || W * H > 16e6) throw new Error("Kích thước GIF không hợp lệ hoặc quá lớn.");
    let gct = null;
    if (flags & 0x80) { const n = 2 << (flags & 7); gct = bytes.subarray(p, p + n * 3); p += n * 3; }
    const canvas = new Uint8ClampedArray(W * H * 4);
    let gce = null;
    while (p < bytes.length) {
        const b = u8();
        if (b === 0x3b) break;
        if (b === 0x21) {
            const label = u8();
            if (label === 0xf9) {
                const len = u8(), f = u8(), delay = u16(), t = u8();
                p += Math.max(0, len - 4);
                gce = { disposal: (f >> 2) & 7, transparent: f & 1 ? t : -1, delay };
            }
            skipBlocks();
            continue;
        }
        if (b !== 0x2c) throw new Error("File GIF bị lỗi.");
        const x = u16(), y = u16(), w = u16(), h = u16(), fl = u8();
        let table = gct;
        if (fl & 0x80) { const n = 2 << (fl & 7); table = bytes.subarray(p, p + n * 3); p += n * 3; }
        const minCode = u8();
        const chunks = [];
        let total = 0;
        for (let n = u8(); n; n = u8()) { chunks.push(bytes.subarray(p, p + n)); p += n; total += n; }
        const data = new Uint8Array(total);
        let o = 0;
        for (const c of chunks) { data.set(c, o); o += c.length; }
        if (!table || minCode < 2 || minCode > 8) throw new Error("File GIF bị lỗi.");
        const idx = lzwDecode(minCode, data, w * h);
        const g = gce || { disposal: 0, transparent: -1, delay: 10 };
        gce = null;
        const backup = g.disposal === 3 ? canvas.slice() : null;
        const rowOf = fl & 0x40 ? (() => {
            const order = [];
            for (const [s, st] of [[0, 8], [4, 8], [2, 4], [1, 2]]) for (let r = s; r < h; r += st) order.push(r);
            return r => order[r];
        })() : r => r;
        for (let r = 0; r < h; r++) {
            const dy = y + rowOf(r);
            if (dy >= H) continue;
            for (let c = 0; c < w && x + c < W; c++) {
                const v = idx[r * w + c];
                if (v === g.transparent) continue;
                const d = (dy * W + x + c) * 4;
                canvas[d] = table[v * 3];
                canvas[d + 1] = table[v * 3 + 1];
                canvas[d + 2] = table[v * 3 + 2];
                canvas[d + 3] = 255;
            }
        }
        yield { rgba: canvas, width: W, height: H, delay: g.delay };
        if (g.disposal === 2) {
            for (let r = y; r < Math.min(H, y + h); r++) canvas.fill(0, (r * W + x) * 4, (r * W + Math.min(W, x + w)) * 4);
        } else if (backup) canvas.set(backup);
    }
}

async function resize(file, image, onProgress) {
    const bytes = new Uint8Array(await file.arrayBuffer());
    const hist = new Uint32Array(32768), frames = [];
    let w, h, source, sourceContext, canvas, context, count = 0;
    for (const f of gifFrames(bytes)) {
        if (!canvas) {
            const scale = Math.min(240 / f.width, 240 / f.height, 1);
            w = Math.max(1, Math.round(f.width * scale));
            h = Math.max(1, Math.round(f.height * scale));
            source = document.createElement("canvas");
            source.width = f.width;
            source.height = f.height;
            sourceContext = source.getContext("2d");
            canvas = document.createElement("canvas");
            canvas.width = w;
            canvas.height = h;
            context = canvas.getContext("2d", { willReadFrequently: true });
            context.imageSmoothingQuality = "high";
        }
        if (++count > 400) throw new Error("GIF có quá nhiều khung hình (tối đa 400).");
        if (onProgress) onProgress("Đang thu nhỏ GIF... khung " + count);
        sourceContext.putImageData(new ImageData(new Uint8ClampedArray(f.rgba), f.width, f.height), 0, 0);
        context.fillStyle = "#000";
        context.fillRect(0, 0, w, h);
        context.drawImage(source, 0, 0, w, h);
        const px = context.getImageData(0, 0, w, h).data;
        const keys = new Uint16Array(w * h);
        for (let p = 0, j = 0; p < px.length; p += 4, j++) {
            const k = ((px[p] >> 3) << 10) | ((px[p + 1] >> 3) << 5) | (px[p + 2] >> 3);
            keys[j] = k;
            hist[k]++;
        }
        frames.push({ keys, delay: Math.max(2, f.delay) });
        if (count % 5 === 0) await new Promise(r => setTimeout(r));
    }
    if (!frames.length) throw new Error("GIF không có khung hình nào.");
    const palette = buildPalette(hist);
    const map = makeMapper(palette);
    for (const f of frames) f.pixels = Uint8Array.from(f.keys, k => map(k));
    for (let step = 1; step <= 4; step++) {
        const picked = [];
        frames.forEach((f, i) => {
            if (i % step === 0) picked.push({ pixels: f.pixels, delay: f.delay });
            else picked[picked.length - 1].delay += f.delay;
        });
        const gif = encodeGif(w, h, palette, picked);
        if (gif.length <= MAX_BYTES) return gif;
    }
    throw new Error("Không thu GIF xuống dưới 1 MB được. Hãy chọn GIF ngắn hơn.");
}

window.GifResize = { supported: true, MAX_BYTES, resize, _test: { gifFrames, buildPalette, makeMapper, encodeGif } };
})();
