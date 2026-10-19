Name:       harbour-icqnina
Summary:    ICQ and AIM messaging on the NINA service
Version:    0.3.3
Release:    1
Group:      Qt/Qt
License:    GPLv3+ and GPLv2+
URL:        https://github.com/kierennicolas/harbour_ICQ_Nina
Source0:    %{name}-%{version}.tar.bz2

# liboscar is private to the app: do not export or depend on it as a library.
%define __provides_exclude_from ^%{_datadir}/%{name}/lib/.*$
%define __requires_exclude ^liboscar.*$

Requires:       sailfishsilica-qt5 >= 0.10.9
# The Qt SQLite driver (history) ships on every Sailfish OS device, and Harbour
# does not allow depending on it; without it the app runs with history off.
BuildRequires:  pkgconfig(sailfishapp) >= 1.0.2
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Gui)
BuildRequires:  pkgconfig(Qt5Network)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5Quick)
BuildRequires:  pkgconfig(Qt5Sql)
BuildRequires:  pkgconfig(Qt5Xml)
BuildRequires:  pkgconfig(Qt5DBus)
BuildRequires:  pkgconfig(Qt5Multimedia)
BuildRequires:  desktop-file-utils

%description
ICQ Nina is an ICQ-first instant messenger for Sailfish OS that connects to
NINA (nina.chat), the revival service for the classic OSCAR protocol. Sign in
with an ICQ number (or AIM screen name), see your server-side contact list
and presence, and chat with notifications and history. Add and remove
contacts, ask for and give authorization, look up ICQ numbers, block
people, and send and receive files (directly, through NINA's relay, or as a
download link). Profile pictures, your own profile, photos (from the
gallery or the camera), voice messages, stickers (add your own) and emoji.
Unofficial; not affiliated with NINA, ICQ or AOL.

%prep
%autosetup -n %{name}-%{version}

%build
%qmake5 CONFIG+=sailfishapp_build VERSION=%{version}
%make_build

%install
%qmake5_install
# mb2 builds without a debuginfo package, so nothing else strips the binaries
# (liboscar is ~55 MB with debug info, ~2 MB without).
%{__strip} --strip-unneeded %{buildroot}%{_bindir}/%{name} %{buildroot}%{_datadir}/%{name}/lib/liboscar.so.1.0.0
desktop-file-install --delete-original --dir %{buildroot}%{_datadir}/applications %{buildroot}%{_datadir}/applications/*.desktop

%files
%defattr(-,root,root,-)
%{_bindir}/%{name}
%{_datadir}/%{name}
%{_datadir}/applications/%{name}.desktop
%{_datadir}/icons/hicolor/*/apps/%{name}.png
