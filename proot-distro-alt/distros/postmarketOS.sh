DISTRO_NAME="PostmarketOS"
DISTRO_COMMENT="V25.12"

TARBALL_URL['aarch64']="https://dl-cdn.alpinelinux.org/alpine/v3.23/releases/aarch64/alpine-minirootfs-3.23.2-aarch64.tar.gz"
TARBALL_SHA256['aarch64']="0a6c278a906bb7dc1c14f3dae426b4494280cdc421697977459bd37f91287d09"
TARBALL_URL['arm']="https://dl-cdn.alpinelinux.org/alpine/v3.23/releases/armv7/alpine-minirootfs-3.23.2-armv7.tar.gz"
TARBALL_SHA256['arm']="79603b670dc749b494aabbcfbb34ce74ae6e4e289e78e3738d7ddeb57fe9b6f0"
TARBALL_URL['x86_64']="https://dl-cdn.alpinelinux.org/alpine/v3.23/releases/x86_64/alpine-minirootfs-3.23.2-x86_64.tar.gz"
TARBALL_SHA256['x86_64']="b50bf42e519420ca2be48dd0efa22aa087c708d0602b67d413406533bef9dab5"

distro_setup() {
        run_proot_cmd sh -c 'echo "http://mirror.postmarketos.org/postmarketos/v25.12/" >> /etc/apk/repositories'
        run_proot_cmd sh -c 'apk add -u --allow-untrusted postmarketos-keys'
        run_proot_cmd sh -c 'apk update && apk upgrade -a'
        run_proot_cmd sh -c 'apk add -q curl'
        run_proot_cmd sh -c 'curl https://porkof48.ru/linux/postmarket_os.os-release > /etc/os-release'
}
