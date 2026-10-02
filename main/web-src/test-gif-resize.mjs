import assert from "node:assert/strict";
import { existsSync } from "node:fs";
import { resolve } from "node:path";
import { pathToFileURL } from "node:url";
import { chromium } from "playwright-core";
import gifenc from "gifenc";
import gifuct from "gifuct-js";

const { GIFEncoder } = gifenc;
const { parseGIF, decompressFrame } = gifuct;

const chrome = process.env.CHROME_PATH || "C:/Program Files/Google/Chrome/Application/chrome.exe";
if (!existsSync(chrome)) throw new Error("Set CHROME_PATH to a Chromium browser for this test.");

function fixture(width, height) {
    const encoder = GIFEncoder();
    const pixels = new Uint8Array(width * height);
    const colors = [[0, 0, 0], [255, 0, 0], [0, 255, 0], [0, 0, 255]];
    for (let frame = 1; frame <= 3; frame++) {
        pixels.fill(frame === 1 ? 1 : 0);
        if (frame > 1) {
            for (let y = height / 4; y < (frame === 2 ? height * 3 / 4 : height); y++) {
                for (let x = width / 4; x < (frame === 2 ? width * 3 / 4 : width); x++) {
                    pixels[y * width + x] = frame;
                }
            }
        }
        encoder.writeFrame(pixels, width, height, {
            palette: colors,
            delay: frame * 40,
            repeat: 3,
            dispose: frame === 1 ? 2 : 1,
            transparent: frame > 1,
            transparentIndex: 0,
        });
    }
    encoder.finish();
    return [...encoder.bytes()];
}

const browser = await chromium.launch({ executablePath: chrome, headless: true });
try {
    const page = await browser.newPage();
    await page.goto(pathToFileURL(resolve("../web/home.html")).href);
    await page.addScriptTag({ path: resolve("../web/gif-resize.js") });
    const result = await page.evaluate(async ({ large, small }) => {
        const toFile = bytes => new File([Uint8Array.from(bytes)], "test.gif", { type: "image/gif" });
        const smallFile = toFile(small);
        const unchanged = await FriendGifResize.resizeGif(smallFile);
        const resized = await FriendGifResize.resizeGif(toFile(large));
        return { unchanged: unchanged === smallFile, bytes: [...new Uint8Array(await resized.arrayBuffer())] };
    }, { large: fixture(320, 160), small: fixture(120, 120) });

    assert.equal(result.unchanged, true);
    assert.ok(result.bytes.length <= 1024 * 1024);
    const gif = parseGIF(Uint8Array.from(result.bytes).buffer);
    assert.equal(gif.lsd.width, 240);
    assert.equal(gif.lsd.height, 120);
    const loop = gif.frames.find(frame => frame.application?.id === "NETSCAPE2.0")?.application.blocks;
    assert.deepEqual([...loop], [1, 3, 0]);
    const frames = gif.frames.filter(frame => frame.image).map(frame => decompressFrame(frame, gif.gct, true));
    assert.equal(frames.length, 3);
    assert.deepEqual(frames.map(frame => frame.delay), [40, 80, 120]);
    assert.ok(frames.every(frame => frame.patch.length === 240 * 120 * 4));
    const colorAt = (frame, x, y) => [...frame.patch.slice((y * 240 + x) * 4, (y * 240 + x) * 4 + 3)];
    assert.deepEqual(colorAt(frames[0], 10, 10), [255, 0, 0]);
    assert.deepEqual(colorAt(frames[1], 10, 10), [0, 0, 0]);
    assert.deepEqual(colorAt(frames[1], 120, 60), [0, 255, 0]);
    assert.deepEqual(colorAt(frames[2], 120, 60), [0, 0, 255]);
    console.log("GIF resize: passthrough, frame timing and disposal passed");
} finally {
    await browser.close();
}
