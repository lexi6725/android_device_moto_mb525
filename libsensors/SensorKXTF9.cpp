/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <poll.h>
#include <unistd.h>
#include <dirent.h>
#include <stdlib.h>
#include <sys/select.h>

//#define LOG_NDEBUG 0
#include <cutils/log.h>

#include "kernel/kxtf9.h"

#include "SensorKXTF9.h"

/*****************************************************************************/

SensorKXTF9::SensorKXTF9() : SensorBase(KXTF9_DEVICE_NAME, "accelerometer"),
    mEnabled(0),
    mInputReader(32)
{
    mPendingEvent.version = sizeof(sensors_event_t);
    mPendingEvent.sensor = SENSOR_TYPE_ACCELEROMETER;
    mPendingEvent.type = SENSOR_TYPE_ACCELEROMETER;
    mPendingEvent.acceleration.status = SENSOR_STATUS_ACCURACY_HIGH;

    openDevice();

    mEnabled = isEnabled();

    if (!mEnabled) {
        closeDevice();
    }
}

SensorKXTF9::~SensorKXTF9()
{
}

int SensorKXTF9::enable(int32_t handle, int en)
{
    int err = 0;
    int newState = en ? 1 : 0;

    if (mEnabled == (unsigned)newState) {
        return err;
    }

    if (!mEnabled) {
        openDevice();
    }

    err = ioctl(dev_fd, KXTF9_IOCTL_SET_ENABLE, &newState);
    err = err < 0 ? -errno : 0;
    LOGE_IF(err, "SensorKXTF9: KXTF9_IOCTL_SET_ENABLE failed (%s)", strerror(-err));

    if (!err || !newState) {
        mEnabled = newState;
        err = setDelay(handle, KXTF9_DEFAULT_DELAY);
    }

    if (!mEnabled) {
        closeDevice();
    }

    return err;
}

int SensorKXTF9::setDelay(int32_t handle, int64_t ns)
{
    int err = 0;

    if (mEnabled) {
        if (ns < 0) {
            return -EINVAL;
        }

        int delay = ns / 1000000;

        err = ioctl(dev_fd, KXTF9_IOCTL_SET_DELAY, &delay);
        err = err < 0 ? -errno : 0;
        LOGE_IF(err, "SensorKXTF9: KXTF9_IOCTL_SET_DELAY failed (%s)", strerror(-err));
    }

    return err;
}

int SensorKXTF9::readEvents(sensors_event_t* data, int count)
{
    if (count < 1) {
        return -EINVAL;
    }

    ssize_t n = mInputReader.fill(data_fd);
    if (n < 0) {
        return n;
    }

    int numEventReceived = 0;
    input_event const* event;

    while (count && mInputReader.readEvent(&event)) {
        switch (event->type) {
            case EV_ABS:
                processEvent(event->code, event->value);
                break;
            case EV_SYN:
                mPendingEvent.timestamp = timevalToNano(event->time);
                *data++ = mPendingEvent;
                count--;
                numEventReceived++;
                break;
            default:
                LOGE("SensorKXTF9: unknown event (type=0x%x, code=0x%x, value=0x%x)",
                        event->type, event->code, event->value);
                break;
        }
        mInputReader.next();
    }

    return numEventReceived;
}

void SensorKXTF9::processEvent(int code, int value)
{
    int state;

    mPendingEvent.orientation.status = 0;
    switch (code) {
        case ABS_X:
            mPendingEvent.type = SENSOR_TYPE_ACCELEROMETER;
            mPendingEvent.acceleration.x = value * KXTF9_CONVERT_A_X;
            break;
        case ABS_Y:
            mPendingEvent.type = SENSOR_TYPE_ACCELEROMETER;
            mPendingEvent.acceleration.y = value * KXTF9_CONVERT_A_Y;
            break;
        case ABS_Z:
            mPendingEvent.type = SENSOR_TYPE_ACCELEROMETER;
            mPendingEvent.acceleration.z = value * KXTF9_CONVERT_A_Z;
            break;

        case ABS_MISC: //0x28
            mPendingEvent.orientation.status = SENSOR_STATUS_ACCURACY_HIGH;
            mPendingEvent.type = SENSOR_TYPE_ORIENTATION;
/*
  Orientation event values:

  Pitch(Y)
  0x01 screen down                0°
  0x02 screen up                  0°

  Roll (Z)
  0x04 portrait, normal use       0°
  0x08 portrait, reversed       180°
  0x10 landscape, usb port up   270° (-90°)
  0x20 landscape, usb port down  90°

*/
            state = value & KXTF9_SENSOR_ROTATION_MASK; //0x3F

            // roll is the orientation used to rotate screen
            if (state == 0x04)
                mPendingEvent.orientation.roll = 0;
            else if (state == 0x8)
                mPendingEvent.orientation.roll = 2;
            else if (state == 0x10)
                mPendingEvent.orientation.roll = 3;
            else if (state == 0x20)
                mPendingEvent.orientation.roll = 1;

            // pitch is the other horizontal rotation
            mPendingEvent.orientation.pitch = (value & 0x3);

            // azimuth (vertical rotation) is not handled by this sensor event

            LOGV("SensorKXTF9: orientation event (code=0x%x, value=0x%x) state=0x%x", code, value, state);

            break;
        default:
            LOGE("SensorKXTF9: unknown event (code=0x%x, value=0x%x)", code, value);
            break;
    }
}

int SensorKXTF9::isEnabled()
{
    int err = 0;
    int enabled = 0;

    err = ioctl(dev_fd, KXTF9_IOCTL_GET_ENABLE, &enabled);
    err = err < 0 ? -errno : 0;
    LOGE_IF(err, "SensorKXTF9: KXTF9_IOCTL_GET_ENABLE failed (%s)", strerror(-err));

    return (err ? 0 : enabled);
}
