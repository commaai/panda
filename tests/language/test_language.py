#!/usr/bin/env python

import subprocess
import sys

checked_ext = ["h", "c", "py", "pyx", "cpp", "hpp", "md", "mk"]

if __name__ == "__main__":
  with open("list.txt", 'r') as handle:

    suffix_cmd = " "
    for i in checked_ext:
      suffix_cmd +=  "-- '*." + i + "' "

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
    sys.exit("Failed: Found profanities")
  else:
    print "Success"
