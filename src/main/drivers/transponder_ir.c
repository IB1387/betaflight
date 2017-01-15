/*
 * This file is part of Cleanflight.
 *
 * Cleanflight is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Cleanflight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Cleanflight.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <platform.h>

#include "dma.h"
#include "nvic.h"
#include "io.h"
#include "timer.h"
#include "transponder_ir.h"

/*
 * Implementation note:
 * Using around over 700 bytes for a transponder DMA buffer is a little excessive, likely an alternative implementation that uses a fast
 * ISR to generate the output signal dynamically based on state would be more memory efficient and would likely be more appropriate for
 * other targets.  However this approach requires very little CPU time and is just fire-and-forget.
 *
 * On an STM32F303CC 720 bytes is currently fine and that is the target for which this code was designed for.
 */
uint8_t transponderIrDMABuffer[TRANSPONDER_DMA_BUFFER_SIZE];

volatile uint8_t transponderIrDataTransferInProgress = 0;

void transponderIrInit(void)
{
    memset(&transponderIrDMABuffer, 0, TRANSPONDER_DMA_BUFFER_SIZE);

    ioTag_t ioTag = IO_TAG_NONE;
    for (int i = 0; i < USABLE_TIMER_CHANNEL_COUNT; i++) {
        if (timerHardware[i].usageFlags & TIM_USE_TRANSPONDER) {
            ioTag = timerHardware[i].tag;
            break;
        }
    }

    transponderIrHardwareInit(ioTag);
}

bool isTransponderIrReady(void)
{
    return !transponderIrDataTransferInProgress;
}

static uint16_t dmaBufferOffset;

void updateTransponderDMABuffer_erlt(const uint8_t* transponderData)
{
    uint8_t erltIrCode = 63; //test transponder ID
    uint8_t bitIndex;


    //Code = 63 -> ir-data = 111111 -> ir-code = 001111110

    dmaBufferOffset = 0;

    uint8_t i;
    for(i = 0; i < 5; i++) //5 x 50us -> 0
    {
        transponderIrDMABuffer[dmaBufferOffset] = 50; //IR-Led on
        dmaBufferOffset++;
    }

    for(i = 0; i < 5; i++) //5 x 50us -> 0
    {
        transponderIrDMABuffer[dmaBufferOffset] = 0; //IR-Led off
        dmaBufferOffset++;
    }

    for(i = 0; i < 13; i++) //13 x 50us -> 1
    {
        transponderIrDMABuffer[dmaBufferOffset] = 50; //IR-Led on
        dmaBufferOffset++;
    }

    for(i = 0; i < 13; i++) //13 x 50us -> 1
    {
        transponderIrDMABuffer[dmaBufferOffset] = 0; //IR-Led off
        dmaBufferOffset++;
    }

    for(i = 0; i < 13; i++) //13 x 50us -> 1
    {
        transponderIrDMABuffer[dmaBufferOffset] = 50; //IR-Led on
        dmaBufferOffset++;
    }

    for(i = 0; i < 13; i++) //13 x 50us -> 1
    {
        transponderIrDMABuffer[dmaBufferOffset] = 0; //IR-Led off
        dmaBufferOffset++;
    }

    for(i = 0; i < 13; i++) //13 x 50us -> 1
    {
        transponderIrDMABuffer[dmaBufferOffset] = 50; //IR-Led on
        dmaBufferOffset++;
    }

    for(i = 0; i < 13; i++) //13 x 50us -> 1
    {
        transponderIrDMABuffer[dmaBufferOffset] = 0; //IR-Led off
        dmaBufferOffset++;
    }

    for(i = 0; i < 5; i++) //5 x 50us -> 0
    {
        transponderIrDMABuffer[dmaBufferOffset] = 50; //IR-Led on
        dmaBufferOffset++;
    }
}

void updateTransponderDMABuffer_erlt(const uint8_t* transponderData)
{
    uint8_t erltIrCode = ((~transponderData[5]) & 63);

    dmaBufferOffset = 0;
    uint16_t i = 0;

    //Header
    for(i = 0; i < NUM_PERIODS_0; i++) //5 x 50us -> 0
    {
        transponderIrDMABuffer[dmaBufferOffset] = 50; //IR-Led on
        dmaBufferOffset++;
    }
    for(i = 0; i < NUM_PERIODS_0; i++) //5 x 50us -> 0
    {
        transponderIrDMABuffer[dmaBufferOffset] = 0; //IR-Led on
        dmaBufferOffset++;
    }

    //Data bits
    uint8_t bitmask = 32;
    uint8_t numPeriods;
    uint8_t value;
    bool parity = true;

    for(i = 0; i < 6; i++) //For every data bit
    {
        if((erltIrCode & bitmask) == 0) //if erltIrCode bit i is 0
            numPeriods = NUM_PERIODS_0;
        else
        {
            numPeriods = NUM_PERIODS_1;
            parity = !parity;
        }
        bitmask = bitmask >> 1;

        if((i & 1) == 0)
            value = 50;
        else
            value = 0;

        uint8_t j;
        for(j = 0; j < numPeriods; j++) //5 x 50us -> 0
        {
            transponderIrDMABuffer[dmaBufferOffset] = value; //IR-Led on
            dmaBufferOffset++;
        }
    }


    //Parity bit
    if(parity)
        numPeriods = NUM_PERIODS_0;
    else
        numPeriods = NUM_PERIODS_1;

    for(i = 0; i < numPeriods; i++) //5 x 50us -> 0
    {
        transponderIrDMABuffer[dmaBufferOffset] = 50; //IR-Led on
        dmaBufferOffset++;
    }

    //TODO: Make delay 20ms + rnd(0,5)ms long!
    for(i = dmaBufferOffset; i < 400; i++)
    {
        transponderIrDMABuffer[dmaBufferOffset] = 0;
        dmaBufferOffset++;
    }
}

void transponderIrWaitForTransmitComplete(void)
{
    static uint32_t waitCounter = 0;

    while(transponderIrDataTransferInProgress) {
        waitCounter++;
    }
}

void transponderIrUpdateData(const uint8_t* transponderData)
{
    transponderIrWaitForTransmitComplete();

    updateTransponderDMABuffer(transponderData);
}


void transponderIrTransmit(void)
{
    transponderIrWaitForTransmitComplete();

    dmaBufferOffset = 0;

    transponderIrDataTransferInProgress = 1;
    transponderIrDMAEnable();
}
