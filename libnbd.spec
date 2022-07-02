%global debug_package %{nil}

%global _lto_cflags %{?_lto_cflags} -ffat-lto-objects

Name: libnbd
Epoch: 100
Version: 1.12.1
Release: 1%{?dist}
Summary: NBD client library in userspace
License: LGPL-2.1-or-later
URL: https://github.com/libguestfs/libnbd/tags
Source0: %{name}_%{version}.orig.tar.gz
%if 0%{?suse_version} > 1500 || 0%{?sle_version} > 150000
BuildRequires: bash-completion
BuildRequires: bash-completion-devel
BuildRequires: gpg2
%else
BuildRequires: bash-completion
BuildRequires: gnupg2
%endif
BuildRequires: autoconf
BuildRequires: automake
BuildRequires: fdupes
BuildRequires: fuse3
BuildRequires: fuse3-devel
BuildRequires: gcc
BuildRequires: glib2-devel
BuildRequires: gnutls-devel
BuildRequires: libtool
BuildRequires: libxml2-devel
BuildRequires: make
BuildRequires: ocaml
BuildRequires: ocaml-findlib-devel
BuildRequires: ocaml-ocamldoc
BuildRequires: pkgconfig
BuildRequires: python3-devel
BuildRequires: util-linux

%description
NBD — Network Block Device — is a protocol for accessing Block Devices
(hard disks and disk-like things) over a Network.

%prep
%autosetup -T -c -n %{name}_%{version}-%{release}
tar -zx -f %{S:0} --strip-components=1 -C .

%build
autoreconf -i
%configure \
    --disable-golang \
    --disable-static \
    --enable-fuse \
    --enable-ocaml \
    --enable-python \
    --with-tls-priority=@LIBNBD,SYSTEM \
    bashcompdir=%{_datadir}/bash-completion/completions \
    PYTHON=%{__python3}
%make_build

%install
%make_install
find $RPM_BUILD_ROOT -name '*.la' -delete
rm $RPM_BUILD_ROOT%{_mandir}/man3/libnbd-golang.3*
find %{buildroot}%{python3_sitearch} -type f -name '*.pyc' -exec rm -rf {} \;
%fdupes -s %{buildroot}%{python3_sitearch}

%check

%if 0%{?suse_version} > 1500 || 0%{?sle_version} > 150000
%package -n libnbd0
Summary: Core library for nbd

%description -n libnbd0
This is the NBD client library in userspace, a simple library for
writing NBD clients.

%package devel
Summary: Development headers for libnbd
Requires: libnbd0 = %{epoch}:%{version}-%{release}

%description devel
This package contains development headers for libnbd.

%package -n ocaml-libnbd
Summary: OCaml language bindings for libnbd
Requires: libnbd = %{epoch}:%{version}-%{release}

%description -n ocaml-libnbd
This package contains OCaml language bindings for libnbd.

%package -n ocaml-libnbd-devel
Summary: OCaml language development package for libnbd
Requires: ocaml-libnbd = %{epoch}:%{version}-%{release}

%description -n ocaml-libnbd-devel
This package contains OCaml language development package for
libnbd.  Install this if you want to compile OCaml software which
uses libnbd.

%package -n python3-libnbd
Summary: Python 3 bindings for libnbd
Requires: libnbd = %{epoch}:%{version}-%{release}
%{?python_provide:%python_provide python3-libnbd}

# The Python module happens to be called lib*.so.  Don't scan it and
# have a bogus "Provides: libnbdmod.*".
%global __provides_exclude_from ^%{python3_sitearch}/lib.*\\.so

%description -n python3-libnbd
python3-libnbd contains Python 3 bindings for libnbd.

%package -n nbdfuse
Summary: FUSE support for libnbd
Requires: libnbd0 = %{epoch}:%{version}-%{release}

%description -n nbdfuse
This package contains FUSE support for libnbd.

%package bash-completion
Summary: Bash tab-completion for libnbd
BuildArch: noarch
Requires: bash-completion >= 2.0
Requires: libnbd0 = %{epoch}:%{version}-%{release}

%description bash-completion
Install this package if you want intelligent bash tab-completion
for libnbd.

%post -n libnbd0 -p /sbin/ldconfig
%postun -n libnbd0 -p /sbin/ldconfig

%files
%doc README
%{_bindir}/nbdcopy
%{_bindir}/nbdinfo
%{_mandir}/man1/nbdcopy.1*
%{_mandir}/man1/nbdinfo.1*

%files -n libnbd0
%license COPYING.LIB
%{_libdir}/libnbd.so.0
%{_libdir}/libnbd.so.0.*

%files devel
%{_includedir}/libnbd.h
%{_libdir}/libnbd.so
%{_libdir}/pkgconfig/libnbd.pc
%{_mandir}/man3/libnbd.3*
%{_mandir}/man1/libnbd-release-notes-1.*.1*
%{_mandir}/man3/libnbd-security.3*
%{_mandir}/man3/nbd_*.3*

%files -n ocaml-libnbd
%{_libdir}/ocaml/nbd
%exclude %{_libdir}/ocaml/nbd/*.a
%exclude %{_libdir}/ocaml/nbd/*.cmxa
%exclude %{_libdir}/ocaml/nbd/*.cmx
%exclude %{_libdir}/ocaml/nbd/*.mli
%{_libdir}/ocaml/stublibs/dllmlnbd.so
%{_libdir}/ocaml/stublibs/dllmlnbd.so.owner

%files -n ocaml-libnbd-devel
%doc ocaml/examples/*.ml
%license ocaml/examples/LICENSE-FOR-EXAMPLES
%{_libdir}/ocaml/nbd/*.a
%{_libdir}/ocaml/nbd/*.cmxa
%{_libdir}/ocaml/nbd/*.cmx
%{_libdir}/ocaml/nbd/*.mli
%{_mandir}/man3/libnbd-ocaml.3*
%{_mandir}/man3/NBD.3*
%{_mandir}/man3/NBD.*.3*

%files -n python3-libnbd
%{python3_sitearch}/*
%{_bindir}/nbdsh
%{_mandir}/man1/nbdsh.1*

%files -n nbdfuse
%{_bindir}/nbdfuse
%{_mandir}/man1/nbdfuse.1*

%files bash-completion
%{_datadir}/bash-completion
%endif

%if !(0%{?suse_version} > 1500) && !(0%{?sle_version} > 150000)
%package devel
Summary: Development headers for libnbd
License: LGPLv2+ and BSD
Requires: libnbd = %{epoch}:%{version}-%{release}

%description devel
This package contains development headers for libnbd.

%package -n ocaml-libnbd
Summary: OCaml language bindings for libnbd
Requires: libnbd = %{epoch}:%{version}-%{release}

%description -n ocaml-libnbd
This package contains OCaml language bindings for libnbd.

%package -n ocaml-libnbd-devel
Summary: OCaml language development package for libnbd
Requires: ocaml-libnbd = %{epoch}:%{version}-%{release}

%description -n ocaml-libnbd-devel
This package contains OCaml language development package for
libnbd.  Install this if you want to compile OCaml software which
uses libnbd.

%package -n python3-libnbd
Summary: Python 3 bindings for libnbd
Requires: libnbd = %{epoch}:%{version}-%{release}
%{?python_provide:%python_provide python3-libnbd}

# The Python module happens to be called lib*.so.  Don't scan it and
# have a bogus "Provides: libnbdmod.*".
%global __provides_exclude_from ^%{python3_sitearch}/lib.*\\.so

%description -n python3-libnbd
python3-libnbd contains Python 3 bindings for libnbd.

%package -n nbdfuse
Summary: FUSE support for libnbd
License: LGPLv2+ and BSD
Requires: libnbd = %{epoch}:%{version}-%{release}

%description -n nbdfuse
This package contains FUSE support for libnbd.

%package bash-completion
Summary: Bash tab-completion for libnbd
BuildArch: noarch
Requires: bash-completion >= 2.0
# Don't use _isa here because it's a noarch package.  This dependency
# is just to ensure that the subpackage is updated along with libnbd.
Requires: libnbd = %{epoch}:%{version}-%{release}

%description bash-completion
Install this package if you want intelligent bash tab-completion
for libnbd.

%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig

%files
%doc README
%license COPYING.LIB
%{_bindir}/nbdcopy
%{_bindir}/nbdinfo
%{_libdir}/libnbd.so.*
%{_mandir}/man1/nbdcopy.1*
%{_mandir}/man1/nbdinfo.1*

%files devel
%doc TODO examples/*.c
%license examples/LICENSE-FOR-EXAMPLES
%{_includedir}/libnbd.h
%{_libdir}/libnbd.so
%{_libdir}/pkgconfig/libnbd.pc
%{_mandir}/man3/libnbd.3*
%{_mandir}/man1/libnbd-release-notes-1.*.1*
%{_mandir}/man3/libnbd-security.3*
%{_mandir}/man3/nbd_*.3*

%files -n ocaml-libnbd
%{_libdir}/ocaml/nbd
%exclude %{_libdir}/ocaml/nbd/*.a
%exclude %{_libdir}/ocaml/nbd/*.cmxa
%exclude %{_libdir}/ocaml/nbd/*.cmx
%exclude %{_libdir}/ocaml/nbd/*.mli
%{_libdir}/ocaml/stublibs/dllmlnbd.so
%{_libdir}/ocaml/stublibs/dllmlnbd.so.owner

%files -n ocaml-libnbd-devel
%doc ocaml/examples/*.ml
%license ocaml/examples/LICENSE-FOR-EXAMPLES
%{_libdir}/ocaml/nbd/*.a
%{_libdir}/ocaml/nbd/*.cmxa
%{_libdir}/ocaml/nbd/*.cmx
%{_libdir}/ocaml/nbd/*.mli
%{_mandir}/man3/libnbd-ocaml.3*
%{_mandir}/man3/NBD.3*
%{_mandir}/man3/NBD.*.3*

%files -n python3-libnbd
%{python3_sitearch}/*
%{_bindir}/nbdsh
%{_mandir}/man1/nbdsh.1*

%files -n nbdfuse
%{_bindir}/nbdfuse
%{_mandir}/man1/nbdfuse.1*

%files bash-completion
%dir %{_datadir}/bash-completion/completions
%{_datadir}/bash-completion/completions/nbdcopy
%{_datadir}/bash-completion/completions/nbdfuse
%{_datadir}/bash-completion/completions/nbdinfo
%{_datadir}/bash-completion/completions/nbdsh
%endif

%changelog