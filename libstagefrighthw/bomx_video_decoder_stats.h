/******************************************************************************
 *    (c)2010-2015 Broadcom Corporation
 *
 * This program is the proprietary software of Broadcom Corporation and/or its licensors,
 * and may only be used, duplicated, modified or distributed pursuant to the terms and
 * conditions of a separate, written license agreement executed between you and Broadcom
 * (an "Authorized License").  Except as set forth in an Authorized License, Broadcom grants
 * no license (express or implied), right to use, or waiver of any kind with respect to the
 * Software, and Broadcom expressly reserves all rights in and to the Software and all
 * intellectual property rights therein.  IF YOU HAVE NO AUTHORIZED LICENSE, THEN YOU
 * HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY WAY, AND SHOULD IMMEDIATELY
 * NOTIFY BROADCOM AND DISCONTINUE ALL USE OF THE SOFTWARE.
 *
 * Except as expressly set forth in the Authorized License,
 *
 * 1.     This program, including its structure, sequence and organization, constitutes the valuable trade
 * secrets of Broadcom, and you shall use all reasonable efforts to protect the confidentiality thereof,
 * and to use this information only in connection with your use of Broadcom integrated circuit products.
 *
 * 2.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
 * AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES, REPRESENTATIONS OR
 * WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH RESPECT TO
 * THE SOFTWARE.  BROADCOM SPECIFICALLY DISCLAIMS ANY AND ALL IMPLIED WARRANTIES
 * OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE,
 * LACK OF VIRUSES, ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION
 * OR CORRESPONDENCE TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING OUT OF
 * USE OR PERFORMANCE OF THE SOFTWARE.
 *
 * 3.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM OR ITS
 * LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL, INDIRECT, OR
 * EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY WAY RELATING TO YOUR
 * USE OF OR INABILITY TO USE THE SOFTWARE EVEN IF BROADCOM HAS BEEN ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES; OR (ii) ANY AMOUNT IN EXCESS OF THE AMOUNT
 * ACTUALLY PAID FOR THE SOFTWARE ITSELF OR U.S. $1, WHICHEVER IS GREATER. THESE
 * LIMITATIONS SHALL APPLY NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF
 * ANY LIMITED REMEDY.
 *
 * $brcm_Workfile: $
 * $brcm_Revision: $
 * $brcm_Date: $
 *
 * Module Description:
 *
 * Revision History:
 *
 * $brcm_Log: $
 *
 *****************************************************************************/
#ifndef BOMX_VIDEO_DECODER_STATS_H__
#define BOMX_VIDEO_DECODER_STATS_H__

#include <utils/Vector.h>
#include <utils/Timers.h>
#include <cutils/properties.h>

#define BOMX_VIDEO_STATS_DEC BOMX_VD_Stats m_stats
#define BOMX_VIDEO_STATS_ADD_EVENT(...) m_stats.addEventEntry(__VA_ARGS__)
#define BOMX_VIDEO_STATS_RESET m_stats.reset()
#define BOMX_VIDEO_STATS_PRINT_BASIC m_stats.printBasicStats()
#define BOMX_VIDEO_STATS_PRINT_DETAILED m_stats.printDetailedStats()

#define BOMX_VDEC_STATS_PROPERTY "media.brcm.vdec_stats.level"
class BOMX_VD_Stats {
public:
    BOMX_VD_Stats(): mStatsLevel(STATS_DISABLED) {
        uint32_t level = (uint32_t)property_get_int32(BOMX_VDEC_STATS_PROPERTY, 0);
        if (level < STATS_LIMIT)
           mStatsLevel = (StatsLevel)level;
    }

    ~BOMX_VD_Stats(){
        reset();
    }

    enum EventType {
        INPUT_FRAME = 0,
        OUTPUT_FRAME = 1,
        DISPLAY_FRAME = 2
    };

    enum StatsLevel {
        STATS_DISABLED = 0,
        STATS_BASIC = 1,
        STATS_DETAILED = 2,
        STATS_LIMIT = 3
    };

    typedef struct EventEntry {
        EventType eventType;
        OMX_TICKS ts;
        nsecs_t rts;
        uint32_t data1;
        uint32_t data2;
        uint32_t data3;
    } EventEntry;

    void addEventEntry(EventType eventType, OMX_TICKS ts,
                      uint32_t data1 = 0, uint32_t data2 = 0, uint32_t data3 = 0) {
        if (mStatsLevel == STATS_DISABLED) return;
        EventData &eventData = eventDataList[eventType];
        EventEntry *entry = eventData.getNew();
        entry->eventType = eventType;
        entry->ts = ts;
        entry->rts = systemTime(CLOCK_MONOTONIC);
        entry->data1 = data1;
        entry->data2 = data2;
        entry->data3 = data3;
        if (eventType == INPUT_FRAME) {
            // Mark the smallest/largest timestamps and accumulate input frame sizes
            // eventData.data1->smallest, eventData.data2->largest
            if (eventData.count() == 1)
                eventData.data1 = eventData.data2 = 0xFFFFFFFF;

            if (!(data1 & OMX_BUFFERFLAG_CODECCONFIG)) {
                if (eventData.data1 == 0xFFFFFFFF) {
                    eventData.data1 = (uint32_t)(ts/1000);
                    eventData.data2 = (uint32_t)(ts/1000);
                }else {
                    if ((uint32_t)(ts/1000) < eventData.data1)
                        eventData.data1 = (uint32_t)(ts/1000);
                    if ((uint32_t)(ts/1000) > eventData.data2)
                        eventData.data2 = (uint32_t)(ts/1000);
                }
            }
            // Should be the total number of bytes
            eventData.counter += data2;
        }
    }

    void reset() {
        if (mStatsLevel == STATS_DISABLED) return;
        for (uint32_t i = 0; i < sizeof(eventDataList)/sizeof(eventDataList[0]); ++i){
            EventData &eventData = eventDataList[i];
            eventData.reset();
        }
    }

    void printBasicStats() {
        if (mStatsLevel < STATS_BASIC) return;
        // Input data stats
        ALOGD("=== Input data ===");
        EventData *eventData = &eventDataList[INPUT_FRAME];
        ALOGD("Total Frames:%u", eventData->count());
        if (eventData->count() > 0) {
            EventEntry *entryFirst = eventData->getByIndex(0);
            EventEntry *entryLast = eventData->getByIndex(eventData->count() - 1);
            int msecDelay = toMillisecondTimeoutDelay(entryFirst->rts, entryLast->rts);
            ALOGD("First frame ts:%u, rcvd at time:0 msec, last frame ts:%u rcvd at time:%d msec",
                  (uint32_t)(entryFirst->ts/1000), (uint32_t)(entryLast->ts/1000), msecDelay);
            uint32_t fps = (eventData->count() * 1000) /(eventData->data2 - eventData->data1);
            uint64_t bitrate = ((uint64_t)(eventData->counter * 8 * 1000))/(eventData->data2 - eventData->data1);
            ALOGD("Smallest ts:%u, largest ts:%u, bitrate:%u, fps:%u",
                  eventData->data1, eventData->data2, (uint32_t)bitrate, fps);
        }

        ALOGD("=== Output data ===");
        eventData = &eventDataList[OUTPUT_FRAME];
        if (eventData->count() > 0) {
            EventEntry *entryFirst = eventData->getByIndex(0);
            EventEntry *entryLast = eventData->getByIndex(eventData->count() - 1);
            ALOGD("Total Frames:%u", eventData->count());
            ALOGD("First frame, ts:%u, serial:%u. Last frame, ts:%u, serial:%u",
                            (uint32_t)(entryFirst->ts/1000), entryFirst->data1,
                            (uint32_t)(entryLast->ts/1000), entryLast->data1);
        }

        ALOGD("=== Display data ===");
        eventData = &eventDataList[DISPLAY_FRAME];
        ALOGD("Total Frames:%u", eventData->count());
        if (eventData->count() > 0) {
            // Count number of dropped frames
            uint32_t droppedFrames = 0;
            uint32_t lastSerial = 0;
            for (uint32_t idx = 0; idx < eventData->count(); ++idx) {
                EventEntry *entry = eventData->getByIndex(idx);
                ALOG_ASSERT(entry);
                if (entry->data1 == 0) {
                    ++droppedFrames;
                    lastSerial = entry->data2;
                }
            }
            ALOGD("Dropped frames:%u Last dropped frame:%u", droppedFrames, lastSerial);
        }
    }

    void printDetailedStats() {
        time_t rawtime;
        struct tm * timeinfo;
        char fname[100];

        if (mStatsLevel < STATS_DETAILED) return;
        EventData *eventData = &eventDataList[INPUT_FRAME];
        if (eventData->count() == 0){
            ALOGE("%s: no data!", __FUNCTION__);
            return;
        }

        // Generate unique file name
        time (&rawtime);
        timeinfo = localtime (&rawtime);
        strftime (fname, sizeof(fname),"/data/nxmedia/vdec-%F_%H_%M_%S.txt", timeinfo);
        ALOGD("%s, output file:%s", __FUNCTION__, fname);
        FILE *fd = fopen(fname, "w");
        if (fd == NULL){
            ALOGE("%s: error creating file", __FUNCTION__);
            return;
        }

        // Two data sets.
        // 1. Time-mark | input-ts
        // 2. Out-time-mark | output-ts | serial | displayed | in-time-mark | delta
        fprintf(fd, "==============================\n");
        fprintf(fd, "Time-mark | input-ts | avail-input\n");
        // First entry of input data must be our earliest available time mark in the stats
        EventEntry *entry = eventData->getByIndex(0);
        nsecs_t baseTime = entry->rts;
        for (uint32_t idx = 0; idx < eventData->count(); ++idx) {
            entry = eventData->getByIndex(idx);
            if (entry->data1 & OMX_BUFFERFLAG_CODECCONFIG)
                continue;
            int msecDelay = toMillisecondTimeoutDelay(baseTime, entry->rts);
            fprintf(fd, "%9u%11u%14u\n", msecDelay, (uint32_t)(entry->ts/1000), entry->data3);
        }

        fprintf(fd, "\n\n==============================\n");
        fprintf(fd, "Out-time-mark | output-ts | serial | in-time-mark | out-in-delta | display | display-time-mark | display-out-delta\n");
        eventData = &eventDataList[OUTPUT_FRAME];
        for (uint32_t idx = 0; idx < eventData->count(); ++idx) {
            entry = eventData->getByIndex(idx);
            int msecDelay = toMillisecondTimeoutDelay(baseTime, entry->rts);
            // find out if frame was displayed.
            // Ugly inner loop for now.
            uint32_t displayed = 0;
            int mSecDelayDisp = 0;
            EventData *dispEventData = &eventDataList[DISPLAY_FRAME];
            for (uint32_t j=0; j<dispEventData->count(); ++j) {
                EventEntry *e = dispEventData->getByIndex(j);
                if (e->data2 == entry->data1){
                    displayed = e->data1;
                    mSecDelayDisp = toMillisecondTimeoutDelay(baseTime, e->rts);
                    break;
                }
            }
            // another ugly inner loop
            int mSecDelayInput = 0;
            EventData *inEventData = &eventDataList[INPUT_FRAME];
            for (uint32_t j=0; j<inEventData->count(); ++j) {
                EventEntry *e = inEventData->getByIndex(j);
                if (e->ts == entry->ts){
                    mSecDelayInput = toMillisecondTimeoutDelay(baseTime, e->rts);
                    break;
                }
            }

            fprintf(fd, "%13u%12u%9u%15u%15u%10u%20u%20u\n",
                    msecDelay, (uint32_t)(entry->ts/1000), entry->data1, mSecDelayInput,
                    msecDelay-mSecDelayInput, displayed, mSecDelayDisp, mSecDelayDisp-msecDelay);
        }
        fclose(fd);
    }

private:
    // evil constructors
    BOMX_VD_Stats(const BOMX_VD_Stats&);
    void operator=(const BOMX_VD_Stats&);

    static const uint32_t KEntriesPerBlock = 1024;
    class EventData {
    public:
        EventData():data1(0),data2(0),counter(0),index(0){}

        EventEntry *getNew(){
            if ((index % KEntriesPerBlock) == 0) {
                EventEntry *block = new EventEntry[KEntriesPerBlock];
                ALOG_ASSERT(block);
                list.push(block);
            }
            EventEntry *entry = list[list.size() - 1];
            entry += index++ % KEntriesPerBlock;
            return entry;
        }

        EventEntry *getByIndex(uint32_t idx){
            if ((list.size() == 0) || (idx >= index))
                return NULL;
            EventEntry *block = list[idx / KEntriesPerBlock];
            return (block + idx % KEntriesPerBlock);
        }

        inline uint32_t count(){
            return index;
        }

        void reset(){
            index = 0;
            data1 = data2 = 0;
            counter = 0;
            for (uint32_t j = 0; j < list.size(); ++j) {
                EventEntry *block = list[j];
                delete [] block;
            }
            list.clear();
        }

    public:
        uint32_t data1, data2;
        uint64_t counter;

    private:
        uint32_t index;
        Vector<EventEntry*> list;
    };
    EventData eventDataList[3];
    StatsLevel mStatsLevel;
};

#endif //BOMX_VIDEO_DECODER_STATS_H__
