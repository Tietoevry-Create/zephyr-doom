/*
 * Zephyr RTOS I2S-based backend for Doom audio (nRF5340 → PCM5102A / MAX98357).
 *
 * Keeps the N_I2S_* API used by the Doom mixer, but routes audio via Zephyr's
 * i2s_buf_write() from a dedicated high-priority feeder thread. If no mixed
 * block is available, the feeder writes silence to avoid underruns.
 */

#include "n_i2s.h"

#include <stdio.h>
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2s.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/printk.h>

#define SAMPLE_RATE 11025
#define SAMPLE_BIT_WIDTH 16
#define NUM_CHANNELS 2
#define BYTES_PER_SAMPLE 2

/* BUFFER_SIZE is the number of int16_t samples in a block (interleaved L/R). */
#define BUFFER_SIZE 512 /* 256 frames x stereo */
#define NUM_BUFFERS 5
#define BUFFER_SIZE_BYTES (BUFFER_SIZE * BYTES_PER_SAMPLE)

typedef enum {
    BUF_EMPTY = 0,
    BUF_FILLED = 1,
} buffer_state_t;

static boolean i2s_started;

static int16_t sampleBuffers[NUM_BUFFERS][BUFFER_SIZE];
static buffer_state_t bufferStates[NUM_BUFFERS];
static int queuedBuffer;
static int16_t zeros[BUFFER_SIZE];

/* Zephyr I2S device + TX slab for the driver's internal queue */
#define I2S_DEV DT_NODELABEL(i2s0)
K_MEM_SLAB_DEFINE(tx_mem_slab, BUFFER_SIZE_BYTES, 4, 4);

#define AUDIO_STACK_SIZE 2048
#define AUDIO_PRIO 0
K_THREAD_STACK_DEFINE(audio_stack, AUDIO_STACK_SIZE);
static struct k_thread audio_thread;

static const struct device *i2s_dev;
static atomic_t g_recoveries = ATOMIC_INIT(0);

static void zero_buffer(int bufIdx) {
    int j;
    for (j = 0; j < BUFFER_SIZE; j++) {
        sampleBuffers[bufIdx][j] = 0;
    }
}

static void clear_buffers(void) {
    int i;
    for (i = 0; i < NUM_BUFFERS; i++) {
        zero_buffer(i);
        bufferStates[i] = BUF_EMPTY;
    }
}

static int i2s_write_block(void *payload) {
    int r;
    do {
        r = i2s_buf_write(i2s_dev, payload, BUFFER_SIZE_BYTES);
        if (r == -EAGAIN) {
            k_sleep(K_MSEC(1));
        }
    } while (r == -EAGAIN);
    return r;
}

static void audio_feeder(void *a, void *b, void *c) {
    struct i2s_config cfg;
    int idx;
    void *payload;
    int r;

    i2s_dev = DEVICE_DT_GET(I2S_DEV);
    if (!device_is_ready(i2s_dev)) {
        printk("I2S not ready\n");
        return;
    }

    cfg.word_size = SAMPLE_BIT_WIDTH;
    cfg.channels = NUM_CHANNELS;
    cfg.format = I2S_FMT_DATA_FORMAT_I2S;
    cfg.options = I2S_OPT_BIT_CLK_MASTER | I2S_OPT_FRAME_CLK_MASTER;
    cfg.frame_clk_freq = SAMPLE_RATE;
    cfg.mem_slab = &tx_mem_slab;
    cfg.block_size = BUFFER_SIZE_BYTES;
    cfg.timeout = 50;
    if (i2s_configure(i2s_dev, I2S_DIR_TX, &cfg) < 0) {
        printk("i2s_configure failed\n");
        return;
    }

    /* Prefill with silence to prime the queue */
    (void)i2s_write_block(zeros);
    (void)i2s_write_block(zeros);

    if (i2s_trigger(i2s_dev, I2S_DIR_TX, I2S_TRIGGER_START) < 0) {
        printk("I2S START failed\n");
        return;
    }

    for (;;) {
        /* Advance ring index */
        queuedBuffer = (queuedBuffer + 1) % NUM_BUFFERS;
        idx = queuedBuffer;

        if (bufferStates[idx] == BUF_FILLED) {
            payload = sampleBuffers[idx];
        } else {
            payload = zeros; /* fallback to silence */
        }

        r = i2s_write_block(payload);
        if (r < 0) {
            atomic_inc(&g_recoveries);
            printk("audio: write=%d, recovering\n", r);
            (void)i2s_trigger(i2s_dev, I2S_DIR_TX, I2S_TRIGGER_STOP);
            (void)i2s_trigger(i2s_dev, I2S_DIR_TX, I2S_TRIGGER_DROP);
            (void)i2s_write_block(zeros);
            (void)i2s_write_block(zeros);
            (void)i2s_trigger(i2s_dev, I2S_DIR_TX, I2S_TRIGGER_START);
        }

        /* Recycle the buffer slot after enqueue (copy-based API) */
        if (payload == sampleBuffers[idx]) {
            bufferStates[idx] = BUF_EMPTY;
        }

        k_yield();
    }
}

void N_I2S_init(void) {
    printf("N_I2S_init (Zephyr I2S)\n");
    memset(zeros, 0, sizeof(zeros));
    clear_buffers();
    queuedBuffer = 0;
    i2s_started = false;
}

boolean N_I2S_next_buffer(int *buf_size, int16_t **buffer) {
    int next;
    next = (queuedBuffer + 1) % NUM_BUFFERS;
    if (bufferStates[next] == BUF_EMPTY) {
        *buf_size = BUFFER_SIZE;
        *buffer = sampleBuffers[next];
        bufferStates[next] = BUF_FILLED;
        return true;
    }
    return false;
}

void N_I2S_process(void) {
    if (!i2s_started) {
        /* Start feeder with higher priority than the main/game thread */
        k_thread_create(&audio_thread, audio_stack,
                        K_THREAD_STACK_SIZEOF(audio_stack), audio_feeder, NULL,
                        NULL, NULL, AUDIO_PRIO, 0, K_NO_WAIT);
        i2s_started = true;
    }
}
