# THIS FILE WAS AUTO-GENERATED
#
#  $ lcitool dockerfile centos-8 libnbd
#
# https://gitlab.com/libvirt/libvirt-ci/-/commit/7f787787a85647c3045ebfa6634966b4b96d5d99

FROM docker.io/library/centos:8

RUN dnf update -y && \
    dnf install 'dnf-command(config-manager)' -y && \
    dnf config-manager --set-enabled -y powertools && \
    dnf install -y centos-release-advanced-virtualization && \
    dnf install -y epel-release && \
    dnf install -y \
        autoconf \
        automake \
        bash-completion \
        ca-certificates \
        ccache \
        clang \
        diffutils \
        fuse3 \
        fuse3-devel \
        gcc \
        gcc-c++ \
        git \
        glib2-devel \
        glibc-devel \
        glibc-langpack-en \
        gnutls-devel \
        gnutls-utils \
        golang \
        iproute \
        jq \
        libev-devel \
        libtool \
        libxml2-devel \
        make \
        nbdkit \
        ocaml \
        ocaml-findlib \
        ocamldoc \
        perl \
        perl-Pod-Simple \
        perl-podlators \
        pkgconfig \
        python3-devel \
        python3-flake8 \
        qemu-img \
        qemu-kvm \
        sed \
        util-linux && \
    dnf autoremove -y && \
    dnf clean all -y && \
    rpm -qa | sort > /packages.txt && \
    mkdir -p /usr/libexec/ccache-wrappers && \
    ln -s /usr/bin/ccache /usr/libexec/ccache-wrappers/c++ && \
    ln -s /usr/bin/ccache /usr/libexec/ccache-wrappers/cc && \
    ln -s /usr/bin/ccache /usr/libexec/ccache-wrappers/clang && \
    ln -s /usr/bin/ccache /usr/libexec/ccache-wrappers/g++ && \
    ln -s /usr/bin/ccache /usr/libexec/ccache-wrappers/gcc

ENV LANG "en_US.UTF-8"
ENV MAKE "/usr/bin/make"
ENV CCACHE_WRAPPERSDIR "/usr/libexec/ccache-wrappers"