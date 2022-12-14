/* simple password generator by Nelson Minar (minar@reed.edu)
 * copyright 1991, all rights reserved.
 * You can use this code as long as my name stays with it.
 *
 * md5 patch by W. Campbell <wcampbel@botbay.net>
 * Modernization, getopt, etc for the Hybrid IRCD team
 * by W. Campbell
 *
 * /dev/random for salt generation added by
 * Aaron Sethman <androsyn@ratbox.org>
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include "rb_lib.h"
#include <pwd.h>

#define FLAG_MD5      0x00000001
#define FLAG_SALT     0x00000002
#define FLAG_PASS     0x00000004
#define FLAG_LENGTH   0x00000008
#define FLAG_BLOWFISH 0x00000010
#define FLAG_ROUNDS   0x00000020
#define FLAG_SHA256   0x00000040
#define FLAG_SHA512   0x00000080


static char *make_md5_salt(int);
static char *make_md5_salt_para(char *);
static char *make_sha256_salt(int);
static char *make_sha256_salt_para(char *);
static char *make_sha512_salt(int);
static char *make_sha512_salt_para(char *);
static char *make_bf_salt(int, int);
static char *make_bf_salt_para(int, char *);
static char *generate_random_salt(char *, int);
static char *generate_poor_salt(char *, int);

static void full_usage(void) __attribute__((noreturn));
static void brief_usage(void) __attribute__((noreturn));

static char saltChars[] = "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
       /* 0 .. 63, ascii - 64 */

int
main(int argc, char *argv[])
{
	char *plaintext = NULL;
	int c;
	char *saltpara = NULL;
	char *salt;
	char *hashed, *hashed2;
	int flag = 0;
	int length = 0;		/* Not Set */
	int rounds = 0;		/* Not set, since blowfish needs 4 by default, a side effect
				 * of this being the encryption type parameter must be
				 * specified before the rounds  parameter.
				 */

	while((c = getopt(argc, argv, "xymbr:h?l:s:p:")) != -1)
	{
		switch (c)
		{
		case 'm':
			flag |= FLAG_MD5;
			break;
		case 'b':
			flag |= FLAG_BLOWFISH;
			rounds = 4;
			break;
		case 'l':
			flag |= FLAG_LENGTH;
			length = atoi(optarg);
			break;
		case 'r':
			flag |= FLAG_ROUNDS;
			rounds = atoi(optarg);
			break;
		case 's':
			flag |= FLAG_SALT;
			saltpara = optarg;
			break;
		case 'p':
			flag |= FLAG_PASS;
			plaintext = optarg;
			break;
		case 'x':
			flag |= FLAG_SHA256;
			break;
		case 'y':
			flag |= FLAG_SHA512;
			break;
		case 'h':
			full_usage();
			/* NOT REACHED */
			break;
		case '?':
			brief_usage();
			/* NOT REACHED */
			break;
		default:
			printf("Invalid Option: -%c\n", c);
			break;
		}
	}

	if(flag & FLAG_MD5)
	{
		if(length == 0)
			length = 8;
		if(flag & FLAG_SALT)
			salt = make_md5_salt_para(saltpara);
		else
			salt = make_md5_salt(length);
	}
	else if(flag & FLAG_BLOWFISH)
	{
		if(length == 0)
			length = 22;
		if(flag & FLAG_SALT)
			salt = make_bf_salt_para(rounds, saltpara);
		else
			salt = make_bf_salt(rounds, length);
	}
	else if(flag & FLAG_SHA256)
	{
		if(length == 0)
			length = 16;
		if(flag & FLAG_SALT)
			salt = make_sha256_salt_para(saltpara);
		else
			salt = make_sha256_salt(length);
	}
	else
	{
		if(length == 0)
			length = 16;
		if(flag & FLAG_SALT)
			salt = make_sha512_salt_para(saltpara);
		else
			salt = make_sha512_salt(length);
	}

	if(flag & FLAG_PASS)
	{
		if(!plaintext)
		{
			fprintf(stderr, "Please enter a valid password\n");
			return 1;
		}

		hashed = rb_crypt(plaintext, salt);
	}
	else
	{
		plaintext = getpass("plaintext: ");
		hashed = rb_crypt(plaintext, salt);
		if (!hashed)
		{
			fprintf(stderr, "rb_crypt() failed\n");
			return 1;
		}
		hashed = strdup(hashed);

		plaintext = getpass("again: ");
		hashed2 = rb_crypt(plaintext, salt);
		if (!hashed2)
		{
			fprintf(stderr, "rb_crypt() failed\n");
			return 1;
		}

		if (strcmp(hashed, hashed2) != 0)
		{
			fprintf(stderr, "Passwords do not match\n");
			return 1;
		}
	}

	printf("%s\n", hashed);
	return 0;
}

char *
make_md5_salt_para(char *saltpara)
{
	static char salt[21];
	if(saltpara && (strlen(saltpara) <= 16))
	{
		/* sprintf used because of portability requirements, the length
		 ** is checked above, so it should not be too much of a concern
		 */
		sprintf(salt, "$1$%s$", saltpara);
		return salt;
	}
	printf("Invalid Salt, please use up to 16 random alphanumeric characters\n");
	exit(1);

	/* NOT REACHED */
	return NULL;
}

char *
make_md5_salt(int length)
{
	static char salt[21];
	if(length > 16)
	{
		printf("MD5 salt length too long\n");
		exit(0);
	}
	salt[0] = '$';
	salt[1] = '1';
	salt[2] = '$';
	generate_random_salt(&salt[3], length);
	salt[length + 3] = '$';
	salt[length + 4] = '\0';
	return salt;
}

char *
make_sha256_salt_para(char *saltpara)
{
	static char salt[21];
	if(saltpara && (strlen(saltpara) <= 16))
	{
		/* sprintf used because of portability requirements, the length
		 ** is checked above, so it should not be too much of a concern
		 */
		sprintf(salt, "$5$%s$", saltpara);
		return salt;
	}
	printf("Invalid Salt, please use up to 16 random alphanumeric characters\n");
	exit(1);

	/* NOT REACHED */
	return NULL;
}

char *
make_sha512_salt_para(char *saltpara)
{
	static char salt[21];
	if(saltpara && (strlen(saltpara) <= 16))
	{
		/* sprintf used because of portability requirements, the length
		 ** is checked above, so it should not be too much of a concern
		 */
		sprintf(salt, "$6$%s$", saltpara);
		return salt;
	}
	printf("Invalid Salt, please use up to 16 random alphanumeric characters\n");
	exit(1);

	/* NOT REACHED */
	return NULL;
}


char *
make_sha256_salt(int length)
{
	static char salt[21];
	if(length > 16)
	{
		printf("SHA256 salt length too long\n");
		exit(0);
	}
	salt[0] = '$';
	salt[1] = '5';
	salt[2] = '$';
	generate_random_salt(&salt[3], length);
	salt[length + 3] = '$';
	salt[length + 4] = '\0';
	return salt;
}

char *
make_sha512_salt(int length)
{
	static char salt[21];
	if(length > 16)
	{
		printf("SHA512 salt length too long\n");
		exit(0);
	}
	salt[0] = '$';
	salt[1] = '6';
	salt[2] = '$';
	generate_random_salt(&salt[3], length);
	salt[length + 3] = '$';
	salt[length + 4] = '\0';
	return salt;
}

char *
make_bf_salt_para(int rounds, char *saltpara)
{
	static char salt[31];
	char tbuf[3];
	if(saltpara && (strlen(saltpara) <= 22))
	{
		/* sprintf used because of portability requirements, the length
		 ** is checked above, so it should not be too much of a concern
		 */
		sprintf(tbuf, "%02d", rounds);
		sprintf(salt, "$2a$%s$%s$", tbuf, saltpara);
		return salt;
	}
	printf("Invalid Salt, please use up to 22 random alphanumeric characters\n");
	exit(1);

	/* NOT REACHED */
	return NULL;
}

char *
make_bf_salt(int rounds, int length)
{
	static char salt[31];
	char tbuf[3];
	if(length > 22)
	{
		printf("Blowfish salt length too long\n");
		exit(0);
	}
	sprintf(tbuf, "%02d", rounds);
	sprintf(salt, "$2a$%s$", tbuf);
	generate_random_salt(&salt[7], length);
	salt[length + 7] = '$';
	salt[length + 8] = '\0';
	return salt;
}

char *
generate_poor_salt(char *salt, int length)
{
	int i;
	srand(time(NULL));
	for(i = 0; i < length; i++)
	{
		salt[i] = saltChars[rand() % 64];
	}
	return (salt);
}

char *
generate_random_salt(char *salt, int length)
{
	char *buf;
	int fd, i;
	if((fd = open("/dev/random", O_RDONLY)) < 0)
	{
		return (generate_poor_salt(salt, length));
	}
	buf = calloc(1, length);
	if(read(fd, buf, length) != length)
	{
		free(buf);
		return (generate_poor_salt(salt, length));
	}

	for(i = 0; i < length; i++)
	{
		salt[i] = saltChars[abs(buf[i]) % 64];
	}
	free(buf);
	return (salt);
}

void
full_usage()
{
	printf("mkpasswd [-m|-b|-x|-y] [-l saltlength] [-r rounds] [-s salt] [-p plaintext]\n");
	printf("-x Generate a SHA256 password\n");
	printf("-y Generate a SHA512 password\n");
	printf("-m Generate an MD5 password\n");
	printf("-b Generate a Blowfish password\n");
	printf("-l Specify a length for a random MD5 or Blowfish salt\n");
	printf("-r Specify a number of rounds for a Blowfish password\n");
	printf("   Default 4, no more than 6 recommended\n");
	printf("-s Specify a salt, up to 16 for MD5, SHA256, and SHA512\n");
	printf("   up to 22 for Blowfish\n");
	printf("-p Specify a plaintext password to use\n");
	printf("Example: mkpasswd -m -s 3dr -p test\n");
	exit(0);
}

void
brief_usage()
{
	printf("mkpasswd - password hash generator\n");
	printf("  SHA512:  mkpasswd [-y] [-l saltlength] [-s salt] [-p plaintext]\n");
	printf("  SHA256:  mkpasswd -x [-l saltlength] [-s salt] [-p plaintext]\n");
	printf("     MD5:  mkpasswd -m [-l saltlength] [-s salt] [-p plaintext]\n");
	printf("Blowfish:  mkpasswd -b [-r rounds] [-l saltlength] [-s salt]\n");
	printf("                       [-p plaintext]\n");
	printf("Use -h for full usage\n");
	exit(0);
}
