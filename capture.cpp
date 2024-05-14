/*
 * Copyright (c) 2015, NVIDIA CORPORATION. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/*
 *  V4L2 video capture example
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <getopt.h>             /* getopt_long() */

#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <asm/types.h>          /* for videodev2.h */

#include <linux/videodev2.h>

#include <cuda_runtime.h>

#include <iostream>

#define CLEAR(x) memset (&(x), 0, sizeof (x))
#define ARRAY_SIZE(a)   (sizeof(a)/sizeof((a)[0]))

struct buffer {
    void *                  start;
    size_t                  length;
};

static const char *     dev_name        = "/dev/video0";
static int              fd              = -1;
struct buffer *         buffers         = NULL;
static unsigned int     n_buffers       = 0;
//static unsigned int     width           = 4096;
static unsigned int     width           = 4096;
static unsigned int     height          = 1540;
static unsigned int     count           = 40;
static unsigned char *  cuda_out_buffer = NULL;
static bool             cuda_zero_copy = false;
static const char *     file_name       = "out.ppm";
//static unsigned int     pixel_format    = V4L2_PIX_FMT_SBGGR10DPCM8; //V4L2_PIX_FMT_SGRBG10;
static unsigned int     pixel_format    = V4L2_PIX_FMT_SBGGR10;
static unsigned int     field           = V4L2_FIELD_INTERLACED;

static void
errno_exit                      (const char *           s)
{
    fprintf (stderr, "%s error %d, %s\n",
            s, errno, strerror (errno));

    exit (EXIT_FAILURE);
}

static int
xioctl                          (int                    fd,
                                 int                    request,
                                 void *                 arg)
{
    int r;

    do r = ioctl (fd, request, arg);
    while (-1 == r && EINTR == errno);

    return r;
}

static void
process_image                   (void *           p)
{
    /* Save image. */
    char output_file_name[64];
    sprintf(output_file_name, "out_%d.ppm", count);

    if (count % 5 == 0){
        printf("Writing out file %s\n", output_file_name);
        FILE *fp = fopen (output_file_name, "wb");
        fwrite (p, 1, width * height *2, fp);
        fclose (fp);
    }

}

static int
read_frame                      (void)
{
    struct v4l2_buffer buf;
    unsigned int i;

    printf("read_frame: IO_METHOD_MMAP\n");
    CLEAR (buf);

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    if (-1 == xioctl (fd, VIDIOC_DQBUF, &buf)) {
        switch (errno) {
            case EAGAIN:
                return 0;

            case EIO:
                /* Could ignore EIO, see spec. */

                /* fall through */

            default:
                errno_exit ("VIDIOC_DQBUF");
        }
    }
    assert (buf.index < n_buffers);

    process_image (buffers[buf.index].start);

    if (-1 == xioctl (fd, VIDIOC_QBUF, &buf))
        errno_exit ("VIDIOC_QBUF");


    return 1;
}

static void
mainloop                        (void)
{
    while (count-- > 0) {
        for (;;) {
            fd_set fds;
            struct timeval tv;
            int r;

            FD_ZERO (&fds);
            FD_SET (fd, &fds);

            /* Timeout. */
            tv.tv_sec = 2;
            tv.tv_usec = 0;

            r = select (fd + 1, &fds, NULL, NULL, &tv);

            if (-1 == r) {
                if (EINTR == errno)
                    continue;

                errno_exit ("select");
            }

            if (0 == r) {
                fprintf (stderr, "select timeout\n");
                exit (EXIT_FAILURE);
            }

            if (read_frame ())
                break;

            /* EAGAIN - continue select loop. */
        }
    }
}

static bool stopCameraCapture(void)
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (-1 == xioctl (fd, VIDIOC_STREAMOFF, &type)) {
        return false;
    }

    return true;
}

static bool startCameraCapture(void)
{
    unsigned int i;
    enum v4l2_buf_type type;

    for (i = 0; i < n_buffers; ++i) {
        struct v4l2_buffer buf;

        CLEAR (buf);

        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = i;

        if (-1 == xioctl (fd, VIDIOC_QBUF, &buf)){
            return false;
        }
            
    }

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (-1 == xioctl (fd, VIDIOC_STREAMON, &type)){
        return false;
    }

    return true;
}

static bool
uninit_device                   (void)
{
    unsigned int i;

    for (i = 0; i < n_buffers; ++i){
        if (-1 == munmap (buffers[i].start, buffers[i].length)){
            return false;
        }
    }

    free (buffers);

    return true;
}

static void
init_mmap                       (void)
{
    struct v4l2_requestbuffers req;

    CLEAR (req);

    req.count               = 4;
    req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory              = V4L2_MEMORY_MMAP;

    if (-1 == xioctl (fd, VIDIOC_REQBUFS, &req)) {
        if (EINVAL == errno) {
            fprintf (stderr, "%s does not support "
                    "memory mapping\n", dev_name);
            exit (EXIT_FAILURE);
        } else {
            errno_exit ("VIDIOC_REQBUFS");
        }
    }

    if (req.count < 2) {
        fprintf (stderr, "Insufficient buffer memory on %s\n",
                dev_name);
        exit (EXIT_FAILURE);
    }

    buffers = (struct buffer *) calloc (req.count, sizeof (*buffers));

    if (!buffers) {
        fprintf (stderr, "Out of memory\n");
        exit (EXIT_FAILURE);
    }

    for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
        struct v4l2_buffer buf;

        CLEAR (buf);

        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = n_buffers;

        if (-1 == xioctl (fd, VIDIOC_QUERYBUF, &buf))
            errno_exit ("VIDIOC_QUERYBUF");

        buffers[n_buffers].length = buf.length;
        buffers[n_buffers].start =
            mmap (NULL /* start anywhere */,
                    buf.length,
                    PROT_READ | PROT_WRITE /* required */,
                    MAP_SHARED /* recommended */,
                    fd, buf.m.offset);

        if (MAP_FAILED == buffers[n_buffers].start)
            errno_exit ("mmap");
    }
}

static bool
initCamera                     (void)
{
    struct v4l2_capability cap;
    struct v4l2_cropcap cropcap;
    struct v4l2_crop crop;
    struct v4l2_format fmt;
    unsigned int min;

    if (-1 == xioctl (fd, VIDIOC_QUERYCAP, &cap)) {
        if (EINVAL == errno) {
            fprintf (stderr, "%s is no V4L2 device\n",
                    dev_name);
            return false;
        } else {
            return false;
        }
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        fprintf (stderr, "%s is no video capture device\n",
                dev_name);
        return false;
    }

    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        fprintf (stderr, "%s does not support streaming i/o\n",
                dev_name);
        return false;
    }

    /* Select video input, video standard and tune here. */
    CLEAR (cropcap);

    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (0 == xioctl (fd, VIDIOC_CROPCAP, &cropcap)) {
        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        crop.c = cropcap.defrect; /* reset to default */

        if (-1 == xioctl (fd, VIDIOC_S_CROP, &crop)) {
            switch (errno) {
                case EINVAL:
                    /* Cropping not supported. */
                    break;
                default:
                    /* Errors ignored. */
                    break;
            }
        }
    } else {
        /* Errors ignored. */
    }


    CLEAR (fmt);

    fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = width;
    fmt.fmt.pix.height      = height;
    fmt.fmt.pix.pixelformat = pixel_format;
    fmt.fmt.pix.field       = field;

    if (-1 == xioctl (fd, VIDIOC_S_FMT, &fmt))
        return false;

    /* Note VIDIOC_S_FMT may change width and height. */

    /* Buggy driver paranoia. */
    min = fmt.fmt.pix.width * 2;
    if (fmt.fmt.pix.bytesperline < min)
        fmt.fmt.pix.bytesperline = min;
    min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
    if (fmt.fmt.pix.sizeimage < min)
        fmt.fmt.pix.sizeimage = min;

    init_mmap ();

    return true;
}

static void
close_device                    (void)
{
    if (-1 == close (fd))
        errno_exit ("close");

    fd = -1;
}

static bool
openCamera                     (void)
{
    struct stat st;

    if (-1 == stat (dev_name, &st)) {
        fprintf (stderr, "Cannot identify '%s': %d, %s\n",
                dev_name, errno, strerror (errno));
        return false;
    }

    if (!S_ISCHR (st.st_mode)) {
        fprintf (stderr, "%s is no device\n", dev_name);
        return false;
    }

    fd = open (dev_name, O_RDWR /* required */ | O_NONBLOCK, 0);

    if (-1 == fd) {
        fprintf (stderr, "Cannot open '%s': %d, %s\n",
                dev_name, errno, strerror (errno));
        return false;
    }

    return true;
}

int main()
{
    bool status;

    //open_device ();
    status = openCamera();
    if (!status){
        std::cout << "Failed to open camera!" << std::endl;
    }

    //init_device();
    status = initCamera();
    if (!status){
        std::cout << "Failed to initialize camera!" << std::endl;
    }

    //Disable this altogether.  We should not need to allocate CUDA memory here
    //init_cuda ();

    //start_capturing ();
    status = startCameraCapture();
    if (!status){
        std::cout << "Failed to start camera capture!" << std::endl;
    }

    mainloop ();

    //stop_capturing ();
    status = stopCameraCapture();
    if (!status){
        std::cout << "Failed to stop camera capture!" << std::endl;
    }

    uninit_device ();

    close_device ();

    //exit (EXIT_SUCCESS);

    return 0;
}
