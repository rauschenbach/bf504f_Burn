@echo Компиляция программы
@echo off
@set PATH=c:\Windows\system32
@if not exist Release_19MHz mkdir Release_19MHz
rem @if not exist burn_19MHz mkdir burn_19MHz 
..\Utils\gmake-378.exe quartz_freq=19MHz


