%{!?cmake_install: %global cmake_install make install DESTDIR=%{buildroot}}
%{!?_libqt5_includedir: %global _libqt5_includedir %{_qt5_headerdir}}


Summary: Statefs Qt bindings
Name: statefs-qt5
Version: x.x.x
Release: 1
License: LGPLv2
Group: System Environment/Tools
URL: http://github.com/nemomobile/statefs-qt
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
%if 0%{?_with_docs:1}
BuildRequires: graphviz
%endif
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
make %{?jobs:-j%jobs}
make doc

%install
rm -rf %{buildroot}
%cmake_install
make doc

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root,-)
%{_libdir}/libstatefs-qt5.so

%files devel
%defattr(-,root,root,-)
%{_libqt5_includedir}/statefs/qt/*.hpp
%{_libdir}/pkgconfig/statefs-qt5.pc

%files doc
%defattr(-,root,root,-)
%{_datarootdir}/doc/statefs-qt5/html/*

%files %{subscriber}
%defattr(-,root,root,-)
%{_libdir}/libcontextkit-statefs-qt5.so
%{_bindir}/contextkit-monitor
%{_libdir}/qt5/qml/Mer/State/*

%files %{subscriber_devel}
%defattr(-,root,root,-)
%{_includedir}/contextproperty.h
%{_libdir}/pkgconfig/contextkit-statefs.pc
%{_libdir}/pkgconfig/contextsubscriber-1.0.pc

%files tests
%defattr(-,root,root,-)
/opt/tests/%{name}/*

%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig
