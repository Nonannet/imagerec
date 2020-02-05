
#define RADIUS_VAL 500 

typedef unsigned short imageBuffer;
typedef int bool;

struct pixel_list {
	imageBuffer value[9];
	int x;
	int y;
};

int setImageSize(int width, int hight);

void normalizeImage(imageBuffer* srcImage, imageBuffer* dstImage);

void gausFilter(imageBuffer* image, imageBuffer* helperImage);

void sobelFilter(imageBuffer* srcImage, imageBuffer* dstImage);

void nonMaximumSuppression(imageBuffer* srcImage, imageBuffer* dstImage, int minSlope);

void houghTransformCircles(imageBuffer* srcImage, imageBuffer* dstImage);

void houghTransformLines(imageBuffer* srcImage, imageBuffer* dstImage);

void houghTransformVerticalLines(imageBuffer* srcImage, imageBuffer* dstImage);

void houghTransformHorizontalLines(imageBuffer* srcImage, imageBuffer* dstImage);

void houghTransformMiniscus(imageBuffer* srcImage, imageBuffer* dstImage);

void findMaxima(imageBuffer* srcImage, imageBuffer* dstImage, int threshold, int minDistance);

int getPixelList(imageBuffer* primImage, imageBuffer* secImage, struct pixel_list* list, int listLenght);

void convertToSlope(imageBuffer* srcImage, imageBuffer* dstImage);

void binarize(imageBuffer* srcImage, imageBuffer* dstImage, int threshold);

void clearBufferArea(imageBuffer* dstImage, int startLine, int endLine);

void clearBuffer(imageBuffer* dstImage);