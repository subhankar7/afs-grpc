/*
 *
 * Copyright 2015, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <iostream>
#include <memory>
#include <string>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <grpc++/grpc++.h>

#include "helloworld.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerWriter;
using grpc::Status;
using helloworld::HelloRequest;
using helloworld::FetchRequest;
using helloworld::FetchReply;
using helloworld::StoreRequest;
using helloworld::StoreReply;
using helloworld::StatRequest;
using helloworld::StatReply;
using helloworld::ListDirRequest;
using helloworld::ListDirReply;
using helloworld::HelloReply;
using helloworld::Greeter;


char afs_path[PATH_MAX];

// Logic and data behind the server's behavior.
class GreeterServiceImpl final : public Greeter::Service {
  Status SayHello(ServerContext* context, const HelloRequest* request,
                  HelloReply* reply) override {
    std::string prefix("Hello ");
    reply->set_message(prefix + request->name());
    return Status::OK;
  }

  Status Fetch(ServerContext* context, const FetchRequest* request,
               FetchReply* reply) override {

    int fd;

    char path[PATH_MAX];
    path[0] = '\0';
    struct stat info;

    strncat(path, afs_path, PATH_MAX);
    strncat(path, (request->path()).c_str(), PATH_MAX);

    printf("AFS PATH: %s\n", path);

    fd = open(path, O_RDONLY);

    if(fd == -1) {
        reply->set_error(-1);
        return Status::OK;
    }

    fstat(fd, &info);

    char *buf = (char *)malloc(info.st_size);

    lseek(fd, 0, SEEK_SET);
    read(fd, buf, info.st_size);
    close(fd);

    printf("Read string: %s\n", buf);

    reply->set_error(0);
    reply->set_buf(std::string(buf,info.st_size));
    reply->set_size(info.st_size);
    return Status::OK;
    
  }

  Status Store(ServerContext* context, const StoreRequest* request,
               StoreReply* reply) override {

      int fd;

      char path[PATH_MAX];
      path[0] = '\0';

      strncat(path, afs_path, PATH_MAX);
      strncat(path, (request->path()).c_str(), PATH_MAX);

      printf("AFS PATH: %s\n", path);

      fd = open(path, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU);

      if(fd == -1) {
          reply->set_error(-1);
          return Status::OK;
      }


      printf("Received String: %s\n", (request->buf()).c_str());
      printf("Size: %d\n", request->size());
      write(fd, (request->buf()).data(), request->size());
      close(fd);

      reply->set_error(0);
      return Status::OK;
  }

  Status GetFileStat(ServerContext* context, const StatRequest* request,
                     StatReply* reply) override {

      int rc = 0;
      struct stat *stbuf;
      char path[PATH_MAX];
      path[0] = '\0';

      strncat(path, afs_path, PATH_MAX);
      strncat(path, (request->path()).c_str(), PATH_MAX);

      stbuf = (struct stat *)malloc(sizeof(struct stat));

      rc = lstat(path, stbuf);

      if(rc !=0 ) {
          reply->set_error(-1);
          free(stbuf);
          return Status::OK;
      }

      printf("Last Mod: %d\n", stbuf->st_mtime);
      reply->set_error(0);
      reply->set_buf(std::string((char *)stbuf,sizeof(struct stat)));
      return Status::OK;
  }

  Status ListDir(ServerContext* context,
                 const ListDirRequest *request,
                 ServerWriter<ListDirReply>* writer) override {

      char path[PATH_MAX];
      path[0] = '\0';

      strncat(path, afs_path, PATH_MAX);
      strncat(path, (request->path()).c_str(), PATH_MAX);

      DIR *dp;
      struct dirent *de;
      ListDirReply reply;

      dp = opendir(path);
      if (dp == NULL) {
          reply.set_error(-1);
          writer->Write(reply);
          return Status::OK;
      }

      de = readdir(dp);
      if (de == 0) {
          reply.set_error(-1);
          writer->Write(reply);
          return Status::OK;
      }


      do {
          reply.set_error(0);
          reply.set_name(std::string(de->d_name));
          writer->Write(reply);
      } while ((de = readdir(dp)) != NULL);

      return Status::OK;
  }
};

void RunServer() {
  std::string server_address("0.0.0.0:12348");
  GreeterServiceImpl service;

  ServerBuilder builder;
  // Listen on the given address without any authentication mechanism.
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  // Register "service" as the instance through which we'll communicate with
  // clients. In this case it corresponds to an *synchronous* service.
  builder.RegisterService(&service);
  // Finally assemble the server.
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;

  // Wait for the server to shutdown. Note that some other thread must be
  // responsible for shutting down the server for this call to ever return.
  server->Wait();
}

int main(int argc, char** argv) {

  if(argc!=2) {
      std::cout << "Usage: server <afs_path>" << std::endl;
      return -1;
  }

  strncpy(afs_path, argv[1], PATH_MAX);

  std::cout << afs_path << std::endl;

  RunServer();

  return 0;
}
