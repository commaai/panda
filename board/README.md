Programming
----

**Panda**

```
./recover.py      # flash bootstub
./flash.py        # flash application
```

Troubleshooting
----

If your panda will not flash and green LED is on, use `recover.py`.
If panda is blinking fast with green LED, use `flash.py`.

Otherwise if LED is off and panda can't be seen with `lsusb` command
use [panda paw](https://comma.ai/shop/products/panda-paw) to go into DFU mode on Comma 2 and external panda.

For the Comma 3's internal panda run these commands. 
You can ignore any output about the device resource being busy.

```
echo 124 > /sys/class/gpio/export
echo "out" > /sys/class/gpio/gpio124/direction
echo 1 > /sys/class/gpio/gpio124/value

echo 134 > /sys/class/gpio/export
echo "out" > /sys/class/gpio/gpio134/direction
echo 1 > /sys/class/gpio/gpio134/value

echo 124 > /sys/class/gpio/export
echo "out" > /sys/class/gpio/gpio124/direction
echo 0 > /sys/class/gpio/gpio124/value
```

With these done you should see "STM Device in DFU mode" or something similar when you run lsusb. If you do you should be able to run the recover.sh script in the panda folder. It may still be held in DFU after the flash, in which case run the following to get it out of DFU

```
echo 124 > /sys/class/gpio/export
echo "out" > /sys/class/gpio/gpio124/direction
echo 1 > /sys/class/gpio/gpio124/value

echo 134 > /sys/class/gpio/export
echo "out" > /sys/class/gpio/gpio134/direction
echo 0 > /sys/class/gpio/gpio134/value

echo 124 > /sys/class/gpio/export
echo "out" > /sys/class/gpio/gpio124/direction
echo 0 > /sys/class/gpio/gpio124/value
```



[dfu-util](http://github.com/dsigma/dfu-util.git) for flashing
