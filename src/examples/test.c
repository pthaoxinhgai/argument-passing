#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[]) {
    // Check if there are at least two arguments (program name and at least one argument)
    if (argc >= 2) {
        size_t total_memory = 0;

        // Print the program name
        printf(" - Program name: %s\n", argv[0]);

        // Calculate and print memory usage for the program name
        size_t program_name_size = strlen(argv[0]) + 1; // +1 for the null terminator
        printf("   Memory for program name: %zu bytes\n", program_name_size);
        total_memory += program_name_size;

        // Print all the arguments starting from the second one
        printf(" - Arguments:\n");
        for (int i = 1; i < argc; i++) {
            size_t argument_size = strlen(argv[i]) + 1; // +1 for the null terminator
            printf("  + Argument %d: %s (Memory: %zu bytes)\n", i, argv[i], argument_size);
            total_memory += argument_size;
        }

        // Calculate memory for pointers in argv
        size_t pointers_memory = argc * sizeof(char *);
        printf("   Memory for pointers (argv): %zu bytes\n", pointers_memory);
        total_memory += pointers_memory;

        // Print total memory usage
        printf(" - Total memory used: %zu bytes\n", total_memory);
    } else {
        // Print an error message if there are not enough arguments
        printf("Error: Insufficient arguments\n");
    }

    return 0;
}
