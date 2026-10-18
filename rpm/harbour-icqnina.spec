Name:       harbour-icqnina
Summary:    ICQ and AIM messaging on the NINA service
Version:    0.1.0
Release:    1
Group:      Qt/Qt
License:    GPLv3+ and GPLv2+
URL:        https://github.com/kierennicolas/harbour_ICQ_Nina
Source0:    %{name}-%{version}.tar.bz2

# liboscar is private to the app: do not export or depend on it as a library.
%define __provides_exclude_from ^%{_datadir}/%{name}/lib/.*$
%define __requires_exclude ^liboscar.*$

Requires:       sailfishsilica-qt5 >= 0.10.9
Requires:       qt5-plugin-sqldriver-sqlite
BuildRequires:  pkgconfig(sailfishapp) >= 1.0.2
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Gui)
BuildRequires:  pkgconfig(Qt5Network)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5Quick)
BuildRequires:  pkgconfig(Qt5Sql)
BuildRequires:  pkgconfig(Qt5Xml)
BuildRequires:  pkgconfig(Qt5DBus)
BuildRequires:  desktop-file-utils

%description
ICQ Nina is an ICQ-first instant messenger for Sailfish OS that connects to
NINA (nina.chat), the revival service for the classic OSCAR protocol. Sign in
with an ICQ number (or AIM screen name), see your server-side contact list
and presence, and chat with notifications and history.
Unofficial; not affiliated with NINA, ICQ or AOL.

%prep
%autosetup -n %{name}-%{version}

%build
%qmake5 CONFIG+=sailfishapp_build VERSION=%{version}
%make_build

%install
%qmake5_install
desktop-file-install --delete-original --dir %{buildroot}%{_datadir}/applications %{buildroot}%{_datadir}/applications/*.desktop

%files
%defattr(-,root,root,-)
%{_bindir}/%{name}
%{_datadir}/%{name}
%{_datadir}/applications/%{name}.desktop
%{_datadir}/icons/hicolor/*/apps/%{name}.png
