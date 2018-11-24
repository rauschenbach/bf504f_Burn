@rem Загрузка загрузчика на вн. flash CPU
@if exist .\Release_19MHz\loader.rom (
cscript !make_programFlash.vbs  --driver "C:\Program Files (x86)\Analog Devices\VisualDSP 5.0\Blackfin\Examples\ADSP-BF506F EZ-KIT Lite\Flash Programmer\BF50x4MBFlash\BF504FEzFlashDriver_BF50x4MBFlash.dxe" --format 1 --offsetL 0x10000 --offsetL2 0x20000 --image %~dp0Release_19MHz\loader.ram %~dp0Release_19MHz\loader.rom --boot --verifyWrites
) else echo "Error! File loader.rom doesn't exist!"


