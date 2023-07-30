# 转封装示例代码 Remux Sample

- remux_tofile.cpp，适合文件转封装文件、直播流转封装文件、直播流转封装直播流
- remux_tofile.cpp，suitable for remux file to file, live streaming to file, live streaming to live streaming
- remux_tostream.cpp，适合文件转封装直播流
- remux_tostream.cpp，suitable for remux file to live streaming

## 环境安装 Environment Installation

可以选择docker环境或者手动安装环境，二选一即可。

You can choose the docker environment or manually install the environment, just choose one of the two.

### Docker环境 Docker Environment

```
#下载镜像 Download Mirror
docker pull stoprefactoring/sample:videoprocessing_remux

#运行容器 Running containers
docker run -dit \
--network=host \
-e TZ="Asia/Shanghai" \
-e LD_LIBRARY_PATH="/usr/local/lib" \
--name sample_remux \
stoprefactoring/sample:videoprocessing_remux \
bin/bash
```

### 手动安装环境 Manual installation environment

以下为Ubuntu22.04示例，依赖FFmpeg6.0

The following is an Ubuntu 22.04 example ,dependency  on FFmpeg 6.0

```
#安装编译环境 Installing the compilation environment
apt -y install gcc      #gcc 4.8以上, gcc 4.8 and above
apt -y install g++      #g++ 9.3以上, g++ 9.3 and above
apt -y install cmake    #cmake 3.5以上, cmake 3.5 and above

#安装FFmpeg6.0 Installing FFmpeg 6.0
apt -y install yasm build-essential pkg-config
git clone --single-branch -b release/6.0 https://github.com/FFmpeg/FFmpeg.git
./configure --enable-shared --enable-nonfree --enable-gpl
make && make install
```

## 编译运行示例代码 Compile and run the sample code

下载本项目代码，使用docker环境的话也需要下载 Download the code of this project，If you are using a docker environment, you will also need to download the code

```
git clone https://github.com/YiiGaa/VideoProcessing.git
```

编译程序 compilation program

```
cd VideoProcessing/remux
mkdir build
cd build
cmake ..
make
```

运行程序 running program

```
./remux_tofile                  #运行remux_tofile.cpp程序
./remux_tostream								#运行remux_tostream.cpp程序
```

## 补充说明 Additional Notes

如果需要测试直播流，需要自己搭建流媒体服务，如SRS等。

If you need to test live streaming, you need to build your own streaming service such as SRS.
