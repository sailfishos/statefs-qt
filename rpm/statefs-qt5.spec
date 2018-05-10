%{!?cmake_install: %global cmake_install make install DESTDIR=%{buildroot}}
%{!?_libqt5_includedir: %global _libqt5_includedir %{_qt5_headerdir}}


Summary: Statefs Qt bindings
Name: statefs-qt5
Version: x.x.x
Release: 1
License: LGPLv2.1
Group: System Environment/Tools
URL: https://git.merproject.org/mer-core/statefs-qt
Source0: %{name}-%{version}.tar.bz2
BuildRequires: cmake >= 2.8
BuildRequires: statefs >= 0.3.18
BuildRequires: pkgconfig(statefs-cpp) >= 0.3.18
BuildRequires: pkgconfig(cor) >= 0.1.17
BuildRequires: pkgconfig(qtaround) >= 0.2.4
BuildRequires: pkgconfig(Qt5Core)
BuildRequires: pkgconfig(Qt5Qml)
BuildRequires: pkgconfig(tut) >= 0.0.3

%description
%{summary}

%package devel
Summary: StateFS Qt5 bindings development files
Group: Development/Libraries
Requires: %{name} = %{version}-%{release}
%description devel
%{summary}

%package doc
Summary: StateFS Qt5 bindings documentation
Group: Documenation
BuildRequires: doxygen
%description doc
%{summary}

%define subscriber -n statefs-contextkit-subscriber
%define subscriber_devel -n statefs-contextkit-subscriber-devel

%package %{subscriber}
Summary: Statefs contextkit subscriber adapter for Qt5
Group: System Environment/Libraries
Requires: statefs >= 0.3.8
Requires: statefs-qt5 = %{version}-%{release}
%description %{subscriber}
%{summary}

%package %{subscriber_devel}
Summary: Contextkit property interface using statefs
Group: System Environment/Libraries
Requires: statefs-contextkit-subscriber = %{version}-%{release}
%description %{subscriber_devel}
Contextkit property interface using statefs instead of contextkit

%package -n statefs-declarative-qt5
Summary: Statefs QML plugin
Group: System Environment/Libraries
Requires: statefs-contextkit-subscriber = %{version}-%{release}
%description -n statefs-declarative-qt5
%{summary}

%package -n contextkit-declarative-qt5
Summary: Contextkit QML plugin
Group: System Environment/Libraries
Requires: statefs-contextkit-subscriber = %{version}-%{release}
Obsoletes: nemo-qml-plugin-contextkit-qt5 <= 1.1.8
Provides: nemo-qml-plugin-contextkit-qt5 = 1.1.9
%description -n contextkit-declarative-qt5
%{summary}

%package tests
Summary:    Tests for %{name}
Group:      System Environment/Libraries
Requires:   %{name} = %{version}-%{release}
%description tests
%summary

%prep
%setup -q

%build
%cmake -DVERSION=%{version} %{?_with_multiarch:-DENABLE_MULTIARCH=ON}
make %{?_smp_mflags}
make doc

%install
rm -rf %{buildroot}
%cmake_install
make doc

%files
%defattr(-,root,root,-)
%{_libdir}/libstatefs-qt5.so
%doc COPYING

%files devel
%defattr(-,root,root,-)
%dir %{_libqt5_includedir}/statefs/qt
%{_libqt5_includedir}/statefs/qt/*.hpp
%{_libdir}/pkgconfig/statefs-qt5.pc

%files doc
%defattr(-,root,root,-)
%{_datarootdir}/doc/statefs-qt5/html/*

%files %{subscriber}
%defattr(-,root,root,-)
%{_libdir}/libcontextkit-statefs-qt5.so
%{_bindir}/contextkit-monitor

%files %{subscriber_devel}
%defattr(-,root,root,-)
%{_includedir}/contextproperty.h
%{_libdir}/pkgconfig/contextkit-statefs.pc
%{_libdir}/pkgconfig/contextsubscriber-1.0.pc

%files -n statefs-declarative-qt5
%defattr(-,root,root,-)
%{_libdir}/qt5/qml/Mer/State/*

%files -n contextkit-declarative-qt5
%defattr(-,root,root,-)
%{_libdir}/qt5/qml/org/freedesktop/contextkit/*

%files tests
%defattr(-,root,root,-)
/opt/tests/%{name}/*

%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig

%post %{subscriber} -p /sbin/ldconfig
%postun %{subscriber} -p /sbin/ldconfig

%post -n statefs-declarative-qt5 -p /sbin/ldconfig
%postun -n statefs-declarative-qt5 -p /sbin/ldconfig

%post -n contextkit-declarative-qt5 -p /sbin/ldconfig
%postun -n contextkit-declarative-qt5 -p /sbin/ldconfig
