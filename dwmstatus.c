/*
 * Copy me if you can.
 * by 20h
 */

#define _BSD_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <X11/Xlib.h>

char *tzlondon = "Europe/London";

static Display *dpy;

char *
smprintf(char *fmt, ...)
{
	va_list fmtargs;
	char *ret;
	int len;

	va_start(fmtargs, fmt);
	len = vsnprintf(NULL, 0, fmt, fmtargs);
	va_end(fmtargs);

	ret = malloc(++len);
	if (ret == NULL) {
		perror("malloc");
		exit(1);
	}

	va_start(fmtargs, fmt);
	vsnprintf(ret, len, fmt, fmtargs);
	va_end(fmtargs);

	return ret;
}

void
settz(char *tzname)
{
	setenv("TZ", tzname, 1);
}

char *
mktimes(char *fmt, char *tzname)
{
	time_t tim;

	settz(tzname);
	tim = time(NULL);
	struct tm tm = *localtime(&tim);

  return smprintf("%02d:%02d:%02d | %02d/%02d/%d", tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_mday, tm.tm_mon+1 , tm.tm_year + 1900);

}

void
setstatus(char *str)
{
	XStoreName(dpy, DefaultRootWindow(dpy), str);
	XSync(dpy, False);
}

char *
loadavg(void)
{
	double avgs[1];

	if (getloadavg(avgs, 1) < 0)
		return smprintf("");

	return smprintf("%02.0f", avgs[0]*100);
}

char *
readfile(char *base, char *file)
{
	char *path, line[513];
	FILE *fd;

	memset(line, 0, sizeof(line));

	path = smprintf("%s/%s", base, file);
	fd = fopen(path, "r");
	free(path);
	if (fd == NULL)
		return NULL;

	if (fgets(line, sizeof(line)-1, fd) == NULL)
		return NULL;
	fclose(fd);

	return smprintf("%s", line);
}

char *
getbattery(char *base)
{
	char *co, status;
	int descap, remcap;

	descap = -1;
	remcap = -1;

	co = readfile(base, "present");
	if (co == NULL)
		return smprintf("");
	if (co[0] != '1') {
		free(co);
		return smprintf("not present");
	}
	free(co);

	co = readfile(base, "charge_full_design");
	if (co == NULL) {
		co = readfile(base, "energy_full_design");
		if (co == NULL)
			return smprintf("");
	}
	sscanf(co, "%d", &descap);
	free(co);

	co = readfile(base, "charge_now");
	if (co == NULL) {
		co = readfile(base, "energy_now");
		if (co == NULL)
			return smprintf("");
	}
	sscanf(co, "%d", &remcap);
	free(co);

	co = readfile(base, "status");
	if (!strncmp(co, "Discharging", 11)) {
		status = '-';
	} else if(!strncmp(co, "Charging", 8)) {
		status = '+';
	} else {
		status = '?';
	}

	if (remcap < 0 || descap < 0)
		return smprintf("invalid");

	return smprintf("%.0f%%%c", ((float)remcap / (float)descap) * 100, status);
}

char *
gettemperature(char *base, char *sensor)
{
	char *co;

	co = readfile(base, sensor);
	if (co == NULL)
		return smprintf("");
	return smprintf("%02.0f°C", atof(co) / 1000);
}

char *
runsyscmd(char *cmd)
{
    FILE *fp;
    char path[1096];
    char *output;
    /* Open the command for reading. */
    fp = popen(cmd, "r");
    if (fp == NULL) {
        printf("Failed to run command\n" );
        return smprintf("");
    }

    /* Read the output  */
    output = smprintf("%s", fgets(path, sizeof(path), fp));

    /* close */
    pclose(fp);

    return output;
}

char *
getvolume()
{
    char *volume;

    volume = runsyscmd("/usr/bin/pamixer --get-volume");

    return volume;
}

char *
getnetwork()
{
    char *nettraffic;
    printf("\u2B07");
    nettraffic = runsyscmd("ifstat2 -i wlp3s0 0.5s 1 | awk 'NR==3 {print \"Down: \" $1 \" KB/s | Up: \" $2 \" KB/s\"}'");

    return nettraffic;
}

int
main(void)
{
    char *status;
    char *avgs;
    char *t0, *gputemp;
    char *tmldn;
    char *volume;
    char *nettraffic;

    if (!(dpy = XOpenDisplay(NULL))) {
        fprintf(stderr, "dwmstatus: cannot open display.\n");
        return 1;
    }

    for (;;sleep(1)) {
        avgs = loadavg();
        tmldn = mktimes("%H:%M:%S", tzlondon);
        t0 = gettemperature("/sys/class/hwmon/hwmon0", "temp1_input");
        gputemp = gettemperature("/sys/class/hwmon/hwmon2", "temp1_input");
        volume = getvolume();
        nettraffic = getnetwork();

        status = smprintf("CPU: %s | GPU: %s | %s | CPU Use: %s% | Vol: %s% | %s",
            t0, gputemp, nettraffic, avgs, volume, tmldn);
        setstatus(status);

        free(t0);
        free(gputemp);
        free(nettraffic);
        free(tmldn);
        free(avgs);
        free(status);
        free(volume);
    }

    XCloseDisplay(dpy);

    return 0;
}

