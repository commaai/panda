# Updating your panda

Panda should update automatically via the [Chffr](http://chffr.comma.ai/) app ([apple](https://itunes.apple.com/us/app/chffr-dash-cam-that-remembers/id1146683979) and [android](https://play.google.com/store/apps/details?id=ai.comma.chffr))

You can manually update it using:
```
sudo pip install --upgrade pandacan
PYTHONPATH="" sudo python -c "import panda; panda.flash_release()"
```
