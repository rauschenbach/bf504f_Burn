@rem Загрузка сразу двух полученных файлов и загрузчика flash CPU
cscript !make_programFlash.vbs  --driver "C:\Program Files (x86)\Analog Devices\VisualDSP 5.1.2\Blackfin\Examples\ADSP-BF506F EZ-KIT Lite\Flash Programmer\BF50x4MBFlash\BF504FEzFlashDriver_BF50x4MBFlash.dxe" --format 1 --offsetL 0x10000 --offsetL2 0x20000 --image %~dp0Release_19MHz\loader.ram %~dp0Release_19MHz\loader.rom --loaderimage %~dp0..\bf504f_loader\Release_19MHz\bootloader.bin --loaderoffset 0 --boot --verifyWrites

