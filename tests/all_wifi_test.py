#!/usr/bin/python
import requests
from automated.helpers import _connect_wifi
from panda import Panda
from nose.tools import assert_equal

if __name__ == "__main__":
  for p in Panda.list():
    dongle_id, pw = Panda(p).get_serial()
    assert(dongle_id.isalnum())
    assert(pw.isalnum())
    _connect_wifi(dongle_id, pw)

    r = requests.get("http://192.168.0.10/")
    print r.text
    wifi_dongle_id = r.text.split("ssid: panda-")[1].split("<br/>")[0]

    assert_equal(str(dongle_id), wifi_dongle_id)

