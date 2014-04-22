# Testing とあるOS

Before trying とあるOS, keep in mind that it is a *hobby project*, not a professional operating system. Both the build process and the OS itself are prone to problems and bugs. If you are not already familiar with systems software and emulators, using とあるOS may prove difficult and annoying. While I do my best to answer questions on IRC, I have a full-time job and other obligations that prevent me from being readily available and I do not put much effort into making the build system generally usable.

## Requirements ##

That said, there are a few things you will need before building とあるOS:

* **A capable build environment**: I develop under an Ubuntu derivative. I suggest the most recent *LTS* release of something like Xubuntu. The most recent version of Ubuntu ships with an a version of `automake` that removes a necessary legacy feature, but this can be corrected with a script provided in `toolchain/legacy-automake.sh`. If you are not using Ubuntu and you experience issues with missing packages, I can attempt to help, but provide no warranty of support.
* **An understanding of Unix build tools**: The build system for とあるOS is a disparate combination of shell scripts, Makefiles, and a bit of Python magic. If you are not already familiar with normal build tools on Unix-like systems, no amount of automation will help you resolve issues.

On Ubuntu and Debian systems, the automated build scripts will attempt to install the prerequisite packages for building the toolchain. The toolchain is then used to build a number of libraries for userspace, the userspace itself, and then the kernel. The standard build tools do not create a bootable harddisk image, though a tool is provided to do this if you want to test with an emulator other than qemu.

Also, from the hardware perspective you need to have a bare minimum of the following:

* Virtual Box VM:
 - **RAM**: At least *3.5GB*.
 - **Hard Drive**: At least *4GB* of free space (Mostly needed for the produced image file).
* Ubuntu:
 - **RAM**: *1-2GB* Depending on the image type you are targeting.
 - **Hard Drive**: *2-4GB* of free space (Mostly needed for the produced image file).

## Building ##

Once you have a capable build environment set up and the repository cloned, start by running `make toolchain`. This will either prompt you for your password (using `sudo` to attempt to install a number of development packages) or yell about not knowing what operating system you're on. In the latter case, the script includes a list of packages with both Debian names and Fedora names, and you should attempt to install all of them using your distribution's package manager.

A complete build of the toolchain with `make toolchain` takes about 30 minutes on my hardware, but I have a rather fast Internet connection and a very capable CPU - it could take many hours on less capable hardware.

If the build process fails, your best bet for support is to sit on my IRC channel (`#toaruos` on Freenode), post your question, and *wait* for me to eventually answer it (if I can). Leaving after five minutes will not get your question answered. If you are not around when I see your question, I won't even attempt to answer it. Do not attempt to report issues with build scripts as bugs - while some issues may actually be bugs in the scripts, they are usually not.

After you have the toolchain build has completed, source the toolchain with `. toolchain/activate.sh` and then run `make` to build the kernel and userspace.

The Makefile also includes some convenience targets for running とあるOS in QEMU. The most important of these is `make kvm`, which will attempt to run a fairly standard graphical environment, with KVM acceleration enabled. If you don't have KVM available, you may use `make run`, but expect the GUI to run poorly as it relies rather heavily on the CPU for compositing.

The default image provides two user accounts, `root` with password `toor` and `local` with password `local`. The latter is the preferred account: as in any Unix-like system, you should not normally log in as `root`. Some system calls require root-level permissions, but as of writing this documentation filesystem permissions are not implemented.

### Building an Image for VirtualBox, etc. ###
An experimental, unsupported image building tool that will produce a "production-ready" image, including GRUB 2, is provided in the `util` directory. The script, `create-image.sh`, must be run as root and will produce a 1GB raw disk image which be converted to other formats as needed.

## Running

The Makefile provides a few phony targets which will spin up qemu in various setups. Most of these targets have "regular" and "KVM-enabled" variants:

- `run`/`kvm`: Start a typical desktop session. The system will boot to a graphical login screen.
- `vga`/`vga-kvm`: (**DEPRECATED**) Boot to a VGA text-mode interface.
- `term`/`term-kvm`: Start the compositor with a single, full-screen terminal logged in as root.
- `term-beta`: Starts a beta version of the new compositor (not useful for end-users).
- `headless`: Start qemu without a graphical interface, allowing for quick use of the serial console.

All modes enable the serial console. This can be disabled by removing `debug_shell` from the default module list (`BOOT_MODULES`) in the Makefile. The serial console provides a kernel-space shell with some debugging commands and can be extended from other modules. A userspace shell can be started with the `shell` command, but will not have a full environment - run `login` to spawn a login prompt from the userspace shell. The serial console is able to determine its width and height with a `divine-size` command, which is useful if you plan on running userspace applications like Vim, otherwise applications will asume a default of 80x24.

## Troubleshooting
If you have any issues at all while attempting to build とあるOS, or you would like help building on an unsupported platform, please join us on IRC (`#toaruos@irc.freenode.net`). Problems encountered while using the build scripts in an unsupported environment will not be considered bugs until thoroughly examined.

