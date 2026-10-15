/*
 * Writes a file and reads it back, which also shows that A: writes work
 * (they live in the emulator's RAM disk and disappear on reboot).
 */
#include <stdio.h>

int main(void)
{
    FILE *file;
    char line[80];
    int i;

    file = fopen("TEST.TXT", "w");
    if (file == NULL)
    {
        printf("cannot create TEST.TXT\n");
        return 1;
    }
    for (i = 1; i <= 5; i++)
        fprintf(file, "line %d written by FILEIO.EXE\r\n", i);
    fclose(file);

    file = fopen("TEST.TXT", "r");
    if (file == NULL)
    {
        printf("cannot read TEST.TXT back\n");
        return 1;
    }
    while (fgets(line, sizeof(line), file) != NULL)
        fputs(line, stdout);
    fclose(file);

    printf("TEST.TXT is %ld bytes on the RAM disk\n", (long)ftell(file));
    return 0;
}
