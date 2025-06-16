# PeerBridge

**PeerBridge** is a small peer-to-peer vlan/vpn app.  
It wraps a native C++ networking core (`peerbridgenet.exe`) in an Electron/React UI so you can create an end-to-end-encrypted “virtual LAN” connection with just a couple of clicks.

---

## Downloading PeerBridge

1. Head to the project’s **[Releases](../../releases)** page.  
2. Grab the installer for your platform  
   * **Windows:** `PeerBridge-Setup-vX.Y.Z.exe`
3. Run the installer and launch the app – that’s it!

## Using PeerBridge

1. Launch the application.
2. Confirm elevation of `peerbridgenet.exe` and allow it acess on private networks.
3. Register or authenticate.
4. Create a lobby or join a friend's lobby.
5. Start the lobby.
6. Done!

---

## Building it yourself

### Prerequisites

| Tool | Version |
|------|---------|
| Node.js | ≥ 16 |
| npm / pnpm | latest |
| CMake | ≥ 3.10 |
| MSYS2 MINGW environment | gcc compiler |
| Git | latest |

### 1. Clone the repo

```bash
# From root dir
git clone https://github.com/your-org/peerbridge.git
cd peerbridge
```

### 2. Install JavaScript dependencies

```bash
# From root dir
cd frontend
npm install
```

### 3. Build the networking core

#### 1. Install `msys2 mingw`:  https://www.msys2.org

#### 2. Install necessary libraries

The networking module has the following dependencies present in msys2 repo:
- Boost (Boost.Asio)
- gRPC
- gTest

Open the `msys2 mingw` terminal and run the following command:
```bash
pacman -S mingw-w64-x86_64-{cmake, mingw32-make, libraries}
```

Since quill isn't available in the msys2 repo, we must build it ourselves

#### 3. Building quill

1. Go to quill github repo
https://github.com/odygrd/quill

2. Clone the project

3. Create build directory

4. Run CMake and `mingw32-make install`

5. Copy the generated header and CMakeLists from `Program Files` folder into the respective `msys2\mingw64` folder.

#### 4. Building the C++ executable

```bash
# From root dir
cd networking
mkdir build
cd build
cmake .. -G "MinGW Makefiles" -DENABLE_DEP_COPY=ON
mingw32-make
```

### 4. Copying the C++ build and running the app

```bash
# From root dir
cd frontend
./copy_cpp_build.bat
npm run dev
```