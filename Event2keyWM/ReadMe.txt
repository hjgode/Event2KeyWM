# WIN32 APPLICATION : Event2keyWM Project Overview

Application that rewrites the registry entries for the center scan button
and then watches the new events to issue a Key press.

The lower left side button will be mapped to be a scan button.

## Installation

Copy DEMO_MapScanKey_v2.cab to the device and start FileBrowser to locate 
DEMO_MapScanKey_v2.cab. Then tap on DEMO_MapScanKey_v2.cab to start the installation.

After installation start the app once manually using Start-Programs-Event2KeyWM.
Then warm boot the device to activate the changes. The DEMO application will be
autostarted using the Windows\StartUp folder.

## Example

The default registry for the center scan button is

	Drivers\HID\ClientDrivers\ITCKeyboard\Layout\CN75AN5-Numeric\0001\Events\Delta:Event1=DeltaLeftScan
	Drivers\HID\ClientDrivers\ITCKeyboard\Layout\CN75AN5-Numeric\0001\Events\State:Event1=StateLeftScan

The application changes those to

	Drivers\HID\ClientDrivers\ITCKeyboard\Layout\CN75AN5-Numeric\0001\Events\Delta:Event1=DeltaMappedToKey
	Drivers\HID\ClientDrivers\ITCKeyboard\Layout\CN75AN5-Numeric\0001\Events\State:Event1=StateMappedToKey

Now, whenever the center scan button is pressed, the application sees the DeltaMappedToKey event and 
then uses keybd_event to simulate a keypress (KeyDown and KeyUp) for the defined VKEY value.

The lower left side button (00,91) will be mapped to Event2 and Event2 will be 
changed to DeltaLeftScan and StateLeftScan.
 
## Configuration

    REGEDIT4
    [HKEY_LOCAL_MACHINE\Software\Intermec\Event2Key]
    "mapToVKEY"=dword:0000000D
    "eventName"="Event1"

mapToVKEY is the value of the VKEY to be fired

eventName is the standard name of the event entries used by the center
scan button

## Uninstall

Go to Start-Task Manager and SwithTo Event2keyWM. Then tap OK to end Event2keyWM.
 
Go to Start-Settings-System-Remove Programs and remove "hsm DEMO MapScanKey".

Go to Start-Settings-System and launch the Keyboard Remapper utility. Then
select Menu-Restore Defaults.

