const connection = document.getElementById("connection");
const displayState = document.getElementById("displayState");
const uploadStatus = document.getElementById("uploadStatus");
const picker = document.getElementById("imageFile");
const preview = document.getElementById("preview");
const faceButton = document.getElementById("showFace");
let previewUrl = null;
let busy = false;

async function refreshStatus() {
    try {
        const response = await fetch("/api/status", { cache: "no-store" });
        if (!response.ok) throw new Error("status");
        const status = await response.json();
        connection.textContent = status.online ? status.ip : "Đã ngắt Wi-Fi";
        displayState.textContent = status.display === "gif" ? "Đang phát GIF" :
            status.display === "image" ? "Đang hiện ảnh" : "Đang hiện mặt Mộc";
        if (!status.gifAvailable) uploadStatus.textContent = "GIF tạm không khả dụng do thiếu bộ nhớ.";
    } catch (_) {
        connection.textContent = "Không kết nối được";
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

async function upload(file) {
    if (busy) return;
    const gif = /\.gif$/i.test(file.name) || file.type === "image/gif";
    const still = /\.(jpe?g|png)$/i.test(file.name) || file.type === "image/jpeg" || file.type === "image/png";
    if (!gif && !still) {
        uploadStatus.textContent = "Chỉ nhận JPG, PNG hoặc GIF.";
        return;
    }
    if (gif && file.size > 1024 * 1024) {
        uploadStatus.textContent = "GIF phải nhỏ hơn 1 MB.";
        return;
    }

    busy = true;
    picker.disabled = faceButton.disabled = true;
    uploadStatus.textContent = "Đang chuẩn bị ảnh...";
    try {
        if (previewUrl) URL.revokeObjectURL(previewUrl);
        previewUrl = URL.createObjectURL(file);
        preview.src = previewUrl;
        preview.classList.remove("hidden");
        const image = await loadImage(previewUrl);
        if (gif && (image.naturalWidth > 240 || image.naturalHeight > 240)) {
            throw new Error("GIF phải có kích thước tối đa 240×240.");
        }

        const body = gif ? file : imageBytes(image);
        uploadStatus.textContent = "Đang gửi tới Mộc...";
        const response = await fetch(gif ? "/api/media/gif" : "/api/media/still", {
            method: "POST", headers: { "Content-Type": "application/octet-stream" }, body,
        });
        if (!response.ok) throw new Error("Tải ảnh thất bại (HTTP " + response.status + ").");
        uploadStatus.textContent = "Đã gửi ảnh tới màn hình.";
        setTimeout(refreshStatus, 500);
    } catch (error) {
        uploadStatus.textContent = error.message;
    } finally {
        busy = false;
        picker.disabled = faceButton.disabled = false;
    }
}

picker.addEventListener("change", () => {
    const file = picker.files[0];
    if (file) upload(file);
    picker.value = "";
});

faceButton.addEventListener("click", async () => {
    if (busy) return;
    busy = true;
    faceButton.disabled = true;
    try {
        const response = await fetch("/api/media/face", { method: "POST" });
        if (!response.ok) throw new Error("Không chuyển được màn hình.");
        uploadStatus.textContent = "Đã trở lại mặt Mộc.";
        setTimeout(refreshStatus, 500);
    } catch (error) {
        uploadStatus.textContent = error.message;
    } finally {
        busy = false;
        faceButton.disabled = false;
    }
});

refreshStatus();
setInterval(refreshStatus, 5000);
