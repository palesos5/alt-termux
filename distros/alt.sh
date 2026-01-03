DISTRO_NAME="ALT Linux"
DISTRO_COMMENT="ALT Linux p10 (Starterkit Rootfs)"

TARBALL_URL['aarch64']="https://ftp.altlinux.org/pub/distributions/ALTLinux/p10/images/cloud/aarch64/alt-p10-rootfs-systemd-aarch64.tar.xz"
TARBALL_SHA256['aarch64']="a6c05f778207607fd3e73737aecd923b976e4f323cfe969a394dc3e12507a053"

TARBALL_URL['arm']="https://ftp.altlinux.org/pub/distributions/ALTLinux/p10/images/cloud/armh/alt-p10-rootfs-systemd-armh.tar.xz"
TARBALL_SHA256['arm']="669894e2e28886326164998818818c32607936a2872337d1217e2501e7195c8c"

TARBALL_URL['i686']="https://ftp.altlinux.org/pub/distributions/ALTLinux/p10/images/cloud/i586/alt-p10-rootfs-systemd-i586.tar.xz"
TARBALL_SHA256['i686']="4b35da851eab166664beeb9870f1b089c52fca9d518b099b22c9c598ed001164"

TARBALL_URL['x86_64']="https://ftp.altlinux.org/pub/distributions/ALTLinux/p10/images/cloud/x86_64/alt-p10-rootfs-systemd-x86_64.tar.xz"
TARBALL_SHA256['x86_64']="2d3b3e5ba4430fb2e09a8714583b8a2a30f97e506226222f00f657c5b72d5be5"

distro_setup() {
  run_proot_cmd apt-get update
  run_proot_cmd apt-get dist-upgrade -y
  run_proot_cmd apt-get install -y apt-utils
}
