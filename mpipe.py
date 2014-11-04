#! /usr/bin/env python
from mathlink import *
from time import sleep

def qanda():
  "Performs one In/Out cycle"
  sendexpression(raw_input())
  getresult()
  return;

def sendexpression(str):
  "Sends str to the Mathematica kernel"
  k.newpacket()
  k.putfunction("EvaluatePacket",1)
  k.putfunction("ToString",1)
  k.putfunction("ToExpression",1)
  k.putstring(str)
  k.endpacket()
  return

def getresult():
  "To be called after sendexpression to get the result"
  if k.nextpacket() == 3:
    print k.getstring()
  else:
    print "I don't know what to tell you"
    geterror()
  return

def gettoken():
  "Returns the current token type"
  t = tokendictionary[k.getnext()]
  #print t
  return t;

# Not used anymore
#def getpacket():
#  "Returns the current packet type"
#  print packetdictionary[k.nextpacket()]
#  return;
 
# possible way to address errors.  If there is an error, then a series
# of gettoken/k.getXXX can generate the result until the string "$Failed"
# is passed.  One should not try to get a token after this string, as the
# link will hang.
def geterror():
  "Returns the error, I hope"
  myerror = None
  curval = None
  while curval != "$Failed":
    curval = returnresult(gettoken())
    print curval
  return;

def returnresult(token):
  val = None
  if token == "MLTKSYM":
    val = k.getsymbol()
  elif token == "MLTKSTR":
    val = k.getstring() 
  elif token == "MLTKFUNC":
    val = k.getfunction() 
  else:
    val = token
  return val

k = env().openargv(['','-linkname','wolfram -mathlink','-linkmode','launch'])
k.connect()
while k.ready()!=1:
  sleep(0.5)
k.nextpacket()
k.getstring()
print "\nmpipe is ready to work\n"
while True:
  qanda()


