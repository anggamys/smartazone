#ifndef SERVICE_UUIDS_H
#define SERVICE_UUIDS_H

// Standard BLE Health Services
namespace BLE_UUIDS {
    // Heart Rate Service
    static const char* HEART_RATE_SERVICE = "0000180D-0000-1000-8000-00805F9B34FB";
    static const char* HEART_RATE_MEASUREMENT = "00002A37-0000-1000-8000-00805F9B34FB";
    
    // Device Information Service
    static const char* DEVICE_INFO_SERVICE = "0000180A-0000-1000-8000-00805F9B34FB";
    static const char* BATTERY_LEVEL = "00002A19-0000-1000-8000-00805F9B34FB";
    
    // Custom service UUIDs (adjust based on your smartwatch)
    static const char* CUSTOM_HEALTH_SERVICE = "12345678-1234-1234-1234-123456789ABC";
    static const char* STEP_COUNT_CHAR = "87654321-4321-4321-4321-CBA987654321";
}

#endif // SERVICE_UUIDS_H
