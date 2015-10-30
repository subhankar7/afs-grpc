/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.

  gcc -Wall hello.c `pkg-config fuse --cflags --libs` -o hello
*/

#define FUSE_USE_VERSION 26

#include <iostream>
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <grpc++/grpc++.h>
#include <sys/stat.h>
#include "greeter_client.h"

#include "helloworld.grpc.pb.h"


using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using helloworld::HelloRequest;
using helloworld::HelloReply; 
using helloworld::Greeter;

static const char *hello_str = "Hello World!\n";
static const char *hello_path = "/first_file";
static GreeterClient *ctx;

char fs_path[PATH_MAX];

static int hello_getattr(const char *path, struct stat *stbuf)
{
	int res = 0;

	memset(stbuf, 0, sizeof(struct stat));

        res = ctx->GetFileStat(path, stbuf);

	if(res<0) {
            res = -ENOENT;
        }

        printf("FUSE Last Mod: %d\n", stbuf->st_mtime);

//        std::string user("world");
//        std::string reply = ctx->SayHello(user);
//        std::cout << "Greeter received: " << reply << std::endl;

	return res;
}

static int hello_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{

        ctx->ListDir(path, buf, filler);
	return 0;
}

unsigned long
hash(unsigned char *str)
{   
    unsigned long hash = 5381;
    int c;
    
    while (c = *str++)
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    
    return hash;
}


static int hello_open(const char *path, struct fuse_file_info *fi)
{
        char *buf;
        int size;
        int rc;
        int fd;
        int isStale = 0;
        int isFetched = 0;
        char cacheFileName[80];
        struct stat cacheFileInfo;
        struct stat remoteFileInfo;
        char local_path[PATH_MAX];
        local_path[0] = '\0';
        char cbuf[] = "Check String";
        char nbuf[1000];

/*	if ((fi->flags & 3) != O_RDONLY)
		return -EACCES; */

        snprintf(cacheFileName, 80, "%lu", hash((unsigned char *)path));

        strncat(local_path, fs_path, PATH_MAX);
        strncat(local_path, cacheFileName, PATH_MAX);
        printf("path: %s\n", local_path);

//        fd = open(local_path, O_RDWR | O_CREAT | O_TRUNC | O_APPEND, S_IRWXU | S_IRWXG | S_IRWXO);

        fd = open(local_path,   O_APPEND | O_RDWR);

        //printf("TRUNCATE ERROR: %d\n", ftruncate(fd,0));
        if(fd == -1) {
            printf("Open Return: %d\n", fd);

            rc = ctx->Fetch(path, &buf, &size);
            if (rc<0) {
                return -ENOENT;
            }

            isFetched = 1;

            fd = creat(local_path, S_IRWXU);
            if(fd==-1) {
                printf("Create Error\n");
                return -errno;
            }
            fd = open(local_path,  O_APPEND | O_RDWR);
            if(fd==-1) printf("Reopen Error\n"); 
        } else {

            lstat(local_path, &cacheFileInfo);
            hello_getattr(path, &remoteFileInfo);

            if(remoteFileInfo.st_mtime > cacheFileInfo.st_mtime) {
                isStale = 1;
            }
  
            if(isStale) {
                rc = ftruncate(fd, 0);
                if(rc<0) {
                    return -errno;
                }
                rc = ctx->Fetch(path, &buf, &size);
                if (rc<0) {
                    return -ENOENT;
                }
                isFetched = 1;
            }
        }

        printf("File descr: %d Size:%d\n", fd, size);


        if(isFetched) {
            write(fd, buf, size);
            fsync(fd);
        }

        printf("File Contents: %s\n", buf);
//        fi->fh_old = 0;
        fi->fh = fd; 

	return 0;
}

static int hello_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
        int rc = 0;

        rc = pread(fi->fh, buf, size, offset);
        if(rc < 0) {
            return -errno;
        }

	return rc;
}

static int hello_write(const char *path, const char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi)
{
    int rc = 0;
    struct stat info;



    printf("File closed: %d\n", fcntl(fi->fh, F_GETFD));
    printf("File closed err: %d\n", errno);
    printf("Write File descriptor: %d\n", fi->fh);
//    rc = pwrite(fi->fh, buf, size, offset);
    rc= write(fi->fh, buf, size);
    fstat(fi->fh, &info);
    printf("Write return: %d\n", info.st_mtime);
    if(rc < 0) {
        printf("Write Error: %d\n", errno);
        int fd;
        char cacheFileName[80];
        char local_path[PATH_MAX];
        local_path[0] = '\0';

        snprintf(cacheFileName, 80, "%lu", hash((unsigned char *)path));
        strncat(local_path, fs_path, PATH_MAX);
        strncat(local_path, cacheFileName, PATH_MAX);

        fd = open(local_path,  O_APPEND | O_RDWR);

        printf("Newdile fd: %d\n", fd);
        lseek(fd,offset,SEEK_SET);
        for(int i=0; i<size; i++) {
            printf("%c", buf[i]);
        }
        rc = write(fd, buf, size);
        close(fd);
        if(rc<0) {
            printf("Return error: %d\n", rc);
            printf("Reqrite error %d\n", errno);
            return -errno;
        }
    }

//    fi->fh_old = 1;

    return rc;
}

static int hello_release(const char *path, struct fuse_file_info *fi)
{
    int rc = 0;
    int isModified=1;
    char *buf;
    struct stat info;
    struct stat remoteFileInfo;

    fsync(fi->fh);

    memset(&info, 0, sizeof(struct stat));
    fstat(fi->fh, &info);
//    hello_getattr(path, &remoteFileInfo);

//    if(remoteFileInfo.st_mtime > info.st_mtime) {
//        isModified = 0;
//    }

    if(isModified) {
        buf = (char *)malloc(info.st_size);

        lseek(fi->fh, 0, SEEK_SET);

        read(fi->fh, buf, info.st_size);

        printf("To be sent: %s\n", buf, info.st_size);

        ctx->Store(path, buf, info.st_size);

        free(buf);
    }
    rc = close(fi->fh);

/*
    char local_path[PATH_MAX];
    local_path[0] = '\0';
    char cacheFileName[80]; 
        snprintf(cacheFileName, 80, "%lu", hash((unsigned char *)path));

        strncat(local_path, fs_path, PATH_MAX);
        strncat(local_path, cacheFileName, PATH_MAX);
    lstat(local_path, &info);
    printf("After Close: %d\n", info.st_mtime); */
    return rc;
}

static int hello_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
        int fd;
        char cacheFileName[80];
        char local_path[PATH_MAX];
        local_path[0] = '\0';
    
        snprintf(cacheFileName, 80, "%lu", hash((unsigned char *)path));
        strncat(local_path, fs_path, PATH_MAX);
        strncat(local_path, cacheFileName, PATH_MAX);
        printf("path: %s\n", local_path);
        fflush(stdout);
        fd = open(local_path, O_CREAT | O_APPEND | O_RDWR, mode );
        printf("Done\n");
        if (fd == -1) {
                printf("Create Error\n");
                return -errno;
        }

        fi->fh = fd;

        ctx->Store(path, NULL, 0);

        printf("Create file descr: %d\n", fi->fh);
        return 0;
}

struct hello_fuse_operations:fuse_operations
{
    hello_fuse_operations ()
    {
        getattr    = hello_getattr;
        readdir    = hello_readdir;
        open       = hello_open;
        read       = hello_read;
        write      = hello_write;
        create     = hello_create;
        release    = hello_release;
    }
};

static struct hello_fuse_operations hello_oper;

int main(int argc, char *argv[])
{
        GreeterClient greeter(
      grpc::CreateChannel("king-01:12348", grpc::InsecureCredentials()));

        ctx = &greeter;

        strncpy(fs_path, realpath(argv[argc-2], NULL), PATH_MAX);
        strncat(fs_path, "/", PATH_MAX);
        argv[argc-2] = argv[argc-1];
        argv[argc-1] = NULL;
        argc--;

        printf("FS PATH: %s\n", fs_path);

	return fuse_main(argc, argv, &hello_oper, NULL);
        /*test*/
}
