DISTRO_NAME="OpenWrt"
DISTRO_COMMENT="OpenWrt 24.10.5 - Embedded Linux for routers (minimal rootfs)"

TARBALL_URL['aarch64']="https://downloads.openwrt.org/releases/24.10.5/targets/armsr/armv8/rootfs.tar.gz"
TARBALL_SHA256['aarch64']="f8f6bba184d691365e15bf6241a3f88afe7af47fc443c6e9ee5de15e1c9d6d43"
TARBALL_URL['arm']="https://downloads.openwrt.org/releases/24.10.5/targets/armsr/armv7/rootfs.tar.gz"
TARBALL_SHA256['arm']="4c9050533cc261721d0c89dc4a459b0a70804350a14c320d3bb7f3be7a9bf860"
TARBALL_URL['i686']="https://downloads.openwrt.org/releases/24.10.5/targets/x86/legacy/rootfs.tar.gz"
TARBALL_SHA256['i686']="279585863332f00cdc876237120e708724aa3346a8721312d28d6f22f051492b"
TARBALL_URL['x86_64']="https://downloads.openwrt.org/releases/24.10.5/targets/x86/64/rootfs.tar.gz"
TARBALL_SHA256['x86_64']="fb3245bce37575f1fc9cc467909c37c5b12e49bf7993a34cd19893511bccf40e"

distro_setup() {
	# Создание обязательных системных директорий
	run_proot_cmd mkdir -p /dev /proc /sys /tmp /var/run /var/lock /var/opkg-lists
	
	# Настройка DNS (без этого opkg не работает)
	run_proot_cmd tee /etc/resolv.conf > /dev/null << 'EOF'
nameserver 8.8.8.8
nameserver 1.1.1.1
EOF
	
	# Инициализация opkg
	run_proot_cmd opkg update 2>/dev/null || true
	
	# Установка минимального набора утилит (без этого даже 'ls' может отсутствовать)
	run_proot_cmd opkg install --force-overwrite base-files 2>/dev/null || true
	
	# Настройка хостнейма и os-release
	run_proot_cmd echo "OpenWrt" > /etc/hostname
	run_proot_cmd tee /etc/os-release > /dev/null << 'EOF'
NAME="OpenWrt"
VERSION="24.10.5"
ID=openwrt
VERSION_ID=24.10.5
PRETTY_NAME="OpenWrt 24.10.5"
EOF
}
