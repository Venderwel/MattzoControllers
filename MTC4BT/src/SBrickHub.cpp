#include <Arduino.h>

#include "SBrickHub.h"

#define MAX_SBRICK_CHANNEL_COUNT 4

static BLEUUID remoteControlServiceUUID(SBRICK_REMOTECONTROL_SERVICE_UUID);
static BLEUUID remoteControlCharacteristicUUID(SBRICK_REMOTECONTROL_CHARACTERISTIC_UUID);

const int8_t CMD_BRAKE = 0;
const int8_t CMD_DRIVE = 1;
const int8_t CMD_SET_WATCHDOG_TIMEOUT = 13;
const int8_t CMD_GET_WATCHDOG_TIMEOUT = 14;
const int8_t CMD_BREAK_WITH_PM = 19;
const int8_t CMD_GET_CHANNEL_STATUS = 34;
const int16_t SBRICK_MAX_CHANNEL_SPEED = 254;
const int16_t SBRICK_MIN_CHANNEL_SPEED = -254;

SBrickHub::SBrickHub(BLEHubConfiguration *config)
    : BLEHub(config)
{
}

bool SBrickHub::SetWatchdogTimeout(const uint8_t watchdogTimeOutInTensOfSeconds)
{
    _watchdogTimeOutInTensOfSeconds = watchdogTimeOutInTensOfSeconds;

    if (!attachCharacteristic(remoteControlServiceUUID, remoteControlCharacteristicUUID))
    {
        return false;
    }

    if (!_remoteControlCharacteristic->canWrite())
    {
        return false;
    }

    uint8_t byteWrite[2] = {CMD_SET_WATCHDOG_TIMEOUT, watchdogTimeOutInTensOfSeconds};
    if (!_remoteControlCharacteristic->writeValue(byteWrite, sizeof(byteWrite), false))
    {
        return false;
    }

    uint8_t byteRead[1] = {CMD_GET_WATCHDOG_TIMEOUT};
    if (!_remoteControlCharacteristic->writeValue(byteRead, sizeof(byteRead), false))
    {
        return false;
    }

    Serial.print("[" + String(xPortGetCoreID()) + "] BLE : Watchdog timeout successfully set to s/10: ");
    Serial.println(_remoteControlCharacteristic->readValue<uint8_t>());

    return true;
}

void SBrickHub::DriveTaskLoop()
{
    for (;;)
    {
        if (!_ebreak)
        {
            // Update current channel speeds, if needed.
            for (int channel = 0; channel < _channelControllers.size(); channel++)
            {
                _channelControllers.at(channel)->UpdateCurrentSpeedPerc();

                // int16_t rawspd = MapSpeedPercToRaw(_channelControllers.at(channel)->GetCurrentSpeedPerc());
                // if (rawspd != 0)
                // {
                //     Serial.print(channel);
                //     Serial.print(": rawspd=");
                //     Serial.println(rawspd);
                // }
            }
        }

        // Construct drive command.
        uint8_t byteCmd[13] = {
            CMD_DRIVE,
            HubChannel::A,
            channelIsDrivingForward(HubChannel::A),
            getRawChannelSpeed(HubChannel::A),
            HubChannel::B,
            channelIsDrivingForward(HubChannel::B),
            getRawChannelSpeed(HubChannel::B),
            HubChannel::C,
            channelIsDrivingForward(HubChannel::C),
            getRawChannelSpeed(HubChannel::C),
            HubChannel::D,
            channelIsDrivingForward(HubChannel::D),
            getRawChannelSpeed(HubChannel::D)};

        // Send drive command.
        if (!_remoteControlCharacteristic->writeValue(byteCmd, sizeof(byteCmd), false))
        {
            Serial.println("Drive failed");
        }

        // Wait half the watchdog timeout (converted from s/10 to s/1000).
        vTaskDelay(_watchdogTimeOutInTensOfSeconds * 50 / portTICK_PERIOD_MS);
    }
}

int16_t SBrickHub::MapSpeedPercToRaw(int speedPerc)
{
    // Map absolute speed (no matter the direction) to raw channel speed.
    return map(abs(speedPerc), 0, 100, 0, SBRICK_MAX_CHANNEL_SPEED);
}

std::array<uint8_t, 3> SBrickHub::getDriveCommand(HubChannel channel)
{
    ChannelController *controller = findControllerByChannel(channel);

    std::array<uint8_t, 3> cmd;
    cmd[0] = channel;
    cmd[1] = controller ? controller->IsDrivingForward() : false;
    cmd[2] = controller ? MapSpeedPercToRaw(controller->GetCurrentSpeedPerc()) : 0;

    return cmd;
}

bool SBrickHub::channelIsDrivingForward(HubChannel channel)
{
    ChannelController *controller = findControllerByChannel(channel);
    return controller ? controller->IsDrivingForward() : false;
}

uint8_t SBrickHub::getRawChannelSpeed(HubChannel channel)
{
    ChannelController *controller = findControllerByChannel(channel);
    return controller ? getRawChannelSpeedForController(controller) : 0;
}