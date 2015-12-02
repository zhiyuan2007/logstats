import struct
def www():
   fp = open("ttt", "wb")
   a = 11
   b = "hello world"

def rrr():
   fp = open("/home/liuguirong/stats/stats-201511130000", "rb")
   len = fp.read(4)
   l, = struct.unpack("i", len)
   print "len is %d!" % (l)
   c = fp.read(l)
   #print "str %s!" %(c)
   
   fp.close()

rrr()

