#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include "dissect-packet.h"
#include "error.h"


void test_aprs_packets()
{
	FILE *fh = fopen("test-files/aprs-packets.txt", "r");

	if (!fh)
		error_exit(true, "cannot open aprs packets file");

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
}

void test_ax25_packets(const std::string & path)
{
	int ok    = 0;
	int lines = 0;
	int valid = 0;

	DIR *d = opendir(path.c_str());

	if (!d)
		error_exit(true, "Cannot open path %s", path.c_str());

	for(;;) {
		struct dirent *de = readdir(d);

		if (!de)
			break;

		if (strstr(de->d_name, ".dat") == nullptr)
			continue;

		std::string file = path + "/" + de->d_name;

		uint8_t buffer[4096] { 0 };

		FILE *fh = fopen(file.c_str(), "rb");
		size_t n = fread(buffer, 1, sizeof buffer, fh);
		fclose(fh);

		auto rc = dissect_packet(buffer, n);

		ok += rc.has_value();

		lines++;

		if (rc.has_value()) {
			valid += rc.value().second->get_valid();

			delete rc.value().second;
		}
	}

	closedir(d);

	printf("%d/%d/%d\n", ok, lines, valid);
}

int main(int argc, char *argv[])
{
	const std::string path = "test-files/TNC_Test_CD_Ver-1.1-decoded-packets/";

	test_ax25_packets(path + "1");
	test_ax25_packets(path + "2");
	test_ax25_packets(path + "3");
	test_ax25_packets(path + "4");

	test_aprs_packets();

	return 0;
}
