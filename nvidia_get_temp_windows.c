/*
  Retreive Hotspot, GPU and Mem junction for nvidia cards on windows
 */

#include <windows.h>
#include <stdio.h>


#define NVAPI_INIT_STRUCT(s,v) { ZeroMemory(&s,sizeof(s)); s.version = sizeof(s) | (v<<16); }
#define MAKE_NVAPI_VERSION(typeName,ver) (NvU32)(sizeof(typeName) | ((ver)<<16))

#define NVAPI_MAX_THERMAL_SENSORS_PER_GPU       3
#define NV_GPU_THERMAL_THERM_CHANNEL_MAX        32
#define NVAPI_MAX_PHYSICAL_GPUS                 64
#define NVAPI_OK                                0

typedef UINT32 NvAPI_Status;
typedef UINT32 NvU32;

typedef enum
{
    NVAPI_THERMAL_CONTROLLER_NONE = 0,
    NVAPI_THERMAL_CONTROLLER_GPU_INTERNAL,
    NVAPI_THERMAL_CONTROLLER_ADM1032,
    NVAPI_THERMAL_CONTROLLER_MAX6649,
    NVAPI_THERMAL_CONTROLLER_MAX1617,
    NVAPI_THERMAL_CONTROLLER_LM99,
    NVAPI_THERMAL_CONTROLLER_LM89,
    NVAPI_THERMAL_CONTROLLER_LM64,
    NVAPI_THERMAL_CONTROLLER_ADT7473,
    NVAPI_THERMAL_CONTROLLER_SBMAX6649,
    NVAPI_THERMAL_CONTROLLER_VBIOSEVT,
    NVAPI_THERMAL_CONTROLLER_OS,
    NVAPI_THERMAL_CONTROLLER_UNKNOWN = -1,
} NV_THERMAL_CONTROLLER;

typedef enum
{
    NVAPI_THERMAL_TARGET_NONE = 0,
    NVAPI_THERMAL_TARGET_GPU = 1,              //!< GPU core temperature requires NvPhysicalGpuHandle
    NVAPI_THERMAL_TARGET_MEMORY = 2,           //!< GPU memory temperature requires NvPhysicalGpuHandle
    NVAPI_THERMAL_TARGET_POWER_SUPPLY = 4,     //!< GPU power supply temperature requires NvPhysicalGpuHandle
    NVAPI_THERMAL_TARGET_BOARD = 8,            //!< GPU board ambient temperature requires NvPhysicalGpuHandle
    NVAPI_THERMAL_TARGET_VCD_BOARD = 9,        //!< Visual Computing Device Board temperature requires NvVisualComputingDeviceHandle
    NVAPI_THERMAL_TARGET_VCD_INLET = 10,       //!< Visual Computing Device Inlet temperature requires NvVisualComputingDeviceHandle
    NVAPI_THERMAL_TARGET_VCD_OUTLET = 11,      //!< Visual Computing Device Outlet temperature requires NvVisualComputingDeviceHandle

    NVAPI_THERMAL_TARGET_ALL = 15,
    NVAPI_THERMAL_TARGET_UNKNOWN = -1,
} NV_THERMAL_TARGET;

typedef struct
{
    NvU32   version;                //!< structure version
    NvU32   count;                  //!< number of associated thermal sensors
    struct
    {
        NV_THERMAL_CONTROLLER       controller;        //!< internal, ADM1032, MAX6649...
        NvU32                       defaultMinTemp;    //!< The min default temperature value of the thermal sensor in degree Celsius
        NvU32                       defaultMaxTemp;    //!< The max default temperature value of the thermal sensor in degree Celsius
        NvU32                       currentTemp;       //!< The current temperature value of the thermal sensor in degree Celsius
        NV_THERMAL_TARGET           target;            //!< Thermal sensor targeted @ GPU, memory, chipset, powersupply, Visual Computing Device, etc.
    } sensor[NVAPI_MAX_THERMAL_SENSORS_PER_GPU];

} NV_GPU_THERMAL_SETTINGS_V1;

typedef struct
{
    NvU32   version;                                //!< structure version
    NvU32   mask;                                   //!< a combination of NV_THERMAL_CONTROLLER
    NvU32   count[8];                               //!< number of associated thermal sensors
    NvU32   temp[NV_GPU_THERMAL_THERM_CHANNEL_MAX]; // !< temperature readings
} NV_GPU_THERMAL_SENSORS_V2;

typedef struct {
    int unused;
} NvPhysicalGpuHandle;

// function pointer types
typedef PVOID (*NvAPI_QueryInterface_t)(unsigned int offset);
typedef NvAPI_Status (*NvAPI_Initialize_t)();
typedef NvAPI_Status (*NvAPI_EnumPhysicalGPUs_t)(NvPhysicalGpuHandle nvGPUHandle[NVAPI_MAX_PHYSICAL_GPUS], NvU32* gpuCount);
typedef NvAPI_Status (*NvAPI_GPU_GetThermalSettings_t)(NvPhysicalGpuHandle gpu, int sensorIndex, NV_GPU_THERMAL_SETTINGS_V1* temp);
typedef NvAPI_Status (*NvAPI_GPU_ThermalGetSensors_t)(NvPhysicalGpuHandle gpu, NV_GPU_THERMAL_SENSORS_V2* thermalSensor);

NvAPI_QueryInterface_t      NvAPI_QueryInterface = NULL;
NvAPI_Initialize_t          NvAPI_Initialize = NULL;
NvAPI_EnumPhysicalGPUs_t    NvAPI_EnumPhysicalGPUs = NULL;
NvAPI_GPU_ThermalGetSensors_t NvAPI_GPU_ThermalGetSensors = NULL;
NvAPI_GPU_GetThermalSettings_t NvAPI_GPU_GetThermalSettings = NULL;

int main()
{
    HMODULE hmod = LoadLibrary(L"nvapi64.dll");
    if (!hmod){
        printf("Cannot load library nvapi64.dll\n");
        return 1;
    }
    NvAPI_QueryInterface = (NvAPI_QueryInterface_t)GetProcAddress(hmod, "nvapi_QueryInterface");

    NvAPI_Initialize = (NvAPI_Initialize_t)(*NvAPI_QueryInterface)(0x0150E828);
    NvAPI_EnumPhysicalGPUs = (NvAPI_EnumPhysicalGPUs_t)(*NvAPI_QueryInterface)(0xE5AC921F);
    NvAPI_GPU_ThermalGetSensors = (NvAPI_GPU_ThermalGetSensors_t)(*NvAPI_QueryInterface)(0x65FE3AAD);
    NvAPI_GPU_GetThermalSettings = (NvAPI_GPU_GetThermalSettings_t)(*NvAPI_QueryInterface)(0xE3640A56);

    // check if functions are valid (they might fail if driver is not properly installed)
    if (!NvAPI_Initialize || !NvAPI_EnumPhysicalGPUs || !NvAPI_GPU_GetThermalSettings || !NvAPI_GPU_ThermalGetSensors)
    {
        goto cleanup;
    }

    (*NvAPI_Initialize)();

    NvPhysicalGpuHandle     npgpu[NVAPI_MAX_PHYSICAL_GPUS];
    NvU32                   gpuCount = 0;
    NvAPI_Status            status = 0;
    UINT32                  _thermalSensorsMask;

    NV_GPU_THERMAL_SETTINGS_V1 thermalSettings = {
        .count = NVAPI_MAX_THERMAL_SENSORS_PER_GPU,
        .version = MAKE_NVAPI_VERSION(NV_GPU_THERMAL_SETTINGS_V1, 1),
        .sensor = 0,
    };


    NvAPI_EnumPhysicalGPUs(npgpu, &gpuCount);
    status = NvAPI_GPU_GetThermalSettings(npgpu[0], NVAPI_THERMAL_TARGET_ALL, &thermalSettings);

    // Stolen from librehardwaremonitor
    for (int thermalSensorsMaxBit = 0; thermalSensorsMaxBit < 32; thermalSensorsMaxBit++) {
        _thermalSensorsMask = 1u << thermalSensorsMaxBit;

        NV_GPU_THERMAL_SENSORS_V2 thermalSensors = {
            .mask = _thermalSensorsMask,
            .version = MAKE_NVAPI_VERSION(NV_GPU_THERMAL_SENSORS_V2, 2),
        };

        status = NvAPI_GPU_ThermalGetSensors(npgpu[0], &thermalSensors);
        if (status != NVAPI_OK) {
            _thermalSensorsMask--;
            break;
        }
    }

    NV_GPU_THERMAL_SENSORS_V2 thermalSensors = {
        .mask = _thermalSensorsMask,
        .version = MAKE_NVAPI_VERSION(NV_GPU_THERMAL_SENSORS_V2, 2),
    };

    for (;;) {
        status = NvAPI_GPU_ThermalGetSensors(npgpu[0], &thermalSensors);
        if (status != NVAPI_OK)
            break;

        printf("GPU: %.1f C\n", thermalSensors.temp[0] / 256.f);
        printf("Hotspot: %.1f C\n", thermalSensors.temp[1] / 256.f);
        printf("MEM junction: %.1f C\n", thermalSensors.temp[9] / 256.f);
        printf("All sensors:\n");

        for (size_t i = 0; i < NV_GPU_THERMAL_THERM_CHANNEL_MAX; i++) {
            if (thermalSensors.temp[i])
                printf("Sensor %d: %.1f C\n", i, thermalSensors.temp[i] / 256.f);
        }

        Sleep(1000);
    }

cleanup:
    FreeLibrary(hmod);

    return 0;
}
