/*
 *    vloopback_motion.c
 *
 *    Video loopback functions for motion.
 *    Copyright 2000 by Jeroen Vreeken (pe1rxq@amsat.org)
 *    Copyright 2008 by Angel Carpintero (motiondevelop@gmail.com)
 *    This software is distributed under the GNU public license version 2
 *    See also the file 'COPYING'.
 *
 */
#include "vloopback_motion2.h"
#if (!defined(WITHOUT_V4L2)) && (!defined(BSD))
#include <dirent.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

/**
 * v4l2_open_vidpipe
 *
 */
static int v4l2_open_vidpipe(void)
{
    int pipe_fd = -1;
    char pipepath[255];
    char buffer[255];
    DIR *dir;
    struct dirent *dirp;
    const char prefix[] = "/sys/class/video4linux/";
    int fd,tfd;
    int len,min;

    if ((dir = opendir(prefix)) == NULL) {
      MOTION_LOG(CRT, TYPE_VIDEO, SHOW_ERRNO, "%s: Failed to open '%s'",
		 prefix);
      return -1;
    }

    while ((dirp = readdir(dir)) != NULL) {
      if (!strncmp(dirp->d_name, "video", 5)) {
	strncpy(buffer, prefix, sizeof(buffer));
	strncat(buffer, dirp->d_name, sizeof(buffer) - strlen(buffer));
	strncat(buffer, "/name", sizeof(buffer) - strlen(buffer));
	fprintf(stderr,"opening buffer: %s\n",buffer);

	if ((fd = open(buffer, O_RDONLY)) >= 0) {
	  if ((len = read(fd, buffer, sizeof(buffer)-1)) < 0) {
	    close(fd);
	    continue;
	  }
	  buffer[len]=0;

	  fprintf(stderr,"Read buffer: %s\n",buffer);


	  if (strncmp(buffer, "Loopback video device",21)) { /* weird stuff after minor */
	    close(fd);
	    continue;
	  }

	  min = atoi(&buffer[21]);

	  strcpy(buffer, "/dev/");
	  strncat(buffer, dirp->d_name, sizeof(buffer) - strlen(buffer));
	  MOTION_LOG(CRT, TYPE_VIDEO, NO_ERRNO,
		     "%s: found video device '%s' %d",
		     buffer,min);

	  if ((tfd = open(buffer, O_RDWR)) >= 0) {
	    strncpy(pipepath, buffer, sizeof(pipepath));

	    if (pipe_fd >= 0)
	      close(pipe_fd);

	    pipe_fd = tfd;
	    break;
	  }
	}
	close(fd);
      }
    }

    closedir(dir);

    if (pipe_fd >= 0)
      MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: Opened %s as input",
		 pipepath);

return pipe_fd;
}

typedef struct capent {char *cap; int code;} capentT;
capentT cap_list[] ={
  {"V4L2_CAP_VIDEO_CAPTURE        ",0x00000001 },
  {"V4L2_CAP_VIDEO_CAPTURE_MPLANE ",0x00001000 },
  {"V4L2_CAP_VIDEO_OUTPUT         ",0x00000002 },
  {"V4L2_CAP_VIDEO_OUTPUT_MPLANE  ",0x00002000 },
  {"V4L2_CAP_VIDEO_M2M	          ",0x00004000 },
  {"V4L2_CAP_VIDEO_M2M_MPLANE     ",0x00008000 },
  {"V4L2_CAP_VIDEO_OVERLAY	  ",0x00000004 },
  {"V4L2_CAP_VBI_CAPTURE	  ",0x00000010 },
  {"V4L2_CAP_VBI_OUTPUT	          ",0x00000020 },
  {"V4L2_CAP_SLICED_VBI_CAPTURE   ",0x00000040 },
  {"V4L2_CAP_SLICED_VBI_OUTPUT    ",0x00000080 },
  {"V4L2_CAP_RDS_CAPTURE	  ",0x00000100 },
  {"V4L2_CAP_VIDEO_OUTPUT_OVERLAY ",0x00000200 },
  {"V4L2_CAP_HW_FREQ_SEEK	  ",0x00000400 },
  {"V4L2_CAP_RDS_OUTPUT	          ",0x00000800 },
  {"V4L2_CAP_TUNER	          ",0x00010000 },
  {"V4L2_CAP_AUDIO	          ",0x00020000 },
  {"V4L2_CAP_RADIO	          ",0x00040000 },
  {"V4L2_CAP_MODULATOR	          ",0x00080000 },
  {"V4L2_CAP_SDR_CAPTURE	  ",0x00100000 },
  {"V4L2_CAP_EXT_PIX_FORMAT       ",0x00200000 },
  {"V4L2_CAP_SDR_OUTPUT	          ",0x00400000 },
  {"V4L2_CAP_READWRITE	          ",0x01000000 },
  {"V4L2_CAP_ASYNCIO	          ",0x02000000 },
  {"V4L2_CAP_STREAMING	          ",0x04000000 },
  {"V4L2_CAP_DEVICE_CAPS	  ",0x80000000 },
  {"Last",0}
};

void show_vcap(struct v4l2_capability *cap) {
  unsigned int vers = cap->version;
  unsigned int c    = cap->capabilities;
  int i;
  printf("Video Capabilities\n");
  printf("\tdriver   =%s",cap->driver);
  printf("\tcard     =%s",cap->driver);
  printf("\tbus_info =%s",cap->bus_info);
  printf("\tversion  =%u.%u.%u\n",(vers >> 16) & 0xFF,(vers >> 8) & 0xFF,vers & 0xFF);
  printf("\tDev capabilities:\n");
  for (i=0;cap_list[i].code;i++)
    if (c & cap_list[i].code) printf("\t\t%s\n",cap_list[i].cap);
}

void show_vfmt(struct v4l2_format *v) {
  printf("\tvtype                =%d\n",v->type);
  printf("\tvfmt.pix.width       =%d\n",v->fmt.pix.width);
  printf("\tvfmt.pix.height      =%d\n",v->fmt.pix.height);
  printf("\tvfmt.pix.pixelformat =%d\n",v->fmt.pix.pixelformat);
  printf("\tvfmt.pix.sizeimage   =%d\n",v->fmt.pix.sizeimage);
  printf("\tvfmt.pix.field       =%d\n",v->fmt.pix.field);
  printf("\tvfmt.pix.bytesperline=%d\n",v->fmt.pix.bytesperline);
  printf("\tvfmt.pix.colorspace  =%d\n",v->fmt.pix.colorspace);
}

/**
 * v4l2_startpipe
 *
 */
static int v4l2_startpipe(const char *dev_name, int width, int height, int type)
{
    int dev;
    struct v4l2_format v;
    struct v4l2_capability vc;

    if (!strcmp(dev_name, "-")) {
        dev = v4l2_open_vidpipe();
    } else {
        dev = open(dev_name, O_RDWR);
        MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO, "%s: Opened %s as input",
                   dev_name);
    }

    if (dev < 0) {
        MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: Opening %s as input failed",
                   dev_name);
        return -1;
    }

    if (ioctl(dev, VIDIOC_QUERYCAP, &vc) == -1) {
	MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: ioctl (VIDIOC_QUERYCAP)");
        return -1;
    }

    show_vcap(&vc);

    memset(&v, 0, sizeof(v));

    v.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

    if (ioctl(dev, VIDIOC_G_FMT, &v) == -1) {
        MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: ioctl (VIDIOC_G_FMT)");
        return -1;
    }
    printf("Original Format******************\n");
    show_vfmt(&v);
    v.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    v.fmt.pix.width = width;
    v.fmt.pix.height = height;
    v.fmt.pix.pixelformat = type;
    v.fmt.pix.sizeimage = 3 * width * height / 2;
    v.fmt.pix.bytesperline = width;
    v.fmt.pix.field = V4L2_FIELD_NONE;
    v.fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;

    printf("Proposed new Format**************\n");
    show_vfmt(&v);

    if (ioctl(dev,VIDIOC_S_FMT, &v) == -1) {
        MOTION_LOG(ERR, TYPE_VIDEO, SHOW_ERRNO, "%s: ioctl (VIDIOC_S_FMT)");
        return -1;
    }
    printf("Final Format*********************\n");
    show_vfmt(&v);
    return dev;
}

/**
 * v4l2_putpipe
 *
 */
static int v4l2_putpipe(int dev, unsigned char *image, int size)
{
    return write(dev, image, size);
}

/**
 * vid_startpipe
 *
 */
int vid_startpipe(const char *dev_name, int width, int height, int type)
{
    return v4l2_startpipe(dev_name, width, height, type);
}

/**
 * vid_putpipe
 *
 */
int vid_putpipe (int dev, unsigned char *image, int size)
{
    return v4l2_putpipe(dev, image, size);
}
#endif /* !WITHOUT_V4L2 && !BSD */
