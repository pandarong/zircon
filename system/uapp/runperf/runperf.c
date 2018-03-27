// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <launchpad/launchpad.h>
#include <zircon/syscalls/object.h>
#include <zircon/syscalls.h>

// The contents of a test configuration file.
typedef struct cfg {
  char id[256];
  char schema[256];
} cfg_t ;

bool read_cfg(FILE* cfg_file, cfg_t* pkg_cfg);

// The contents of a test metadata file.
typedef struct meta {
  char id[256];
  char schema[256];
  char out_path[256];
  char res_path[256];
  char* argv;
} meta_t;

void write_metadata(meta_t* pkg_meta, FILE* meta_file);

void run_binary(const char** argv,
                int argc,
                char* path,
                FILE* out_file);

void process_package(char* pkg_name,
                     char* pkg_path,
                     cfg_t* pkg_cfg);


// The path where test binary stdout and stderr are written.
const char out_pathtemp[] = "/system/data/testdata/%s.output";

int main(int argc, char** argv) {
  char pkg_dir[64];

  // Parse command line flags.
  int i = 1;
  while (i < argc) {
    if (strcmp(argv[i], "-p") == 0) {
      strcpy(pkg_dir, argv[i+1]);
    }
    i++;
  }

  // Iterate over all packages in the packages directory.
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
         cfg_t pkg_cfg;
         if (!read_cfg(cfg_file, &pkg_cfg)) {
           printf("FAILURE: could not parse config file\n");
           continue;
         }
         fclose(cfg_file);

         process_package(pkg_name, pkg_path, &pkg_cfg);
       }
    }
  } else {
    perror("opendir");
    return 1;
  }

  return 0;
}


bool read_cfg(FILE* cfg_file, cfg_t* pkg_cfg) {
  memset(pkg_cfg, 0, sizeof(cfg_t));

  ssize_t read;
  size_t len = 0;

  char *line = NULL;
  char *key = NULL;
  char *val = NULL;

  while ((read = getline(&line, &len, cfg_file)) != -1) {
    if ((key = strsep(&line, "=")) == NULL ||
        (val = strsep(&line, "=")) == NULL) {
      break;
    }

    if (strcmp(key, "id") == 0) {
      strncpy(pkg_cfg->id, val, strlen(val)-1);
    } else if (strcmp(key, "results_schema") == 0) {
      strncpy(pkg_cfg->schema, val, strlen(val)-1);
    } else {
      printf("WARNING: unknown cfg key: %s\n", key);
    }
  }

  if (strlen(pkg_cfg->id) == 0 ||
      strlen(pkg_cfg->schema) == 0) {
   return false;
 }

  printf("Config:\n");
  printf("--id     = %s\n", pkg_cfg->id);
  printf("--schema = %s\n", pkg_cfg->schema);

  if (line) {
    free(line);
  }
  if (key) {
    free(key);
  }
  if (val) {
    free(val);
  }
  return true;
}

void process_package(char* pkg_name,
                     char* pkg_path,
                     cfg_t* pkg_cfg) {
  printf("Processing package: %s\n", pkg_path);

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
         // The path to the binary
         char bin_path[256];
         memset(bin_path, 0, 64);
         sprintf(bin_path, "%s/%s", bin_dir, ent->d_name);

         // The file to redirect stdout and stderr to.
         char out_path[256];
         sprintf(out_path, "/tmp/%s.out", pkg_cfg->id);
         FILE* out_file = fopen(out_path, "w");
         if (out_file == NULL) {
           printf("FAILURE: could not open output file %s\n", out_path);
           continue;
         }

         //
         // Build the command line for the binary.
         //

         // The file to write results to.
         char res_path[256];
         sprintf(res_path, "/tmp/%s.results", pkg_cfg->id);

         const char* argv[] = {bin_path, res_path};
         int argc = 2;

         run_binary(argv, argc, bin_path, out_file);

         // The file to write metadata to.
         char meta_path[256];
         sprintf(meta_path, "/tmp/%s.meta", pkg_cfg->id);
         FILE* meta_file = fopen(meta_path, "w");
         if (meta_file == NULL) {
           printf("FAILURE: could not open metadata file %s\n", meta_path);
           continue;
         }

         // Write metadata file
         meta_t pkg_meta;
         memset(&pkg_meta, 0, sizeof(meta_t));
         strcpy(pkg_meta.id, pkg_cfg->id);
         strcpy(pkg_meta.schema, pkg_cfg->schema);
         strcpy(pkg_meta.out_path, out_path);
         // TODO: copy argv
         strcpy(pkg_meta.res_path, res_path);
         write_metadata(&pkg_meta, meta_file);

         fclose(out_file);
         fclose(meta_file);
       }
    }
  } else {
    perror("opendir");
  }

  return;
}

void write_metadata(meta_t* pkg_meta, FILE* f) {
  fprintf(f, "id=%s\n", pkg_meta->id);
  fprintf(f, "results_schema=%s\n", pkg_meta->schema);
  fprintf(f, "out_path=%s\n", pkg_meta->out_path);
  fprintf(f, "res_path=%s\n", pkg_meta->res_path);
  fprintf(f, "argv=<todo>\n");
}

// DONE(kjharland): Add a tracing example.
// TODO(kjharland): Add better doc comments to this file.
// DONE(kjharland): Use name from runbenchmarks.cfg as the test name.
// DONE(kjharland): Redirect std(out|err) to res_dir/name.std(out|err)
// DONE(kjharland): Generate metadata file from runbenchmarks.cfg
void run_binary(const char** argv,
                int argc,
                char* path,
                FILE* out_file) {
  launchpad_t* lp;
  zx_status_t status;
  status = launchpad_create(0, path, &lp);

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

   // Capture stdout and stderr for later.
   int fds[2];
   if (pipe(fds)) {
      printf("FAILURE: Failed to create pipe: %s\n", strerror(errno));
      return;
   }
   status = launchpad_clone_fd(lp, fds[1], STDOUT_FILENO);
   if (status != ZX_OK) {
     printf("FAILURE: launchpad_clone_fd() returned %d\n", status);
     return;
   }
   status = launchpad_transfer_fd(lp, fds[1], STDERR_FILENO);
   if (status != ZX_OK) {
     printf("FAILURE: launchpad_transfer_fd() returned %d\n", status);
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

   // Write stdout and stderr to file.
   char buf[1024];
   ssize_t bytes_read = 0;
   while ((bytes_read = read(fds[0], buf, 1024)) > 0) {
     fwrite(buf, 1, bytes_read, out_file);
     fwrite(buf, 1, bytes_read, stdout);
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
