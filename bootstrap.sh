sudo apt-get -y install vlc xscreensaver
pip install poetry
poetry export -f requirements.txt > requirements.txt
pip install --no-input -r requirements.txt
cp ./.xscreensaver ~/
python configure_xscreensaver.py
chmod +x ./lotos_saver.py
systemctl --user enable xscreensaver
xscreensaver -no-splash </dev/null &>/dev/null &