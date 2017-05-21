/*******************************************************************************
#	 	luvcview: Sdl video Usb Video Class grabber           .        #
#This package work with the Logitech UVC based webcams with the mjpeg feature. #
#All the decoding is in user space with the embedded jpeg decoder              #
#.                                                                             #
# 		Copyright (C) 2005 2006 Laurent Pinchart &&  Michel Xhaard     #
#                                                                              #
# This program is free software; you can redistribute it and/or modify         #
# it under the terms of the GNU General Public License as published by         #
# the Free Software Foundation; either version 2 of the License, or            #
# (at your option) any later version.                                          #
#                                                                              #
# This program is distributed in the hope that it will be useful,              #
# but WITHOUT ANY WARRANTY; without even the implied warranty of               #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                #
# GNU General Public License for more details.                                 #
#                                                                              #
# You should have received a copy of the GNU General Public License            #
# along with this program; if not, write to the Free Software                  #
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA    #
#                                                                              #
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <string.h>
#include <pthread.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <signal.h>
#include <X11/Xlib.h>
#include "v4l2uvc.h"
#include "utils.h"
#include "color.h"
#include "shell.h"

/* Fixed point arithmetic */
#define FIXED Sint32
#define FIXED_BITS 16
#define TO_FIXED(X) (((Sint32)(X))<<(FIXED_BITS))
#define FROM_FIXED(X) (((Sint32)(X))>>(FIXED_BITS))

#define INCPANTILT 64 // 1?

static const char version[] = VERSION;
struct vdIn *videoIn;

#define CAM_VIEW_RAW_FRAME_CAPTURE          (1)
#define CAM_VIEW_RAW_FRAME_STREAM_CAPTURE   (2)

#define CAM_VIEW_OUTPUT_FILENAME_MAX_LEN    (100)
#define CAM_VIEW_DEV_NAME_MAX_LEN           (20)
#define CAM_VIEW_DEFAULT_DEVNAME            "/dev/video1"
#define CAM_VIEW_DEFAULT_AVI_OUTPUT_NAME    "video.avi"
#define CAM_VIEW_DEFAULT_STREAM_OUTPUT_NAME "stream.raw"
#define CAM_VIEW_DEFAULT_RECORD_FORMAT      "MJPG"
#define CAM_VIEW_DEFAULT_CAPTURE_FILE_PREFIX "pic"

typedef struct {
    char devname[CAM_VIEW_DEV_NAME_MAX_LEN];
    int32_t width;
    int32_t height;
    int32_t formatIn;
    int32_t formatOut;
    int32_t mmapgrab;
    float fps;
    char avifilename[CAM_VIEW_OUTPUT_FILENAME_MAX_LEN];
    char streamfilename[CAM_VIEW_OUTPUT_FILENAME_MAX_LEN];
} cam_view_cfg_data_t;

cam_view_cfg_data_t g_cam_view_cfg;

/*====================== Shell Cmd Functions Start =======================*/
static int32_t cam_view_print_usage(int32_t argc, char *argv[])
{
    printf("usage:\n");
    printf("help\n"
           "  print this message\n");
    printf("dev <dev>\n"
           "  /dev/videoX       use videoX device\n");
    printf("-g   use read method for grab instead mmap\n");
    printf("infmt <YUYV|yuv|MJPG|jpg>\n"
           "  choose input video format (YUYV/yuv and MJPG/jpg are valid, default value is MJPG)\n");
    printf("outfmt <YUYV|yuv|MJPG|jpg>\n"
           "  choose output video format (YUYV/yuv and MJPG/jpg are valid, default value is YUYV)\n");
    printf("framerate <fps>\n"
           "  use specified frame rate\n");
    printf("size <widthxheight>\n"
           "  use specified input size\n");
    printf("mmapgrab <enable|disable>\n"
           "  choose default mmap(enable) or RW IO(disable).\n");
    printf("setaviname <avifile>\n"
           "  set avi file name, default video.avi\n");
    printf("setstreamname <streamfile>\n"
           "  set raw stream file name, default stream.raw\n");
    printf("listvideofmt\n"
           "  query valid video formats\n");
    printf("listvideoctrl\n"
           "  query valid controls and settings\n");
    printf("listcfg\n"
           "  read and set control settings from camview.cfg\n");
    printf("capture <interval ms>\n"
           "  capture a frame at a special interval\n");
    printf("startstream\n"
           "  start raw stream capturing and save data to file\n");
    printf("startpreview\n"
           "  start preview on LCD\n");
    printf("startrecord\n"
           "  start raw frame stream capturing and save data to file\n");
    printf("exit\n"
           "  exit cam view\n");

    return 0;
}

static void cam_view_sigcatch (int32_t sig)
{
    fprintf(stderr, "Exiting...\n");
    if (videoIn)
        videoIn->signalquit = 0;
}

int cam_view_getch(void)
{
    struct termios tm, tm_old;
    int fd = 0, ch;

    if (tcgetattr(fd, &tm) < 0) {
        return -1;
    }

    tm_old = tm;
    cfmakeraw(&tm);
    if (tcsetattr(fd, TCSANOW, &tm) < 0) {
        return -1;
    }

    ch = getchar();
    if (tcsetattr(fd, TCSANOW, &tm_old) < 0) {
        return -1;
    }

    return ch;
}

static int32_t cam_view_set_dev(int32_t argc, char *argv[])
{
    int32_t dev_name_len = 0;
    const char *tmp = NULL;

    if (argc > 2) {
	    printf("too many arguments, aborting.\n");
		return (1);
	}

    if (1 == argc) {
        printf("current device: %s\n", g_cam_view_cfg.devname);
    } else {
        tmp = argv[1];
        dev_name_len = strlen(tmp);
        if (dev_name_len > CAM_VIEW_DEV_NAME_MAX_LEN) {
            printf("device name too long, aborting.\n");
		    return (1);
        }
        memset(g_cam_view_cfg.devname, 0, CAM_VIEW_DEV_NAME_MAX_LEN);
        strcpy(g_cam_view_cfg.devname, tmp);
    }
    return 0;
}

static int32_t cam_view_set_mmapgrab(int32_t argc, char *argv[])
{
    const char *tmp = NULL;

    if (argc > 2) {
	    printf("too many arguments, aborting.\n");
		return (1);
	}

	tmp = argv[1];
    if (1 == argc) {
        printf("current stream method is %s\n", g_cam_view_cfg.mmapgrab ? "mmap" : "RW IO");
    } else {
	    if (strcasecmp(tmp, "enabled") == 0) {
            g_cam_view_cfg.mmapgrab = 1;
	    	printf("stream method is set to mmap.\n");
        } else if (strcasecmp(tmp, "disabled") == 0) {
            g_cam_view_cfg.mmapgrab = 0;
	    	printf("stream method is set to RW IO\n");
        } else {
	    	printf("unknown grab setting. Aborting.\n");
	        return (1);
        }
	}
    return 0;
}

static int32_t cam_view_set_input_format(int32_t argc, char *argv[])
{
    const char *tmp = NULL;
    int32_t format = V4L2_PIX_FMT_MJPEG;

    if (argc > 2) {
	    printf("too many arguments, aborting.\n");
		return (1);
	}

    if (1 == argc) {
        if (V4L2_PIX_FMT_YUYV == g_cam_view_cfg.formatIn)
            printf("current input format is yuv/YUYV.\n");
        else if (V4L2_PIX_FMT_MJPEG == g_cam_view_cfg.formatIn)
            printf("current input format is jpg/MJPG.\n");
        else {
            printf("unrecognized format: %d\n", g_cam_view_cfg.formatIn);
            return 1;
        }
    } else {
	    tmp = argv[1];

	    if (strcasecmp(tmp, "yuv") == 0 || strcasecmp(tmp, "YUYV") == 0) {
            printf("Input format is set to yuv/YUYV.\n");
	        format = V4L2_PIX_FMT_YUYV;
        } else if (strcasecmp(tmp, "jpg") == 0 || strcasecmp(tmp, "MJPG") == 0) {
            printf("Input format is set to jpg/MJPG.\n");
		    format = V4L2_PIX_FMT_MJPEG;
        } else {
		    printf("Unknown format specified. Aborting.\n");
	        return (1);
	    }
        g_cam_view_cfg.formatIn = format;
    }
    return 0;
}

static int32_t cam_view_set_size(int32_t argc, char *argv[])
{
    const char *tmp = NULL;
    char *separateur = NULL;
    int32_t width = 0, height = 0;

    if (argc > 2) {
	    printf("too many arguments, aborting.\n");
		return (1);
	}

    if (1 == argc) {
        printf("current size is %dx%d.\n", g_cam_view_cfg.width, g_cam_view_cfg.height);
    } else {
        tmp = argv[1];

        width = strtoul(tmp, &separateur, 10);
	    if (*separateur != 'x') {
	        printf("error in size use widthxheight\n");
		    return (1);
	    } else {
	    	++separateur;
		    height = strtoul(separateur, &separateur, 10);
		    if (*separateur != 0)
			    printf("hmm.. dont like that!! trying this height\n");
        }
        g_cam_view_cfg.width = width;
        g_cam_view_cfg.height = height;
        printf("size is set to %dx%d.\n", width, height);
	}

    return 0;
}

static int32_t cam_view_set_output_format(int32_t argc, char *argv[])
{
    const char *tmp = NULL;
    int32_t format = V4L2_PIX_FMT_YUYV;

    if (argc > 2) {
	    printf("too many arguments, aborting.\n");
		return (1);
	}

    if (1 == argc) {
        if (V4L2_PIX_FMT_YUYV == g_cam_view_cfg.formatOut)
            printf("current output format is yuv/YUYV.\n");
        else if (V4L2_PIX_FMT_MJPEG == g_cam_view_cfg.formatOut)
            printf("current output format is jpg/MJPG.\n");
        else {
            printf("unrecognized format: %d\n", g_cam_view_cfg.formatOut);
            return 1;
        }
    } else {
	    tmp = argv[1];

	    if (strcasecmp(tmp, "yuv") == 0 || strcasecmp(tmp, "YUYV") == 0) {
            printf("Output format is set to yuv/YUYV.\n");
	        format = V4L2_PIX_FMT_YUYV;
        } else if (strcasecmp(tmp, "jpg") == 0 || strcasecmp(tmp, "MJPG") == 0) {
            printf("Output format is set to jpg/MJPG.\n");
		    format = V4L2_PIX_FMT_MJPEG;
        } else {
		    printf("Unknown format specified. Aborting.\n");
	        return (1);
	    }
        g_cam_view_cfg.formatOut = format;
    }
    return 0;

}

static int32_t cam_view_set_frame_rate(int32_t argc, char *argv[])
{
    const char *tmp= NULL;
    float fps = 0.0;
    char *separateur = NULL;

    if (argc > 2) {
	    printf("too many arguments, aborting.\n");
		return (1);
	}

    if (1 == argc) {
        printf("current fps is %.2f\n", g_cam_view_cfg.fps);
    } else {
        tmp = argv[1];

        fps = strtof(tmp, &separateur);
        if(*separateur != '\0') {
            printf("Invalid frame rate '%s' specified. "
                    "Only numbers are supported. Aborting.\n", tmp);
            return (1);
        }

        g_cam_view_cfg.fps = fps;
        printf("fsp is set to %.2f\n", fps);
    }

    return 0;
}

static int32_t cam_view_start_capture(int32_t argc, char *argv[])
{
    const char *tmp= NULL;
    struct timeval ref_time, end_time;
    struct vdIn *videoIn;
    int32_t interval = 0;
    int32_t time_dur = 0;

    if (argc > 2) {
	    printf("too many arguments, aborting.\n");
		return (1);
	}

    if (1 == argc) {
        printf("capture one frame ... \n");
    } else {
        tmp = argv[1];

        interval = atoi(tmp);
        if (0 == interval) {
            printf("invalid interval value\n"); 
            return 1;
        }
    }

    videoIn = (struct vdIn *)calloc(1, sizeof(struct vdIn));
    if (!videoIn)
        printf("unable to allocate memory for video\n");

    if (v4l2uvc_init_videoIn
			(videoIn,
             (char *)g_cam_view_cfg.devname,
             g_cam_view_cfg.width,
             g_cam_view_cfg.height,
             g_cam_view_cfg.fps,
             g_cam_view_cfg.formatIn,
			 g_cam_view_cfg.mmapgrab,
             g_cam_view_cfg.avifilename) < 0) {
        free(videoIn);
        printf("Unable to init video device!\n");
		return (1);
    }

    initLut();

    printf("Capturing frames ... press 'q' or 'Q' to stop\n");
    gettimeofday(&ref_time, NULL);
    while (videoIn->signalquit) {
        if (1 == argc)
        {
            /* Capture one frame, so quit after capture */
            videoIn->signalquit = 0;
        }
        else if ('q' == cam_view_getch() || 'Q' == cam_view_getch())
        {
            videoIn->signalquit = 0;
        }
        if (v4l2uvc_capture(videoIn) < 0) {
            printf("error capturing frame");
            v4l2uvc_close_v4l2(videoIn);
            free(videoIn);
            return 1;
        }

        gettimeofday(&end_time, NULL);
        time_dur = (end_time.tv_sec - ref_time.tv_sec) * 1000000 + (end_time.tv_usec - ref_time.tv_usec);
        if (time_dur > interval * 1000) {
            switch (g_cam_view_cfg.formatOut) {
            case V4L2_PIX_FMT_YUYV:
                utils_get_picture_yv2(CAM_VIEW_DEFAULT_CAPTURE_FILE_PREFIX, videoIn->framebuffer,
                        videoIn->width, videoIn->height);
                break;
            case V4L2_PIX_FMT_MJPEG:
                utils_get_picture_mjpg(CAM_VIEW_DEFAULT_CAPTURE_FILE_PREFIX, videoIn->tmpbuffer,
                        videoIn->tmpbuf_byteused);
                break;
            default:
                printf("invalid video format\n");
                videoIn->signalquit = 0;
            }
        }

        gettimeofday(&ref_time, NULL);

        if (0 == interval)
            videoIn->signalquit = 0;
    }
    v4l2uvc_close_v4l2(videoIn);
    free(videoIn);
    freeLut();

    return 0;
}

static int32_t cam_view_start_stream(int32_t argc, char *argv[])
{
    const char *tmp= NULL;

    if (argc > 1) {
	    printf("too many arguments, aborting.\n");
		return (1);
	}

    videoIn->captureFile = fopen(g_cam_view_cfg.streamfilename, "wb");
	if(videoIn->captureFile == NULL)
		perror("Unable to open file for raw stream capturing");
	else
		printf("Starting raw stream capturing to %s...\n", g_cam_view_cfg.streamfilename);

    videoIn = (struct vdIn *)calloc(1, sizeof(struct vdIn));
    if (!videoIn)
        printf("unable to allocate memory for video\n");

    if (v4l2uvc_init_videoIn
			(videoIn,
             (char *)g_cam_view_cfg.devname,
             g_cam_view_cfg.width,
             g_cam_view_cfg.height,
             g_cam_view_cfg.fps,
             g_cam_view_cfg.formatIn,
			 g_cam_view_cfg.mmapgrab,
             g_cam_view_cfg.avifilename) < 0) {
        printf("Unable to init video device!\n");
		return (1);
    }

    videoIn->captureFile = NULL;

    return 0;
}

#if 0
static void *cam_view_record_thread(void *arg)
{
    struct vdIn *vd = (struct vdIn *)arg;

    while(1)
    {
        memset(&vd->buf, 0, sizeof(struct v4l2_buffer));
        vd->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        vd->buf.memory = V4L2_MEMORY_MMAP;
        ret = ioctl(vd->fd, VIDIOC_DQBUF, &vd->buf);
        if (ret < 0)
        {
            printf("Unable to dequeue buffer");
            return ;
        }

        memcpy(vd->tmpbuffer, vd->mem[vd->buf.index], vd->buf.bytesused);

        AVI_write_frame(vd->avifile, vd->tmpbuffer,vd->buf.bytesused, vd->framecount);

        vd->framecount++;

        ret = ioctl(vd->fd, VIDIOC_QBUF, &vd->buf);
        if (ret < 0)
        {
            printf("Unable to requeue buffer");
            exit(1);
        }
    }
}
#endif

static int32_t cam_view_start_record(int32_t argc, char *argv[])
{
    const char *tmp= NULL;
    int32_t    rtn = 0;
    struct vdIn *videoIn = NULL;
#if 0
    pthread_t  record_thread;
    void *thread_result;
#endif

    if (argc > 1) {
	    printf("too many arguments, aborting.\n");
		return (1);
	}

#if 0
    videoIn = (struct vdIn *)calloc(1, sizeof(struct vdIn));
    if (!videoIn)
        printf("unable to allocate memory for video\n");

    if (v4l2uvc_init_videoIn
			(videoIn,
             (char *)g_cam_view_cfg.devname,
             g_cam_view_cfg.width,
             g_cam_view_cfg.height,
             g_cam_view_cfg.fps,
             g_cam_view_cfg.format,
			 g_cam_view_cfg.mmapgrab,
             g_cam_view_cfg.avifilename) < 0) {
        printf("Unable to init video device!\n");
		return (1);
    }

    if (!videoIn->isstreaming) {
        if (video_enable(videoIn)) {
            printf("can't enable video\n");
            return 1;
        }
    }

    videoIn->avifile = AVI_open_output_file(videoIn->avifilename);
    if (NULL == videoIn->avifile) {
        printf("error opening avifile %s\n", videoIn->avifile);
        return 1;
    }

    AVI_set_video(videoIn->avifile, videoIn->width, videoIn->height, videoIn->fps,
            CAM_VIEW_DEFAULT_RECORD_FORMAT);
    printf("start recording to %s, press 'q' or 'Q' to stop\n", videoIn->avifilename);

#if 0
    rtn = pthread_create(&record_thread, NULL, cam_view_record_thread, (void *)videoIn);
    if (rtn != 0)
    {
        perror("Thread creation failed!");
        return 1;
    }
#endif

    while(1)
    {
        if (('q' == cam_view_getch(stdin)) || 'Q' == cam_view_getch(stdin))
            break;
        memset(&videoIn->buf, 0, sizeof(struct v4l2_buffer));
        videoIn->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        videoIn->buf.memory = V4L2_MEMORY_MMAP;
        ret = ioctl(videoIn->fd, VIDIOC_DQBUF, &videoIn->buf);
        if (ret < 0)
        {
            printf("Unable to dequeue buffer");
            return ;
        }

        memcpy(vd->tmpbuffer, vd->mem[vd->buf.index], vd->buf.bytesused);

        AVI_write_frame(vd->avifile, vd->tmpbuffer,vd->buf.bytesused, vd->framecount);

        vd->framecount++;

        ret = ioctl(vd->fd, VIDIOC_QBUF, &vd->buf);
        if (ret < 0)
        {
            printf("Unable to requeue buffer");
            exit(1);
        }
    }
    AVI_close(videoIn->avifile);
    videoIn->avifile = NULL;
    v4l2uvc_close_v4l2(videoIn);
    free(videoIn);
#endif

    return 0;
}

static int32_t cam_view_start_preview(int32_t argc, char *argv[])
{
    const char *tmp= NULL;

    if (argc > 1) {
	    printf("too many arguments, aborting.\n");
		return (1);
	}

    return 0;
}

static int32_t cam_view_set_aviname(int32_t argc, char *argv[])
{
    const char *tmp= NULL;

    if (argc > 2) {
	    printf("too many arguments, aborting.\n");
		return (1);
	}

    if (1 == argc) {
        printf("current output avi filename is %s\n", g_cam_view_cfg.avifilename);
    } else {
        int32_t tmp_len = 0;

        tmp = argv[1];
        tmp_len = strlen(tmp);
        if (tmp_len > CAM_VIEW_OUTPUT_FILENAME_MAX_LEN) {
            printf("file name too long, aborting.\n");
            return 1;
        }

        memset(g_cam_view_cfg.avifilename, 0, CAM_VIEW_OUTPUT_FILENAME_MAX_LEN);
        strcpy(g_cam_view_cfg.avifilename, tmp);
        printf("current output avi filename is set to %s\n", g_cam_view_cfg.avifilename);
    }

    return 0;
}

static int32_t cam_view_set_streamname(int32_t argc, char *argv[])
{
    const char *tmp= NULL;

    if (argc > 2) {
	    printf("too many arguments, aborting.\n");
		return (1);
	}

    if (1 == argc) {
        printf("current output stream filename is %s\n", g_cam_view_cfg.streamfilename);
    } else {
        int32_t tmp_len = 0;

        tmp = argv[1];
        tmp_len = strlen(tmp);
        if (tmp_len > CAM_VIEW_OUTPUT_FILENAME_MAX_LEN) {
            printf("file name too long, aborting.\n");
            return 1;
        }

        memset(g_cam_view_cfg.streamfilename, 0, CAM_VIEW_OUTPUT_FILENAME_MAX_LEN);
        strcpy(g_cam_view_cfg.streamfilename, tmp);
        printf("current output stream filename is set to %s\n", g_cam_view_cfg.streamfilename);
    }

    return 0;
}

static int32_t cam_view_list_video_format(int32_t argc, char *argv[])
{
    if (argc >= 2) {
	    printf("too many parameters, aborting.\n");
		return (1);
	}

	v4l2uvc_check_videoIn(videoIn, (char *)g_cam_view_cfg.devname);

    return 0;
}

static int32_t cam_view_list_video_control(int32_t argc, char *argv[])
{
    if (argc >= 2) {
	    printf("too many parameters, aborting.\n");
		return (1);
	}

    v4l2uvc_enum_controls(videoIn->fd);

    return 0;
}

static int32_t cam_view_list_config_file(int32_t argc, char *argv[])
{
    if (argc >= 2) {
	    printf("too many parameters, aborting.\n");
		return (1);
	}

    //v4l2uvc_load_controls(videoIn->fd);

    return 0;
}

static int32_t cam_view_exit(int32_t argc, char *argv[])
{
    if (argc >= 2) {
	    printf("too many parameters, aborting.\n");
		return (1);
	}
 
    if (videoIn) {
        v4l2uvc_close_v4l2(videoIn);
        free(videoIn);
    }

    shell_signal_exit();
}

static shell_cmd_tbl_t cam_view_cmd_tbl[] = {
    { "dev", 2, cam_view_set_dev },
    { "size", 2, cam_view_set_size },
    { "infmt", 2, cam_view_set_input_format },
    { "outfmt", 2, cam_view_set_output_format },
    { "framerate", 2, cam_view_set_frame_rate },
    { "mmapgrab", 2, cam_view_set_mmapgrab },
    { "setaviname", 2, cam_view_set_aviname },
    { "setstreamname", 2, cam_view_set_streamname },
    { "listvideofmt", 1, cam_view_list_video_format },
    { "listvideoctrl", 1, cam_view_list_video_control },
    { "listcfg", 1, cam_view_list_config_file },
    { "capture", 3, cam_view_start_capture },
    { "startstream", 1, cam_view_start_stream },
    { "startpreview", 1, cam_view_start_preview },
    { "startrecord", 1, cam_view_start_record },
    { "exit", 1, cam_view_exit },
    { "help",  1, cam_view_print_usage },
};

/*====================== Shell Cmd Functions End =======================*/
static int32_t cam_view_init(void)
{
    memset(&g_cam_view_cfg, 0, sizeof(cam_view_cfg_data_t));
    g_cam_view_cfg.width = 640;
    g_cam_view_cfg.height = 480;
    g_cam_view_cfg.formatIn = V4L2_PIX_FMT_MJPEG;
    g_cam_view_cfg.formatOut = V4L2_PIX_FMT_YUYV;
    g_cam_view_cfg.fps = 30.0;
    g_cam_view_cfg.mmapgrab = 1;
    strcpy(g_cam_view_cfg.devname,  CAM_VIEW_DEFAULT_DEVNAME);
    strcpy(g_cam_view_cfg.avifilename, CAM_VIEW_DEFAULT_AVI_OUTPUT_NAME);
    strcpy(g_cam_view_cfg.streamfilename, CAM_VIEW_DEFAULT_STREAM_OUTPUT_NAME);

    return 0;
};

int main(int argc, char *argv[])
{
    (void)signal(SIGINT,  cam_view_sigcatch);
    (void)signal(SIGQUIT, cam_view_sigcatch);
    (void)signal(SIGKILL, cam_view_sigcatch);
    (void)signal(SIGTERM, cam_view_sigcatch);
    (void)signal(SIGABRT, cam_view_sigcatch);
    (void)signal(SIGTRAP, cam_view_sigcatch);

    if (cam_view_init())
        exit (1);

	printf("Camera View %s\n\n", version);
    shell_init(cam_view_cmd_tbl, sizeof(cam_view_cmd_tbl) / sizeof(shell_cmd_tbl_t), SHELL_CONFIG_PROMPT);

	while (!shell_get_exit_signal()) {
        char ch_in = cam_view_getch();

        shell_getc(ch_in);

        if (shell_get_cmd_exec_flag())
        {
            shell_exec_fun();
        }
	}

    return 0;
}

