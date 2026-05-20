# msrChat 部署指南

## 服务端部署（腾讯云）

### 1. 服务器依赖安装

```bash
sudo apt update && sudo apt install -y build-essential cmake libboost-all-dev libsqlite3-dev libssl-dev libprotobuf-dev protobuf-compiler libspdlog-dev nlohmann-json3-dev git
```

### 2. 克隆代码

```bash
git clone https://gitee.com/msrnnnnn/msrChat.git
```

### 3. 编译

```bash
cd msrChat/server/ChatServer
chmod +x build.sh && ./build.sh
```

### 4. 后台启动

```bash
cd build && nohup ./ChatServer > server.log 2>&1 &
```

### 5. 确认运行

```bash
ps aux | grep ChatServer
```

### 6. 停止服务

```bash
pkill ChatServer
```

---

## 客户端打包（Windows）

### 1. Release 编译

Qt Creator 中选择 **MSVC2022 64-bit Release** 套件，编译项目。

### 2. 打包依赖

在 PowerShell 中执行：

```powershell
cd "E:\Study\Project\Chat\msrChat\client\QmsrChat\build\Desktop_Qt_6_10_0_MSVC2022_64bit-Release"
& "E:\Qt\6.10.0\msvc2022_64\bin\windeployqt.exe" --release --qmldir "E:\Qt\6.10.0\msvc2022_64\qml" QmsrChat.exe
```

### 3. 打包发送

- 整个 `Desktop_Qt_6_10_0_MSVC2022_64bit-Release` 文件夹压缩为 zip
- 发给朋友

### 4. 朋友运行前需安装

VC++ 运行时（大多数电脑自带，没有则需安装）：
https://aka.ms/vs/17/release/vc_redist.x64.exe

---

## 客户端配置

朋友电脑上修改 `config.ini`：

```ini
[ChatServer]
host=175.178.118.47
port=8080
```

---

## 服务端信息

- 公网 IP：`175.178.118.47`
- 端口：`8080`
