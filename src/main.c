#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef __WIN32
#include <malloc.h>
#endif

#include "elfload.h"
#include "elfload_dlfcn.h"
#include "elfload_exec.h"
#include "whereami.h"

int main(int argc, char **argv, char **envp)
{
    struct ELF_File *f;
    void **newstack;
    int i, envc, progarg;
    char *dir, *fil, *longnm, *dirs[4];
    int maybe = 0;

    progarg = -1;
    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (!strcmp(argv[i], "-m")) {
                maybe = 1;
            } else if (!strcmp(argv[i], "--")) {
                progarg = i + 1;
                break;
            }
        } else {
            progarg = i;
            break;
        }
    }
    if (progarg == -1) {
        fprintf(stderr, "Use: elfload [-m] <elf file> [arguments]\n");
        return 1;
    }

    whereAmI(argv[0], &dir, &fil);
    elfload_dlinstdir = dir;

    longnm = malloc(1024 * sizeof(char));
    if (longnm == NULL) {
        perror("malloc");
        exit(1);
    }
    sprintf(longnm, "%s/../lib/gelfload", dir);
    dirs[0] = longnm;
    dirs[1] = "./usr/local/lib";
    dirs[2] = "./lib/x86_64-linux-gnu";
    dirs[3] = "./usr/lib/x86_64-linux-gnu";

    /* load them all in */
    f = loadELF(argv[progarg], 4, dirs, maybe);

    if (!f) {
        /* try just execing it */
        execv(argv[progarg], argv + progarg);
        fprintf(stderr, "Failed to load %s.\n", argv[progarg]);
        return 1;
    }

    /* relocate them */
    relocateELFs();

    /* initialize .so files */
    initELF(f);

    /* make its stack */
    for (envc = 0; envp[envc]; envc++);
    newstack = (void**)
        alloca((argc + envc + 2) * sizeof(void*));
    newstack[0] = (void*) (size_t) (argc - progarg);
    for (i = progarg; i < argc; i++) {
        newstack[i - progarg + 1] = (void*) argv[i];
    }
    newstack[i - progarg + 1] = NULL;

    for (i = 0; i < envc; i++) {
        newstack[i-progarg+argc+2] = (void*) envp[i];
    }
    newstack[i-progarg+argc+2] = NULL;

    /* and call it */
    WITHSTACK_JMP(newstack, f->ehdr->e_entry + f->offset);
}
