function signalText(rssi) {
    if (rssi >= -50) {
        return "Rất mạnh";
    }

    if (rssi >= -60) {
        return "Mạnh";
    }

    if (rssi >= -70) {
        return "Trung bình";
    }

    return "Yếu";
}

let selectedSsid = "";

function selectWifi(ssid) {
    selectedSsid = ssid;

    const panel = document.getElementById("passwordPanel");
    const ssidLabel = document.getElementById("selectedSsid");
    const password = document.getElementById("wifiPassword");

    ssidLabel.textContent = ssid;
    password.value = "";

    panel.classList.remove("hidden");
    password.focus();
}

document.getElementById("togglePassword").addEventListener("click", () => {
    const password = document.getElementById("wifiPassword");
    const button = document.getElementById("togglePassword");

    if (password.type === "password") {
        password.type = "text";
        button.textContent = "Ẩn";
    }
    else {
        password.type = "password";
        button.textContent = "Hiện";
    }
});

document.getElementById("connectButton").addEventListener("click", async () => {
    const password = document.getElementById("wifiPassword").value;
    const button = document.getElementById("connectButton");

    if (!selectedSsid) {
        return;
    }

    button.disabled = true;
    button.textContent = "Đang kết nối...";

    try {
        const body = new URLSearchParams();
        body.append("ssid", selectedSsid);
        body.append("password", password);

        const response = await fetch("/api/wifi/connect", {
            method: "POST",
            headers: {
                "Content-Type": "application/x-www-form-urlencoded",
            },
            body: body.toString(),
        });

        if (!response.ok) {
            throw new Error("HTTP error");
        }

        const result = await response.json();

        if (result.success) {
            alert("Kết nối Wi-Fi thành công!");
        }
        else {
            alert("Không thể kết nối Wi-Fi. Kiểm tra lại mật khẩu.");
        }
    }
    catch (error) {
        alert("Không thể gửi yêu cầu kết nối.");
    }

    button.disabled = false;
    button.textContent = "Kết nối";
});

async function scanWifi() {
    const status = document.getElementById("status");
    const list = document.getElementById("networks");

    status.style.display = "block";
    status.textContent = "Đang quét Wi-Fi...";
    list.innerHTML = "";

    try {
        const response = await fetch("/api/wifi/scan");

        if (!response.ok) {
            throw new Error("Wi-Fi scan failed");
        }

        const data = await response.json();

        status.style.display = "none";

        if (!data.networks.length) {
            status.style.display = "block";
            status.textContent = "Không tìm thấy Wi-Fi";
            return;
        }

        data.networks.forEach(network => {
            const button = document.createElement("button");
            button.className = "network";
            button.type = "button";
            button.onclick = () => selectWifi(network.ssid);

            const ssid = document.createElement("div");
            ssid.className = "ssid";
            ssid.textContent = "📶 " + network.ssid;

            const info = document.createElement("div");
            info.className = "info";
            info.textContent = signalText(network.rssi) + " · " + network.rssi + " dBm · Kênh " + network.channel;

            button.appendChild(ssid);
            button.appendChild(info);
            list.appendChild(button);
        });
    }
    catch (error) {
        status.style.display = "block";
        status.textContent = "Không thể quét Wi-Fi";
    }
}

document.getElementById("refreshButton").addEventListener("click", scanWifi);

scanWifi();