#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <memory.h>
#include <errno.h>

#include <math.h>
#include <unistd.h>
#include <sys/wait.h>
#include <limits.h>

#define PATH_LEN 2048

char *module_name;

void printError(const char *module_name, const char *error_msg, const char *file_name) {
    printf("%s: %s %s\n", module_name, error_msg, file_name ? file_name : "");
}

char* basename(char *filename){

    char *p = strrchr(filename, '/');
    return p ? p + 1 : filename;
}

double getSinTaylorMember(double x, int member_number) {
    double result = 1;
    for (int i = 1; i <= member_number * 2 + 1; i++) {
        result *= x / i;
    }

    return (member_number % 2) ? -result : result;
}

int writeResult(const int array_size, FILE *tmp_file, const char* result_path) {
    FILE *result_file;
    if (!(result_file = fopen(result_path, "w+"))){
        printError(module_name, "Error with opening file for writing result: ", result_path);
        return 1;
    }

    double *result = (double *)malloc(sizeof(double) * array_size);
    if (!result){
        printError(module_name, "Error with allocation memory for result array.", NULL);
        fclose(result_file);
        return 1;
    }
    memset(result, 0, sizeof(double) * array_size);

    char* full_result_path = (char *)malloc(PATH_LEN * sizeof(char));
    if (!realpath(result_path, full_result_path)) {
        printError(module_name, "Cannot get full path of the file: ", result_path);
        free(full_result_path);
        free(result);
        fclose(result_file);
        return 1;
    }

    int pid, i;
    double member_value;

    fseek(tmp_file, 0, 0);
    errno = 0;
    while (!feof(tmp_file)) {
	while (fscanf(tmp_file, "%d %d %lf", &pid, &i, &member_value) != EOF){
	   result[i] += member_value;	
	}
    }

    for (i = 0; i < array_size; i++) {
        if (fprintf(result_file, "y[%d] = %.9lf\n", i, result[i]) == -1){
            printError(module_name, "Error writing array member to result file.", full_result_path);
            free(full_result_path);
            fclose(result_file);
            return 1;
        };
    }

    if (fclose(result_file) == -1){
        printError(module_name, "Error closing result path.", full_result_path);
        free(result);
        free(full_result_path);
        fclose(result_file);
        return 1;
    }
    if (fclose(tmp_file) == -1){
        printError(module_name, "Error closing temp file.", NULL);
        free(result);
        free(full_result_path);
        fclose(result_file);
        return 1;
    }

    free(result);
    free(full_result_path);
    fclose(result_file);
    return 0;
}

int countFunctionValues(const int array_size, const int taylor_members_count, const char *result_path) {
    FILE *tmp_file;

    if (!(tmp_file = tmpfile())) {
        printError(module_name, "Error with creating temp file.", NULL);
        return 1;
    }

    pid_t pid;
    int run_processes = 0;
    for (int i = 0; i < array_size; i++) {
        double x = (2 * M_PI * i) / array_size;

	if (x != 0){
	   x = M_PI - x;	
	}

        for (int j = 0; j < taylor_members_count; j++) {
            if (run_processes == taylor_members_count){
                // don't save information about *status
                wait(NULL);
                run_processes--;
            }
            pid = fork();

            if (pid == 0) {
                double member = getSinTaylorMember(x, j);
                if (fprintf(tmp_file, "%d %d %.9lf\n", getpid(), i, member) == -1){
                    printError(module_name, "Error writing result to temp file", NULL);
                    return -1;
                };
                printf("%d %d %.9lf\n", getpid(), i, member);
                return 0;

            } else if (pid == -1) {
                printError(module_name, "Error with creating child process.", NULL);
                fclose(tmp_file);
                return 1;

            }
            run_processes++;
        }
    }

    while (wait(NULL) > 0){
        // to finish all children processes
    };

    writeResult(array_size, tmp_file, result_path);
    return 0;
}

int main(int argc, char *argv[]) {
    module_name = basename(argv[0]);

    int array_size,
        taylor_members_count;

    if (argc != 4) {
        printError(module_name, "Wrong number of parameters.", NULL);
        return 1;
    }

    if ((array_size = atoi(argv[1])) < 1) {
        printError(module_name, "Array size must be positive.", NULL);
        return 1;
    };
    if ((taylor_members_count = atoi(argv[2])) < 1) {
        printError(module_name, "Number of taylor members must be positive.", NULL);
        return 1;
    };

    countFunctionValues(array_size, taylor_members_count, argv[3]);
    return 0;
}
