import sys
import os
import time
def compress(dir, upload_dir):
  os.chdir(dir)
  for filename in os.listdir(dir):
     if filename.endswith(".log"):
         file_prefix, suffix = filename.rsplit(".", 1)
         tarfilename = file_prefix + ".tar.gz"
         cmd = "tar -zcvf %s %s.log %s.idx --remove-files" % (tarfilename, file_prefix, file_prefix)
         os.system(cmd)
         os.rename(tarfilename, os.path.join(upload_dir, tarfilename))


def start_compress(dir, middle_dir, upload_dir):
   while True:
       compress(middle_dir, upload_dir)
       print "all files compress ok at ", time.strftime("%c")
       time.sleep(60)
       cmd = "find %s -type f -mmin +60|xargs -I f mv f %s" % (dir, middle_dir)
       os.system(cmd)

if __name__ == "__main__":
   dir = "/upload/1/logs"
   middle_dir = "/upload/ready"
   upload_dir = "/upload/rsync/gz_upload"
   if len(sys.argv) == 4:
      dir = sys.argv[1]
      middle_dir = sys.argv[2]
      upload_dir = sys.argv[3]

   start_compress(dir, middle_dir, upload_dir)
