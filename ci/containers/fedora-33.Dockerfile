# THIS FILE WAS AUTO-GENERATED
#
#  $ lcitool dockerfile fedora-33 libnbd
#
# https://gitlab.com/libvirt/libvirt-ci/-/commit/7f787787a85647c3045ebfa6634966b4b96d5d99

FROM registry.fedoraproject.org/fedora:33

RUN dnf install -y nosync && \
    echo -e '#!/bin/sh\n\
if test -d /usr/lib64\n\
then\n\
    export LD_PRELOAD=/usr/lib64/nosync/nosync.so\n\
else\n\
    export LD_PRELOAD=/usr/lib/nosync/nosync.so\n\
fi\n\
exec "$@"' > /usr/bin/nosync && \
    chmod +x /usr/bin/nosync && \
    nosync dnf update -y && \
    nosync dnf install -y \
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
        nbd \
        nbdkit \
        ocaml \
        ocaml-findlib \
        ocamldoc \
        perl-Pod-Simple \
        perl-base \
        perl-podlators \
        pkgconfig \
        python3-devel \
        python3-flake8 \
        qemu \
        qemu-img \
        sed \
        util-linux && \
    nosync dnf autoremove -y && \
    nosync dnf clean all -y && \
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
