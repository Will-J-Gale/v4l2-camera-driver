//From: https://www.marcusfolkesson.se/blog/capture-a-picture-with-v4l2/
// YUYV format https://fourcc.org/pixel-format/yuv-yuy2/

/*
    YUYV format:
    [Y0, U0, Y1, V0, Y1, U1, Y2, V1, Y2, U2, Y3, V2, Y3, U3, Y4, V3]

    Split into macro pixels
    [Y0, U0, Y1, V0, | Y1, U1, Y2, V1, | Y2, U2, Y3, V2, | Y3, U3, Y4, V3]
                  
*/

#include <iostream>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <linux/videodev2.h>

#include <opencv4/opencv2/opencv.hpp>

#define NBUF 3
#define WIDTH 640
#define HEIGHT 480

void query_capabilites(int fd)
{
    struct v4l2_capability cap;

    if (-1 == ioctl(fd, VIDIOC_QUERYCAP, &cap)) {
        perror("Query capabilites");
        exit(EXIT_FAILURE);
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        fprintf(stderr, "Device is no video capture device\n");
        exit(EXIT_FAILURE);
    }

    // if (!(cap.capabilities & V4L2_CAP_READWRITE)) 
    // {
    //     fprintf(stderr, "Device does not support read i/o\n");
    //     exit(EXIT_FAILURE);
    // }

    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        fprintf(stderr, "Devices does not support streaming i/o\n");
        exit(EXIT_FAILURE);
    }
}

int set_format(int fd, int width, int height) 
{
    struct v4l2_format format = {0};
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = width;
    format.fmt.pix.height = height;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    // format.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    // format.fmt.pix.field = V4L2_FIELD_INTERLACED;
    format.fmt.pix.field = V4L2_FIELD_NONE;

    int res = ioctl(fd, VIDIOC_S_FMT, &format);

    if(res == -1) {
        perror("Could not set format");
        exit(EXIT_FAILURE);
    }

    // struct v4l2_streamparm stream_param = {0};
    // stream_param.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    // stream_param.parm.capture.timeperframe.numerator = 1;
    // stream_param.parm.capture.timeperframe.denominator = 10;

    // res = ioctl(fd, VIDIOC_S_PARM, &stream_param);

    // if(res == -1)
    // {
    //     perror("Could not set FPS");
    //     exit(EXIT_FAILURE);
    // }

    return res;
}

int request_buffer(int fd, int count) 
{
    struct v4l2_requestbuffers req = {0};
    req.count = count;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (-1 == ioctl(fd, VIDIOC_REQBUFS, &req))
    {
        perror("Requesting Buffer");
        exit(1);
    }
    return req.count;
}

int query_buffer(int fd, int index, void** buffer) 
{
    struct v4l2_buffer buf = {0};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = index;
    int res = ioctl(fd, VIDIOC_QUERYBUF, &buf);
    if(res == -1) {
        perror("Could not query buffer");
        return 2;
    }

    *buffer = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
    
    if(*buffer == MAP_FAILED)
    {
        std::cout << "MMAP Error" << std::endl;
        perror("MMAP generation faled");
    }
    
    return buf.length;
}

int queue_buffer(int fd, int index) 
{
    struct v4l2_buffer bufd = {0};
    bufd.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    bufd.memory = V4L2_MEMORY_MMAP;
    bufd.index = index;
    if(-1 == ioctl(fd, VIDIOC_QBUF, &bufd))
    {
        perror("Queue Buffer");
        return 1;
    }
    return bufd.bytesused;
}

int start_streaming(int fd) 
{
    unsigned int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(ioctl(fd, VIDIOC_STREAMON, &type) == -1)
    {
        perror("VIDIOC_STREAMON");
        exit(1);
    }

    return 0;
}

int dequeue_buffer(int fd) 
{
    struct v4l2_buffer bufd = {0};
    bufd.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    bufd.memory = V4L2_MEMORY_MMAP;
    if(-1 == ioctl(fd, VIDIOC_DQBUF, &bufd))
    {
        perror("DeQueue Buffer");
        return 1;
    }
    std::cout << "Buffer length: " << bufd.length << " Bytes used: " << bufd.bytesused << std::endl;
    return bufd.index;
}

int stop_streaming(int fd) 
{
    unsigned int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(ioctl(fd, VIDIOC_STREAMOFF, &type) == -1)
    {
        perror("VIDIOC_STREAMOFF");
        exit(1);
    }

    return 0;
}

int show(const void* buffer, int size)
{
    // cv::Mat image(HEIGHT, WIDTH, CV_16U, (uchar*)buffer);
    uchar* yuy2 = (uchar*) buffer;
    uint num_yuv_pixels = (size / sizeof(uchar)) / 4;

    cv::Mat yuv = cv::Mat::zeros(cv::Size(WIDTH, HEIGHT), CV_8U);
    cv::Mat gray = cv::Mat::zeros(cv::Size(WIDTH, HEIGHT), CV_8U);
    cv::Mat u_gray = cv::Mat::zeros(cv::Size(WIDTH, HEIGHT), CV_8U);
    cv::Mat v_gray = cv::Mat::zeros(cv::Size(WIDTH, HEIGHT), CV_8U);

    std::cout << "Num YUV macro pixels: " << num_yuv_pixels << " UV image size: " << u_gray.cols * u_gray.rows << " WxH: " << WIDTH*HEIGHT << std::endl;
    
    for(int yuv_pixel = 0; yuv_pixel < num_yuv_pixels; yuv_pixel++)
    {
        int gray_index = yuv_pixel * 2;
        int yuv_index = yuv_pixel * 4;

        uchar y1 = yuy2[yuv_index];
        uchar u = yuy2[yuv_index+1];
        uchar y2 = yuy2[yuv_index+2];
        uchar v = yuy2[yuv_index+3];

        gray.data[gray_index] = y1;
        gray.data[gray_index+1] = y2;

        u_gray.data[gray_index] = u;
        u_gray.data[gray_index+1] = u;

        v_gray.data[gray_index] = v;
        v_gray.data[gray_index+1] = v;
    }

    // for(int yuv_pixel=0, rgb_index=0; yuv_pixel < size / sizeof(uchar); yuv_pixel+=4, rgb_index+=2)
    // {
    //     gray.data[rgb_index] = yuy2[yuv_pixel];
    //     gray.data[rgb_index+1] = yuy2[yuv_pixel+2];
    // }

    cv::imshow("Image", gray);
    cv::imshow("U", u_gray);
    cv::imshow("V", v_gray);
    return cv::waitKey(1);
}

int main()
{
    void* buffer[NBUF];
    int size, index, nbufs;
    int fd = open("/dev/video0", O_RDWR);

    if(fd == -1)
    {
        perror("Device cannot be read");
        return 1;
    }

    query_capabilites(fd);
    set_format(fd, WIDTH, HEIGHT);
    nbufs = request_buffer(fd, NBUF);
    if ( nbufs > NBUF) {
        fprintf(stderr, "Increase NBUF to at least %i\n", nbufs);
        exit(1);
    }

    for (int i = 0; i < NBUF; i++) 
    {
        /* Assume all sizes is equal.. */
        size = query_buffer(fd, i, &buffer[i]);
        queue_buffer(fd, i);
    }

    std::cout << fd << std::endl;
    start_streaming(fd);

    while(true)
    {
        //Capture image
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        struct timeval tv = {0};
        tv.tv_sec = 2;

        int r = select(fd+1, &fds, NULL, NULL, &tv);

        if(-1 == r){
            perror("Waiting for Frame");
            exit(1);
        }

        index = dequeue_buffer(fd);
        int exit = show(buffer[index], size);

        if(exit == 113)
        {
            break;
        }

        queue_buffer(fd, index);
    }

    stop_streaming(fd);

    // int file = open("output.raw", O_RDWR | O_CREAT, 0666);
    // write(file, buffer[index], size);
    // close(file);
    close(fd);

    return 0;
}