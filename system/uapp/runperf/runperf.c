// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>

#include <launchpad/launchpad.h>
#include <zircon/syscalls/object.h>
#include <zircon/syscalls.h>

void run_binary(char* name, char* path, char* out_dir);
void process_package(char* pkg_name, char* pkg_path, char* out_dir);

int main(int argc, char** argv) {
  char pkg_dir[64];
  char out_dir[64];

  int i = 1;
  while (i < argc) {
    if (strcmp(argv[i], "-p") == 0) {
      strcpy(pkg_dir, argv[i+1]);
    } else if (strcmp(argv[i], "-o") == 0) {
      strcpy(out_dir, argv[i+1]);
      // Trim trailing slash
      if (out_dir[strlen(out_dir) - 1] == '/') {
          out_dir[strlen(out_dir) - 1] = 0;
      }
    }
    i++;
  }

  DIR *dir;
  struct dirent *ent;
  if ((dir = opendir(pkg_dir)) != NULL) {
    while ((ent = readdir(dir)) != NULL) {
       // Ignore . and ..
       if (strcmp(ent->d_name, ".") != 0 &&
           strcmp(ent->d_name, "..") != 0) {
         char* pkg_name = ent->d_name;

         // Set the package path.
         char pkg_path[256];
         memset(pkg_path, 0, 256);
         sprintf(pkg_path, "%s/%s", pkg_dir, pkg_name);

         // Set the expected path to the runbenchmarks.cfg file.
         char cfg_path[256];
         memset(cfg_path, 0, 256);
         sprintf(cfg_path, "%s/0/data/runbenchmarks.cfg", pkg_path);

         // Check whether the config file exists in the package.
         FILE *cfg_file;
         cfg_file = fopen(cfg_path, "r");
         if (cfg_file == NULL) {
           // Continue on to next entry.
           continue;
         }

         printf("Found config %s. Loading...\n", cfg_path);
         //
         // ... Parse the configuration file.
         // ... Do something with that info.
         //
         process_package(pkg_name, pkg_path, out_dir);
       }
    }
  } else {
    perror("opendir");
    return 1;
  }

  return 0;
}

void process_package(char* pkg_name, char* pkg_path, char* out_dir) {
  printf("Processing pacakge: %s\n", pkg_path);

  // The path to the directory containing benchmark executables
  char bin_dir[256];
  memset(bin_dir, 0, 256);
  sprintf(bin_dir, "%s/0/test/benchmarks/", pkg_path);

  // Iterate over all of the files in the benchmarks directory, executing each
  // one.
  DIR *dir;
  struct dirent *ent;
  if ((dir = opendir(bin_dir)) != NULL) {
    while ((ent = readdir(dir)) != NULL) {
       if (strcmp(ent->d_name, ".") != 0 &&
           strcmp(ent->d_name, "..") != 0) {
         char bin_path[256];
         memset(bin_path, 0, 64);
         sprintf(bin_path, "%s/%s", bin_dir, ent->d_name);
         run_binary(ent->d_name, bin_path, out_dir);
       }
    }
  } else {
    perror("opendir");
  }

  return;
}

// TODO(kjharland): Use name from runbenchmarks.cfg as the test name.
// TODO(kjharland): Redirect std(out|err) to out_dir/name.std(out|err)
// TODO(kjharland): Generate metadata file from runbenchmarks.cfg
void run_binary(char* name, char* path, char* out_dir) {
  launchpad_t* lp;
  zx_status_t status;
  status = launchpad_create(0, path, &lp);

//  char output_opt[128];
//  sprintf(output_opt, "--fbenchmark_out=%s/%s.results.json", out_dir, name);

  // Build the command line
  const char* argv[] = {path};//, output_opt};
  int argc = 1;//2;

  if (status != ZX_OK) {
      printf("FAILURE: launchpad_create() returned %d\n", status);
      return;
  }

   status = launchpad_load_from_file(lp, argv[0]);
   if (status != ZX_OK) {
     printf("FAILURE: launchpad_load_from_file() returned %d\n", status);
     return;
   }

   status = launchpad_clone(lp, LP_CLONE_ALL);
   if (status != ZX_OK) {
     printf("FAILURE: launchpad_clone() returned %d\n", status);
     return;
   }

   launchpad_set_args(lp, argc, argv);
   const char* errmsg;
   zx_handle_t handle;
   status = launchpad_go(lp, &handle, &errmsg);
   if (status != ZX_OK) {
     printf("FAILURE: Failed to launch %s: %d: %s\n", path, status, errmsg);
     return;
   }

   status = zx_object_wait_one(handle, ZX_PROCESS_TERMINATED,
                               ZX_TIME_INFINITE, NULL);

   zx_info_process_t proc_info;
   status = zx_object_get_info(handle, ZX_INFO_PROCESS, &proc_info,
        sizeof(proc_info), NULL, NULL);
   zx_handle_close(handle);

   printf("Finished %s!\n", path);
   return;
}
