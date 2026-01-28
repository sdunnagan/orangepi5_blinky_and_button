//-----------------------------------------------------------------------------
// File:         button.c
//
// Description:  App that handles a button and LED on breadboard connected
//               RockPro64.
//
// Notes:
// - Uses blocking I/O on /dev/gpio_button for event detection
// - Relies on kernel-managed GPIO via character device + sysfs
// - Implements atomic state toggling using simple integer flip
// - Handles file position reset with lseek() for sysfs writes
// - Guarantees LED turn-off on exit during cleanup
//-----------------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <stdatomic.h>

#define GPIO_BUTTON_DEVICE "/dev/gpio_button"
#define GPIO_LED_SYSFS_PATH "/sys/class/gpio_button/gpio_button_sysfs/led_status"

static volatile sig_atomic_t keep_running = 1;

void sig_handler(int sig)
{
    keep_running = 0;
}

int main()
{
    int button_fd = -1, led_fd = -1;
    char event_flag;
    char led_value[2];
    int current_led_state = 0;
    int retval = EXIT_SUCCESS;

    // Register signal handler.
    struct sigaction sa;
    sa.sa_handler = sig_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGSEGV, &sa, NULL); 

    // Open LED sysfs in read/write mode.
    led_fd = open(GPIO_LED_SYSFS_PATH, O_RDWR);
    if (led_fd < 0) {
        fprintf(stderr, "Failed to open LED sysfs: %s\n", strerror(errno));
        retval = EXIT_FAILURE;
        goto cleanup;
    }

    // Read initial LED state
    if (read(led_fd, led_value, sizeof(led_value)) <= 0) {
        fprintf(stderr, "Failed to read initial LED state: %s\n", strerror(errno));
        retval = EXIT_FAILURE;
        goto cleanup;
    }
    current_led_state = atoi(led_value);

    // Open button device
    button_fd = open(GPIO_BUTTON_DEVICE, O_RDONLY);
    if (button_fd < 0) {
        fprintf(stderr, "Failed to open GPIO button device: %s\n", strerror(errno));
        retval = EXIT_FAILURE;
        goto cleanup;
    }

    printf("LED Control App - Initial State: %d\n", current_led_state);

    while (keep_running) {
        // Block until button event
        if (read(button_fd, &event_flag, sizeof(event_flag)) < 0) {
            if (errno == EINTR) break; // SIGINT received
            fprintf(stderr, "Read error: %s\n", strerror(errno));
            retval = EXIT_FAILURE;
            goto cleanup;
        }

        // Toggle LED state
        current_led_state = !current_led_state;
        snprintf(led_value, sizeof(led_value), "%d", current_led_state);

        // Reset file offset and write to LED sysfs
        if (lseek(led_fd, 0, SEEK_SET) < 0) {
            fprintf(stderr, "lseek failed: %s\n", strerror(errno));
            retval = EXIT_FAILURE;
            goto cleanup;
        }

        if (write(led_fd, led_value, 1) < 0) {
            fprintf(stderr, "LED write failed: %s\n", strerror(errno));
            retval = EXIT_FAILURE;
            goto cleanup;
        }

        printf("LED Toggled → %d\n", current_led_state);
    }

cleanup:
    printf("\nCleaning up...\n");

    if (led_fd >= 0) {
        lseek(led_fd, 0, SEEK_SET);
        write(led_fd, "0", 1);
        close(led_fd);
    }

    if (button_fd >= 0) {
        close(button_fd);
    }

    return retval;
}
