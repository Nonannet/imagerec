#include <stdio.h>
#include "imageRec.h"

const short sobelFilterKernel[3][3] = { { 3, 10, 3 }, { 0, 0, 0 }, { -3, -10, -3 } };
const imageBuffer gausFilterKernel[5] = { 25, 61, 83, 61, 25 };

int imageWidth = 0;
int imageHight = 0;
int imageBufferSize = 0;

int setImageSize(int width, int hight)
{
	imageWidth = width;
	imageHight = hight;
	imageBufferSize = width * hight;

	return imageBufferSize;
}

void normalizeImage(imageBuffer* srcImage, imageBuffer* dstImage)
{
	int min = 0xFFFF;
	int max = 0;
	unsigned int factor;

	for (int i = 0; i < imageBufferSize; i++)
	{
		if (srcImage[i] < min) min = srcImage[i];
		if (srcImage[i] > max) max = srcImage[i];
	}

	factor = (1 << 30) / (max - min + 1);

	for (int i = 0; i < imageBufferSize; i++)
	{
		dstImage[i] = (imageBuffer)((srcImage[i] - min) * factor / (1<<14));
	}
}

void gausFilter(imageBuffer * image, imageBuffer * helperImage)
{
	int n = sizeof(gausFilterKernel) / sizeof(gausFilterKernel[0]);
	int acc;
	int acck = 0;
	int i = 0;

	for (int y = 0; y < imageHight; y++)
	{
		for (int x = 0; x < imageWidth; x++)
		{
			acc = 0;
			acck = 0;

			for (int j = 0; j < n; j++)
			{
				int pos = x + j - n / 2;

				if (pos >= 0 && pos < imageWidth)
				{
					acc += image[i + j - n / 2] * gausFilterKernel[j];
					acck += gausFilterKernel[j];
				}
			}

			helperImage[i] = (unsigned short)(acc / acck);

			i++;
		}
	}

	i = 0;

	for (int y = 0; y < imageHight; y++)
	{
		for (int x = 0; x < imageWidth; x++)
		{
			acc = 0;
			acck = 0;

			for (int j = 0; j < n; j++)
			{
				int pos = y + j - n / 2;

				if (pos >= 0 && pos < imageHight)
				{
					acc += helperImage[i + (j - n / 2) * imageWidth] * gausFilterKernel[j];
					acck += gausFilterKernel[j];
				}
			}

			image[i] = (unsigned short)(acc / acck);

			i++;
		}
	}
}

int getX(imageBuffer combinedXy)
{
	return (combinedXy & 0xFF) - 0x80;
}

int getY(imageBuffer combinedXy)
{
	return (combinedXy / 0x100) - 0x80;
}

void sobelFilter(imageBuffer * srcImage, imageBuffer * dstImage)
{
	int i = 0;

	for (int y = 0; y < imageHight; y++)
	{
		for (int x = 0; x < imageWidth; x++)
		{
			int dx = 0;
			int dy = 0;

			if (x > 0 && x < imageWidth - 1 && y > 0 && y < imageHight - 1)
			{
				for (int ix = 0; ix < 3; ix++)
				{
					for (int iy = 0; iy < 3; iy++)
					{
						int srcVal = srcImage[(x + ix - 1) + (y + iy - 1) * imageWidth];

						dx += srcVal * sobelFilterKernel[ix][iy];
						dy += srcVal * sobelFilterKernel[iy][ix];
					}
				}

				dx /= 0x800;
				dy /= 0x800;

				if (dx > 127) dx = 127;
				if (dx < -128) dx = -128;
				if (dy > 127) dy = 127;
				if (dy < -128) dy = -128;

				//dstImage[i] = combineXy(dx,dy);
				dstImage[i] = (unsigned short)(dx + 0x80 + (dy + 0x80) * 0x100);
			}
			else
			{
				dstImage[i] = 0x8080;
			}

			i++;
		}
	}
}

int getSlope(imageBuffer value)
{
	int dy = (value / 0x100) - 0x80;
	int dx = (value & 0xFF) - 0x80;

	return (dx * dx + dy * dy);
}

//srcImage must be a result from sobelFilter
void nonMaximumSuppression(imageBuffer * srcImage, imageBuffer * dstImage, int minSlope)
{
	int i = 0;
	int slope;

	for (int y = 0; y < imageHight; y++)
	{
		for (int x = 0; x < imageWidth; x++)
		{

			if (x > 0 && x < imageWidth - 1 && y > 0 && y < imageHight - 1)
			{
				slope = getSlope(srcImage[i]);

				if (slope < minSlope)
				{
					dstImage[i] = 0x8080;
				}
				else
				{
					int count = 0;

					if (getSlope(srcImage[i - 1 - imageWidth]) > slope) count++;
					if (getSlope(srcImage[i - 0 - imageWidth]) > slope) count++;
					if (getSlope(srcImage[i + 1 - imageWidth]) > slope) count++;
					if (getSlope(srcImage[i - 1]) > slope) count++;
					if (getSlope(srcImage[i + 1]) > slope) count++;
					if (getSlope(srcImage[i - 1 + imageWidth]) > slope) count++;
					if (getSlope(srcImage[i - 0 + imageWidth]) > slope) count++;
					if (getSlope(srcImage[i + 1 + imageWidth]) > slope) count++;

					if (count > 2)
						dstImage[i] = 0x8080;
					else
						dstImage[i] = srcImage[i];
				}
			}
			else
			{
				dstImage[i] = 0x8080;
			}

			i++;
		}
	}
}


/*void drawLine(imageBuffer* dstImage, int x0, int y0, int x1, int y1)
{
	int x = x0;
	int y = y0;
	int dx = x1 - x0;
	int dy = y1 - y0;
	int err = dx + dy, e2; // error value e_xy

	while (true)
	{
		dstImage[y * IMAGE_WIDTH + x] += 1;// 64 + cr;
		if (x == x1 && y == y1) break;
		e2 = 2 * err;
		if (e2 > dy) { err += dy; x++; }
		if (e2 < dx) { err += dx; y++; }
	}
}*/

void houghTransformCircles(imageBuffer * srcImage, imageBuffer * dstImage)
{
	int i = 0;
	int xslope, yslope;

	for (int y = 0; y < imageHight; y++)
	{
		for (int x = 0; x < imageWidth; x++)
		{
			if (srcImage[i] != 0x8080)
			{
				xslope = getX(srcImage[i]);
				yslope = getY(srcImage[i]);

				if (xslope * xslope > yslope * yslope)
				{
					for (int xi = 0; xi < imageWidth; xi++)
					{
						int yi = (xi - x) * yslope / xslope + y;

						if (yi > 0 && yi < imageHight)
							dstImage[yi * imageWidth + xi]++;
					}
				}
				else
				{
					for (int yi = 0; yi < imageHight; yi++)
					{
						int xi = (yi - y) * xslope / yslope + x;

						if (xi > 0 && xi < imageWidth)
							dstImage[yi * imageWidth + xi]++;
					}
				}
			}

			i++;
		}
	}

}

void intenerlal_houghTransformLines(imageBuffer * srcImage, imageBuffer * dstImage, int filter)
{
	int i = 0;
	const int cx = imageWidth / 2;
	const int cy = imageHight / 2;

	for (int y = 0; y < imageHight; y++)
	{
		for (int x = 0; x < imageWidth; x++)
		{
			if (srcImage[i] != 0x8080)
			{
				int xslope = getX(srcImage[i]);
				int yslope = getY(srcImage[i]);
				int xslope2 = xslope * xslope;
				int yslope2 = yslope * yslope;

				if (filter == 0 || (filter == 1 && yslope2 > xslope2) || (filter == 2 && yslope2 < xslope2))
				{

					int inter = ((x - cx) * xslope + (y - cy) * yslope) * 0x100 / (xslope2 + yslope2);
					int htx = cx + inter * xslope / 0x100;
					int hty = cy + inter * yslope / 0x100;

					if (htx > 0 && htx < imageWidth && hty > 0 && hty < imageHight && dstImage[hty * imageWidth + htx] < 0xFFFF)
						dstImage[hty * imageWidth + htx] ++;

					//std::cout << htx << " " << hty << " - " << xslope << " " << yslope << " " << ".\n";
					//printf("%i %i %i %i\n", htx, hty, xslope, yslope);
				}

			}

			i++;
		}
	}
}

void houghTransformLines(imageBuffer* srcImage, imageBuffer* dstImage)
{
	intenerlal_houghTransformLines(srcImage, dstImage, 0);
}

void houghTransformVerticalLines(imageBuffer* srcImage, imageBuffer* dstImage)
{
	intenerlal_houghTransformLines(srcImage, dstImage, 2);
}

void houghTransformHorizontalLines(imageBuffer* srcImage, imageBuffer* dstImage)
{
	intenerlal_houghTransformLines(srcImage, dstImage, 1);
}

static int usqrt4(int val) {
	int a, b;

	a = 256;   // starting point is relatively unimportant

	b = val / a; a = (a + b) / 2;
	b = val / a; a = (a + b) / 2;
	b = val / a; a = (a + b) / 2;
	b = val / a; a = (a + b) / 2;
	b = val / a; a = (a + b) / 2;

	return a;
}

void houghTransformMiniscus(imageBuffer * srcImage, imageBuffer * dstImage)
{
	int i = 0;

	for (int y = 0; y < imageHight; y++)
	{
		for (int x = 0; x < imageWidth; x++)
		{
			if (srcImage[i] != 0)
			{
				int xslope = getX(srcImage[i]);
				int yslope = getY(srcImage[i]);

				if (xslope != 0)
				{
					int preCalc = 0x1000 * yslope / xslope;


					for (int htx = 0; htx < imageWidth; htx++)
					{
						int dx = htx - x;
						//int dy = dx * yslope / xslope;
						int dy = dx * preCalc / 0x1000;

						if (dx != 0)
						{
							int r = usqrt4(dx * dx + dy * dy);
							//int r = sqrt(dx * dx + dy * dy);

							int hty = y + dy - r;

							if (hty >= 0 && hty < imageHight)
								dstImage[hty * imageWidth + htx] ++;
						}

					}

				}
			}

			i++;
		}
	}
}

void findMaxima(imageBuffer* srcImage, imageBuffer* dstImage, int threshold, int minDistance)
{
	int ws2 = minDistance / 2;
	int maxVal;
	int maxInd;

	for (int i = 0; i < imageBufferSize; i++)
	{
		dstImage[i] = srcImage[i];
	}

	for (int y = ws2; y < imageHight; y += ws2)
	{
		for (int x = ws2; x < imageWidth; x += ws2)
		{
			maxVal = threshold;
			maxInd = -1;

			for (int ix = -ws2; ix < ws2; ix++)
			{
				for (int iy = -ws2; iy < ws2; iy++)
				{
					int i = (x + ix) + (y + iy) * imageWidth;

					if (srcImage[i] > maxVal)
					{
						maxInd = i;
						maxVal = srcImage[i];
					}
				}
			}

			for (int ix = -ws2; ix < ws2; ix++)
			{
				for (int iy = -ws2; iy < ws2; iy++)
				{
					int i = (x + ix) + (y + iy) * imageWidth;
					if (i != maxInd)
						dstImage[i] = 0;
				}
			}


		}
	}
}

void bubbleSort(struct pixel_list* array, int length)
{
	int i, j;
	struct pixel_list tmp;

	for (i = 1; i < length; i++)
	{
		for (j = 0; j < length - i; j++)
		{
			if (array[j].value[4] < array[j + 1].value[4])
			{
				tmp = array[j];
				array[j] = array[j + 1];
				array[j + 1] = tmp;
			}
		}
	}
}

int getPixelList(imageBuffer* primImage, imageBuffer* secImage, struct pixel_list* list, int listLenght)
{
	int i = 0;
	int j = 0;

	for (int y = 1; y < imageHight - 1; y++)
	{
		for (int x = 1; x < imageWidth - 1; x++)
		{
			i = x + y * imageWidth;

			if (primImage[i] != 0 && j < listLenght)
			{
				list[j].value[0] = secImage[x + (y - 1) * imageWidth - 1];
				list[j].value[1] = secImage[x + (y - 1) * imageWidth];
				list[j].value[2] = secImage[x + (y - 1) * imageWidth + 1];

				list[j].value[3] = secImage[i - 1];
				list[j].value[4] = secImage[i];
				list[j].value[5] = secImage[i + 1];

				list[j].value[6] = secImage[x + (y + 1) * imageWidth - 1];
				list[j].value[7] = secImage[x + (y + 1) * imageWidth];
				list[j].value[8] = secImage[x + (y + 1) * imageWidth + 1];

				list[j].x = x;
				list[j].y = y;
				j++;
			}
			
			i++;
		}
	}

	bubbleSort(list, listLenght);

	return j;
}

void convertToSlope(imageBuffer* srcImage, imageBuffer* dstImage)
{
	for (int i = 0; i < imageBufferSize; i++)
	{
		dstImage[i] = (unsigned short)getSlope(srcImage[i]);
	}
}

void binarize(imageBuffer * srcImage, imageBuffer * dstImage, int threshold)
{
	for (int i = 0; i < imageBufferSize; i++)
	{
		dstImage[i] = (srcImage[i] > threshold) ? 0xFFFF : 0;
	}
}

void clearBufferArea(imageBuffer * dstImage, int startLine, int endLine)
{
	for (int i = 0; i < imageBufferSize; i++)
	{
		if (i / imageWidth >= startLine && i / imageWidth < endLine)
			dstImage[i] = 0;
	}
}

void clearBuffer(imageBuffer* dstImage)
{
	clearBufferArea(dstImage, 0, imageHight);
}
