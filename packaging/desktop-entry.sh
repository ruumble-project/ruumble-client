# shared by the package scripts: the desktop entry of an installed Ruumble Client (program on PATH)
desktop_entry() {
  cat <<'DESK'
[Desktop Entry]
Type=Application
Name=Ruumble Client
GenericName=Voice chat with the Ruumble building
Comment=Mumble client with Ruumble built in
Exec=ruumble-client %u
Icon=ruumble-client
Terminal=false
Categories=Network;Chat;
StartupWMClass=ruumble-client
DESK
}
