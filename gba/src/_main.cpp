#include <tonc.h>

#include "Benchmark.h"
#include "BuildConfig.h"
#include "Palette.h"
#include "Protocol.h"
#include "SPISlave.h"
#include "Utils.h"
#include "_state.h"
#include "gbajam/Demo.h"

extern "C" {
#include "gsmplayer/player.h"
}

#define TRY(ACTION) \
  if (!(ACTION))    \
    goto reset;

// -----
// STATE
// -----

SPISlave* spiSlave = new SPISlave();
DATA_EWRAM u8 compressedPixels[TOTAL_PIXELS];

// ---------
// BENCHMARK
// ---------

#ifdef BENCHMARK
int main() {
  Benchmark::init();
  Benchmark::mainLoop();
  return 0;
}
#endif

// ------------
// DECLARATIONS
// ------------

void init();
void mainLoop();
bool sendKeysAndReceiveMetadata();
bool receiveAudio();
bool receivePixels();
void render();
bool isNewVBlank();
void driveAudio();
u32 transfer(u32 packetToSend, bool withRecovery = true);
bool sync(u32 command);
u32 x(u32 cursor);
u32 y(u32 cursor);

// -----------
// DEFINITIONS
// -----------

#ifndef BENCHMARK
int main() {
  Demo::run();

  init();
  mainLoop();

  return 0;
}
#endif

inline void init() {
  enableMode4AndBackground2();
  overclockEWRAM();
  enableMosaic(DRAW_SCALE_X, DRAW_SCANLINES ? 1 : DRAW_SCALE_Y);
  dma3_cpy(pal_bg_mem, MAIN_PALETTE, sizeof(COLOR) * PALETTE_COLORS);
  // player_init();
}

CODE_IWRAM void mainLoop() {
  state.hasAudio = false;
  state.isVBlank = false;
  state.isAudioReady = false;

reset:
  // transfer(CMD_RESET, false);

  while (true) {
    TRY(sync(CMD_FRAME_START))
    TRY(sendKeysAndReceiveMetadata())
    if (state.hasAudio) {
      TRY(sync(CMD_AUDIO))
      TRY(receiveAudio())
    }
    TRY(sync(CMD_PIXELS))
    TRY(receivePixels())
    TRY(sync(CMD_FRAME_END))

    render();
  }
}

inline bool sendKeysAndReceiveMetadata() {
  u16 keys = pressedKeys();
  u32 metadata = spiSlave->transfer(keys);
  if (spiSlave->transfer(metadata) != keys)
    return false;

  state.expectedPackets = (metadata >> PACKS_BIT_OFFSET) & PACKS_BIT_MASK;
  state.startPixel = metadata & START_BIT_MASK;
  state.hasAudio = (metadata & AUDIO_BIT_MASK) != 0;

  u32 diffsStart = (state.startPixel / 8) / PACKET_SIZE;
  for (u32 i = diffsStart; i < TEMPORAL_DIFF_SIZE / PACKET_SIZE; i++)
    ((u32*)state.temporalDiffs)[i] = transfer(i);

  return true;
}

inline bool receiveAudio() {
  for (u32 i = 0; i < AUDIO_SIZE_PACKETS; i++)
    ((u32*)state.audioChunks)[i] = transfer(i);

  state.isAudioReady = true;

  return true;
}

inline bool receivePixels() {
  for (u32 i = 0; i < state.expectedPackets; i++)
    ((u32*)compressedPixels)[i] = transfer(i);

  return true;
}

inline void render() {
  u32 decompressedPixels = 0;
  u32 cursor = state.startPixel;

#define DRIVE_AUDIO_IF_NEEDED()       \
  if (!(cursor % 8) && isNewVBlank()) \
    driveAudio();
#define DRAW_PIXEL(PIXEL) m4Draw(y(cursor) * DRAW_WIDTH + x(cursor), PIXEL);
#define DRAW_NEXT()                                \
  u8 pixel = compressedPixels[decompressedPixels]; \
  DRAW_PIXEL(pixel);                               \
  decompressedPixels++;                            \
  cursor++;
#define DRAW_BATCH(TIMES)                         \
  u32 target = min(cursor + TIMES, TOTAL_PIXELS); \
  while (cursor < target) {                       \
    DRIVE_AUDIO_IF_NEEDED()                       \
    DRAW_NEXT()                                   \
  }

  while (cursor < TOTAL_PIXELS) {
    DRIVE_AUDIO_IF_NEEDED()
    u32 diffCursor = cursor / 8;
    u32 diffCursorBit = cursor % 8;
    if (diffCursorBit == 0) {
      u32 diffWord, diffHalfWord, diffByte;

      if ((diffWord = ((u32*)state.temporalDiffs)[diffCursor / 4]) == 0) {
        // (none of the next 32 pixels changed)
        cursor += 32;
        continue;
      } else if (diffWord == 0xffffffff) {
        // (all next 32 pixels changed)
        DRAW_BATCH(32)
        continue;
      } else if ((diffHalfWord = ((u16*)state.temporalDiffs)[diffCursor / 2]) ==
                 0) {
        // (none of the next 16 pixels changed)
        cursor += 16;
        continue;
      } else if (diffHalfWord == 0xffff) {
        // (all next 16 pixels changed)
        DRAW_BATCH(16)
        continue;
      } else if ((diffByte = state.temporalDiffs[diffCursor]) == 0) {
        // (none of the 8 pixels changed)
        cursor += 8;
        continue;
      } else if (diffByte == 0xff) {
        // (all next 8 pixels changed)
        DRAW_BATCH(8)
        continue;
      }
    }

    u8 diffByte = state.temporalDiffs[diffCursor];
    if (BIT_IS_HIGH(diffByte, diffCursorBit)) {
      // (a pixel changed)
      DRAW_NEXT()
    } else
      cursor++;
  }
}

inline bool isNewVBlank() {
  if (!state.isVBlank && IS_VBLANK) {
    state.isVBlank = true;
    return true;
  } else if (state.isVBlank && !IS_VBLANK)
    state.isVBlank = false;

  return false;
}

CODE_IWRAM void driveAudio() {
  // if (player_needsData() && state.isAudioReady) {
  //   player_play((const unsigned char*)state.audioChunks, AUDIO_CHUNK_SIZE);
  //   state.isAudioReady = false;
  // }

  // spiSlave->stop();
  // player_run();
  // spiSlave->start();
}

inline u32 transfer(u32 packetToSend, bool withRecovery) {
  bool breakFlag = false;
  u32 receivedPacket =
      spiSlave->transfer(packetToSend, isNewVBlank, &breakFlag);

  if (breakFlag) {
    driveAudio();

    if (withRecovery) {
      sync(CMD_RECOVERY);
      spiSlave->transfer(packetToSend);
      receivedPacket = spiSlave->transfer(packetToSend);
    }
  }

  return receivedPacket;
}

inline bool sync(u32 command) {
  u32 local = command + CMD_GBA_OFFSET;
  u32 remote = command + CMD_RPI_OFFSET;
  bool wasVBlank = IS_VBLANK;

  while (true) {
    bool breakFlag = false;
    bool isOnSync =
        spiSlave->transfer(local, isNewVBlank, &breakFlag) == remote;

    if (breakFlag) {
      driveAudio();
      continue;
    }

    if (isOnSync)
      return true;
    else {
      bool isVBlank = IS_VBLANK;

      if (!wasVBlank && isVBlank)
        wasVBlank = true;
      else if (wasVBlank && !isVBlank)
        return false;
    }
  }
}

inline u32 x(u32 cursor) {
  return (cursor % RENDER_WIDTH) * DRAW_SCALE_X;
}

inline u32 y(u32 cursor) {
  return (cursor / RENDER_WIDTH) * DRAW_SCALE_Y;
}
