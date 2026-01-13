/*
 * i8kctl.c -- Utility to query the i8k kernel module on Dell laptops to
 * retrieve information
 *
 * Copyright (C) 2013-2017 Vitor Augusto <vitorafsr@gmail.com>
 * Copyright (C) 2001  Massimo Dal Zotto <dz@debian.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

#include "i8k.h"
#include "i8kctl.h"

static int i8k_fd;
static int use_hwmon = 0;
static char hwmon_path[256] = {0};

/* Helper function to find dell-smm-hwmon device */
static int
find_dell_hwmon()
{
    DIR *dir;
    struct dirent *entry;
    char path[512];
    char name[64];
    FILE *fp;

    dir = opendir("/sys/class/hwmon");
    if (!dir) {
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') {
            continue;
        }

        snprintf(path, sizeof(path), "/sys/class/hwmon/%s/name", entry->d_name);
        fp = fopen(path, "r");
        if (fp) {
            if (fgets(name, sizeof(name), fp)) {
                /* Remove newline */
                name[strcspn(name, "\n")] = 0;
                if (strcmp(name, "dell_smm") == 0) {
                    snprintf(hwmon_path, sizeof(hwmon_path), "/sys/class/hwmon/%s", entry->d_name);
                    fclose(fp);
                    closedir(dir);
                    return 0;
                }
            }
            fclose(fp);
        }
    }

    closedir(dir);
    return -1;
}

/* Helper function to read integer from sysfs file */
static int
read_hwmon_int(const char *filename)
{
    char path[512];
    FILE *fp;
    int value;

    snprintf(path, sizeof(path), "%s/%s", hwmon_path, filename);
    fp = fopen(path, "r");
    if (!fp) {
        return -1;
    }

    if (fscanf(fp, "%d", &value) != 1) {
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return value;
}

/* Helper function to write integer to sysfs file */
static int
write_hwmon_int(const char *filename, int value)
{
    char path[512];
    FILE *fp;

    snprintf(path, sizeof(path), "%s/%s", hwmon_path, filename);
    fp = fopen(path, "w");
    if (!fp) {
        return -1;
    }

    if (fprintf(fp, "%d\n", value) < 0) {
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return 0;
}

/* Helper function to read string from sysfs file */
static char *
read_hwmon_string(const char *filename)
{
    char path[512];
    FILE *fp;
    static char buffer[256];

    snprintf(path, sizeof(path), "%s/%s", hwmon_path, filename);
    fp = fopen(path, "r");
    if (!fp) {
        return NULL;
    }

    if (fgets(buffer, sizeof(buffer), fp) == NULL) {
        fclose(fp);
        return NULL;
    }

    /* Remove newline */
    buffer[strcspn(buffer, "\n")] = 0;
    fclose(fp);
    return buffer;
}

char *
i8k_get_bios_version()
{
    char i8k_ioctl_bios_version[4];

    char i8k_proc_format[16];
    char i8k_proc_bios_version[4];
    char i8k_proc_serial_version[16];
    int i8k_proc_cpu_temp;
    int i8k_proc_left_fan;
    int i8k_proc_right_fan;
    int i8k_proc_left_speed;
    int i8k_proc_right_speed;
    int i8k_proc_ac_power;
    int i8k_proc_fn_key;

    char proc_i8k_str[64];
    int args[1];
    int rc_read;
    int ret_nargs;

    if (use_hwmon) {
        /* Try to read BIOS version from DMI */
        FILE *fp = fopen("/sys/class/dmi/id/bios_version", "r");
        if (fp) {
            static char bios_ver[64];
            if (fgets(bios_ver, sizeof(bios_ver), fp)) {
                bios_ver[strcspn(bios_ver, "\n")] = 0;
                fclose(fp);
                return strdup(bios_ver);
            }
            fclose(fp);
        }
        return strdup("?");
    }

    if ((rc_read=read(i8k_fd, proc_i8k_str, 64*sizeof(char))) != -1) {

        ret_nargs = sscanf(proc_i8k_str, "%s %s %s %d %d %d %d %d %d %d\n",
               i8k_proc_format,
               i8k_proc_bios_version,
               i8k_proc_serial_version,
               &i8k_proc_cpu_temp,
               &i8k_proc_left_fan,
               &i8k_proc_right_fan,
               &i8k_proc_left_speed,
               &i8k_proc_right_speed,
               &i8k_proc_ac_power,
               &i8k_proc_fn_key);

        if (ret_nargs == 10) {
            if (strncmp(i8k_proc_format, I8K_PROC_FMT, 16) == 0) {

                // Stop this function here and return.
                // Information from /proc/i8k is more complete.
                // So it is preferred here.
                // ioctl for BIOS version at the kernel module i8k returns
                // only the SMM version. The information of bios version in
                // '/proc/i8k' is from SMM data and DMI table.
                return strdup(i8k_proc_bios_version);
            }

        }
    }

    // Useless in some Dell systems for i8k kernel module dated 2013-05-30.
    if (ioctl(i8k_fd, I8K_BIOS_VERSION, &args) != 0) {
        i8k_ioctl_bios_version[0] = (args[0] >> 16) & 0xff;
        i8k_ioctl_bios_version[1] = (args[0] >>  8) & 0xff;
        i8k_ioctl_bios_version[2] = (args[0]      ) & 0xff;
        i8k_ioctl_bios_version[3] = '\0';
        return strdup(i8k_ioctl_bios_version);
    }

    return 0;
}

char *
i8k_get_machine_id()
{
    char args[16];
    int rc;

    if (use_hwmon) {
        /* Try to read machine ID from DMI */
        FILE *fp = fopen("/sys/class/dmi/id/product_name", "r");
        if (fp) {
            static char machine_id[64];
            if (fgets(machine_id, sizeof(machine_id), fp)) {
                machine_id[strcspn(machine_id, "\n")] = 0;
                fclose(fp);
                return strdup(machine_id);
            }
            fclose(fp);
        }
        return NULL;
    }

    if ((rc=ioctl(i8k_fd, I8K_MACHINE_ID, &args)) < 0) {
	return NULL;
    }

    return strdup(args);
}

int
i8k_set_fan(int fan, int speed)
{
    int args[2];
    int rc;

    if (use_hwmon) {
        /* Map fan state to pwm value and set pwm_enable to manual mode */
        const char *pwm_file = (fan == I8K_FAN_LEFT) ? "pwm1" : "pwm2";
        const char *pwm_enable_file = (fan == I8K_FAN_LEFT) ? "pwm1_enable" : "pwm2_enable";
        int pwm_value;

        /* Set to manual mode (1) */
        if (write_hwmon_int(pwm_enable_file, 1) < 0) {
            return -1;
        }

        /* Map speed to pwm value */
        switch (speed) {
            case I8K_FAN_OFF:
                pwm_value = 0;
                break;
            case I8K_FAN_LOW:
                pwm_value = 128;
                break;
            case I8K_FAN_HIGH:
                pwm_value = 255;
                break;
            default:
                return -1;
        }

        if (write_hwmon_int(pwm_file, pwm_value) < 0) {
            return -1;
        }

        return speed;
    }

    args[0] = fan;
    args[1] = speed;
    if ((rc=ioctl(i8k_fd, I8K_SET_FAN, &args)) < 0) {
	return rc;
    }

    return args[0];
}

int
i8k_get_fan_status(int fan)
{
    int args[1];
    int rc;

    if (use_hwmon) {
        /* Map pwm value (0-255) to fan state (0=off, 1=low, 2=high) */
        const char *pwm_file = (fan == I8K_FAN_LEFT) ? "pwm1" : "pwm2";
        int pwm = read_hwmon_int(pwm_file);
        if (pwm < 0) {
            return pwm;
        }
        if (pwm == 0) {
            return I8K_FAN_OFF;
        } else if (pwm < 128) {
            return I8K_FAN_LOW;
        } else {
            return I8K_FAN_HIGH;
        }
    }

    args[0] = fan;
    if ((rc=ioctl(i8k_fd, I8K_GET_FAN, &args)) < 0) {
	return rc;
    }

    return args[0];
}

int
i8k_get_fan_speed(int fan)
{
    int args[1];
    int rc;

    if (use_hwmon) {
        /* fan is 0 for right, 1 for left in i8k, but 1/2 in hwmon */
        const char *fan_file = (fan == I8K_FAN_LEFT) ? "fan1_input" : "fan2_input";
        return read_hwmon_int(fan_file);
    }

    args[0] = fan;
    if ((rc=ioctl(i8k_fd, I8K_GET_SPEED, &args)) < 0) {
	return rc;
    }

    return args[0];
}

int
i8k_get_cpu_temp()
{
    int args[1];
    int rc;

    if (use_hwmon) {
        /* hwmon reports temperature in millidegrees, convert to degrees */
        int temp = read_hwmon_int("temp1_input");
        if (temp < 0) {
            return temp;
        }
        return temp / 1000;
    }

    if ((rc=ioctl(i8k_fd, I8K_GET_TEMP, &args)) < 0) {
	return rc;
    }

    return args[0];
}

int
i8k_get_power_status()
{
    int args[1];
    int rc;

    if (use_hwmon) {
        /* Power status not available via hwmon, return -1 */
        return -1;
    }

    if ((rc=ioctl(i8k_fd, I8K_POWER_STATUS, &args)) < 0) {
	return rc;
    }

    return args[0];
}

int
i8k_get_fn_status()
{
    int args[1];
    int rc;

    if (use_hwmon) {
        /* Fn key status not available via hwmon, return -1 */
        return -1;
    }

    if ((rc=ioctl(i8k_fd, I8K_FN_STATUS, &args)) < 0) {
	return rc;
    }

    return args[0];
}

int
fan(int argc, char **argv)
{
    int left, right;

    if ((argc > 1) && isdigit(argv[1][0])) {
	left = i8k_set_fan(I8K_FAN_LEFT, atoi(argv[1]));
    } else {
	left = i8k_get_fan_status(I8K_FAN_LEFT);
    }

    if ((argc > 2) && isdigit(argv[2][0])) {
	right = i8k_set_fan(I8K_FAN_RIGHT, atoi(argv[2]));
    } else {
	right = i8k_get_fan_status(I8K_FAN_RIGHT);
    }

    printf("%d %d\n", left, right);
    return 0;
}

int
fan_speed(int argc, char **argv)
{
    int left, right;

    left = i8k_get_fan_speed(I8K_FAN_LEFT);
    right = i8k_get_fan_speed(I8K_FAN_RIGHT);

    printf("%d %d\n", left, right);
    return 0;
}

int
bios_version()
{
    char *version;

    if ((version=i8k_get_bios_version()) == NULL) {
	version = "?";
    }

    printf("%s\n", version);
    return 0;
}

int
machine_id()
{
    char *machine_id;

    if ((machine_id=i8k_get_machine_id()) == NULL) {
	machine_id = "?";
    }

    printf("%s\n", machine_id);
    return 0;
}

int
cpu_temperature()
{
    printf("%d\n", i8k_get_cpu_temp());
    return 0;
}

int
ac_status()
{
    printf("%d\n", i8k_get_power_status());
    return 0;
}

int
fn_key()
{
    printf("%d\n", i8k_get_fn_status());
    return 0;
}

int
status()
{
    int fn_key, cpu_temp, ac_power;
    int left_fan, right_fan, left_speed, right_speed;
    char *bios_version, *bios_machine_id;

    bios_version    = i8k_get_bios_version();
    bios_machine_id = i8k_get_machine_id();
    cpu_temp        = i8k_get_cpu_temp();
    left_fan        = i8k_get_fan_status(I8K_FAN_LEFT);
    right_fan       = i8k_get_fan_status(I8K_FAN_RIGHT);
    left_speed      = i8k_get_fan_speed(I8K_FAN_LEFT);
    right_speed     = i8k_get_fan_speed(I8K_FAN_RIGHT);
    ac_power        = i8k_get_power_status();
    fn_key          = i8k_get_fn_status();

    /*
     * Info:
     *
     * 1)  Proc format version (this will change if format changes)
     * 2)  BIOS version
     * 3)  BIOS machine ID
     * 4)  Cpu temperature
     * 5)  Left fan status
     * 6)  Right fan status
     * 7)  Left fan speed
     * 8)  Right fan speed
     * 9)  AC power
     * 10) Fn Key status
     */
    printf("%s %s %s %d %d %d %d %d %d %d\n",
	   I8K_PROC_FMT,
	   bios_version,
	   bios_machine_id,
	   cpu_temp,
	   left_fan,
	   right_fan,
	   left_speed,
	   right_speed,
	   ac_power,
	   fn_key);

    return 0;
}

void
usage()
{
    printf("Usage: i8kctl [fan [<l> <r>] | speed | version | bios | id | temp" \
           "| ac | fn]\n");
    printf("       i8kctl [-h]\n");
}

#ifdef LIB
void init()
{
    i8k_fd = open(I8K_PROC, O_RDONLY);
    if (i8k_fd < 0)
    {
        /* Try to use hwmon interface as fallback */
        if (find_dell_hwmon() == 0) {
            use_hwmon = 1;
            i8k_fd = 0; /* Dummy value */
        } else {
            perror("can't open " I8K_PROC " or find dell_smm hwmon device");
            exit(-1);
        }
    }
}
void finish()
{
    if (!use_hwmon) {
        close(i8k_fd);
    }
}
#else
int
main(int argc, char **argv)
{
    if (argc >= 2) {
	if ((strcmp(argv[1],"-h")==0) || (strcmp(argv[1],"--help")==0)) {
	    usage();
	    exit(0);
	}
    }

    i8k_fd = open(I8K_PROC, O_RDONLY);
    if (i8k_fd < 0) {
        /* Try to use hwmon interface as fallback */
        if (find_dell_hwmon() == 0) {
            use_hwmon = 1;
            i8k_fd = 0; /* Dummy value */
        } else {
            fprintf(stderr, "can't open " I8K_PROC " or find dell_smm hwmon device\n");
            exit(-1);
        }
    }

    /* -2 as a magic number: if var 'ret' reachs the end of main() as -2, than
     * no command was executed, and the user input was an invalid command
     */
    int ret = -2;

    /* No args, print status: same output as 'cat /proc/i8k' */
    if (argc < 2) {
        ret = status();
        close(i8k_fd);
        return ret;
    }

    if (strcmp(argv[1],"fan")==0) {
        argc--; argv++;
        ret = fan(argc,argv);
    }
    else if (strcmp(argv[1],"version")==0) {
        printf("%s\n", I8K_PROC_FMT);
        ret = 0;
    }
    else if (strcmp(argv[1],"speed")==0) {
        ret = fan_speed(argc,argv);
    }
    else if (strcmp(argv[1],"bios")==0) {
        ret = bios_version();
    }
    else if (strcmp(argv[1],"id")==0) {
        ret = machine_id();
    }
    else if (strcmp(argv[1],"temp")==0) {
        ret = cpu_temperature();
    }
    else if (strcmp(argv[1],"ac")==0) {
        ret = ac_status();
    }
    else if (strcmp(argv[1],"fn")==0) {
        ret = fn_key();
    }

    if (!use_hwmon) {
        close(i8k_fd);
    }

    if (ret == -2) // no command executed
        fprintf(stderr,"invalid arg: %s\n", argv[1]);

    return 0;
}
#endif
