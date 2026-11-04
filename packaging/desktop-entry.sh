# shared by the package scripts: the desktop entry of an installed Ruumble Desk (program on PATH)
desktop_entry() {
  cat <<'DESK'
[Desktop Entry]
Type=Application
Name=Ruumble Desk
GenericName=Voice chat with the Ruumble building
Comment=Mumble client with Ruumble built in
Exec=ruumble-desk %u
Icon=ruumble-desk
Terminal=false
Categories=Network;Chat;
StartupWMClass=ruumble-desk
DESK
}
