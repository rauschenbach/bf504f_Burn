@echo Компиляция программы
@echo off
@set PATH=c:\Windows\system32
@if not exist Release_8MHz mkdir Release_8MHz
rem @if not exist burn_8MHz mkdir burn_8MHz 
..\Utils\gmake-378.exe quartz_freq=8MHz

