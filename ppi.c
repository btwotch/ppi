#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>


struct fdinfo
{
	char *path;
	size_t size;
	size_t pos;

};


int termwidth()
{
	struct winsize w;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1)
		return 20; // default value
	
	return w.ws_col;
}


void print_percent_progress(char *txt, int p)
{
	int i, width = termwidth() - 5;
	int begin = strlen(txt)+3;

	printf("%s  ", txt);
	for (i = begin; i < width; i++)
	{
		if (i == begin)
		{
			printf("|");
			continue;
		}
		if (i == width-1)
		{
			printf("|");
			continue;
		}
		if ((100*(i-begin)/(width-begin)) < p)
		{
			if ((100*(i+1-begin)/(width-begin)) < p)
				printf("-");
			else
				printf(">");
		}
		else
			printf(" ");
	}
	if (p > 0 && p < 100)
		printf(" %d%%", p);

	fflush(stdout);
}

void pos1()
{
	printf("\x1b[1G");
}

void print_help()
{
	printf("usage: ppi <pid>\n");
}

char* link_dereference(char *path)
{
	char *ret;
	struct stat sb;

	if (lstat(path, &sb) == -1)
		return NULL;

	ret = calloc(1, sb.st_size+1);

	if (readlink(path, ret, sb.st_size+1) < 0)
	{
		free(ret);
		return NULL;
	}

	ret[sb.st_size] = '\0';

	return ret;
}

size_t file_size(char *path)
{
	struct stat sb;

	stat(path, &sb);

	return sb.st_size;
}

// return NULL if fd is not interesting
struct fdinfo* get_fdinfo(pid_t pid, int fd)
{
	int fdinfo_path_len = strlen("/proc/XXXXX/fdinfo/XXXX"), flags = -1;
	size_t pos = -1, size;
	char fdinfo_path[fdinfo_path_len+1]; // max 9999 fds!!
	char line[1024], *path;
	FILE *fp;
	struct fdinfo* ret;

	snprintf(fdinfo_path, fdinfo_path_len+1, "/proc/%d/fdinfo/%d", pid, fd);

	fp = fopen(fdinfo_path, "r");
	if (fp == NULL)
	{
		fprintf(stderr, "FATAL: could not open %s\n", fdinfo_path);
		return NULL;
	}

	while (!feof(fp))
	{
		fgets(line, 1024, fp);
		if (!strncmp(line, "pos:\t", 5))
			pos = atol(line+5);
		else if (!strncmp(line, "flags:\t", 7))
			flags = atoi(line+7);

		if (flags != -1 && pos != -1)
			break;
	}	

	if (flags & O_WRONLY) // only reads are interesting
		return NULL;

	snprintf(fdinfo_path, fdinfo_path_len+1, "/proc/%d/fd/%d", pid, fd);
	path = link_dereference(fdinfo_path);

	if (path == NULL)
	{
		fprintf(stderr, "FATAL: could not dereference %s\n", fdinfo_path);
		return NULL;
	}
	size = file_size(path);

	if (pos == size || pos == 0 || size == 0) // not interesting
	{
		free(path);
		return NULL;
	}

	ret = malloc(sizeof(struct fdinfo));
	ret->path = path;
	ret->size = size;
	ret->pos = pos;

	return ret;
}

void dump_fdinfo(struct fdinfo* fi)
{
	printf("path: %s size: %ld pos: %ld\n", fi->path, fi->size, fi->pos);

}

void enumerate_fds(pid_t pid)
{
	int proc_fds_len = strlen("/proc/XXXXX/fd"), fd_path_len, fd;
	char proc_fds[proc_fds_len+1];
	char *fd_path = NULL;
	DIR *d;
	struct dirent *de;
	struct stat st;
	struct fdinfo* fi;

	snprintf(proc_fds, proc_fds_len+1, "/proc/%d/fd", pid);

	d = opendir(proc_fds);
	if (d == NULL)
	{
		fprintf(stderr, "FATAL: could not open %s\n", proc_fds);
		return;
	}

	while ((de = readdir(d)) != NULL)
	{
		fd_path_len = strlen(de->d_name)+proc_fds_len+2;
		fd_path = realloc(fd_path, fd_path_len);
		snprintf(fd_path, fd_path_len, "/proc/%d/fd/%s", pid, de->d_name);
		stat(fd_path, &st);
		if (S_ISREG(st.st_mode))
		{
			fd = atoi(de->d_name);
			fi = get_fdinfo(pid, fd);
			if (fi != NULL)
			{
				print_percent_progress(fi->path, (100*fi->pos)/fi->size);
				printf("\n");
				//dump_fdinfo(fi);

				free(fi->path);
				free(fi);
			}

		}
		
		
	}

	if (fd_path != NULL)
		free(fd_path);

	closedir(d);

}


int main(int argc, char **argv)
{
	pid_t pid;

	if (argc != 2)
	{
		print_help();
		exit(1);
	}
	
	pid = atoi(argv[1]);

	enumerate_fds(pid);

	

	return 0;
}
