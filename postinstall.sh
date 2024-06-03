if [ ! -d ~/.config/lotos-screensaver ]; then
  mkdir ~/.config/lotos-screensaver
fi

mkdir -p ~/.config/systemd/user

systemctl --user stop lotos.service
systemctl --user stop lotos.timer
systemctl --user disable lotos.service
systemctl --user disable lotos.timer

rm -f ~/.config/systemd/user/lotos.service
rm -f ~/.config/systemd/user/lotos.timer

cp -r /usr/local/share/lotos-screensaver/config ~/Documents/
cp -r /usr/local/share/lotos-screensaver/media ~/.config/lotos-screensaver/
cp /usr/local/share/lotos-screensaver/config/systemd/lotos.service ~/.config/systemd/user/
cp /usr/local/share/lotos-screensaver/config/systemd/lotos.timer ~/.config/systemd/user/

systemctl --user daemon-reload
systemctl --user enable lotos.timer
systemctl --user start lotos.timer