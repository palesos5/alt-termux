# Custom Proot Distro Repository

A collection of Proot Distro configurations for different Linux distributions. This repository provides easy-to-install configurations that can be used with Termux's `proot-distro` command.

## What is Proot Distro?

Proot Distro is a utility for Termux that makes it easy to install and manage various Linux distributions in a chroot environment. It allows you to run native GNU/Linux distributions without rooting your device.

## Installation

To install the configurations from this repository:

```bash
# Clone the repository
git clone https://github.com/yourusername/My-proot-distro-repo.git
cd My-proot-distro-repo

# Run the install script
./install.sh
```

The installation script will copy all configuration files from the `distros` directory to `$PREFIX/etc/proot-distro/`.

## Usage

After installation, you can use the installed distributions with:

```bash
# List available distributions
proot-distro list

# Install a distribution (example)
proot-distro install <distro-name>

# Login to a distribution (example)
proot-distro login <distro-name>
```

## Contributing

If you'd like to contribute new distribution configurations:

1. Create a new configuration file in the `distros` directory
2. Follow the existing format and conventions
3. Test the configuration before submitting
4. Submit a pull request

## Directory Structure

- `distros/` - Contains distribution configuration files
- `install.sh` - Installation script
- `README.md` - This file
- `LICENSE` - License information

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
