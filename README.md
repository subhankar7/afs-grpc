# afs-grpc

Run on server:
    ./greeter-server <afs-directory>
      
    e.g. ./greeter_server /afs/cs.wisc.edu/u/s/u/subhankar/private/739/fuse-2.9.4/nex/afs
    

Run on client :
    ./hello -d <cache-dir> <mountpoint-dir>
    
    e.g. ./hello -d ~/cache1 /tmp/fuse
  
  
The file-system will be available at /tmp/fuse . It can be unmounted using :
    fusermount -u <mountpoint-dir>
    
    e.g. fusermount -u /tmp/fuse
