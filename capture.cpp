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

#include <iostream>

#define CLEAR(x) memset (&(x), 0, sizeof (x))

struct buffer {
    void *                  start;
    size_t                  length;
};

static const char *     dev_name        = "/dev/video0";
static int              fd              = -1;
struct buffer *         buffers         = NULL;
static unsigned int     n_buffers       = 0;
static unsigned int     width           = 4096;
static unsigned int     height          = 1540;
static unsigned int     count           = 40;
static unsigned char *  cuda_out_buffer = NULL;
static bool             cuda_zero_copy = false;
static const char *     file_name       = "out.ppm";
static unsigned int     pixel_format    = V4L2_PIX_FMT_SBGGR10;
static unsigned int     field           = V4L2_FIELD_INTERLACED;

//Function declarations
static int xioctl(int fd, int request, void * arg);
static bool openCamera(void);
static bool initCamera(void);
static bool initMemoryMap(void);
static bool startCameraCapture(void);
static bool captureLoop(void);
static int acquireFrame(void);
static void outputImage(void * p);
static bool stopCameraCapture(void);
static bool closeCamera(void);

//@brief 
static int xioctl(int fd, int request, void * arg)
{
    int r;

    do r = ioctl (fd, request, arg);
    while (-1 == r && EINTR == errno);

    return r;
}

//! @brief Open the camera device
//! @return true if camera device is successfully opened, false otherwise
static bool openCamera(void)
{
    struct stat st;

    if (-1 == stat (dev_name, &st)) {
        std::cout << "Cannot identify " << dev_name << ":" << errno << "," << strerror(errno) << std::endl;

        return false;
    }

    if (!S_ISCHR (st.st_mode)) {
        std::cout << dev_name << " is no device" << std::endl;
        return false;
    }

    fd = open (dev_name, O_RDWR /* required */ | O_NONBLOCK, 0);

    if (-1 == fd) {
        std::cout << "Cannot open " << dev_name << ":" << errno << "," << strerror(errno) << std::endl;
        return false;
    }

    return true;
}

//! @brief Initialize camera for image capture
//! @return true if camera is successfully initialized for streaming, false otherwise
static bool initCamera(void)
{
    struct v4l2_capability cap;
    struct v4l2_cropcap cropcap;
    struct v4l2_crop crop;
    struct v4l2_format fmt;
    unsigned int min;
    bool status;

    if (-1 == xioctl (fd, VIDIOC_QUERYCAP, &cap)) {
        if (EINVAL == errno) {
            std::cout << dev_name << " is no V4L2 device" << std::endl;

            return false;
        } else {
            return false;
        }
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        std::cout << dev_name << " is no video capture device" << std::endl;
        return false;
    }

    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        std::cout << dev_name << " does not support streaming i/o" << std::endl;
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

    status = initMemoryMap ();
    if (!status){
        std::cout << "Failed to initialize mapped memory!" << std::endl;
        return false;
    }

    return true;
}

//! @brief Initialize memory map used for acquiring images
//! @return true if memory map is successfully allocated, false otherwise
static bool initMemoryMap(void)
{
    struct v4l2_requestbuffers req;

    CLEAR (req);

    req.count               = 4;
    req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory              = V4L2_MEMORY_MMAP;

    if (-1 == xioctl (fd, VIDIOC_REQBUFS, &req)) {
        if (EINVAL == errno) {
            std::cout << dev_name << " does not support memory mapping!" << std::endl;
            return false;
        } else {
            std::cout << "VIDIOC_REQBUFS failed!" << std::endl;
            return false;
        }
    }

    if (req.count < 2) {
        std::cout << "Insufficient buffer memory on " << dev_name << std::endl;
        return false;
    }

    buffers = (struct buffer *) calloc (req.count, sizeof (*buffers));

    if (!buffers) {
        std::cout << "Out of memory" << std::endl;
        return false;
    }

    for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
        struct v4l2_buffer buf;

        CLEAR (buf);

        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = n_buffers;

        if (-1 == xioctl (fd, VIDIOC_QUERYBUF, &buf)){
            std::cout << "VIDEOC_QUERYBUF" << std::endl;
            return false;
        }

        buffers[n_buffers].length = buf.length;
        buffers[n_buffers].start =
            mmap (NULL /* start anywhere */,
                    buf.length,
                    PROT_READ | PROT_WRITE /* required */,
                    MAP_SHARED /* recommended */,
                    fd, buf.m.offset);

        if (MAP_FAILED == buffers[n_buffers].start) {
            std::cout << "mmap" << std::endl;
            return false;
        }
    }
    return true;
}

//! @brief Start camera capture
//! @return true if camera capture is successfully started, false otherwise
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

//! @brief Main capture loop for capturing images from camera
//! @return true if image is successfully captured from camera, false otherwise
static bool captureLoop(void)
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
                if (EINTR == errno){
                    continue;
                }

                return false;
            }

            if (0 == r) {
                std::cout << "select timeout" << std::endl;
                return false;
            }

            if (acquireFrame())
                break;

            /* EAGAIN - continue select loop. */
        }
    }
}

//! @brief Acquire a frame from v4l2
//! @return true if frame is successfully acquired, false otherwise
static int acquireFrame(void)
{
    struct v4l2_buffer buf;
    unsigned int i;

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
                return false;
        }
    }
    assert (buf.index < n_buffers);

    outputImage(buffers[buf.index].start);

    if (-1 == xioctl (fd, VIDIOC_QBUF, &buf)){
        return false;
    }

    return 1;
}

//! @brief Output image to a file
static void outputImage(void * p)
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

//! @brief Stop camera capture
//! @return true if camera capture is successfully stopped, false otherwise
static bool stopCameraCapture(void)
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (-1 == xioctl (fd, VIDIOC_STREAMOFF, &type)) {
        return false;
    }

    return true;
}

//! @brief Close the camera
//! @return true if camera is successfully closed, false otherwise
static bool closeCamera(void){
    unsigned int i;

    for (i = 0; i < n_buffers; ++i){
        if (-1 == munmap (buffers[i].start, buffers[i].length)){
            return false;
        }
    }

    free (buffers);

    if (-1 == close (fd)){
        return false;
    }
        
    fd = -1;

    return true;
}

int main()
{
    bool status;

    status = openCamera();
    if (!status){
        std::cout << "Failed to open camera!" << std::endl;
    }

    status = initCamera();
    if (!status){
        std::cout << "Failed to initialize camera!" << std::endl;
    }

    status = startCameraCapture();
    if (!status){
        std::cout << "Failed to start camera capture!" << std::endl;
    }

    //Main loop for capturing images
    captureLoop();

    status = stopCameraCapture();
    if (!status){
        std::cout << "Failed to stop camera capture!" << std::endl;
    }

    status = closeCamera();
    if (!status){
        std::cout << "Failed to close camera!" << std::endl;
    }

    return 0;
}
