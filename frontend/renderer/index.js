const { io } = require("socket.io-client");

document.addEventListener("DOMContentLoaded", () => {
    const statusDiv = document.getElementById("status");
    const usernameInput = document.getElementById("username");
    const connectBtn = document.getElementById("connectBtn");
    const messagesDiv = document.getElementById("messages");
    const peersList = document.getElementById("conn-peers");
    const randNum = document.getElementById("rand-num");

    let socket;

    connectBtn.addEventListener("click", () => {
        const username = usernameInput.value.trim();
        if (!username) {
            alert("Please enter a username");
            return;
        }

        // Connect to the backend server
        socket = io("http://localhost:5000");

        // Handle successful connection
        socket.on("connected", () => {
            statusDiv.textContent = "Connected";
            statusDiv.style.color = "green";
            socket.emit("register", { username: username, port: 12345 });
        });

        // Display new peer message
        socket.on("new_peer", (data) => {
            const message = document.createElement("div");
            message.textContent = `New peer connected: ${data.username} (${data.ip}:${data.port})`;
            messagesDiv.appendChild(message);
            socket.emit("discover_peers");
        });

        // Display peer list
        socket.on("peer_list", (data) => {
            console.log(data);
            peersList.innerHTML = "Connected peers:";
            data.forEach((peer) => {
                if (peer.username == username) return;
                console.log(peer.username);
                const message = document.createElement("div");
                message.textContent = `Peer: ${peer.username} (${peer.ip}:${peer.port})`;
                peersList.appendChild(message);
            });
        });

        // Handle disconnection
        socket.on("disconnect", () => {
            statusDiv.textContent = "Disconnected";
            statusDiv.style.color = "red";
        });

        // Handle peer disconnected event
        socket.on("peer_disconnected", () => {
            socket.emit("discover_peers");
        });

        // TEST: intermitent server->client communication
        socket.on("rand-num", (data) => {
            randNum.innerHTML = "Random number from server: " + data.number;
        });
    });
});
