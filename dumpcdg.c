#include <stdio.h>

FILE *f;
typedef struct 
{
	unsigned char command;
	unsigned char instruction;
	unsigned char parityQ[2];
	unsigned char data[16];
	unsigned char parityP[4];
} SubCode;

int main(int argc, char **argv)
{
	f = fopen(argv[1], "rb");
	char s[24];
	int size;
	while (!feof(f))
	{
		if ((size = fread(s, sizeof(char), 24, f)) == sizeof(s))
		{
			for (int i = 0; i < sizeof(s); i++)
				printf("%02X ", (int)s[i]);
			printf("\n\n");
		}
		else if (size != 0)
			fprintf(stderr, "Error reading file, not enough bytes %d\n", size);
	}
	fclose(f);
}

