@rem Загрузка сразу двух полученных файлов и загрузчика flash CPU
cscript !make_programFlash.vbs  --driver "C:\Program Files (x86)\Analog Devices\VisualDSP 5.0\Blackfin\Examples\ADSP-BF506F EZ-KIT Lite\Flash Programmer\BF50x4MBFlash\BF504FEzFlashDriver_BF50x4MBFlash.dxe" --format 1 --offsetL 0x10000 --offsetL2 0x20000 --image %~dp0Release_8MHz\loader.ram %~dp0Release_8MHz\loader.rom --loaderimage %~dp0..\bf504f_loader\Release_8MHz\bootloader.bin --loaderoffset 0 --boot --verifyWrites

