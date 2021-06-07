# THIS FILE WAS AUTO-GENERATED
#
#  $ lcitool dockerfile ubuntu-1804 libnbd
#
# https://gitlab.com/libvirt/libvirt-ci/-/commit/7f787787a85647c3045ebfa6634966b4b96d5d99

FROM docker.io/library/ubuntu:18.04

RUN export DEBIAN_FRONTEND=noninteractive && \
    apt-get update && \
    apt-get install -y eatmydata && \
    eatmydata apt-get dist-upgrade -y && \
    eatmydata apt-get install --no-install-recommends -y \
            autoconf \
            automake \
            bash-completion \
            bsdmainutils \
            ca-certificates \
            ccache \
            clang \
            diffutils \
            flake8 \
            g++ \
            gcc \
            git \
            gnutls-bin \
            golang \
            iproute2 \
            jq \
            libc6-dev \
            libev-dev \
            libglib2.0-dev \
            libgnutls28-dev \
            libtool-bin \
            libxml2-dev \
            locales \
            make \
            nbd-client \
            nbd-server \
            ocaml \
            ocaml-findlib \
            ocaml-nox \
            perl \
            perl-base \
            pkgconf \
            python3-dev \
            qemu \
            qemu-utils \
            sed && \
    eatmydata apt-get autoremove -y && \
    eatmydata apt-get autoclean -y && \
    sed -Ei 's,^# (en_US\.UTF-8 .*)$,\1,' /etc/locale.gen && \
    dpkg-reconfigure locales && \
    dpkg-query --showformat '${Package}_${Version}_${Architecture}\n' --show > /packages.txt && \
    mkdir -p /usr/libexec/ccache-wrappers && \
    ln -s /usr/bin/ccache /usr/libexec/ccache-wrappers/c++ && \
    ln -s /usr/bin/ccache /usr/libexec/ccache-wrappers/cc && \
    ln -s /usr/bin/ccache /usr/libexec/ccache-wrappers/clang && \
    ln -s /usr/bin/ccache /usr/libexec/ccache-wrappers/g++ && \
    ln -s /usr/bin/ccache /usr/libexec/ccache-wrappers/gcc

ENV LANG "en_US.UTF-8"
ENV MAKE "/usr/bin/make"
ENV CCACHE_WRAPPERSDIR "/usr/libexec/ccache-wrappers"
