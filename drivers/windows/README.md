```
                                                                                                        ;"   ^;     ;'   ",
______/\\\\\\\\\\\____/\\\\\\\\\_______/\\\\\\\\\\\\\\\______/\\\\\\\\\\_____________/\\\____           ;    s$$$$$$$s     ;
 _____\/////\\\///___/\\\///////\\\____\/\\\///////////_____/\\\///////\\\__________/\\\\\____          ,  ss$$$$$$$$$$s  ,'
  _________\/\\\_____\///______\//\\\___\/\\\_______________\///______/\\\_________/\\\/\\\____         ;s$$$$$$$$$$$$$$$
   _________\/\\\_______________/\\\/____\/\\\\\\\\\\\\_____________/\\\//________/\\\/\/\\\____        $$$$$$$$$$$$$$$$$$
    _________\/\\\____________/\\\//______\////////////\\\__________\////\\\_____/\\\/__\/\\\____      $$$$P""Y$$$Y""W$$$$$
     _________\/\\\_________/\\\//____________________\//\\\____________\//\\\__/\\\\\\\\\\\\\\\\_     $$$$  p"$$$"q  $$$$$
      __/\\\___\/\\\_______/\\\/____________/\\\________\/\\\___/\\\______/\\\__\///////////\\\//__    $$$$  .$$$$$.  $$$$
       _\//\\\\\\\\\_______/\\\\\\\\\\\\\\\_\//\\\\\\\\\\\\\/___\///\\\\\\\\\/_____________\/\\\____  _ $$$$$$$$$$$$$$$$
        __\/////////_______\///////////////___\/////////////_______\/////////_______________\///_____| |  "Y$$$"*"$$$Y"
                                                                                _ __   __ _ _ __   __| | __ _"$b.$$"
                                                                               | '_ \ / _` | '_ \ / _` |/ _` |
                                                                               | |_) | (_| | | | | (_| | (_| |
                                                                               | .__/ \__,_|_| |_|\__,_|\__,_|
                                                                               | |     A comma.ai product.
                                                                               |_| (Code by Jessy Diamond Exum)
```


# What is J2534?

J2534 is an API that tries to provide a consistent way to send/receive
messages over the many different protocols supported by the OBD II
port. The place this is perhaps most obvious, is sending data over
different protocols (each using unique packetizing methods) using the
same data format.

For each PassThru Device that should be used with J2534 (in this case,
the panda), a 'driver' has to be written that can be loaded by a
client application wanting to send/receive data.

A lot of J2534 has good ideas behind it, but the standard has some odd choices:

* Platform Locked: Requires using the Windows Registry to find installed J2534 libraries/drivers. Drivers have to be DLLs.
* Architecture Locked: So far there is only support for x86.
* No device autodetect, and poor support for selecting from multiple devices.
* Constant vague language about important behavior (small differences between vendors).
* Most common differences become standard in later revisions.

# Why use J2534 with the panda?

J2534 is the only interface supported by most professional grade
vehicle diagnostics systems (such as HDS). These tools are useful for
diagnosing vehicles, as well as reverse engineering some lesser known
features.

# What parts are supported with panda?

- [ ] **J1850VPW** *(Outdated, and not physically supported by the panda)*
- [ ] **J1850PWM** *(Outdated, and not physically supported by the panda)*
- [X] **CAN**
- [X] **ISO15765**
- [ ] **ISO9141** *(This protocol could be implemented if 5 BAUD init support is added to the panda.)*
- [ ] **ISO14230/KWP2000** *(Could be supported with FAST init, 5baud init if panda adds support for 5bps serial)*

# Building the Project:

This project was developed with Visual Studio 2015, the Windows SDK,
and the Windows Driver Kit (WDK). At the time of writing, there is not
a stable WDK for Visual Studio 2017, but this project should build
with the new WDK and Visual Studio when it is available.

The WDK is only required for creating the signed WinUSB inf file. The
WDK may also provide the headers for WinUSB.

To build all the projects required for the installer, in Visual
Studio, select **Build->Batch Build.** In the project list select:

- **"panda"** *Release|x86*
- **"panda"** *Release|x64*
- **"panda Driver Package"** Debug|x86 (Note this inf file works with x86/amd64).
- **"pandaJ2534DLL"** *Release|x86*

The installer is generated with [NullSoft NSIS](http://nsis.sourceforge.net/Main_Page).
Use NSIS to run panda_install.nsi after building all the required projects.

# Installing

Installation would be straightforward were it not for the USB Driver
that needs to be setup. The driver itself is only a WinUSB inf file
(no actual driver), but it still needs to be signed.

Driver signing is a recent requirement of Windows (64 bit versions
only for some reason). If your Windows refuses to install the driver
(It almost certainly will), there are three choices:

- Self Sign the Driver.
- Disable Driver Signature Verification
- Purchase a certificate signed by a trusted authority.

Since self signed certificates have no chain of trust to a known
certificate authority, if you self sign, you will have to add your
cert to the root certificate store of your Windows' installation. This
is dangerous because it means anything signed with your cert will be
trusted. If you make your own cert, add a password so someone can't
copy it and screw with your computer's trust.

Disabling Signature Verification allows you to temporarily install
drivers without a trusted signature. Once you reboot, new drivers will
need to be verified again, but any installed drivers will stay where
they are. This option is irritating if you are installing and
uninstalling the inf driver multiple times, but overall, is safer than
the custom root certificate described above.

Purchasing a signed certificate is the best long term option, but it
is not useful for open source contributors, since the certificate will
be kept safe by the comma.ai team. Developers should use one of the
other two options.

**Note that certificate issues apply no matter if you are building
  from source or running an insaller .exe file.**

# Developing:

- Edit and merge pandaJ2534DLL\J2534register_x64.reg to register your development J2534 DLL.
- Add your output directory (panda\drivers\windows\Debug_x86) to your system PATH to avoid insanity.

# ToDo Items:

- Get official signing key for WinUSB driver inf file.
- Implement TxClear and RxClear. (Requires reading vague specifications).
- Apply a style-guide and consistent naming convention for Classes/Functions/Variables.
- Send multiple messages (each with a different address) from a given connection at the same time.
- Implement ISO14230/KWP2000 FAST (LIN communication is already supported with the raw panda USB driver).
- Find more documentation about SW_CAN_PS (Single Wire CAN, aka GMLAN).
- Find example of client using a _PS version of a protocol (PS is pin select, and may support using different CAN buses).


# Known Issues:

- ISO15765 Multi-frame TX: Hardware delays make transmission overshoot
  STMIN by several milliseconds. This does not violate the requirements
  of STMIN, it just means it is a little slower than it could be.

- All Tx messages from a single Connection are serialized. This can be
  relaxed to allow serialization of messages based on their address
  (making multiple queues, effectively one queue per address).
  
# Other:
Panda head ASCII art by dcau