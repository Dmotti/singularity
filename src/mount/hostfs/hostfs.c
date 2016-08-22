/* 
 * Copyright (c) 2015-2016, Gregory M. Kurtzer. All rights reserved.
 * 
 * “Singularity” Copyright (c) 2016, The Regents of the University of California,
 * through Lawrence Berkeley National Laboratory (subject to receipt of any
 * required approvals from the U.S. Dept. of Energy).  All rights reserved.
 * 
 * This software is licensed under a customized 3-clause BSD license.  Please
 * consult LICENSE file distributed with the sources of this project regarding
 * your rights to use or distribute this software.
 * 
 * NOTICE.  This Software was developed under funding from the U.S. Department of
 * Energy and the U.S. Government consequently retains certain rights. As such,
 * the U.S. Government has been granted for itself and others acting on its
 * behalf a paid-up, nonexclusive, irrevocable, worldwide license in the Software
 * to reproduce, distribute copies to the public, prepare derivative works, and
 * perform publicly and display publicly, and to permit other to do so. 
 * 
*/

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <pwd.h>

#include "file.h"
#include "util.h"
#include "message.h"
#include "privilege.h"
#include "config_parser.h"
#include "sessiondir.h"
#include "rootfs/rootfs.h"


#define MAX_LINE_LEN 4096



int singularity_mount_hostfs(void) {
    FILE *mounts;
    char *line = NULL;
    char *container_dir = singularity_rootfs_dir();

    config_rewind();
    if ( config_get_key_bool("mount hostfs", 0) <= 0 ) {
        message(DEBUG, "Not mounting host file systems per configuration\n");
        return(0);
    }

    message(DEBUG, "Checking to see if /proc/mounts exists\n");
    if ( is_file("/proc/mounts") < 0 ) {
        message(WARNING, "Can not probe for currently mounted host file systems\n");
        return(1);
    }

    message(DEBUG, "Opening /proc/mounts\n");
    if ( ( mounts = fopen("/proc/mounts", "r") ) == NULL ) {
        message(ERROR, "Could not open /proc/mounts for reading: %s\n", strerror(errno));
        return(1);
    }

    line = (char *)malloc(MAX_LINE_LEN);

    message(DEBUG, "Getting line by line\n");
    while ( fgets(line, MAX_LINE_LEN, mounts) ) {
        char *source;
        char *mountpoint;
        char *filesystem;

        if ( line == NULL ) {
            message(DEBUG, "Skipping empty line in /proc/mounts\n");
            continue;
        }

        chomp(line);

        if ( line[0] == '#' || strlen(line) <= 1 ) {
            message(VERBOSE3, "Skipping blank or comment line in /proc/mounts\n");
            continue;
        }
        if ( ( source = strtok(strdup(line), " ") ) == NULL ) {
            message(VERBOSE3, "Could not obtain mount source from /proc/mounts: %s\n", line);
            continue;
        }
        if ( ( mountpoint = strtok(NULL, " ") ) == NULL ) {
            message(VERBOSE3, "Could not obtain mount point from /proc/mounts: %s\n", line);
            continue;
        }
        if ( ( filesystem = strtok(NULL, " ") ) == NULL ) {
            message(VERBOSE3, "Could not obtain file system from /proc/mounts: %s\n", line);
            continue;
        }

        if ( strcmp(mountpoint, "/") == 0 ) {
            message(DEBUG, "Skipping root (/): %s,%s,%s\n", source, mountpoint, filesystem);
            continue;
        }
        if ( strncmp(mountpoint, "/sys", 4) == 0 ) {
            message(DEBUG, "Skipping /sys based file system: %s,%s,%s\n", source, mountpoint, filesystem);
            continue;
        }
        if ( strncmp(mountpoint, "/proc", 5) == 0 ) {
            message(DEBUG, "Skipping /proc based file system: %s,%s,%s\n", source, mountpoint, filesystem);
            continue;
        }
        if ( strncmp(mountpoint, "/dev", 4) == 0 ) {
            message(DEBUG, "Skipping /dev based file system: %s,%s,%s\n", source, mountpoint, filesystem);
            continue;
        }
        if ( strncmp(mountpoint, "/run", 4) == 0 ) {
            message(DEBUG, "Skipping /run based file system: %s,%s,%s\n", source, mountpoint, filesystem);
            continue;
        }
        if ( strncmp(mountpoint, "/var", 4) == 0 ) {
            message(DEBUG, "Skipping /var based file system: %s,%s,%s\n", source, mountpoint, filesystem);
            continue;
        }
        if ( strncmp(mountpoint, container_dir, strlen(container_dir)) == 0 ) {
            message(DEBUG, "Skipping container_dir (%s) based file system: %s,%s,%s\n", container_dir, source, mountpoint, filesystem);
            continue;
        }
        if ( strcmp(filesystem, "tmpfs") == 0 ) {
            message(DEBUG, "Skipping tmpfs file system: %s,%s,%s\n", source, mountpoint, filesystem);
            continue;
        }
        if ( strcmp(filesystem, "cgroup") == 0 ) {
            message(DEBUG, "Skipping cgroup file system: %s,%s,%s\n", source, mountpoint, filesystem);
            continue;
        }


        if ( ( is_dir(mountpoint) == 0 ) && ( is_dir(joinpath(container_dir, mountpoint)) < 0 ) ) {
            if ( singularity_rootfs_overlay_enabled() > 0 ) {
                priv_escalate();
                if ( s_mkpath(joinpath(container_dir, mountpoint), 0755) < 0 ) {
                    priv_drop();
                    message(WARNING, "Could not create bind point directory in container %s: %s\n", mountpoint, strerror(errno));
                    continue;
                }
                priv_drop();
            } else {
                message(WARNING, "Non existant 'bind point' directory in container: '%s'\n", mountpoint);
                continue;
            }
        }


        priv_escalate();
        message(VERBOSE, "Binding '%s'(%s) to '%s/%s'\n", mountpoint, filesystem, container_dir, mountpoint);
        if ( mount(mountpoint, joinpath(container_dir, mountpoint), NULL, MS_BIND|MS_NOSUID|MS_REC, NULL) < 0 ) {
            message(ERROR, "There was an error binding the path %s: %s\n", mountpoint, strerror(errno));
            ABORT(255);
        }
        priv_drop();

    }

    free(line);
    fclose(mounts);
    return(0);
}
