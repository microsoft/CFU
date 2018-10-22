# Tools related to Component Firware Update
# Contents
This folder contains 
- A sample CFU driver and 
- Examples of adopting the driver to enable firmware update scenarios using the componentized and monolothic packaging techniques.

# CFU Standalone tool sample

The CFU driver sample sends firmware image file(s) to a device in need of an update. Before sending the firmware image, the driver sends several commands to the device with firmware offers. Only if the device accepts, the driver sends the firmware payload. The communication between the driver and the device is in accordance with the [CFU protocol](https://github.com/Microsoft/CFU/tree/master/Documentation/CFU-Protocol), an open source specification (included with CFU) based on the HID protocol.

The CFU driver sample is implemented using the [User-Mode Driver Framework](https://docs.microsoft.com/en-us/windows-hardware/drivers/wdf/overview-of-the-umdf) 2.0 model and based on the the [Microsoft Devices Driver Module Framework (DMF)](https://blogs.windows.com/buildingapps/2018/08/15/introducing-driver-module-framework/).
As a firmware developer, you can customize the driver for the purposes of adopting the CFU model to enable firmware updates for your component(s). For more information, see [Customizing the Component Firmware Update Driver](https://github.com/Microsoft/CFU/blob/master/Documentation/CFU-Driver/cfu-driver.md).

# Build the sample

First, install Visual Studio and Windows Driver Kit (WDK) following the steps [here](https://docs.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk).
