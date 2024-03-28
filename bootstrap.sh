sudo apt-get -y install xscreensaver libopencv-dev python3-opencv
export PYTHON_KEYRING_BACKEND=keyring.backends.null.Keyring
pip install --no-input -r requirements.txt
cp ./.xscreensaver ~/
python configure_xscreensaver.py
chmod +x ./lotos_saver.py
xscreensaver -no-splash </dev/null &>/dev/null &
