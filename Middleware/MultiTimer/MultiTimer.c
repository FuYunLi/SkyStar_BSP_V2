/*
 * Copyright (c) 2021 0x1abin
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "MultiTimer.h"

static MultiTimer             *timerList             = NULL;
static PlatformTicksFunction_t platformTicksFunction = NULL;

int multiTimerInstall(PlatformTicksFunction_t ticksFunc)
{
    if (ticksFunc == NULL)
    {
        return -1;
    }
    platformTicksFunction = ticksFunc;
    return 0;
}

static void removeTimer(MultiTimer *timer)
{
    MultiTimer **current = &timerList;
    while (*current)
    {
        if (*current == timer)
        {
            *current = timer->next;
            break;
        }
        current = &(*current)->next;
    }
}

int multiTimerStart(MultiTimer *timer, uint64_t timing, MultiTimerCallback_t callback, void *userData)
{
    if (!timer || !callback || platformTicksFunction == NULL)
    {
        return -1;
    }

    removeTimer(timer);

    timer->deadline = platformTicksFunction() + timing;
    timer->callback = callback;
    timer->userData = userData;

    MultiTimer **current = &timerList;
    while (*current && ((*current)->deadline < timer->deadline))
    {
        current = &(*current)->next;
    }
    timer->next = *current;
    *current    = timer;

    return 0;
}

int multiTimerStop(MultiTimer *timer)
{
    removeTimer(timer);
    return 0;
}

int multiTimerYield(void)
{
    if (platformTicksFunction == NULL)
    {
        return -1;
    }
    uint64_t currentTicks = platformTicksFunction();
    while (timerList && (currentTicks >= timerList->deadline))
    {
        MultiTimer *timer = timerList;
        timerList         = timer->next;

        if (timer->callback)
        {
            timer->callback(timer, timer->userData);
        }
    }
    return timerList ? (int)(timerList->deadline - currentTicks) : 0;
}
