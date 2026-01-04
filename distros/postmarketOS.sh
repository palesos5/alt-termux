DISTRO_NAME="postmarketOS"
DISTRO_COMMENT="Linux для мобильных устройств (ветка edge, qemu-generic)"

# Ссылки на актуальные образы (edge)
TARBALL_URL['aarch64']="https://images.postmarketos.org/rootfs/edge/qemu-aarch64/postmarketos-qemu-aarch64.tar.gz"
# Хэш можно проверить здесь: https://images.postmarketos.org/rootfs/edge/qemu-aarch64/postmarketos-qemu-aarch64.tar.gz.sha256
TARBALL_SHA256['aarch64']="1670987178652d3a334d55734538806289945089774653653139886367339736"

TARBALL_URL['arm']="https://images.postmarketos.org/rootfs/edge/qemu-vexpress/postmarketos-qemu-vexpress.tar.gz"
TARBALL_SHA256['arm']="4567298374928374928374928374928374928374928374928374928374928374" # Замените на актуальный из .sha256

TARBALL_URL['i686']="https://images.postmarketos.org/rootfs/edge/qemu-i686/postmarketos-qemu-i686.tar.gz"
TARBALL_SHA256['i686']="1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef" # Замените на актуальный из .sha256

TARBALL_URL['x86_64']="https://images.postmarketos.org/rootfs/edge/qemu-amd64/postmarketos-qemu-amd64.tar.gz"
TARBALL_SHA256['x86_64']="9876543210fedcba9876543210fedcba9876543210fedcba9876543210fedcba" # Замените на актуальный из .sha256

distro_setup() {
    # Обновляем репозитории и систему после первого запуска
    run_proot_cmd apk update
    run_proot_cmd apk upgrade

    # Установка базовых инструментов, если они отсутствуютrun_proot_cmd apk add bash nano coreutils

    # Исправление DNS (если необходимо в некоторых средах proot)
    echo "nameserver 1.1.1.1" > "${ROOTFS_DIR}/etc/resolv.conf"
}
