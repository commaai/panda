#!/usr/bin/env python

import subprocess
import sys

ignored_ext = [".pdf", ".jpg", ".png", "profanities.txt"]

if __name__ == "__main__":
  with open("list.txt", 'r') as handle:

    suffix_cmd = " "
    for i in ignored_ext:
      suffix_cmd +=  "-- './*' ':(exclude)*" + i + "' "

    found_profanity = False
    for line in handle:
      line = line.rstrip('\n').rstrip(" ")
      try:
        cmd = "cd ../../; git grep --ignore-case -w '" + line + "'" + suffix_cmd
        res = subprocess.check_output(cmd, shell=True, stderr=subprocess.STDOUT)
        print res
        found_profanity = True
      except subprocess.CalledProcessError as e:
        pass
  if found_profanity:
    sys.exit("Found profanities")
