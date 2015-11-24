#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h> 
#include <fcntl.h>

typedef struct
{
    uint32_t startAddress;
    uint32_t endAddress;
    uint32_t crc;
    uint32_t appVersionAddress;
} __attribute__ ((aligned (256))) AppDescriptionBlock;

unsigned int	crc32(unsigned int start, unsigned char *data, unsigned int count)
{
	int		crc;
	unsigned int	byte, c;
	const unsigned int g0 = 0xEDB88320, g1 = g0 >> 1,
			g2 = g0 >> 2,	g3 = g0 >> 3, g4 = g0 >> 4, g5 = g0 >> 5,
			g6 = (g0 >> 6) ^ g0, g7 = ((g0 >> 6) ^ g0) >> 1;

	crc = start;
	while (count--) {
		byte = *data++;
		//Get next byte.
			crc = crc ^ byte;
		c = ((crc << 31 >> 31) & g7) ^ ((crc << 30 >> 31) & g6) ^
			((crc << 29 >> 31) & g5) ^ ((crc << 28 >> 31) & g4) ^
			((crc << 27 >> 31) & g3) ^ ((crc << 26 >> 31) & g2) ^
			((crc << 25 >> 31) & g1) ^ ((crc << 24 >> 31) & g0);
		crc = ((unsigned)crc >> 8) ^ c;
	}
	return ~crc;
}

int fileLength(int fh)
{
	struct stat fileStat;
    	if(fstat(fh, &fileStat) < 0)     
        	return -1;
	return fileStat.st_size;
}

int getOptInt(char *optarg)
{
	int val=0;
	if(optarg[0] == '0' && optarg[1] == 'x')
		sscanf (optarg + 2, "%x", &val);
	else
		val=atoi(optarg);

	return val;
}

int		main       (int argc, char **argv)
{
	int		size = 65536;
	char           *cvalue = NULL;
	int		index;
	int		c;
	int		bootdescriptor_start = 0x1e00;
	int		application_start = 0x2000;
	int		application_version = 0x0000;
	char		bootloader_literal[] = "bootloader.bin";
	char           *bootloader_fname = bootloader_literal;

	char		application_literal[] = "application.bin";
	char           *application_fname = application_literal;

	char		image_literal[] = "image.bin";
	char           *image_fname = image_literal;

	opterr = 0;
	while ((c = getopt(argc, argv, "s:b:B:a:A:V:")) != -1)
		switch (c) {
		case 's':
			size = getOptInt(optarg);
			break;
		case 'b':
			bootloader_fname = optarg;
			break;
		case 'B':
			bootdescriptor_start = getOptInt(optarg);
			break;
		case 'a':
			application_fname = optarg;
			break;
		case 'A':
			application_start = getOptInt(optarg);
			break;
		case 'V':
			application_version = getOptInt(optarg);
			break;
		case '?':
			fprintf(stderr, "mkimage -a <application filename> -A <application start> -b <bootloader filename> -B <boot descriptor start> -V <version offset> -s <size> <image filename>\n");
			return 1;
		default:
			abort();
		}
		
	if (optind < argc)
		image_fname = argv[optind];


	printf("Parameters:\n\n");
	printf("Flash Size           : %d/0x%04x\n", size, size);
	printf("Boot Loader          : %s\n", bootloader_fname);
	printf("Boot Descriptor Start: %d/0x%04x\n", bootdescriptor_start, bootdescriptor_start);
	printf("Application          : %s\n", application_fname);
	printf("Application Start    : %d/0x%04x\n", application_start, application_start);
	printf("Version Offset       : %d/0x%04x\n", application_version, application_version);
	printf("Image                : %s\n", image_fname);
	
	unsigned char  *image = (unsigned char *)calloc(size, 1);
	int fhApplication = open(application_fname, O_RDONLY);
	int fhBootloader = open(bootloader_fname, O_RDONLY);
	int fhImage = open(image_fname, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);

	if (fhApplication > 0 && fhBootloader > 0 && fhImage > 0) {
		AppDescriptionBlock *adb = (AppDescriptionBlock *) (image+bootdescriptor_start);
		int length = fileLength(fhBootloader);
		printf("Read %d bytes...\n", length);
		if(read(fhBootloader, image, length) != length) {
			fprintf(stderr, "Cannot read boot loader.\n");
		}
		length = fileLength(fhApplication);
		printf("Read %d bytes...\n", length);
		if(read(fhApplication, image + application_start, length) != length) {
			fprintf(stderr, "Cannot read application.\n");
		}
		adb->startAddress = application_start;
		adb->endAddress = application_start + length;
		adb->crc = crc32(0xFFFFFFFF, image + application_start, length);
		adb->appVersionAddress = application_version;
		printf("CRC32                : 0x%08x\n\n", adb->crc);
		write(fhImage, image, size);
	}
	
	if (fhImage > 0)
		close(fhImage);
	else
		fprintf(stderr, "%s not found\n", image_fname);
	if (fhBootloader > 0)
		close(fhBootloader);
	else
		fprintf(stderr, "%s not found\n", bootloader_fname);
	if (fhApplication > 0)
		close(fhApplication);
	else
		fprintf(stderr, "%s not found\n", application_fname);
	free(image);
	return 0;
}
