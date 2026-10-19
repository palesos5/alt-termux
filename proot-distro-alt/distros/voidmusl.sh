# This is a default distribution plug-in.
# Do not modify this file as your changes will be overwritten on next update.
# If you want customize installation, please make a copy.
DISTRO_NAME="Void Linux(musl)"
DISTRO_COMMENT="Version '20250202'."

TARBALL_URL['aarch64']="https://repo-default.voidlinux.org/live/current/void-aarch64-musl-ROOTFS-20250202.tar.xz"
TARBALL_SHA256['aarch64']="8f66e05401a953d151b3e82d132437840e0b24a51edff27f13202c9010dfa27d"
TARBALL_URL['x86_64']="https://repo-default.voidlinux.org/live/current/void-x86_64-musl-ROOTFS-20250202.tar.xz"
TARBALL_SHA256['x86_64']="27b2945ea7e2ca2a6ab123ce6f284b255adea2182820da25483d700ffe16e7c2"

distro_setup() {
	# Set default shell to bash.
	run_proot_cmd usermod --shell /bin/bash root
	# Fix issue where come CA certificates links may not be created.
	run_proot_cmd update-ca-certificates --fresh
}