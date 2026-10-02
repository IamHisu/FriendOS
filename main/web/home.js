const connection = document.getElementById("connection");
const displayState = document.getElementById("displayState");
const uploadStatus = document.getElementById("uploadStatus");
const gifNotice = document.getElementById("gifNotice");
const picker = document.getElementById("imageFile");
const pickerLabel = picker.closest(".file-picker");
const preview = document.getElementById("preview");
const screenEmpty = document.getElementById("screenEmpty");
const faceButton = document.getElementById("showFace");
let previewUrl = null;
let busy = false;
let refreshing = false;
let currentDisplay = "face";

function setMessage(text, kind) {
    uploadStatus.textContent = text;
    uploadStatus.className = kind || "";
}

function renderScreen() {
    const showLocal = previewUrl && currentDisplay !== "face";
    preview.classList.toggle("hidden", !showLocal);
    screenEmpty.classList.toggle("hidden", showLocal);
    screenEmpty.textContent = currentDisplay === "face" ? "Đang hiện mặt Mộc"
        : currentDisplay === "gif" ? "Đang phát GIF trên Mộc" : "Đang hiện ảnh trên Mộc";
}

function setBusy(value) {
    busy = value;
    picker.disabled = faceButton.disabled = value;
    pickerLabel.classList.toggle("disabled", value);
}

async function refreshStatus() {
    if (refreshing || busy) return;
    refreshing = true;
    try {
        const response = await fetch("/api/status", { cache: "no-store", signal: AbortSignal.timeout(4000) });
        if (!response.ok) throw new Error("status");
        const status = await response.json();
        connection.textContent = status.online ? status.ip : "Đã ngắt Wi-Fi";
        connection.className = status.online ? "online" : "offline";
        currentDisplay = status.display === "gif" || status.display === "image" ? status.display : "face";
        displayState.textContent = currentDisplay === "gif" ? "Đang phát GIF" :
            currentDisplay === "image" ? "Đang hiện ảnh" : "Đang hiện mặt Mộc";
        gifNotice.textContent = status.gifAvailable ? "" : "GIF tạm không khả dụng do thiếu bộ nhớ.";
        renderScreen();
    } catch (_) {
        connection.textContent = "Không kết nối được";
        connection.className = "offline";
    } finally {
        refreshing = false;
    }
}

function imageBytes(image) {
    const canvas = document.createElement("canvas");
    canvas.width = canvas.height = 240;
    const context = canvas.getContext("2d", { willReadFrequently: true });
    if (!context) throw new Error("Trình duyệt không hỗ trợ Canvas.");
    context.fillStyle = "#000";
    context.fillRect(0, 0, 240, 240);
    const scale = Math.min(240 / image.naturalWidth, 240 / image.naturalHeight);
    const width = Math.max(1, Math.round(image.naturalWidth * scale));
    const height = Math.max(1, Math.round(image.naturalHeight * scale));
    context.drawImage(image, (240 - width) / 2, (240 - height) / 2, width, height);
    const pixels = context.getImageData(0, 0, 240, 240).data;
    const bytes = new Uint8Array(240 * 240 * 2);
    for (let i = 0, j = 0; i < pixels.length; i += 4, j += 2) {
        const value = ((pixels[i] & 0xf8) << 8) | ((pixels[i + 1] & 0xfc) << 3) | (pixels[i + 2] >> 3);
        bytes[j] = value & 0xff;
        bytes[j + 1] = value >> 8;
    }
    return bytes;
}

function loadImage(url) {
    return new Promise((resolve, reject) => {
        const image = new Image();
        image.onload = () => resolve(image);
        image.onerror = () => reject(new Error("Không đọc được ảnh."));
        image.src = url;
    });
}

function clearPreview() {
    if (previewUrl) URL.revokeObjectURL(previewUrl);
    previewUrl = null;
    preview.removeAttribute("src");
}

async function upload(file) {
    if (busy) return;
    const gif = /\.gif$/i.test(file.name) || file.type === "image/gif";
    const still = /\.(jpe?g|png)$/i.test(file.name) || file.type === "image/jpeg" || file.type === "image/png";
    if (!gif && !still) {
        setMessage("Chỉ nhận JPG, PNG hoặc GIF.", "error");
        return;
    }
    const sourceLimit = 8 * 1024 * 1024;
    if (gif && file.size > sourceLimit) {
        setMessage("GIF gốc tối đa 8 MB.", "error");
        return;
    }

    setBusy(true);
    setMessage("Đang chuẩn bị ảnh...");
    try {
        clearPreview();
        previewUrl = URL.createObjectURL(file);
        preview.src = previewUrl;
        const image = await loadImage(previewUrl);
        let body;
        if (!gif) body = imageBytes(image);
        else {
            if (typeof FriendGifResize === "undefined") throw new Error("Không tải được bộ xử lý GIF. Hãy mở lại trang.");
            body = await FriendGifResize.resizeGif(file, (done, total) => {
                setMessage(`Đang xử lý GIF: ${done}/${total} frame...`);
            });
            if (body !== file) {
                URL.revokeObjectURL(previewUrl);
                previewUrl = URL.createObjectURL(body);
                preview.src = previewUrl;
            }
        }
        setMessage("Đang gửi tới Mộc...");
        const response = await fetch(gif ? "/api/media/gif" : "/api/media/still", {
            method: "POST", headers: { "Content-Type": "application/octet-stream" }, body,
        });
        if (!response.ok) throw new Error(response.status === 503
            ? "Mộc đang bận hoặc thiếu bộ nhớ. Thử lại sau vài giây."
            : "Tải ảnh thất bại (HTTP " + response.status + ").");
        currentDisplay = gif ? "gif" : "image";
        renderScreen();
        setMessage(gif && body !== file ? `Đã gửi GIF chuẩn hóa (${Math.round(body.size / 1024)} KB).`
            : "Đã gửi ảnh tới màn hình.", "ok");
        setTimeout(refreshStatus, 500);
    } catch (error) {
        clearPreview();
        renderScreen();
        setMessage(error.message, "error");
    } finally {
        setBusy(false);
    }
}

picker.addEventListener("change", () => {
    const file = picker.files[0];
    if (file) upload(file);
    picker.value = "";
});

faceButton.addEventListener("click", async () => {
    if (busy) return;
    setBusy(true);
    try {
        const response = await fetch("/api/media/face", { method: "POST" });
        if (!response.ok) throw new Error("Không chuyển được màn hình.");
        clearPreview();
        currentDisplay = "face";
        renderScreen();
        setMessage("Đã trở lại mặt Mộc.", "ok");
        setTimeout(refreshStatus, 500);
    } catch (error) {
        setMessage(error.message, "error");
    } finally {
        setBusy(false);
    }
});

refreshStatus();
setInterval(refreshStatus, 5000);
