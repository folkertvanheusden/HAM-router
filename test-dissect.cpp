#include <stdio.h>
#include <string.h>

#include "dissect-packet.h"
#include "error.h"


int main(int argc, char *argv[])
{
	FILE *fh = fopen(argv[1], "r");

	if (!fh)
		error_exit(true, "cannot open %s", argv[1]);

	constexpr int buffer_size = 4096;

	int ok    = 0;
	int lines = 0;

	while(!feof(fh))
	{
		char *buffer = reinterpret_cast<char *>(calloc(1, buffer_size));

		buffer[0] = '<';
		buffer[1] = char(0xff);
		buffer[2] = 0x01;

		if (fgets(&buffer[3], buffer_size - 3, fh) == NULL) {
			free(buffer);
			break;
		}

		auto rc = dissect_packet(reinterpret_cast<uint8_t *>(buffer), strlen(buffer));
		ok += rc.has_value();

		lines++;

		free(buffer);
	}

	fclose(fh);

	printf("%d/%d\n", ok, lines);

	return 0;
}
