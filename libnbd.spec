%global debug_package %{nil}

Name: libnbd
Epoch: 100
Version: 1.11.8
Release: 1%{?dist}
Summary: NBD client library in userspace
License: LGPLv2+
URL: https://github.com/libguestfs/libnbd/tags
Source0: %{name}_%{version}.orig.tar.gz
BuildRequires: autoconf
BuildRequires: automake
BuildRequires: libtool
BuildRequires: gnupg2
BuildRequires: gcc
BuildRequires: make
BuildRequires: gnutls-devel
BuildRequires: libxml2-devel
BuildRequires: fuse3
BuildRequires: fuse3-devel
BuildRequires: python3-devel
BuildRequires: ocaml
BuildRequires: ocaml-findlib-devel
BuildRequires: ocaml-ocamldoc
BuildRequires: glib2-devel
BuildRequires: bash-completion
BuildRequires: util-linux

%description
NBD — Network Block Device — is a protocol for accessing Block Devices
(hard disks and disk-like things) over a Network.

This is the NBD client library in userspace, a simple library for
writing NBD clients.

The key features are:

 * Synchronous and asynchronous APIs, both for ease of use and for
   writing non-blocking, multithreaded clients.

 * High performance.

 * Minimal dependencies for the basic library.

 * Well-documented, stable API.

 * Bindings in several programming languages.

%prep
%autosetup -T -c -n %{name}_%{version}-%{release}
tar -zx -f %{S:0} --strip-components=1 -C .

%build
%configure \
    --disable-static \
    --with-tls-priority=@LIBNBD,SYSTEM \
    PYTHON=%{__python3} \
    --enable-python \
    --enable-ocaml \
    --enable-fuse \
    --disable-golang

make %{?_smp_mflags}

%install
%make_install

# Delete libtool crap.
find $RPM_BUILD_ROOT -name '*.la' -delete

# Delete the golang man page since we're not distributing the bindings.
rm $RPM_BUILD_ROOT%{_mandir}/man3/libnbd-golang.3*

%check

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
Recommends: fuse3

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
%{python3_sitearch}/libnbdmod*.so
%{python3_sitearch}/nbd.py
%{python3_sitearch}/nbdsh.py
%{python3_sitearch}/__pycache__/nbd*.py*
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

%changelog
