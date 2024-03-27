sudo apt-get -y install xscreensaver libopencv-dev pthon3-opencv
pip install --no-input -r requirements.txt
cp ./.xscreensaver ~/
python configure_xscreensaver.py
chmod +x ./lotos_saver.py
sudo systemctl enable xscreensaver
xscreensaver -no-splash </dev/null &>/dev/null &