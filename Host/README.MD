# CFU Sample Drivers 
The sample CFU drivers are based on the Microsoft Devices Driver Module Framework (DMF). This folder contains:
    Sample CFU driver (ComponentFirmwareUpdateDriver) and 
    Examples of adopting the driver to enable firmware update scenarios using the componentized and monolothic packaging techniques.

Two Scenarios are:
1. A Laptop MCU Firmware Update, to update a MCU and it's 2 sub-components, in a hypothetical laptop.
2. A Dock Firmware Update:, to update a MCU and its sub-component ,in a hypothetical Dock.

# Folders

ComponentFirmwareUpdateDriver: Sample CFU Driver  
ComponentizedPackageExample: 2 Examples based on Componentized Packages which uses Extension INF.
MonolithicPackageExample: 2 Examples monolothic Packages.
DMF:  This folder should contain the DMF source code. Use 'git submodule update' to pull the DMF contents.


# How to use this?

Use any of the below Solutions to see the examples of how to use the sample driver using Extension INF.

ComponentizedPackageExample\DockFWUpdate\DockFirmwareUpdateWithExtension.sln
ComponentizedPackageExample\LaptopMCUFWUpdate\LaptopMCUFirmwareUpdateWithExtension.sln


Use any of the below Solutions to see the examples of how to use the sample driver in a single package.

MonolithicPackageExample\DockFWUpdate\DockFirmwareUpdate.sln
MonolithicPackageExample\LaptopMCUFWUpdate\LaptopMCUFirmwareUpdate.sln
