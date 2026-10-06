#include "drives.h"
#include <mntent.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/statvfs.h>
int drives_read(Drives *drives) {
    FILE *mounts = setmntent("/proc/mounts", "r");
    if (!mounts) return -1;
    Drives next = {0}; struct mntent *mount;
    while ((mount = getmntent(mounts)) && next.count < DRIVE_MAX) {
        /* Only local storage: probing network filesystems can stall the UI. */
        bool windows = (!strcmp(mount->mnt_type, "9p") || !strcmp(mount->mnt_type, "drvfs")) && !strncmp(mount->mnt_dir, "/mnt/", 5);
        if (strcmp(mount->mnt_dir, "/") && !windows && strncmp(mount->mnt_fsname, "/dev/", 5)) continue;
        if (!strncmp(mount->mnt_dir, "/snap/", 6) || !strncmp(mount->mnt_fsname, "/dev/loop", 9)) continue;
        bool duplicate = false;
        for (size_t i = 0; i < next.count; ++i)
            if (!strcmp(next.items[i].path, mount->mnt_dir) || (!windows && !strcmp(next.items[i].source, mount->mnt_fsname))) duplicate = true;
        if (duplicate) continue;
        struct statvfs stats;
        if (statvfs(mount->mnt_dir, &stats) || !stats.f_blocks || !stats.f_frsize) continue;
        if ((uint64_t)stats.f_blocks > UINT64_MAX / stats.f_frsize) continue;
        Drive *drive = &next.items[next.count++];
        snprintf(drive->path, sizeof(drive->path), "%s", mount->mnt_dir);
        snprintf(drive->source, sizeof(drive->source), "%s", mount->mnt_fsname);
        drive->total = (uint64_t)stats.f_blocks * stats.f_frsize;
        drive->available = (uint64_t)(stats.f_bavail > stats.f_blocks ? stats.f_blocks : stats.f_bavail) * stats.f_frsize;
    }
    endmntent(mounts); *drives = next; return 0;
}
