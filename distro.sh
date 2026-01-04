distros=("Alpine" "alt" "gentoo" "postmarketOS" "void" "voidmusl")

echo "Select a distribution:"

select distro in "${distros[@]}"; do
    if [ -n "$distro" ]; then
        proot-distro install "$distro" && proot-distro login "$distro"
        break
    else
        echo "Invalid selection. Please try again."
    fi
done
