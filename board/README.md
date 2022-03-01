Dependencies
--------

**Mac**

```
xcode-select --install
./get_sdk_mac.sh
```

**Debian / Ubuntu**

```
./get_sdk.sh
```


Programming
----

**Panda**

```
scons -u       # Compile
./flash_h7.sh  # for panda red
./flash.sh     # for other pandas
```

Troubleshooting
----

If your panda will not flash and is quickly blinking a single Green LED, use:
```
./recover_h7.sh  # for panda red
./recover.sh     # for other pandas
```

If your panda is not being recognized (for example after flashing wrong firmware),
use [panda paw](https://comma.ai/shop/products/panda-paw) to put panda into boot mode.


[dfu-util](http://github.com/dsigma/dfu-util.git) for flashing
