//-----------------------------------------------------------------------------
// File:         blinky.c
//
// Description:  Application that blinks an LED using libgpiod2.
//
// Notes:
// - Uses libgpiod2 API for GPIO control.
// - Requests the GPIO line once, toggles it in a loop.
// - Supports daemon mode (background) or foreground execution (-D).
// - Command-line options to pick chip, line, and interval.
// - Graceful shutdown on SIGINT/SIGTERM; sets line low at exit.
// - Syslog + stderr diagnostics.
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>
#include <signal.h>
#include <gpiod.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#define DEBUG_PRINT(fmt, ...) \
    fprintf(stderr, "%s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#define ERROR_PRINT(fmt, ...) \
    fprintf(stderr, "%s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)

static volatile sig_atomic_t stop_flag = 0;

// Defaults match my breadboard wiring.
static const char *chip_arg = "/dev/gpiochip3";
static int line_offset = 24;

static int interval_ms = 1000;   /* blink period: 1000ms high + 1000ms low */
static int initial_value = 0;    /* start low */
static int active_low = 0;       /* if set, invert electrical level */

/* libgpiod2 objects kept for the whole program lifetime */
static struct gpiod_chip *chip = NULL;
static struct gpiod_line_request *req = NULL;

/* Normalize chip argument: if it's just "gpiochip4", turn into "/dev/gpiochip4" */
static const char *normalize_chip_arg(const char *arg, char *buf, size_t bufsz)
{
    if (!arg) return "/dev/gpiochip4";
    if (strchr(arg, '/')) return arg; /* already a path */
    snprintf(buf, bufsz, "/dev/%s", arg);
    return buf;
}

static void msleep(int ms)
{
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR) {
        /* resume */
    }
}

static int gpio_prepare(void)
{
    int ret = -1;
    char chipbuf[128];
    const char *chip_path = normalize_chip_arg(chip_arg, chipbuf, sizeof(chipbuf));

    /* Open chip */
    chip = gpiod_chip_open(chip_path);
    if (!chip) {
        syslog(LOG_ERR, "gpiod_chip_open(%s) failed: %s", chip_path, strerror(errno));
        ERROR_PRINT("gpiod_chip_open(%s) failed: %s", chip_path, strerror(errno));
        return -1;
    }

    /* Line settings: output, optionally active-low */
    struct gpiod_line_settings *settings = gpiod_line_settings_new();
    if (!settings) {
        syslog(LOG_ERR, "gpiod_line_settings_new() failed");
        ERROR_PRINT("gpiod_line_settings_new() failed");
        goto out_chip;
    }
    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);
    if (active_low)
        gpiod_line_settings_set_active_low(settings, true);

    /* Line config: assign our single line's settings */
    struct gpiod_line_config *lcfg = gpiod_line_config_new();
    if (!lcfg) {
        syslog(LOG_ERR, "gpiod_line_config_new() failed");
        ERROR_PRINT("gpiod_line_config_new() failed");
        gpiod_line_settings_free(settings);
        goto out_chip;
    }
    unsigned int offsets[1] = { (unsigned int)line_offset };
    if (gpiod_line_config_add_line_settings(lcfg, offsets, 1, settings) < 0) {
        syslog(LOG_ERR, "gpiod_line_config_add_line_settings() failed: %s", strerror(errno));
        ERROR_PRINT("gpiod_line_config_add_line_settings() failed: %s", strerror(errno));
        gpiod_line_config_free(lcfg);
        gpiod_line_settings_free(settings);
        goto out_chip;
    }
    gpiod_line_settings_free(settings);

    /* Request config */
    struct gpiod_request_config *rcfg = gpiod_request_config_new();
    if (!rcfg) {
        syslog(LOG_ERR, "gpiod_request_config_new() failed");
        ERROR_PRINT("gpiod_request_config_new() failed");
        gpiod_line_config_free(lcfg);
        goto out_chip;
    }
    gpiod_request_config_set_consumer(rcfg, "blinky");

    /* Make the request */
    req = gpiod_chip_request_lines(chip, rcfg, lcfg);
    gpiod_request_config_free(rcfg);
    gpiod_line_config_free(lcfg);

    if (!req) {
        syslog(LOG_ERR, "gpiod_chip_request_lines() failed on %s offset %d: %s",
               chip_path, line_offset, strerror(errno));
        ERROR_PRINT("gpiod_chip_request_lines() failed on %s offset %d: %s",
                    chip_path, line_offset, strerror(errno));
        goto out_chip;
    }

    /* Set initial value */
    if (gpiod_line_request_set_value(req, line_offset, initial_value) < 0) {
        syslog(LOG_ERR, "Initial set_value failed: %s", strerror(errno));
        ERROR_PRINT("Initial set_value failed: %s", strerror(errno));
        goto out_req;
    }

    return 0;

out_req:
    gpiod_line_request_release(req);
    req = NULL;
out_chip:
    gpiod_chip_close(chip);
    chip = NULL;
    return ret;
}

static void gpio_cleanup(void)
{
    if (req) {
        /* ensure LOW on exit unless active_low wants the opposite */
        int final = 0;
        (void)gpiod_line_request_set_value(req, line_offset, final);
        gpiod_line_request_release(req);
        req = NULL;
    }
    if (chip) {
        gpiod_chip_close(chip);
        chip = NULL;
    }
}

static void *blinky_thread(void *arg)
{
    (void)arg;
    int val = initial_value;

    while (!stop_flag) {
        val = !val;
        if (gpiod_line_request_set_value(req, line_offset, val) < 0) {
            syslog(LOG_ERR, "set_value failed: %s", strerror(errno));
            ERROR_PRINT("set_value failed: %s", strerror(errno));
            break;
        }
        syslog(LOG_DEBUG, "Set gpio %d to %d", line_offset, val);
        msleep(interval_ms);
    }

    /* drive low at exit */
    (void)gpiod_line_request_set_value(req, line_offset, 0);
    return NULL;
}

static void signal_handler(int signo)
{
    (void) signo;
    stop_flag = 1;
}

static void print_usage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s [-D] [-c CHIP] [-l LINE] [-i MS] [-a]\n"
        "  -D        Do not daemonize (stay in foreground)\n"
        "  -c CHIP   GPIO chip path or name (default: /dev/gpiochip4)\n"
        "  -l LINE   GPIO line offset (default: 25)\n"
        "  -i MS     Blink interval in milliseconds (default: 1000)\n"
        "  -a        Active-low (invert electrical level)\n"
        "  -h        Show this help\n",
        prog);
}

int main(int argc, char *argv[])
{
    bool daemonize = true;
    int opt;

    while ((opt = getopt(argc, argv, "Dc:l:i:ah")) != -1) {
        switch (opt) {
        case 'D': daemonize = false; break;
        case 'c': chip_arg = optarg; break;
        case 'l': {
            long v = strtol(optarg, NULL, 0);
            if (v < 0 || v > 1023) { fprintf(stderr, "Bad line: %s\n", optarg); return EXIT_FAILURE; }
            line_offset = (int)v;
            break;
        }
        case 'i': {
            long v = strtol(optarg, NULL, 0);
            if (v < 1 || v > 600000) { fprintf(stderr, "Bad interval: %s\n", optarg); return EXIT_FAILURE; }
            interval_ms = (int)v;
            break;
        }
        case 'a': active_low = 1; break;
        case 'h': print_usage(argv[0]); return EXIT_SUCCESS;
        default:  print_usage(argv[0]); return EXIT_FAILURE;
        }
    }

    /* Signals */
    signal(SIGINT,  signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGSEGV, signal_handler);

    setlogmask(LOG_UPTO(LOG_DEBUG));
    openlog("blinky", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
    syslog(LOG_INFO, "Starting: chip=%s line=%d interval_ms=%d active_low=%d",
           chip_arg, line_offset, interval_ms, active_low);

    if (gpio_prepare() < 0) {
        syslog(LOG_ERR, "GPIO setup failed");
        closelog();
        return EXIT_FAILURE;
    }

    if (daemonize) {
        if (daemon(0, 0) < 0) {
            syslog(LOG_ERR, "daemon() failed: %s", strerror(errno));
            gpio_cleanup();
            closelog();
            return EXIT_FAILURE;
        }
    }

    pthread_t th;
    if (pthread_create(&th, NULL, blinky_thread, NULL) != 0) {
        syslog(LOG_ERR, "pthread_create failed");
        gpio_cleanup();
        closelog();
        return EXIT_FAILURE;
    }

    while (!stop_flag) {
        msleep(200);
    }

    pthread_join(th, NULL);
    gpio_cleanup();
    syslog(LOG_INFO, "Exiting");
    closelog();
    return EXIT_SUCCESS;
}
