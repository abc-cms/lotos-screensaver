### Installation
Run:
```
chmod +x ./postinstall.sh
sudo apt install -y cmake libjsoncpp-dev libspdlog-dev libcairo2-dev extra-cmake-modules libopencv-dev libxcb-image0-dev libxcb-screensaver0-dev
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
sudo make install
../postinstall.sh
```

Configuration file `~/.config/lotos-screensaver/config/config.yaml`.
Media files `~/.config/lotos-screensaver/media`.
