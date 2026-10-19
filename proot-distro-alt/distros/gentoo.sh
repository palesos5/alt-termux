# This is a default distribution plug-in.
# Do not modify this file as your changes will be overwritten on next update.
# If you want customize installation, please make a copy.
DISTRO_NAME="Gentoo Linux"
DISTRO_COMMENT="Version '20251221T231557Z',"

TARBALL_URL['aarch64']="https://distfiles.gentoo.org/releases/arm64/autobuilds/20251221T231557Z/stage3-arm64-openrc-20251221T231557Z.tar.xz"
TARBALL_SHA256['aarch64']="c2eb43ea490678ad7c2f907561cdd681f6245c65c53b773340bb6f4ce7543d66"

TARBALL_URL['x86_64']="https://distfiles.gentoo.org/releases/amd64/autobuilds/20251228T163057Z/stage3-amd64-openrc-20251228T163057Z.tar.xz"
TARBALL_SHA256['x86_64']="5fcee76c577744d6cf5303d21a462e25108b056ab014034c227bd4d4fea9d673"

distro_setup() {
	# Set default shell to bash.
	run_proot_cmd usermod --shell /bin/bash root
	# Fix issue where come CA certificates links may not be created.
	run_proot_cmd update-ca-certificates --fresh
	# Set proper permissions for /tmp
	run_proot_cmd chmod 1777 /tmp
	# Set proper permissions for /var/tmp
	run_proot_cmd chmod 1777 /var/tmp

	# Configure make.conf
	run_proot_cmd mkdir -p /etc/portage
	run_proot_cmd sh -c 'echo "FEATURES=\"-pid-sandbox -ipc-sandbox -network-sandbox -sandbox -usersandbox -strict -userpriv\"" >> /etc/portage/make.conf'
	run_proot_cmd sh -c 'echo "GENTOO_MIRRORS=\"https://mirror.yandex.ru/gentoo-distfiles/\"" >> /etc/portage/make.conf'
	run_proot_cmd sh -c 'sed -i "s/COMMON_FLAGS=\"-O2 -pipe\"/COMMON_FLAGS=\"-march=native -O2 -pipe\"/g" /etc/portage/make.conf'

	# Add MAKEOPTS with half the number of CPU cores
	run_proot_cmd sh -c 'cores=$(nproc --all); half_cores=$(( (cores + 1) / 2 )); echo "MAKEOPTS=\"-j$half_cores\"" >> /etc/portage/make.conf'

	# Fix permissions for portage temporary directory
	run_proot_cmd mkdir -p /var/tmp/portage
	run_proot_cmd chmod 777 /var/tmp/portage

	# Configure Portage to work better in proot environment
	run_proot_cmd sh -c 'echo "PORTAGE_TMPDIR=\"/tmp\"" >> /etc/portage/make.conf'
	run_proot_cmd sh -c 'echo "PORTAGE_INST_GID=0" >> /etc/portage/make.conf'
	run_proot_cmd sh -c 'echo "PORTAGE_INST_UID=0" >> /etc/portage/make.conf'
	run_proot_cmd emerge-websync
}
