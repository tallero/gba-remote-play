#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

// Render modes:
// - Low quality (fast):
//   120x80, with DRAW_SCALE=2
// - High quality (slow):
//   240x160, with DRAW_SCALE=1
//   [!]                                                         [!]
//     When using this mode, move `compressedPixels` outside State
//     (from IWRAM to EWRAM). Otherwise, it'll crash.

// RENDER
#define RENDER_WIDTH 120
#define RENDER_HEIGHT 160
#define TOTAL_PIXELS (RENDER_WIDTH * RENDER_HEIGHT)
#define DRAW_SCALE_X 2
#define DRAW_SCALE_Y 1
#define DRAW_SCANLINES true
#define DRAW_WIDTH (RENDER_WIDTH * DRAW_SCALE_X)
#define DRAW_HEIGHT (RENDER_HEIGHT * DRAW_SCALE_Y)
#define TOTAL_SCREEN_PIXELS (DRAW_WIDTH * DRAW_HEIGHT)
#define PALETTE_COLORS 256

// TRANSFER
#define PACKET_SIZE 4
#define COLOR_SIZE 2
#define PIXEL_SIZE 1
#define AUDIO_CHUNK_SIZE 1980   // (sizeof(gsm_frame) * 60)
#define AUDIO_CHUNK_PADDING 0   // (so every chunk it's exactly 495 packets)
#define AUDIO_SIZE_PACKETS 495  // -----------------------------^^^
#define SPI_MODE 3
#define TRANSFER_SYNC_PERIOD 32
#define COLORS_PER_PACKET (PACKET_SIZE / COLOR_SIZE)
#define PIXELS_PER_PACKET (PACKET_SIZE / PIXEL_SIZE)
#define MAX_PIXELS_SIZE (TOTAL_PIXELS / PIXELS_PER_PACKET)
#define AUDIO_PADDED_SIZE (AUDIO_CHUNK_SIZE + AUDIO_CHUNK_PADDING)

// DIFFS
#define TEMPORAL_DIFF_SIZE (TOTAL_PIXELS / 8)
#define TEMPORAL_DIFF_PADDED_SIZE (TEMPORAL_DIFF_SIZE + 4)
#define MAX_RLE 255

// FILES
#define CONFIG_FILENAME "config.cfg"
#define PALETTE_CACHE_FILENAME "palette.cache"

// COMMANDS
#define AUDIO_BIT_MASK 0b10000000000000000000000000000000
#define COMPR_BIT_MASK 0b01000000000000000000000000000000
#define PACKS_BIT_MASK 0b00000000000000000001111111111111
#define START_BIT_MASK 0b00000000000000001111111111111111
#define PACKS_BIT_OFFSET 16
#define CMD_RESET 0x99887766
#define CMD_RPI_OFFSET 1
#define CMD_GBA_OFFSET 2
#define CMD_FRAME_START 0x12345610
#define CMD_AUDIO 0x12345620
#define CMD_PIXELS 0x12345630
#define CMD_FRAME_END 0x12345640
#define CMD_RECOVERY 0x98765490

#endif  // PROTOCOL_H
