/** ********************************************************************
 * DWM STATUS (originally) by <clement@6pi.fr>
 * freeBSD modifications by <jake@gnu.space>
 *
 * Compile with:
 * gcc -Wall -pedantic -std=c99 -lX11 -lasound dwmstatus.c
 *
 **/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <wchar.h>
#include <unistd.h>
#include <time.h>
#include <X11/Xlib.h>

#include <alsa/asoundlib.h>
#include <alsa/mixer.h>

#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/resource.h>
#include <vm/vm_param.h>
#include <sys/vmmeter.h>

#define CPU_NBR 4
#define BAR_HEIGHT 15

int   getMemPercent();
int getDiskPercent();
size_t get_num_cpus();
void get_cp_times(long* pcpu, size_t count);
double get_cpu_usage(long* current, long* previous);
char* getDateTime();
void getTemperature(char* formatted);
int   getVolume();
void  setStatus(Display *dpy, char *str);

char* vBar(int percent, int w, int h, char* fg_color, char* bg_color);
char* hBar(int percent, int w, int h, char* fg_color, char* bg_color);
int h2Bar(char *string, size_t size, int percent, int w, int h, char *fg_color, char *bg_color);
int hBarBordered(char *string, size_t size, int percent, int w, int h, char *fg_color, char *bg_color, char *border_color);
int getBatteryBar(char *string, size_t size, int w, int h);
void percentColorGeneric(char* string, int percent, int invert);


/* *******************************************************************
 * MAIN
 ******************************************************************* */

// TODO: Add signal handler to cleanup allocs on SIGINT
int
main(void)
{
  const int MSIZE = 1024;
  Display *dpy;
  char *status;

  char *datetime;
  int vol;
  char temp[20];
  char *cpu_bar[CPU_NBR];

  const size_t hw_ncpu = get_num_cpus();
  const size_t pcpu_count = hw_ncpu * CPUSTATES;
  long* pcpu_current = calloc(sizeof(long), pcpu_count);
  long* pcpu_previous = calloc(sizeof(long), pcpu_count);
  get_cp_times(pcpu_current, pcpu_count);
  float usage;

  int mem_percent;
  char *mem_bar;

  int disk_percent;
  char *disk_bar;

  char *fg_color = "#EEEEEE";
  char cpu_color[8];

  if (!(dpy = XOpenDisplay(NULL))) {
    fprintf(stderr, "Cannot open display.\n");
    return EXIT_FAILURE;
  }

  status = (char*) malloc(sizeof(char)*MSIZE);
  if(!status)
    return EXIT_FAILURE;

   while(1)
    {

      mem_percent = getMemPercent();
      mem_bar = hBar(mem_percent, 20, 9,  "#006CAD", "#444444");
      disk_percent = getDiskPercent();
      disk_bar = hBar(disk_percent, 20, 9, "#006CAD", "#444444");
      getTemperature(temp);
      datetime = getDateTime();
      vol = getVolume();
      get_cp_times(pcpu_current, pcpu_count);

      for(int i = 0; i < hw_ncpu; ++i)
      {
        usage = get_cpu_usage(&pcpu_current[i * CPUSTATES], &pcpu_previous[i * CPUSTATES]);
        percentColorGeneric(cpu_color, (int)usage, 1);
	      cpu_bar[i] = vBar((int)usage, 2, 13, cpu_color, "#444444");
      }

      int ret = snprintf(
               status,
               MSIZE,
               "^c%s^ [VOL %d%%] [CPU ^f1^%s^f4^%s^f4^%s^f4^%s^f3^^c%s^] [MEM ^f1^%s^f20^^c%s^ %d%%] [DISK ^f1^%s^f20^^c%s^ %d%%] [TEMP %sC ^c%s^] %s ",
               fg_color,
               vol,
               cpu_bar[0],
               cpu_bar[1],
               cpu_bar[2],
               cpu_bar[3],
               fg_color,
               mem_bar,
               fg_color,
               mem_percent,
               disk_bar,
               fg_color,
               disk_percent,
               temp,
               fg_color, datetime
               );
      if(ret >= MSIZE)
        fprintf(stderr, "error: buffer too small %d/%d\n", MSIZE, ret);

      free(datetime);
      for(int i = 0; i < CPU_NBR; ++i)
	      free(cpu_bar[i]);

      free(mem_bar);

      setStatus(dpy, status);
      sleep(1);
    }

   free(pcpu_current);
   free(pcpu_previous);
}

/* *******************************************************************
 * FUNCTIONS
 ******************************************************************* */

char* vBar(int percent, int w, int h, char* fg_color, char* bg_color)
{
  char *value;
  if((value = (char*) malloc(sizeof(char)*128)) == NULL)
    {
      fprintf(stdout, "Cannot allocate memory for buf.\n");
      exit(1);
    }
  char* format = "^c%s^^r0,%d,%d,%d^^c%s^^r0,%d,%d,%d^";

  int bar_height = (percent*h)/100;
  int y = (BAR_HEIGHT - h)/2;
  snprintf(value, 128, format, bg_color, y, w, h, fg_color, y + h-bar_height, w, bar_height);

  return value;
}

char* hBar(int percent, int w, int h, char* fg_color, char* bg_color)
{
  char *value;
  if((value = (char*) malloc(sizeof(char)*128)) == NULL)
    {
      fprintf(stderr, "Cannot allocate memory for buf.\n");
      exit(1);
    }
  char* format = "^c%s^^r0,%d,%d,%d^^c%s^^r0,%d,%d,%d^";

  int bar_width = (percent*w)/100;
  int y = (BAR_HEIGHT - h)/2;
  snprintf(value, 128, format, bg_color, y, w, h, fg_color, y, bar_width, h);
  return value;
}

int hBar2(char *string, size_t size, int percent, int w, int h, char *fg_color, char *bg_color)
{
  char *format = "^c%s^^r0,%d,%d,%d^^c%s^^r%d,%d,%d,%d^";
  int bar_width = (percent*w)/100;

  int y = (BAR_HEIGHT - h)/2;
  return snprintf(string, size, format, fg_color, y, bar_width, h, bg_color, bar_width, y, w - bar_width, h);
}

int hBarBordered(char *string, size_t size, int percent, int w, int h, char *fg_color, char *bg_color, char *border_color)
{
	char tmp[128];
	hBar2(tmp, 128, percent, w - 2, h -2, fg_color, bg_color);
	int y = (BAR_HEIGHT - h)/2;
	char *format = "^c%s^^r0,%d,%d,%d^^f1^%s";
	return snprintf(string, size, format, border_color, y, w, h, tmp);
}

void
setStatus(Display *dpy, char *str)
{
  XStoreName(dpy, DefaultRootWindow(dpy), str);
  XSync(dpy, False);
}

void percentColorGeneric(char* string, int percent, int invert)
{
	char *format = "#%X0%X000";
	int a = (percent*15)/100;
	int b = 15 - a;
  if(!invert) {
    snprintf(string, 8, format, b, a);
  }
  else {
    snprintf(string, 8, format, a, b);
  }
}

void percentColor(char* string, int percent)
{
  percentColorGeneric(string, percent, 0);
}

int
getMemPercent()
{
	int hw_physmem[] = { CTL_HW, HW_PHYSMEM };
	unsigned long physmem;

	int hw_pagesize[] = { CTL_HW, HW_PAGESIZE };
	unsigned long pagesize;

	int vm_vmtotal[] = { CTL_VM, VM_METER };
	struct vmtotal vmdata;

	size_t len;
	int result;

	len = sizeof(physmem);
	result = sysctl(hw_physmem, sizeof(hw_physmem) / sizeof(*hw_physmem), &physmem, &len, NULL, 0);
	if (result != 0) return 1;

	len = sizeof(pagesize);
	result = sysctl(hw_pagesize, sizeof(hw_pagesize) / sizeof(*hw_pagesize), &pagesize, &len, NULL, 0);
	if (result != 0) return 1;

	len = sizeof(vmdata);
  result = sysctl(vm_vmtotal, sizeof(vm_vmtotal) / sizeof(*vm_vmtotal), &vmdata, &len, NULL, 0);

  float avail = (float)(pagesize*vmdata.t_free);
  float free = (float)((physmem - avail)/physmem * 100);
  return free;
}

int
getDiskPercent()
{
  FILE *fp;
  char path[1035];

  fp = popen("df -hl | grep 'tank/ROOT/initial' | awk '{print $5}' | sed 's/%//g'", "r");
  if (fp == NULL)
  {
    printf("failed to get disk size\n");
    return 0;
  }

  int ret;
  while(fgets(path, sizeof(path)-1, fp) != NULL) {
    sscanf(path, "%d", &ret);
  }

  pclose(fp);
  return ret;
}

size_t
get_num_cpus()
{
  int hw_ncpu;
  size_t hw_ncpu_size = sizeof(hw_ncpu);
  if (sysctlbyname("hw.ncpu", &hw_ncpu, &hw_ncpu_size, NULL, 0) < 0)
  {
    // Could not fetch num of CPUs
    return 0;
  }

  return (size_t)(hw_ncpu);
}

void
get_cp_times(long* pcpu, size_t count)
{
  size_t pcpu_size = sizeof(*pcpu) * count;
  if (sysctlbyname("kern.cp_times", pcpu, &pcpu_size, NULL, 0) < 0)
    {
      return;
    }
}

double
get_cpu_usage(long* current, long* previous)
{
  long total = 0;
  for (size_t i = 0; i < CPUSTATES; i ++)
  {
    long prev = current[i];
    current[i] -= previous[i];
    previous[i] = prev;
    total += current[i];
  }

  return (100L - (100L * current[CPUSTATES -1] / (total ? total : 1L)));
}

char *
getDateTime()
{
  char *buf;
  time_t result;
  struct tm *resulttm;

  if((buf = (char*) malloc(sizeof(char)*65)) == NULL)
    {
      fprintf(stderr, "Cannot allocate memory for buf.\n");
      exit(1);
    }

  result = time(NULL);
  resulttm = localtime(&result);
  if(resulttm == NULL)
    {
      fprintf(stderr, "Error getting localtime.\n");
      exit(1);
    }

  if(!strftime(buf, sizeof(char)*65-1, "[%a %b %d] [%H:%M]", resulttm))
    {
      fprintf(stderr, "strftime is 0.\n");
      exit(1);
    }

  return buf;
}

void
getTemperature(char *formatted)
{
  int temp;
  size_t temp_size = sizeof(temp);
  if (sysctlbyname("hw.acpi.thermal.tz0.temperature", &temp, &temp_size, NULL, 0) < 0)
  {
    // Couldn't determine temp
    printf("couldn't access temperature\n");
    sprintf(formatted, "%d.%d", 0, 0);
  }
  sprintf(formatted, "%d.%d", (((temp)-2731) / 10), abs(((temp)-2731) % 10));
}

int
getVolume()
{
  const char* MIXER = "Master";

  float vol = 0;
  long pmin, pmax, pvol;

  snd_mixer_t *handle;
  snd_mixer_selem_id_t *sid;
  snd_mixer_elem_t *elem;
  snd_mixer_selem_id_alloca(&sid);

  if(snd_mixer_open(&handle, 0) < 0)
    return 0;

  if(snd_mixer_attach(handle, "default") < 0
     || snd_mixer_selem_register(handle, NULL, NULL) < 0
     || snd_mixer_load(handle) > 0)
    {
      snd_mixer_close(handle);
      return 0;
    }

  for(elem = snd_mixer_first_elem(handle); elem; elem = snd_mixer_elem_next(elem))
    {
      snd_mixer_selem_get_id(elem, sid);
      if(!strcmp(snd_mixer_selem_id_get_name(sid), MIXER))
        {
          snd_mixer_selem_get_playback_volume_range(elem, &pmin, &pmax);
          snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_MONO, &pvol);
          vol = ((float)pvol / (float)(pmax - pmin)) * 100;
        }
    }

  snd_mixer_close(handle);

  return vol;
}
