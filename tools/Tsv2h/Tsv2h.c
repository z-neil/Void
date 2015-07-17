/* Copyright 2014 Neil Edelman, distributed under the terms of the
 GNU General Public License, see copying.txt

 converts a tab-separted-values file into a C header

 @since 2014
 @author Neil */

#include <stdlib.h> /* malloc free */
#include <stdio.h>  /* fprintf */
#include <string.h> /* strncpy, strrchr */
#include <ctype.h>  /* toupper */

/* constants */
static const char *programme   = "Tsv2h";
static const char *year        = "2014";
static const int versionMajor  = 1;
static const int versionMinor  = 0;

/* private */
static void usage(const char *argvz);

/* fixme: this is not the way to do it, it should be a struct */

enum int_types {
	FOREIGN,
	AUTOINCREMENT,
	BMP,
	INT,
	VEC2F,
	STRING
};

static const char *types[] = {
	"foreign",
	"autoincrement",
	"bmp",
	"int",
	"vec2f",
	"string"
};

static const int no_types = sizeof(types) / sizeof(char *);

static const int auto_increment_start = 1;
static const char *delimiters = "\t\n\r";

/** private (entry point)
 @param resouces file
 @param which file is turned into *.h
 @bug the file name should be < 1024 */
int main(int argc, char **argv) {
	/* may need to change the bounds */
	char name[1024], classname[1024], *a, *base, *res_fn, *tsv_fn;
	char read[1024], *line, *word;
	int len, type, first, ai;
	int no_tsv_types = 0;
	int tsv_type[64];
	const int max_tsv_types = sizeof(tsv_type) / sizeof(int);
	FILE *fp;

	/* check that the user specified file and give meaningful names to args */
	if(argc != 3 || *argv[1] == '-' || *argv[2] == '-') {
		usage(argv[0]);
		return EXIT_FAILURE;
	}
	res_fn = argv[1];
	tsv_fn = argv[2];

	/* extract the user-specifed filename; change into a c varname, name */
	if((base = strrchr(tsv_fn, '/'))) {
		base++;
	} else {
		base = tsv_fn;
	}
	strncpy(name, base, sizeof(name) / sizeof(char));
	name[sizeof(name) / sizeof(char) - 1] = '\0';
	for(a = name; *a; a++) {
		if(*a == '.') { /* we don't want any extensions */
			*a = '\0';
			break;
		} else if((*a < 'A' || *a > 'Z')
		   && (*a < 'a' || *a > 'z')
		   && (*a < '0' || *a > '9')) { /* no crazy chars */
			*a = '_';
		}
	}
	strncpy(classname, name, sizeof(classname) / sizeof(char));
	name[sizeof(classname) / sizeof(char) - 1] = '\0';
	for(a = classname; ; ) {
		*a = toupper(*a);
		if(!(a = strchr(a, '_'))) break;
		memmove(a, a + 1, strlen(a + 1) + 1);
	}

	/* open the resources file! */
	if(!(fp = fopen(res_fn, "r"))) {
		perror(res_fn);
		return EXIT_FAILURE;
	}
	printf("/** auto-generated from %s by %s %d.%d\n",
		   base, programme, versionMajor, versionMinor);
	/* line */
	while(fgets(read, sizeof(read) / sizeof(char), fp)) {
		if(!(len = strlen(line = read))) break;
		/* word */
		if(!(word = strsep(&line, delimiters))) {
			continue; /* blank line? */
		} else {
			/* go back to line unless the first word is name */
			if(*word == '#') continue;
			/*printf("cmp(<%s>, <%s>)?\n", word, name);*/
			if(strcmp(word, name)) continue;
		}
		/* we have a hit in the first field; fill up the rest */
		fprintf(stdout, "Type definition <%s>:\n", word);
		while((word = strsep(&line, delimiters)) && *word) {
			/*fprintf(stderr, "read <%s>\n", word);*/
			/* lol; I guess it doesn't have to be fast */
			for(type = 0; type < no_types; type++) {
				/*printf(" cmp(<%s>, <%s>)?\n", word, types[type]);*/
				if(!strcmp(word, types[type])) break;
			}
			/* assume it's a foreign key since it doesn't match */
			if(type >= no_types) type = FOREIGN;
			tsv_type[no_tsv_types] = type;
			no_tsv_types++;
			if(no_tsv_types >= max_tsv_types) {
				fprintf(stderr, "Warning: type truncated at <%s>; %d maximum.\n", word, max_tsv_types);
				break;
			}
			fprintf(stdout, "field %s\n", types[type]);
		}
		printf("end. */\n\n");
		break;
	}
	fclose(fp);

	/* print output */
	printf("static struct %s %s[] = {\n", classname, name);
	if(!(fp = fopen(tsv_fn, "r"))) {
		perror(tsv_fn);
	} else {
		ai = auto_increment_start;
		while(fgets(read, sizeof(read) / sizeof(char), fp)) {
			line = read;
			if(*line == '#') continue; /* lazy */
			if(ai != auto_increment_start) printf(",\n");
			printf("\t{ ");
			if(!(len = strlen(read))) break;
			first = -1;
			for(type = 0; type < no_tsv_types; type++) {
				if(first) {
					first = 0;
				} else {
					printf(", ");
				}
				switch(tsv_type[type]) {
					case FOREIGN:
						/* this is done at runtime by resolve_foreign_keys() */
						if(!(word = strsep(&line, delimiters))) word = "null";
						printf("\"%s\", 0 /* fk */", word);
						break;
					case AUTOINCREMENT:
						/* probably won't use this -- impossible to override
						 specific ones */
						printf("%d", ai);
						break;
					case BMP:
						if(!(word = strsep(&line, delimiters))) word = "null";
						printf("\"%s\", 0 /* bmp */", word);
						break;
					case INT:
						if(!(word = strsep(&line, delimiters))) word = "0";
						printf("%s", word);
						break;
					case VEC2F:
						if(!(word = strsep(&line, delimiters))) word = "0.0f";
						printf("{ %s, ", word);
						if(!(word = strsep(&line, delimiters))) word = "0.0f";
						printf("%s }", word);
						break;
					case STRING:
						if(!(word = strsep(&line, delimiters))) word = "null";
						printf("\"%s\"", word);
				}
			}
			printf(" }");
			ai++;
		}
	}
	printf("\n};\n\n");
	printf("static const int no_%s = sizeof(%s) / sizeof(struct %s);\n\n", name, name, classname);

	return EXIT_SUCCESS;
}

/* private */

static void usage(const char *argvz) {
	fprintf(stderr, "Usage: %s <resources> <tsv>\n", argvz);
	fprintf(stderr, "Converts <tsv> into a C .h according to <resources>.\n");
	fprintf(stderr, "Version %d.%d.\n\n", versionMajor, versionMinor);
	fprintf(stderr, "%s Copyright %s Neil Edelman\n", programme, year);
	fprintf(stderr, "This program comes with ABSOLUTELY NO WARRANTY.\n");
	fprintf(stderr, "This is free software, and you are welcome to redistribute it\n");
	fprintf(stderr, "under certain conditions; see copying.txt.\n\n");
}