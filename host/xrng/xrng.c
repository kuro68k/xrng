// xrng.c

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <conio.h>
#include <time.h>
#include "libusb.h"


#define BUFFER_SIZE			8192
#define	DEFAULT_TIMEOUT_MS	100
#define	EP_BULK_IN			0x81
#define	EP_BULK_OUT			0x02


int main(int argc, char* argv[])
{
	uint16_t vid = 0x8282;
	uint16_t pid = 0x9999;

	int res;
	libusb_context *context = NULL;
	struct libusb_device_handle *dev;
	struct libusb_device_descriptor desc;

	if ((res = libusb_init(&context)) < 0)
		return res;
	
	//libusb_set_debug(context, 3);	// verbosity level 3

	dev = libusb_open_device_with_vid_pid(context, vid, pid);
	if (dev == NULL)
	{
		fprintf(stderr, "libusb: could not open %04X:%04X.", vid, pid);
		res = -1;
		goto exit;
	}

	if ((res = libusb_get_device_descriptor(libusb_get_device(dev), &desc)) < 0)
		goto exit;

	char temp[64];
	res = libusb_get_string_descriptor_ascii(dev, desc.iManufacturer, temp, sizeof(temp));
	printf("Manufacturer:\t%s\n", temp);
	res = libusb_get_string_descriptor_ascii(dev, desc.iProduct, temp, sizeof(temp));
	printf("Product:\t%s\n", temp);

	if ((res = libusb_claim_interface(dev, 0)) < 0)
	{
		fprintf(stderr, "libusb: unable to claim interface 0 (%s)", libusb_strerror(res));
		goto exit;
	}

	int rx;
	uint8_t buffer[BUFFER_SIZE];
	ULONG bytes = 0;
	ULONG bytes_per_second = 0;
	time_t tick = clock() + CLOCKS_PER_SEC;
	int discard = 1;
	FILE *fp = fopen("rng.dat", "w");

	while (!_kbhit())
	{
		res = libusb_bulk_transfer(dev, EP_BULK_IN, buffer, BUFFER_SIZE, &rx, DEFAULT_TIMEOUT_MS);
		if (res < 0)
		{
			fprintf(stderr, "libusb_bulk_transfer failed (%d).\n", res);
			fprintf(stderr, "%s\n", libusb_strerror(res));
			goto exit;
		}
		if (rx != BUFFER_SIZE)
		{
			fprintf(stderr, "libusb_bulk_transfer wrong packet size (%d).\n", rx);
			res = -1;
			goto exit;
		}

		if (discard > 0)
			discard--;
		else
		{
			bytes += rx;
			bytes_per_second += rx;
			fwrite(buffer, sizeof(buffer), 1, fp);

			time_t now = clock();
			if (now >= tick)
			{
				tick = now + CLOCKS_PER_SEC;
				double megabits = ((double)bytes_per_second * 8) / (1024 * 1024);
				double megabytes = (double)bytes / (1024 * 1024);
				printf("%.03lf MB\t%.03lf Mb/s\n", megabytes, megabits);
				bytes_per_second = 0;
			}
		}
	}

	fclose(fp);
	libusb_release_interface(dev, 0);

	res = 0;
exit:
	if (dev != NULL)
		libusb_close(dev);
	libusb_exit(context);

	return res;
}

