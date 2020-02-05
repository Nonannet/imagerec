#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>
#include <time.h>
#include <linux/videodev2.h>
#include "imageRec.h"
#include "cmdTcpipClient.h"

imageBuffer* buffers[3];
struct pixel_list maxima[32];

unsigned char* buffer;
int useTcpIp = 0;
int silent = 0;
int tcpsilent = 0;

static int xioctl(int fd, int request, void* arg)
{
	int r;

	do r = ioctl(fd, (unsigned long)request, arg);
	while (-1 == r && EINTR == errno);

	return r;
}

int configImageFormat(int fd, struct v4l2_cropcap *cropcap, unsigned int camWidth, unsigned int camHight)
{
	struct v4l2_capability caps = {};

	if (-1 == xioctl(fd, VIDIOC_QUERYCAP, &caps))
	{
		perror("Querying capabilities failed");
		return 1;
	}

	if (!silent) printf("Driver caps:\n"
		"  Driver: \"%s\"\n"
		"  Card: \"%s\"\n"
		"  Bus: \"%s\"\n"
		"  Version: %d.%d\n"
		"  Capabilities: %08x\n",
		caps.driver, caps.card, caps.bus_info, (caps.version >> 16) && 0xff,
		(caps.version >> 24) && 0xff, caps.capabilities);

	cropcap->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (-1 == xioctl(fd, VIDIOC_CROPCAP, cropcap))
	{
		perror("Querying cropping capabilities failed");
		return 1;
	}

	int formatIsSupported = 0;

	struct v4l2_fmtdesc fmtdesc = { 0 };

	fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	char fourcc[5] = { 0 };
	char c, e;

	while (0 == xioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc))
	{
		strncpy(fourcc, (char*)& fmtdesc.pixelformat, 4);

		if (fmtdesc.pixelformat == V4L2_PIX_FMT_YUYV)
			formatIsSupported = 1;

		c = fmtdesc.flags & 1 ? 'C' : ' ';
		e = fmtdesc.flags & 2 ? 'E' : ' ';
		if (!silent) printf("  %s: %c%c %s\n", fourcc, c, e, fmtdesc.description);
		fmtdesc.index++;
	}

	if (!formatIsSupported)
	{
		perror("Pixel format is not supported\n");
		return 1;
	}

	struct v4l2_format fmt = { 0 };
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width = camWidth;
	fmt.fmt.pix.height = camHight;
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
	fmt.fmt.pix.field = V4L2_FIELD_NONE;

	if (!silent) printf("Requested camera settings:\n"
		"  Width: %d\n"
		"  Height: %d\n"
		"  Pixel format: %s\n",
		fmt.fmt.pix.width,
		fmt.fmt.pix.height,
		fourcc);

	if (-1 == xioctl(fd, VIDIOC_S_FMT, &fmt))
	{
		perror("Setting pixel format failed");
		return 1;
	}

	strncpy(fourcc, (char*)& fmt.fmt.pix.pixelformat, 4);

	if (!silent) printf("Selected camera settings:\n"
		"  Width: %d\n"
		"  Height: %d\n"
		"  Pixel format: %s\n",
		fmt.fmt.pix.width,
		fmt.fmt.pix.height,
		fourcc);
	return 0;
}

int initMmap(int fd)
{
	struct v4l2_requestbuffers req = { 0 };
	req.count = 1;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;

	if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req))
	{
		perror("Requesting buffer failed (initMmap)");
		return 1;
	}

	struct v4l2_buffer buf = { 0 };
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;
	buf.index = 0;
	if (-1 == xioctl(fd, VIDIOC_QUERYBUF, &buf))
	{
		perror("Querying buffer failed (initMmap)");
		return 1;
	}

	buffer = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);

	return 0;
}

int captureImage(int fd, imageBuffer* destBuffer, unsigned int targetBufferSize)
{
	unsigned int buffSize;

	struct v4l2_buffer buf = { 0 };
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;
	buf.index = 0;
	if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
	{
		perror("Query buffer failed (captureImage)");
		return 1;
	}

	if (-1 == xioctl(fd, VIDIOC_STREAMON, &buf.type))
	{
		perror("Query start Capture failed");
		return 1;
	}

	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	struct timeval tv = { 0 };
	tv.tv_sec = 2;
	int r = select(fd + 1, &fds, NULL, NULL, &tv);
	if (-1 == r)
	{
		perror("Waiting for frame timed out");
		return 1;
	}

	if (-1 == xioctl(fd, VIDIOC_DQBUF, &buf))
	{
		perror("Retrieving frame failed");
		return 1;
	}

	if (buf.bytesused / 2 > targetBufferSize)
		buffSize = targetBufferSize;
	else
		buffSize = buf.bytesused / 2; 

	//Extract 8 Bit monocrome Y data from YUYV
	for (int i = 0; i < buffSize; i++)
		destBuffer[i] = (imageBuffer)buffer[i * 2] * 0x100;

	return 0;
}

void printOut(char text[])
{
	int i = 0;

	if (useTcpIp && !tcpsilent)
	{
		while (text[i] != 0) i++;
		sendTcpData((char*)text, i);
	}
	else
	{
		if (!silent) printf("%s", text);
	}
}

void printOutChar(char character)
{
	if (useTcpIp)
		sendTcpData(&character, 1);
	else
		printf("%c", character);
}

void numberToString(int number, int divPos) {
	int i = 10000;
	int dCount;
	int tempVal;
	char divVal;

	if (number < 0)
	{
		printOutChar('-');
		number *= -1;
	}

	if (number)
	{
		dCount = 5 - divPos;
		tempVal = number;
		while (i)
		{
			if (number >= i)
			{
				divVal = (char)(tempVal / i);
				printOutChar(divVal + 48);
				tempVal -= divVal * i;
			}
			else if (dCount < 2)
			{
				printOutChar('0');
			}

			i /= 10;
			dCount -= 1;
			if (!dCount && i) printOutChar('.');
		}
	}
	else
	{
		printOutChar('0');
	}
}

int write16BitTiff(unsigned short* imageData, unsigned int width, unsigned int height, char* filePath)
{
	FILE* f;

	const unsigned char ofst = 0x86; //134=4+4+2+10*12+4

	unsigned char iwlo = (unsigned char)(width & 0xFF);
	unsigned char iwhi = (unsigned char)(width >> 8);
	unsigned char ihlo = (unsigned char)(height & 0xFF);
	unsigned char ihhi = (unsigned char)(height >> 8);

	unsigned int imagePixels = width * height;
	unsigned int imageBytes = imagePixels * 2;
	unsigned char iblo = (unsigned char)(imageBytes & 0xFF);
	unsigned char ibhi = (unsigned char)((imageBytes >> 8) & 0xFF);
	unsigned char ibvh = (unsigned char)(imageBytes >> 16);

	unsigned char file_header[] =
	{
		'I',  'I',    42,    0, // Little endian magic number
		0x08, 0x00, 0x00, 0x00, // IFD header at byte 8
		0x0A, 0x00,             // 10 IFD tags
		0xfe, 0x00, 0x04, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //TIFFTAG_SUBFILETYPE = 0
		0x00, 0x01, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00, iwlo, iwhi, 0x00, 0x00, //TIFFTAG_IMAGEWIDTH = *
		0x01, 0x01, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00, ihlo, ihhi, 0x00, 0x00, //TIFFTAG_IMAGELENGTH = *
		0x02, 0x01, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, //BITS_PER_SAMPLE = 16
		0x03, 0x01, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, //TIFFTAG_COMPRESSION = 1 (Non)
		0x06, 0x01, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, //TIFFTAG_PHOTOMETRIC = 1 (BlackIsZero)
		0x11, 0x01, 0x04, 0x00, 0x01, 0x00, 0x00, 0x00, ofst, 0x00, 0x00, 0x00, //TIFFTAG_STRIPOFFSETS (image data address)
		0x12, 0x01, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, //TIFFTAG_ORIENTATION = 1 (start top left)
		0x15, 0x01, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, //TIFFTAG_SAMPLESPERPIXEL = 1
		0x17, 0x01, 0x04, 0x00, 0x01, 0x00, 0x00, 0x00, iblo, ibhi, ibvh, 0x00, //TIFFTAG_STRIPBYTECOUNTS = 1
		0x00, 0x00, 0x00, 0x00
	};

	f = fopen(filePath, "wb");

	if (f > 0)
	{
		fwrite(file_header, 1, sizeof(file_header), f);
		fwrite(imageData, 2, imagePixels, f);

		fclose(f);
	}

	return (f > 0);
}

int main(int argc, char* argv[])
{
	int exitFlag = 0;
	char buff[1];
	int cbuff = 0;
	int nbuff = 0;
	int itemCount = 0;
	int port = 0;
	char* deviceName = "/dev/video0";
	char* filePath = "out.tif";
	imageBuffer* tmpib;
	const int  buffCount = 3;
	char* pEnd = 0;
	unsigned int buffSize;
	unsigned int camWidth = 640;
	unsigned int camHeight = 480;
	int maxNumb = 32;
	int maxMaxNumb = sizeof(maxima) / sizeof(struct pixel_list);
	char* commandStr = "";
	int i = 0;
	int j;
	int fnIncrPos = -1;
	int fnIncr = 0;

	while(i < argc)
	{
		if (*argv[i] == '-')
		{
			switch (*(argv[i] + 1))
			{
				case 'p':
					pEnd = 0;
					i++;
					if (*commandStr)
						printf("Option -p is skiped because -c is specified\n");
					else
						if (i < argc) port = strtol(argv[i], &pEnd, 10);
					break;

				case 'c':
					i++;

					if (port)
						printf("Option -c is skiped because -p is specified\n");
					else
						if (i < argc) commandStr = argv[i];
					break;

				case 'd':
					i++;
					if (i < argc)
						deviceName = argv[i];
					break;

				case 'f':
					i++;
					if (i < argc)
					{
						j = 0;
						filePath = argv[i];
						while (filePath[j] != 0)
						{
							if (filePath[j] == '#') fnIncrPos = j;
							j++;
						}
					}
					break;

				case 'r':
					i++;
					if (i < argc)
					{
						pEnd = 0;
						camWidth = (unsigned int)strtol(argv[i], &pEnd, 10);
						if (*pEnd != 0 && *(pEnd + 1) != 0)
							camHeight = strtol(pEnd + 1, &pEnd, 10);
					}
					break;

				case 'n':
					i++;
					if (i < argc)
						maxNumb = strtol(argv[i], &pEnd, 10);
					if (maxNumb > maxMaxNumb) maxNumb = maxMaxNumb;
					break;

				case 's':
					silent = 1;
					break;

				case '?':
				case 'h':
				case '-':
					printf("Usage: imagerec [-options]\n");
					printf("options:\n");
					printf("         -h                 show help\n");
					printf("         -c commands        ASCII command string\n");
					printf("         -d port            TCP/IP-port to listen on\n");
					printf("         -p device          capture device name\n");
					printf("         -r whidth*hight    image resolution\n");
					printf("         -n pixels          max number of pixels for l command\n");
					printf("         -f file            file path for w command (# for index)\n");
					printf("         -d                 return result data only\n");
					printf("\n");
					printf("examples: imagerec -d /dev/video0 -r 640*480 -p 5044\n");
					printf("          imagerec -d /dev/video0 -r 640*480 -c cgnexCngml -s\n");
					printf("          imagerec -d /dev/video0 -r 640*480 -c cgnexCnw -f result.tif\n");
					printf("          imagerec -d /dev/video0 -r 640*480 -c cgnwexsonwrCnw -f result#.tif\n");
					printf("\n");
					printf("single byte ASCII commands:\n");
					printf("          c     capture image\n");
					printf("          n     normalize image\n");
					printf("          g     apply gausian blur\n");
					printf("          e     edge detection with sobel filter, must be\n");
					printf("                folowd by 'x', 'o' or a hough transformation\n");
					printf("          x     remove non edge pixel, must be folowd by\n");
					printf("                'o' or a hough transformation\n");
					printf("          o     convert directional slope to absolute slope\n");
					printf("          C     circle hough transformation\n");
					printf("          L     line hough transformation\n");
					printf("          H     line hough transformation (horizontal only)\n");
					printf("          V     line hough transformation (vertical only)\n");
					printf("          M     miniscus hough transformation\n");
					printf("          b     binarize\n");
					printf("          m     remove non-local-maxima pixels\n");
					printf("          l     list brightes pixels (from max. 32 non black pixels)\n");
					printf("          p     list brightes pixel clusters (3x3)\n");
					printf("          q     close connection\n");
					printf("          s     store a copy of the current buffer\n");
					printf("          r     recall a copy of the stored buffer\n");
					printf("          w     write buffer to disk (TIF format)\n");
					printf("          z     set index for output file name to zero\n");
					printf("          d     show result data only\n");
					printf("          i     show info and result data (default)\n");
					printf("\n");
					printf("example: echo \"cngexCngmlq\" | nc localhost 5044\n");
					return 0;

				default:
					printf("Unknown option: -%c\n", *(argv[i] + 1));
					printf("Type imagerec -h for help\n");
					return 0;
			}

		}
		i++;
	}

	if (!silent)
	{
		if (port > 0)
			printf("Start up ImageRecTool, listen on tcp port %i\n", port);
		else
			printf("Start up ImageRecTool\n", port);
		printf("Type imagerec -h for help\n");
	}

	int fd = open((const char*)deviceName, O_RDWR);
	if (fd == -1)
	{
		perror("Opening video device failed");
		return 1;
	}

	struct v4l2_cropcap crcap = { 0 };

	if (configImageFormat(fd, &crcap, camWidth, camHeight))
		return 1;

	buffSize = (unsigned)setImageSize((int)crcap.bounds.width, (int)crcap.bounds.height);
	for (int i = 0; i < buffCount; i++)
	{
		buffers[i] = (imageBuffer*)malloc(buffSize * sizeof(imageBuffer));
	}

	if (initMmap(fd))
		return 1;

	if (port > 0)
	{
		if (!setupCmdTcpServer(port))
		{
			perror("Opening tcp port failed");
			return 1;
		}
		useTcpIp = 1;
	}
	
	int ret = 1;

	while (!exitFlag)
	{
		if (useTcpIp)
		{
			ret = waitForTcpData(buff, 1);
		}
		else
		{
			buff[0] = *commandStr;
			commandStr++;
		}

		if (ret)
		{
			cbuff = nbuff;

			switch (buff[0])
			{
				case 'c':
					if (captureImage(fd, buffers[cbuff], buffSize * sizeof(imageBuffer)))
						return 1;
					printOut("#image captured\n");
					break;
				case 'n':
					nbuff = !cbuff;
					normalizeImage(buffers[cbuff], buffers[nbuff]); //0.2 ms
					printOut("#image normalized\n");
					break;
				case 'g':
					gausFilter(buffers[cbuff], buffers[!cbuff]); // 14 ms
					printOut("#gaus filter applied\n");
					break;
				case 'e':
					nbuff = !cbuff;
					sobelFilter(buffers[cbuff], buffers[nbuff]); //14 ms
					printOut("#edges detection by sobel filter applied\n");
					break;
				case 'x':
					nbuff = !cbuff;
					nonMaximumSuppression(buffers[cbuff], buffers[nbuff], 128); //9 ms
					printOut("#non-edges removed\n");
					break;
				case 'M':
					nbuff = !cbuff;
					clearBuffer(buffers[nbuff]); //1.5 ms
					houghTransformMiniscus(buffers[cbuff], buffers[nbuff]); //36 ms with sqrt
					printOut("#miniscus hough transformed\n");
					break;
				case 'L':
					nbuff = !cbuff;
					clearBuffer(buffers[nbuff]); //1.5 ms
					houghTransformLines(buffers[cbuff], buffers[nbuff]);
					printOut("#line hough transformed\n");
					break;
				case 'H':
					nbuff = !cbuff;
					clearBuffer(buffers[nbuff]); //1.5 ms
					houghTransformHorizontalLines(buffers[cbuff], buffers[nbuff]);
					printOut("#horizontal lines hough transformed\n");
					break;
				case 'V':
					nbuff = !cbuff;
					clearBuffer(buffers[nbuff]); //1.5 ms
					houghTransformVerticalLines(buffers[cbuff], buffers[nbuff]);
					printOut("#vertical lines hough transformed\n");
					break;
				case 'C':
					nbuff = !cbuff;
					clearBuffer(buffers[nbuff]); //1.5 ms
					houghTransformCircles(buffers[cbuff], buffers[nbuff]);
					printOut("#circle hough transformed\n");
					break;
				case 'm':
					nbuff = !cbuff;
					findMaxima(buffers[cbuff], buffers[nbuff], 0x10000 / 5, 32);  // 0.5 ms
					printOut("#non-maxima pixels removed\n");
					break;
				case 'b':
					binarize(buffers[cbuff], buffers[cbuff], 0x10000 / 2); //0.2 ms
					printOut("#image normalized\n");
					break;
				case 'o':
					nbuff = !cbuff;
					convertToSlope(buffers[cbuff], buffers[nbuff]); //0.2 ms
					printOut("#converted to slope\n");
					break;
				case 'l':
					itemCount = getPixelList(buffers[cbuff], buffers[cbuff], maxima, maxNumb);
					printOut("#pixel list: (x y value)\n");
					for (int i = 0; i < itemCount; i++)
					{
						numberToString(maxima[i].x, 0);
						printOutChar(' ');
						numberToString(maxima[i].y, 0);
						printOutChar(' ');
						numberToString(maxima[i].value[4], 0);
						printOutChar('\n');
					}
					if (useTcpIp) sendTcpData("\n", 1);
					printOut("#end of list\n");
					break;
				case 'p':
					itemCount = getPixelList(buffers[cbuff], buffers[!cbuff], maxima, maxNumb);
					printOut("#pixel list: (x y value)\n");
					for (int i = 0; i < itemCount; i++)
					{
						numberToString(maxima[i].x, 0);
						printOutChar(' ');
						numberToString(maxima[i].y, 0);
						for (j = 0; j < 9; j++)
						{
							printOutChar(' ');
							numberToString(maxima[i].value[j], 0);
						}
						printOutChar('\n');
					}
					if (useTcpIp) sendTcpData("\n", 1);
					printOut("#end of list\n");
					break;
				case 'w':
					if (fnIncrPos > -1) filePath[fnIncrPos] = fnIncr + 48;
					if (write16BitTiff(buffers[cbuff], crcap.bounds.width, crcap.bounds.height, filePath))
						printOut("#image written\n");
					else
						printOut("#error open file\n");
					
					fnIncr++;
					if (fnIncr > 9) fnIncr = 0;
					break;
				case 's':
					//swap current buffer with storage buffer (buffer 2)
					tmpib = buffers[2];
					buffers[2] = buffers[nbuff];
					buffers[nbuff] = tmpib;

					nbuff = 2;
					printOut("#image data stored\n");
					break;
				case 'r':
					nbuff = 2;
					printOut("#image data recalled\n");
					break;
				case 'q':
					nbuff = 2;
					printOut("#close socket\n");
					closeTcpConnection();
					break;
				case 'Q':
					printOut("#quit application\n");
					exitFlag = 1;
					break;
				case 'z':
					fnIncr = 0;
					printOut("#reset index to zero\n");
					break;
				case 'i':
					tcpsilent = 0;
					printOut("#show info and results over tcp/ip\n");
					break;
				case 'd':
					tcpsilent = 1;
					printOut("#show only results over tcp/ip\n");
					break;
				case 0:
					//end of -c command parameter
					exitFlag = 1;
					break;
				default:
					if (buff[0] > 32)
						printOut("#unknown command\n");
			}
		}
	}

	if (useTcpIp)
	{
		closeTcpConnection();
		close(fd);
	}

	if (!silent) printf("quit program\n");
	return 0;
}